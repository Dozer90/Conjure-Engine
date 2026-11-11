using System;
using Conjure.Components;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// The hierarchy, identity and enabled-state half of Entity (docs/design/15_bindings.md 2.5): the entity_*
/// entries beyond the scripting core. Predicates and finders answer false / Null / 0 for a dead entity
/// (B11); setters throw EntityNotAliveException.
/// </summary>
public readonly partial struct Entity
{
    /// <summary>
    /// Renames the entity (scene.name; the getter is Name).
    /// </summary>
    public unsafe void SetName(string name)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->EntitySetName, NativeFunction.EntitySetName);
        Span<byte> scratch = stackalloc byte[Utf8Marshal.ScratchSize];
        Utf8Encoded encoded = Utf8Marshal.Encode(name, scratch);
        fixed (byte* pinned = encoded.Bytes)
        {
            NativeCheck.ThrowIfFailed(api->EntitySetName(Id, pinned, encoded.Length), NativeFunction.EntitySetName);
        }
    }

    /// <summary>
    /// The parent, or Null for a root.
    /// </summary>
    public unsafe Entity Parent
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->EntityGetParent, NativeFunction.EntityGetParent);
            return new Entity(api->EntityGetParent(Id));
        }
    }

    /// <summary>
    /// The topmost ancestor (the entity itself for a root; Null for a dead entity).
    /// </summary>
    public unsafe Entity Root
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->EntityGetRoot, NativeFunction.EntityGetRoot);
            return new Entity(api->EntityGetRoot(Id));
        }
    }

    /// <summary>
    /// Copies up to 'children.Length' children in sibling order and returns the total number of children.
    /// </summary>
    public int GetChildren(Span<Entity> children) => new Transform(this).GetChildren(children);

    /// <summary>
    /// The number of children.
    /// </summary>
    public int ChildCount => new Transform(this).ChildCount;

    /// <summary>
    /// The self enabled state (scene.disabled absent); false for a dead entity. Setting it inside a query
    /// scope is deferred to the scope end (R5).
    /// </summary>
    public unsafe bool Enabled
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->EntityIsEnabled, NativeFunction.EntityIsEnabled);
            return api->EntityIsEnabled(Id) > 0;
        }
        set
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->EntitySetEnabled, NativeFunction.EntitySetEnabled);
            NativeCheck.ThrowIfFailed(api->EntitySetEnabled(Id, value ? 1 : 0), NativeFunction.EntitySetEnabled);
        }
    }

    /// <summary>
    /// The gameplay tag (StringId.Null without one or for a dead entity); set it with SetTag.
    /// </summary>
    public unsafe StringId Tag
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->EntityGetTag, NativeFunction.EntityGetTag);
            return new StringId(api->EntityGetTag(Id));
        }
    }

    /// <summary>
    /// Sets the gameplay tag ("" removes it).
    /// </summary>
    public unsafe void SetTag(string tag)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->EntitySetTag, NativeFunction.EntitySetTag);
        Span<byte> scratch = stackalloc byte[Utf8Marshal.ScratchSize];
        Utf8Encoded encoded = Utf8Marshal.Encode(tag, scratch);
        fixed (byte* pinned = encoded.Bytes)
        {
            NativeCheck.ThrowIfFailed(api->EntitySetTag(Id, pinned, encoded.Length), NativeFunction.EntitySetTag);
        }
    }

    /// <summary>
    /// True when the tag equals StringId.Of(tag).
    /// </summary>
    public bool HasTag(string tag) => Tag == StringId.Of(tag);

    /// <summary>
    /// The layer index 0..31 (0 without a layer component; -1 for a dead entity). The setter rejects values
    /// outside 0..31 with ArgumentException.
    /// </summary>
    public unsafe int Layer
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->EntityGetLayer, NativeFunction.EntityGetLayer);
            int status = api->EntityGetLayer(Id);
            return status < 0 ? -1 : status;
        }
        set
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->EntitySetLayer, NativeFunction.EntitySetLayer);
            NativeCheck.ThrowIfFailed(api->EntitySetLayer(Id, value), NativeFunction.EntitySetLayer);
        }
    }

    /// <summary>
    /// The editor identity (scene.guid); a null id without one or for a dead entity.
    /// </summary>
    public unsafe AssetId Guid
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->EntityGetGuid, NativeFunction.EntityGetGuid);
            UuidData value = default;
            int status = api->EntityGetGuid(Id, &value);
            if (NativeCheck.Is(status, BindingStatus.ComponentMissing) || NativeCheck.Is(status, BindingStatus.EntityNotAlive))
            {
                return default;
            }
            NativeCheck.ThrowIfFailed(status, NativeFunction.EntityGetGuid);
            return value;
        }
    }

    /// <summary>
    /// Deep-copies the entity and its subtree (scripts included) next to it; returns the copy's root.
    /// Inside a query scope the structure is locked: InvalidOperationException.
    /// </summary>
    public unsafe Entity Duplicate()
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->EntityDuplicate, NativeFunction.EntityDuplicate);
        ulong copy = api->EntityDuplicate(Id);
        if (copy == 0)
        {
            throw NativeCheck.LastErrorException(NativeFunction.EntityDuplicate);
        }
        return new Entity(copy);
    }

    /// <summary>
    /// The entity carrying this guid, or Null.
    /// </summary>
    public static unsafe Entity FindByGuid(AssetId guid)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->EntityFindByGuid, NativeFunction.EntityFindByGuid);
        UuidData value = guid;
        return new Entity(api->EntityFindByGuid(&value));
    }

    /// <summary>
    /// Copies up to 'entities.Length' live entities tagged 'tag' and returns the total count.
    /// </summary>
    public static unsafe int FindByTag(string tag, Span<Entity> entities)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->EntityFindByTag, NativeFunction.EntityFindByTag);
        Span<byte> scratch = stackalloc byte[Utf8Marshal.ScratchSize];
        Utf8Encoded encoded = Utf8Marshal.Encode(tag, scratch);
        fixed (byte* pinnedTag = encoded.Bytes)
        fixed (Entity* pinned = entities)
        {
            int status = api->EntityFindByTag(pinnedTag, encoded.Length, (ulong*)pinned, entities.Length);
            NativeCheck.ThrowIfFailed(status, NativeFunction.EntityFindByTag);
            return status;
        }
    }

    /// <summary>
    /// Copies up to 'entities.Length' live entities on 'layer' and returns the total count.
    /// </summary>
    public static unsafe int FindByLayer(int layer, Span<Entity> entities)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->EntityFindByLayer, NativeFunction.EntityFindByLayer);
        fixed (Entity* pinned = entities)
        {
            int status = api->EntityFindByLayer(layer, (ulong*)pinned, entities.Length);
            NativeCheck.ThrowIfFailed(status, NativeFunction.EntityFindByLayer);
            return status;
        }
    }

    /// <summary>
    /// Creates 'count' entities with identity transforms under 'parent' (Null = roots) into 'entities',
    /// which must hold at least 'count'; returns the number created. Inside a query scope the ids are
    /// reserved and the entities exist once the scope closes (R5).
    /// </summary>
    public static unsafe int CreateMany(int count, Span<Entity> entities, Entity parent = default)
    {
        if (count < 0 || count > entities.Length)
        {
            throw new ArgumentOutOfRangeException(nameof(count), "count must fit the entities span");
        }
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->EntityCreateMany, NativeFunction.EntityCreateMany);
        fixed (Entity* pinned = entities)
        {
            int status = api->EntityCreateMany(count, parent.Id, (ulong*)pinned);
            NativeCheck.ThrowIfFailed(status, NativeFunction.EntityCreateMany);
            return status;
        }
    }

    /// <summary>
    /// The transform facade of this entity.
    /// </summary>
    public Transform Transform => new Transform(this);

    /// <summary>
    /// The rigid body facade of this entity.
    /// </summary>
    public Rigidbody Rigidbody => new Rigidbody(this);

    /// <summary>
    /// The character controller facade of this entity.
    /// </summary>
    public CharacterController CharacterController => new CharacterController(this);

    /// <summary>
    /// The audio source facade of this entity.
    /// </summary>
    public AudioSource AudioSource => new AudioSource(this);
}
