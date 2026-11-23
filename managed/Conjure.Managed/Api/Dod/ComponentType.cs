using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;
using Conjure.Interop;
using Conjure.Runtime;

namespace Conjure.Dod;

/// <summary>
/// The native component id of a mirror or [EcsComponent] struct, resolved once per type and per native
/// handshake (NativeBridge.Generation: every script_host::initialise / shutdown may bring a registry with
/// other ids). 0 means the mirror does not match the native layout (docs/design/03_scripting.md 3.9,
/// R61): RequireId then throws InvalidOperationException, and 0 is never cached. The reflected part (the
/// attributes, the layout and its hash) never changes for a type, so it is computed, and an undescribable
/// layout logged, once per type; an unresolved id only repeats the cheap native lookup.
/// </summary>
public static unsafe class ComponentType<T> where T : unmanaged
{
    private static uint _id;
    private static int _generation = -1;

    private static bool _described;
    private static bool _isMirror;
    private static bool _isEcsComponent;
    private static bool _layoutInvalid;
    private static ulong _nameHash;
    private static uint _size;
    private static ulong _layoutHash;

    public static uint Id
    {
        get
        {
            if (_id == 0 || _generation != NativeBridge.Generation)
            {
                _generation = NativeBridge.Generation;
                _id = Resolve();
            }
            return _id;
        }
    }

    public static uint RequireId()
    {
        uint id = Id;
        if (id == 0)
        {
            throw new InvalidOperationException(
                typeof(T).FullName + " is not a registered component or its layout does not match the native type");
        }
        return id;
    }

    private static uint Resolve()
    {
        NativeApi* api = NativeBridge.Api;
        if (api == null)
        {
            return 0;
        }
        if (!_described)
        {
            Describe();
        }
        if (_isMirror)
        {
            if (_layoutInvalid || api->ComponentLookupType == null)
            {
                return 0;
            }
            return api->ComponentLookupType(_nameHash, _size, _layoutHash);
        }
        if (_isEcsComponent)
        {
            ScriptCatalog? catalog = AssemblyLoader.CurrentCatalog;
            return catalog == null ? 0 : catalog.ComponentIdOf(typeof(T));
        }
        return 0;
    }

    /// <summary>
    /// Fills the reflected cache. _described is set only once every field of it is written, so an unexpected
    /// exception leaves the type undescribed and the next access reflects, and throws, again.
    /// </summary>
    private static void Describe()
    {
        NativeComponentAttribute? mirror = typeof(T).GetCustomAttribute<NativeComponentAttribute>();
        if (mirror == null)
        {
            _isEcsComponent = typeof(T).GetCustomAttribute<EcsComponentAttribute>() != null;
            _described = true;
            return;
        }
        ulong nameHash = NameHash.Fnv1a64(mirror.RegisteredName);
        List<ComponentFieldLayout> layout;
        int size;
        try
        {
            layout = LayoutHash.Describe(typeof(T), out size);
        }
        catch (InvalidOperationException exception)
        {
            NativeBridge.Log(LogLevel.Error, "[NativeComponent] " + typeof(T).FullName + ": " + exception.Message);
            _isMirror = true;
            _layoutInvalid = true;
            _described = true;
            return;
        }
        // A tag mirror (no fields) matches a native tag: size 0 and no reflected layout (hash 0).
        ulong layoutHash = layout.Count == 0 ? 0UL : LayoutHash.Compute(layout);
        _nameHash = nameHash;
        _size = (uint)size;
        _layoutHash = layoutHash;
        _isMirror = true;
        _described = true;
    }
}
