using Conjure.Interop;

namespace Conjure;

/// <summary>
/// Prefab instantiation through the bindings' prefab_* entries (NotSupportedException until the
/// bindings fill them, D11). Instances exist at once; their scripts are created in the current phase's
/// flush.
/// </summary>
public static unsafe class Prefab
{
    public static Entity Instantiate(UuidData prefab, Entity parent = default)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PrefabInstantiate, NativeFunction.PrefabInstantiate);
        ulong root = 0;
        api->PrefabInstantiate(&prefab, parent.Id, null, &root);
        return new Entity(root);
    }

    public static Entity Instantiate(UuidData prefab, in TransformData local, Entity parent = default)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PrefabInstantiate, NativeFunction.PrefabInstantiate);
        ulong root = 0;
        fixed (TransformData* pinned = &local)
        {
            api->PrefabInstantiate(&prefab, parent.Id, pinned, &root);
        }
        return new Entity(root);
    }

    public static bool IsInstanceRoot(Entity entity)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PrefabIsInstanceRoot, NativeFunction.PrefabIsInstanceRoot);
        return api->PrefabIsInstanceRoot(entity.Id) > 0;
    }
}
