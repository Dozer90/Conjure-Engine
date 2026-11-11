using System;
using System.Runtime.InteropServices;

namespace Conjure.Components;

/// <summary>
/// An entity inside a component mirror: ecs::entity as native lays it out, two u32 (index, generation) at
/// 4-byte alignment. Entity itself packs the same bits into one ulong, whose 8-byte alignment would move
/// the fields that follow it in scene.parent and physics.character_controller; mirrors use this handle and
/// convert at the boundary.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public readonly struct EntityHandle : IEquatable<EntityHandle>
{
    public static readonly EntityHandle Null = default;

    public readonly uint Index;
    public readonly uint Generation;

    public EntityHandle(uint index, uint generation)
    {
        Index = index;
        Generation = generation;
    }

    public EntityHandle(Entity entity)
    {
        Index = entity.Index;
        Generation = entity.Generation;
    }

    public bool IsNull => Index == 0 && Generation == 0;

    /// <summary>
    /// The packed Entity (index | generation &lt;&lt; 32).
    /// </summary>
    public Entity Entity => new Entity(Index | ((ulong)Generation << 32));

    public bool Equals(EntityHandle other) => Index == other.Index && Generation == other.Generation;

    public override bool Equals(object? obj) => obj is EntityHandle other && Equals(other);

    public override int GetHashCode() => HashCode.Combine(Index, Generation);

    public override string ToString() => Index + ":" + Generation;

    public static implicit operator Entity(EntityHandle handle) => handle.Entity;

    public static implicit operator EntityHandle(Entity entity) => new EntityHandle(entity);

    public static bool operator ==(EntityHandle left, EntityHandle right) => left.Equals(right);

    public static bool operator !=(EntityHandle left, EntityHandle right) => !left.Equals(right);
}
