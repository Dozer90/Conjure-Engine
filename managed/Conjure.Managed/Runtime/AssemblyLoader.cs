using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;
using Conjure.Interop;

namespace Conjure.Runtime;

/// <summary>
/// The load/unload state machine of the game assembly (docs/design/03_scripting.md 3.8): Load makes a
/// new context current and keeps the previous one until native, having destroyed its instances, calls
/// UnloadPrevious. Unload completion is detected with a WeakReference and a bounded GC loop; a context
/// that survives is a zombie, counted for the editor.
/// </summary>
public static class AssemblyLoader
{
    public const int MaxCollectPasses = 10;

    private static GameAssemblyLoadContext? _current;
    private static GameAssemblyLoadContext? _previous;
    private static ScriptCatalog? _previousCatalog;
    private static readonly List<WeakReference> _zombies = new List<WeakReference>();

    /// <summary>
    /// The catalog of the current context; null before the first load.
    /// </summary>
    public static ScriptCatalog? CurrentCatalog { get; private set; }

    public static string? CurrentPath => _current?.GameAssemblyPath;

    public static bool HasPrevious => _previous != null;

    /// <summary>
    /// Load contexts that did not unload (dead references are pruned on every query).
    /// </summary>
    public static int ZombieCount
    {
        get
        {
            _zombies.RemoveAll(reference => !reference.IsAlive);
            return _zombies.Count;
        }
    }

    /// <summary>
    /// Loads a game assembly into a new collectible context and makes it current. Throws when the file
    /// cannot be loaded or its catalog cannot be built; the current context is then unchanged.
    /// </summary>
    public static ScriptCatalog Load(string path)
    {
        GameAssemblyLoadContext context = new GameAssemblyLoadContext(path);
        ScriptCatalog catalog;
        try
        {
            Assembly assembly = context.LoadGameAssembly();
            catalog = ScriptCatalog.Build(assembly);
        }
        catch
        {
            context.Unload();
            throw;
        }

        if (_previous != null)
        {
            // Two loads without an unload between them: retire the older one now.
            UnloadPrevious();
        }
        _previous = _current;
        _previousCatalog = CurrentCatalog;
        _current = context;
        CurrentCatalog = catalog;
        return catalog;
    }

    /// <summary>
    /// Unloads the context retired by the last Load. Returns 0 when it was collected, 1 when it leaked.
    /// </summary>
    public static int UnloadPrevious()
    {
        if (_previous == null)
        {
            return 0;
        }
        _previousCatalog?.Drop();
        _previousCatalog = null;
        WeakReference reference = UnloadCore(ref _previous);
        return WaitForCollection(reference) ? 0 : 1;
    }

    /// <summary>
    /// Unloads every context (shutdown). Returns the number that leaked.
    /// </summary>
    public static int UnloadAll()
    {
        int leaked = UnloadPrevious();
        if (_current != null)
        {
            CurrentCatalog?.Drop();
            CurrentCatalog = null;
            WeakReference reference = UnloadCore(ref _current);
            if (!WaitForCollection(reference))
            {
                ++leaked;
            }
        }
        return leaked;
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static WeakReference UnloadCore(ref GameAssemblyLoadContext? field)
    {
        GameAssemblyLoadContext context = field!;
        field = null;
        WeakReference reference = new WeakReference(context);
        context.Unload();
        return reference;
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static bool WaitForCollection(WeakReference reference)
    {
        for (int pass = 0; pass < MaxCollectPasses && reference.IsAlive; ++pass)
        {
            GC.Collect();
            GC.WaitForPendingFinalizers();
        }
        if (reference.IsAlive)
        {
            _zombies.Add(reference);
            NativeBridge.Log(LogLevel.Warn,
                "a game assembly load context is still alive after unload; usual culprits: static fields, timers, running threads");
            return false;
        }
        return true;
    }
}
