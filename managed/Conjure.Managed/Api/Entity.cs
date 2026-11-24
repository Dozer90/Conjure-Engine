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
/// nothing to do (the List.Remove idiom). An entity Create (or CreateMany, or Prefab.Instantiate) reserved inside
/// a ComponentQuery scope is not alive until the scope closes: IsAlive, HasComponent, TryGetComponent, Name, Tag
/// and the other getters answer as for a dead entity and GetComponent throws EntityNotAliveException, while
/// AddComponent, SetComponent, RemoveComponent, AddScript, Destroy and the Name, Tag, Layer and Enabled setters
/// on it are recorded and applied when the scope closes (the component requests in call order, Enabled once the
/// entity exists), so the entity exists with its components then; Destroy on a Prefab.Instantiate root cancels
/// the instantiation. The writes of the component facades (Transform, Rigidbody, CharacterController,
/// AudioSource) on a reserved entity are kept and made in call order right after the scope closed, and their
/// reads throw EntityNotAliveException until then.
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

    /// <summary>
    /// True when the entity exists. False for a dead entity and for one Create reserved inside the current
    /// ComponentQuery scope, which exists once the scope closes.
    /// </summary>
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
    /// EntityNotAliveException for a dead entity. On an entity Create reserved in the current ComponentQuery scope
    /// the rename is recorded: the getter answers "" until the scope closes and the entity is created named.
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
    /// Creates an entity now. Inside a ComponentQuery scope the id is reserved and the entity exists once the
    /// scope closes: until then it is not alive (IsAlive, HasComponent and GetComponent answer as for a dead
    /// entity), and AddComponent, SetComponent, RemoveComponent, AddScript and Destroy on it are recorded and
    /// applied in call order when the scope closes, so the entity is created with its components and scripts. The
    /// facade writes (Transform, Rigidbody, CharacterController, AudioSource) on it are kept and made in call order
    /// right after that, once the scope closed; the facade reads throw EntityNotAliveException until then.
    /// </summary>
    public static unsafe Entity Create(string name = "")
    {
        NativeApi* api = NativeBridge.RequireApi();
        byte[] bytes = Encoding.UTF8.GetBytes(name ?? "");
        Entity created;
        fixed (byte* pinned = bytes)
        {
            created = new Entity(api->EntityCreate(pinned, bytes.Length));
        }
        ReservedWrites.NoteCreated(created);
        return created;
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
    /// closes), otherwise at the end of the phase after their OnDestroy ran with the entity alive. An entity Create
    /// (or CreateMany) reserved in the current scope is never created: the destroy wins over everything recorded
    /// on it. On the root of a Prefab.Instantiate reserved in the current scope it cancels the instantiation, so
    /// neither the root nor any entity of the prefab is created. False when there was nothing to destroy (a dead
    /// entity); it never throws for that.
    /// </summary>
    public unsafe bool Destroy()
    {
        NativeApi* api = NativeBridge.RequireApi();
        return api->EntityDestroy(Id) != 0;
    }

    /// <summary>
    /// Queues a script of class T on the entity; the instance exists after the current phase's flush. On an entity
    /// Create reserved in the current ComponentQuery scope the script is recorded and the entity is created with it
    /// when the scope closes (a later Destroy, or a removal of its scripts, drops it). Throws
    /// EntityNotAliveException for a dead entity and InvalidOperationException when the entity cannot take
    /// another script: it already carries the maximum (8, also counting the scripts recorded on a reserved
    /// entity), or it is alive with none yet and a ComponentQuery scope holds the structure locked, or no script
    /// runtime is running.
    /// </summary>
    public unsafe void AddScript<T>() where T : ScriptBehaviour
    {
        NativeApi* api = NativeBridge.RequireApi();
        string fullName = typeof(T).FullName ?? typeof(T).Name;
        int status = api->ScriptAdd(Id, NameHash.Fnv1a64(fullName));
        if (status >= 0)
        {
            return;
        }
        throw ScriptAddFailure(
            status,
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
    /// True when the entity carries a T; false for a dead entity (a predicate never throws for that, B11), and for
    /// one Create reserved inside the current ComponentQuery scope even after an AddComponent of T: the component
    /// exists once the scope closes. Throws InvalidOperationException when T does not resolve to a registered
    /// component.
    /// </summary>
    public unsafe bool HasComponent<T>() where T : unmanaged
    {
        NativeApi* api = NativeBridge.RequireApi();
        return api->ComponentHas(Id, ComponentType<T>.RequireId()) != 0;
    }

    /// <summary>
    /// A copy of the component. Throws EntityNotAliveException for a dead entity (also for one Create reserved
    /// inside the current ComponentQuery scope, until the scope closes) and InvalidOperationException when the
    /// entity lacks it.
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
    /// dead, or was Create reserved inside the current ComponentQuery scope.
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
    /// dead entity and InvalidOperationException when the entity already has it (or, with it absent, when the
    /// world refuses the structural change). Inside a ComponentQuery scope the add is deferred to the scope end
    /// and cannot fail at the call, also on an entity Create reserved in that scope, which is then created with
    /// the component: at the flush the add overwrites a component the entity holds by then, as SetComponent would
    /// (a second add of T in the scope included), and one the flush cannot apply (the entity died meanwhile) is
    /// skipped with a DEBUG count; after a Destroy recorded in the scope the add is dropped with the entity, without
    /// a count.
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
    /// for a dead entity and InvalidOperationException when the entity lacks it. On an entity Create reserved in
    /// the current ComponentQuery scope it never throws: the write is recorded and applied in call order when the
    /// scope closes, after the AddComponent calls before it, and skipped with a DEBUG count when no AddComponent of
    /// T is left on the entity at the scope end (none was made, or a later RemoveComponent took it).
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
    /// was nothing to remove, because the entity lacks it or is dead. It never throws for that. On an entity
    /// Create reserved in the current scope it removes, in call order, what AddComponent recorded before it: true
    /// after an AddComponent of T (the entity is then created without it), false when none was recorded. On the
    /// root of a Prefab.Instantiate reserved in the scope it is always recorded and returns true, since the
    /// prefab's components are not known before the scope closes.
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
    /// The exception of a failed script_add: EntityNotAliveException for INSTANCE_NOT_FOUND (no entity, neither
    /// alive nor reserved in the current scope), InvalidOperationException saying the script runtime is not
    /// running for NOT_INITIALISED, otherwise InvalidOperationException saying "script_add: entity &lt;id&gt;
    /// &lt;detail&gt;". The status picks the type: IsAlive answers false for a reserved entity, which is not dead.
    /// </summary>
    private Exception ScriptAddFailure(int status, string detail)
    {
        string prefix = FailurePrefix(NativeFunction.ScriptAdd);
        if (status == ScriptStatus.InstanceNotFound)
        {
            return new EntityNotAliveException(prefix + " is not alive");
        }
        if (status == ScriptStatus.NotInitialised)
        {
            return new InvalidOperationException(prefix + ": the script runtime is not running");
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
