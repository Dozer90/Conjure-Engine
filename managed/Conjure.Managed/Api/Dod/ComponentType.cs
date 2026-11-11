using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;
using Conjure.Interop;
using Conjure.Runtime;

namespace Conjure.Dod;

/// <summary>
/// The native component id of a mirror or [EcsComponent] struct, resolved once per type and per native
/// handshake (NativeBridge.Generation: every script_host::initialize / shutdown may bring a registry with
/// other ids). 0 means the mirror does not match the native layout (docs/design/03_scripting.md 3.9,
/// R61): RequireId then throws InvalidOperationException, and 0 is never cached.
/// </summary>
public static unsafe class ComponentType<T> where T : unmanaged
{
    private static uint _id;
    private static int _generation = -1;

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
        NativeComponentAttribute? mirror = typeof(T).GetCustomAttribute<NativeComponentAttribute>();
        if (mirror != null)
        {
            if (api->ComponentLookupType == null)
            {
                return 0;
            }
            List<ComponentFieldLayout> layout;
            int size;
            try
            {
                layout = LayoutHash.Describe(typeof(T), out size);
            }
            catch (InvalidOperationException exception)
            {
                NativeBridge.Log(LogLevel.Error, "[NativeComponent] " + typeof(T).FullName + ": " + exception.Message);
                return 0;
            }
            // A tag mirror (no fields) matches a native tag: size 0 and no reflected layout (hash 0).
            ulong layoutHash = layout.Count == 0 ? 0UL : LayoutHash.Compute(layout);
            return api->ComponentLookupType(NameHash.Fnv1a64(mirror.RegisteredName), (uint)size, layoutHash);
        }
        if (typeof(T).GetCustomAttribute<EcsComponentAttribute>() != null)
        {
            ScriptCatalog? catalog = AssemblyLoader.CurrentCatalog;
            return catalog == null ? 0 : catalog.ComponentIdOf(typeof(T));
        }
        return 0;
    }
}
