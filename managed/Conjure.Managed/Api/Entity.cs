using System;
using System.Runtime.CompilerServices;
using System.Text;
using Conjure.Dod;
using Conjure.Interop;
using Conjure.Runtime;

namespace Conjure;

/// <summary>
/// An entity handle: the packed 64-bit id (index | generation &lt;&lt; 32) and nothing else. Every member
/// calls native on the main thread; a dead entity answers false / default.
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
    /// The name given at creation (Entity.Create) or by the scene; "" when none.
    /// </summary>
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
    /// Destroys the entity: at once when it has no live scripts, otherwise at the end of the phase after
    /// their OnDestroy ran with the entity alive. False for a dead entity.
    /// </summary>
    public unsafe bool Destroy()
    {
        NativeApi* api = NativeBridge.RequireApi();
        return api->EntityDestroy(Id) != 0;
    }

    /// <summary>
    /// Queues a script of class T on the entity; the instance exists after the current phase's flush.
    /// </summary>
    public unsafe bool AddScript<T>() where T : ScriptBehaviour
    {
        NativeApi* api = NativeBridge.RequireApi();
        string fullName = typeof(T).FullName ?? typeof(T).Name;
        return api->ScriptAdd(Id, NameHash.Fnv1a64(fullName)) >= 0;
    }

    /// <summary>
    /// The first live script of type T on the entity, or null.
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

    public unsafe bool HasComponent<T>() where T : unmanaged
    {
        NativeApi* api = NativeBridge.RequireApi();
        return api->ComponentHas(Id, ComponentType<T>.RequireId()) != 0;
    }

    /// <summary>
    /// A copy of the component; throws InvalidOperationException when the entity lacks it.
    /// </summary>
    public unsafe T GetComponent<T>() where T : unmanaged
    {
        NativeApi* api = NativeBridge.RequireApi();
        T value = default;
        if (api->ComponentGet(Id, ComponentType<T>.RequireId(), &value) == 0)
        {
            throw new InvalidOperationException("entity " + Id + " has no " + typeof(T).Name);
        }
        return value;
    }

    public unsafe bool TryGetComponent<T>(out T value) where T : unmanaged
    {
        NativeApi* api = NativeBridge.RequireApi();
        T local = default;
        bool found = api->ComponentGet(Id, ComponentType<T>.RequireId(), &local) != 0;
        value = local;
        return found;
    }

    public unsafe bool AddComponent<T>(in T value) where T : unmanaged
    {
        NativeApi* api = NativeBridge.RequireApi();
        T bound = value;
        AssetRefBinder<T>.Bind(ref bound);
        return api->ComponentAdd(Id, ComponentType<T>.RequireId(), &bound) != 0;
    }

    public unsafe bool SetComponent<T>(in T value) where T : unmanaged
    {
        NativeApi* api = NativeBridge.RequireApi();
        T bound = value;
        AssetRefBinder<T>.Bind(ref bound);
        return api->ComponentSet(Id, ComponentType<T>.RequireId(), &bound) != 0;
    }

    public unsafe bool RemoveComponent<T>() where T : unmanaged
    {
        NativeApi* api = NativeBridge.RequireApi();
        return api->ComponentRemove(Id, ComponentType<T>.RequireId()) != 0;
    }

    public bool Equals(Entity other) => Id == other.Id;

    public override bool Equals(object? obj) => obj is Entity other && Equals(other);

    public override int GetHashCode() => Id.GetHashCode();

    public override string ToString() => Index + ":" + Generation;

    public static bool operator ==(Entity left, Entity right) => left.Id == right.Id;

    public static bool operator !=(Entity left, Entity right) => left.Id != right.Id;
}
