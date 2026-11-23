using System;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;
using System.Text;
using Conjure.Dod;
using Conjure.Interop;
using Conjure.Runtime;

namespace Conjure;

/// <summary>
/// An entity handle: the packed 64-bit id (index | generation &lt;&lt; 32) and nothing else. Every member
/// calls native on the main thread. One failure convention (docs/design/15_bindings.md B11): predicates,
/// finders and getters of identity (IsAlive, HasComponent, TryGetComponent, Name, Tag, GetScript) answer false
/// / "" / null for a dead entity; GetComponent, AddComponent, SetComponent, AddScript and every setter throw,
/// EntityNotAliveException for a dead entity and InvalidOperationException when the component is missing or
/// already present (or the world refuses the change); RemoveComponent and Destroy return false when there was
/// nothing to do (the List.Remove idiom). An entity Create reserved inside a ComponentQuery scope is not alive
/// until the scope closes.
/// </summary>
public readonly partial struct Entity : IEquatable<Entity>
{
    public static readonly Entity Null = default;

    public readonly ulong Id;

    public Entity(ulong id)
    {
        Id = id;
    }

    public bool IsNull => Id == 0;

    public uint Index => (uint)(Id & 0xFFFFFFFFu);

    public uint Generation => (uint)(Id >> 32);

    public unsafe bool IsAlive
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            return api->EntityIsAlive(Id) != 0;
        }
    }

    /// <summary>
    /// The name given at creation (Entity.Create), by the scene or by this setter; "" when none and for a dead
    /// entity. Setting it renames the entity (scene.name; "" or null clears it) and throws
    /// EntityNotAliveException for a dead entity.
    /// </summary>
    [AllowNull]
    public unsafe string Name
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            byte* stack = stackalloc byte[256];
            int needed = api->EntityGetName(Id, stack, 256);
            if (needed <= 0)
            {
                return "";
            }
            if (needed <= 256)
            {
                return Encoding.UTF8.GetString(stack, needed);
            }
            byte[] heap = new byte[needed];
            fixed (byte* pinned = heap)
            {
                api->EntityGetName(Id, pinned, needed);
            }
            return Encoding.UTF8.GetString(heap);
        }
        set => WriteName(value);
    }

    /// <summary>
    /// Creates an entity now (inside a ComponentQuery scope the id is reserved and the entity exists once
    /// the scope closes).
    /// </summary>
    public static unsafe Entity Create(string name = "")
    {
        NativeApi* api = NativeBridge.RequireApi();
        byte[] bytes = Encoding.UTF8.GetBytes(name ?? "");
        fixed (byte* pinned = bytes)
        {
            return new Entity(api->EntityCreate(pinned, bytes.Length));
        }
    }

    /// <summary>
    /// The first live entity with this name, or Null.
    /// </summary>
    public static unsafe Entity Find(string name)
    {
        NativeApi* api = NativeBridge.RequireApi();
        byte[] bytes = Encoding.UTF8.GetBytes(name ?? "");
        fixed (byte* pinned = bytes)
        {
            return new Entity(api->EntityFindByName(pinned, bytes.Length));
        }
    }

    /// <summary>
    /// Destroys the entity: at once when it has no live scripts (inside a ComponentQuery scope, once the scope
    /// closes), otherwise at the end of the phase after their OnDestroy ran with the entity alive. False when
    /// there was nothing to destroy (a dead entity); it never throws for that.
    /// </summary>
    public unsafe bool Destroy()
    {
        NativeApi* api = NativeBridge.RequireApi();
        return api->EntityDestroy(Id) != 0;
    }

    /// <summary>
    /// Queues a script of class T on the entity; the instance exists after the current phase's flush. Throws
    /// EntityNotAliveException for a dead entity and InvalidOperationException when the entity cannot take
    /// another script: it already carries the maximum (8), or it has none yet and a ComponentQuery scope holds
    /// the structure locked.
    /// </summary>
    public unsafe void AddScript<T>() where T : ScriptBehaviour
    {
        NativeApi* api = NativeBridge.RequireApi();
        string fullName = typeof(T).FullName ?? typeof(T).Name;
        if (api->ScriptAdd(Id, NameHash.Fnv1a64(fullName)) >= 0)
        {
            return;
        }
        throw CallFailure(
            NativeFunction.ScriptAdd,
            "cannot take " + typeof(T).Name + ": it carries the maximum of scripts, or it has none yet and a query "
                + "scope holds the structure locked");
    }

    /// <summary>
    /// The first live script of type T on the entity, or null (also for a dead entity).
    /// </summary>
    public unsafe T? GetScript<T>() where T : ScriptBehaviour
    {
        NativeApi* api = NativeBridge.RequireApi();
        int* slots = stackalloc int[8];
        int count = api->ScriptGetSlots(Id, slots, 8);
        int limit = count < 8 ? count : 8;
        for (int index = 0; index < limit; ++index)
        {
            if (ScriptRuntime.InstanceAt(slots[index]) is T match)
            {
                return match;
            }
        }
        return null;
    }

    /// <summary>
    /// True when the entity carries a T; false for a dead entity (a predicate never throws for that, B11).
    /// Throws InvalidOperationException when T does not resolve to a registered component.
    /// </summary>
    public unsafe bool HasComponent<T>() where T : unmanaged
    {
        NativeApi* api = NativeBridge.RequireApi();
        return api->ComponentHas(Id, ComponentType<T>.RequireId()) != 0;
    }

    /// <summary>
    /// A copy of the component. Throws EntityNotAliveException for a dead entity and
    /// InvalidOperationException when the entity lacks it.
    /// </summary>
    public unsafe T GetComponent<T>() where T : unmanaged
    {
        NativeApi* api = NativeBridge.RequireApi();
        T value = default;
        if (api->ComponentGet(Id, ComponentType<T>.RequireId(), &value) != 0)
        {
            return value;
        }
        throw ComponentFailure<T>(NativeFunction.ComponentGet);
    }

    /// <summary>
    /// Copies the component into 'value' and returns true; false (and default) when the entity lacks it or is
    /// dead.
    /// </summary>
    public unsafe bool TryGetComponent<T>(out T value) where T : unmanaged
    {
        NativeApi* api = NativeBridge.RequireApi();
        T local = default;
        bool found = api->ComponentGet(Id, ComponentType<T>.RequireId(), &local) != 0;
        value = local;
        return found;
    }

    /// <summary>
    /// Adds the component with 'value' (its AssetRef fields bound first). Throws EntityNotAliveException for a
    /// dead entity, including one Create reserved inside the current ComponentQuery scope, and
    /// InvalidOperationException when the entity already has it (or, with it absent, when the world refuses the
    /// structural change). Inside a scope the add to a live entity is deferred to the scope end and cannot fail
    /// at the call: at the flush it overwrites a component the entity holds by then, as SetComponent would, and
    /// one the flush cannot apply (the entity died meanwhile) is skipped with a DEBUG count.
    /// </summary>
    public unsafe void AddComponent<T>(in T value) where T : unmanaged
    {
        NativeApi* api = NativeBridge.RequireApi();
        T bound = value;
        AssetRefBinder<T>.Bind(ref bound);
        if (api->ComponentAdd(Id, ComponentType<T>.RequireId(), &bound) != 0)
        {
            return;
        }
        throw ComponentFailure<T>(NativeFunction.ComponentAdd);
    }

    /// <summary>
    /// Overwrites the component with 'value' (its AssetRef fields bound first). Throws EntityNotAliveException
    /// for a dead entity and InvalidOperationException when the entity lacks it.
    /// </summary>
    public unsafe void SetComponent<T>(in T value) where T : unmanaged
    {
        NativeApi* api = NativeBridge.RequireApi();
        T bound = value;
        AssetRefBinder<T>.Bind(ref bound);
        if (api->ComponentSet(Id, ComponentType<T>.RequireId(), &bound) != 0)
        {
            return;
        }
        throw ComponentFailure<T>(NativeFunction.ComponentSet);
    }

    /// <summary>
    /// Removes the component (inside a ComponentQuery scope, at the scope end) and returns true; false when there
    /// was nothing to remove, because the entity lacks it or is dead. It never throws for that.
    /// </summary>
    public unsafe bool RemoveComponent<T>() where T : unmanaged
    {
        NativeApi* api = NativeBridge.RequireApi();
        return api->ComponentRemove(Id, ComponentType<T>.RequireId()) != 0;
    }

    /// <summary>
    /// The exception of a failed component_add, component_get or component_set of T: EntityNotAliveException
    /// when the entity is not alive, otherwise InvalidOperationException saying "&lt;entry&gt;: entity &lt;id&gt;
    /// already has T" (add) or "... has no T" (get, set), as HasComponent answers. When the component is in the
    /// state the call needed, native refused for another reason (a structural change the world does not allow at
    /// that moment) and the message says "could not add / read / write T" instead of naming a false cause. The
    /// core entries answer 0 without a reason, so IsAlive and HasComponent are asked only on this failure path;
    /// a success pays nothing extra.
    /// </summary>
    internal Exception ComponentFailure<T>(NativeFunction function) where T : unmanaged
    {
        string prefix = FailurePrefix(function);
        if (!IsAlive)
        {
            return new EntityNotAliveException(prefix + " is not alive");
        }
        string name = typeof(T).Name;
        bool held = HasComponent<T>();
        if (function == NativeFunction.ComponentAdd)
        {
            return new InvalidOperationException(prefix + (held ? " already has " : " could not add ") + name);
        }
        if (!held)
        {
            return new InvalidOperationException(prefix + " has no " + name);
        }
        string verb = function == NativeFunction.ComponentGet ? " could not read " : " could not write ";
        return new InvalidOperationException(prefix + verb + name);
    }

    /// <summary>
    /// The exception of a failed script_add: EntityNotAliveException when the entity is not alive, otherwise
    /// InvalidOperationException saying "script_add: entity &lt;id&gt; &lt;detail&gt;". One IsAlive call, made
    /// only on the failure path, picks the type.
    /// </summary>
    private Exception CallFailure(NativeFunction function, string detail)
    {
        string prefix = FailurePrefix(function);
        if (!IsAlive)
        {
            return new EntityNotAliveException(prefix + " is not alive");
        }
        return new InvalidOperationException(prefix + " " + detail);
    }

    /// <summary>
    /// "&lt;entry&gt;: entity &lt;index:generation&gt;", the start of every failure message.
    /// </summary>
    private string FailurePrefix(NativeFunction function)
    {
        return NativeFunctionNames.Of(function) + ": entity " + ToString();
    }

    public bool Equals(Entity other) => Id == other.Id;

    public override bool Equals(object? obj) => obj is Entity other && Equals(other);

    public override int GetHashCode() => Id.GetHashCode();

    public override string ToString() => Index + ":" + Generation;

    public static bool operator ==(Entity left, Entity right) => left.Id == right.Id;

    public static bool operator !=(Entity left, Entity right) => left.Id != right.Id;
}
