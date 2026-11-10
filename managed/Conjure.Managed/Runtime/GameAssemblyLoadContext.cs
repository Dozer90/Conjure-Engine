using System;
using System.IO;
using System.Reflection;
using System.Runtime.Loader;

namespace Conjure.Runtime;

/// <summary>
/// The collectible load context of one game assembly (docs/design/03_scripting.md 2.3). Conjure.Managed
/// resolves to the SDK already loaded in the default context so ScriptBehaviour keeps one identity; the
/// game's own dependencies load from bytes so dotnet build can overwrite the files while the editor runs;
/// everything else falls through to the default context.
/// </summary>
public sealed class GameAssemblyLoadContext : AssemblyLoadContext
{
    private readonly AssemblyDependencyResolver _resolver;

    public GameAssemblyLoadContext(string gameAssemblyPath) :
        base("conjure-game:" + Path.GetFileNameWithoutExtension(gameAssemblyPath), isCollectible: true)
    {
        GameAssemblyPath = gameAssemblyPath;
        _resolver = new AssemblyDependencyResolver(gameAssemblyPath);
    }

    public string GameAssemblyPath { get; }

    /// <summary>
    /// Loads the game assembly itself from bytes (with its .pdb when present).
    /// </summary>
    public Assembly LoadGameAssembly()
    {
        byte[] bytes = File.ReadAllBytes(GameAssemblyPath);
        string pdbPath = Path.ChangeExtension(GameAssemblyPath, ".pdb");
        using MemoryStream assemblyStream = new MemoryStream(bytes, writable: false);
        if (File.Exists(pdbPath))
        {
            using MemoryStream pdbStream = new MemoryStream(File.ReadAllBytes(pdbPath), writable: false);
            return LoadFromStream(assemblyStream, pdbStream);
        }
        return LoadFromStream(assemblyStream);
    }

    protected override Assembly? Load(AssemblyName assemblyName)
    {
        if (assemblyName.Name == "Conjure.Managed")
        {
            return typeof(ScriptBehaviour).Assembly;
        }
        string? path = _resolver.ResolveAssemblyToPath(assemblyName);
        if (path == null)
        {
            return null;
        }
        using MemoryStream stream = new MemoryStream(File.ReadAllBytes(path), writable: false);
        return LoadFromStream(stream);
    }

    protected override IntPtr LoadUnmanagedDll(string unmanagedDllName)
    {
        string? path = _resolver.ResolveUnmanagedDllToPath(unmanagedDllName);
        if (path == null)
        {
            return IntPtr.Zero;
        }
        return LoadUnmanagedDllFromPath(path);
    }
}
