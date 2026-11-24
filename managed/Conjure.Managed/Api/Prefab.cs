using System;
using Conjure.Dod;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// A prefab asset and its instantiation through the prefab_* entries (docs/design/15_bindings.md 2.5). The
/// asset must be resident (AssetNotLoadedException otherwise; Load it first and wait for IsLoaded).
/// Instances exist at once and the roots carry PrefabInstance; inside a query scope the instantiation is
/// deferred to the scope end and the returned ids become alive then (R5). AddComponent, SetComponent,
/// RemoveComponent, AddScript, Destroy and the Name, Tag, Layer and Enabled setters on such a root are recorded
/// behind the instantiation and applied in call order when the scope closes (RemoveComponent always returns true
/// then: the prefab's components are not known before the flush). The scripts AddScript records join the ones
/// the prefab brings, and those that would take the root past the maximum of 8 are dropped at the scope end with
/// a warning. A Destroy cancels the instantiation: neither the root nor any entity of the prefab is created. The
/// facade writes (Transform and the rest) on the root are made right after the scope closed, in call order.
/// </summary>
public readonly struct Prefab
{
    public readonly AssetRef<PrefabAsset> Asset;

    public Prefab(AssetRef<PrefabAsset> asset)
    {
        Asset = asset;
    }

    public Prefab(AssetId id)
    {
        Asset = new AssetRef<PrefabAsset>(id);
    }

    public bool IsValid => Asset.IsValid;

    public bool IsLoaded => Asset.IsLoaded;

    /// <summary>
    /// Requests the asset (AssetRef.Load).
    /// </summary>
    public void Load(AssetLoadPriority priority = AssetLoadPriority.Normal) => Asset.Load(priority);

    /// <summary>
    /// Instantiates the prefab under 'parent' (Entity.Null = a new root) with the root's local pose from the
    /// asset or 'local' when given; returns the instance root.
    /// </summary>
    public unsafe Entity Instantiate(Entity parent = default, TransformData? local = null)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PrefabInstantiate, NativeFunction.PrefabInstantiate);
        UuidData id = Asset.Id;
        ulong root = 0;
        int status;
        if (local.HasValue)
        {
            TransformData pose = local.Value;
            status = api->PrefabInstantiate(&id, parent.Id, &pose, &root);
        }
        else
        {
            status = api->PrefabInstantiate(&id, parent.Id, null, &root);
        }
        NativeCheck.ThrowIfFailed(status, NativeFunction.PrefabInstantiate);
        Entity instance = new Entity(root);
        ReservedWrites.NoteCreated(instance);
        return instance;
    }

    /// <summary>
    /// Instantiates 'count' copies under 'parent'; 'roots' must hold at least 'count' entries. Returns the
    /// number of roots written.
    /// </summary>
    public unsafe int InstantiateMany(int count, Span<Entity> roots, Entity parent = default)
    {
        if (count < 0 || count > roots.Length)
        {
            throw new ArgumentOutOfRangeException(nameof(count), "count must fit the roots span");
        }
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PrefabInstantiateMany, NativeFunction.PrefabInstantiateMany);
        UuidData id = Asset.Id;
        fixed (Entity* pinned = roots)
        {
            int status = api->PrefabInstantiateMany(&id, parent.Id, count, (ulong*)pinned);
            NativeCheck.ThrowIfFailed(status, NativeFunction.PrefabInstantiateMany);
            ReservedWrites.NoteCreated(roots.Slice(0, status));
            return status;
        }
    }

    /// <summary>
    /// The prefab at an asset path (an invalid Prefab when no source knows the path).
    /// </summary>
    public static Prefab Find(string path) => new Prefab(AssetRef<PrefabAsset>.Find(path));

    /// <summary>
    /// The prefab an instance root came from; an invalid reference for an entity that is not an instance
    /// root (or is dead).
    /// </summary>
    public static unsafe AssetRef<PrefabAsset> SourceOf(Entity entity)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PrefabSourceOf, NativeFunction.PrefabSourceOf);
        UuidData id = default;
        int status = api->PrefabSourceOf(entity.Id, &id);
        if (NativeCheck.Is(status, BindingStatus.ComponentMissing) || NativeCheck.Is(status, BindingStatus.EntityNotAlive))
        {
            return default;
        }
        NativeCheck.ThrowIfFailed(status, NativeFunction.PrefabSourceOf);
        return new AssetRef<PrefabAsset>(id);
    }

    /// <summary>
    /// True when the entity is the root of a prefab instance (false for a dead entity, B11).
    /// </summary>
    public static unsafe bool IsInstanceRoot(Entity entity)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PrefabIsInstanceRoot, NativeFunction.PrefabIsInstanceRoot);
        return api->PrefabIsInstanceRoot(entity.Id) > 0;
    }

    public override string ToString() => "Prefab(" + Asset.Id + ")";
}
