using System;
using System.Numerics;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// The transform facade of an entity (docs/design/15_bindings.md 2.5): the local pose through
/// transform_get_local / set_local, the world pose through transform_get_world / set_world (composed up the
/// parent chain by native) and the hierarchy edits of transform_set_parent_ex. Position, Rotation and Scale
/// are two native calls each (a get and a set); batch edits through Local. Every getter and setter throws
/// EntityNotAliveException on a dead entity (3.4).
/// </summary>
public readonly struct Transform
{
    public readonly Entity Entity;

    public Transform(Entity entity)
    {
        Entity = entity;
    }

    /// <summary>
    /// The pose relative to the parent (or to the world for a root).
    /// </summary>
    public unsafe TransformData Local
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->TransformGetLocal, NativeFunction.TransformGetLocal);
            TransformData value = TransformData.Identity;
            NativeCheck.ThrowIfFailed(api->TransformGetLocal(Entity.Id, &value), NativeFunction.TransformGetLocal);
            return value;
        }
        set
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->TransformSetLocal, NativeFunction.TransformSetLocal);
            NativeCheck.ThrowIfFailed(api->TransformSetLocal(Entity.Id, &value), NativeFunction.TransformSetLocal);
        }
    }

    public Vector3 Position
    {
        get => Local.Position;
        set
        {
            TransformData local = Local;
            local.Position = value;
            Local = local;
        }
    }

    public Quaternion Rotation
    {
        get => Local.Rotation;
        set
        {
            TransformData local = Local;
            local.Rotation = value;
            Local = local;
        }
    }

    public Vector3 Scale
    {
        get => Local.Scale;
        set
        {
            TransformData local = Local;
            local.Scale = value;
            Local = local;
        }
    }

    /// <summary>
    /// The world pose: the locals composed up the parent chain; setting it rewrites the local so the world
    /// pose becomes 'value'.
    /// </summary>
    public unsafe TransformData World
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->TransformGetWorld, NativeFunction.TransformGetWorld);
            TransformData value = TransformData.Identity;
            NativeCheck.ThrowIfFailed(api->TransformGetWorld(Entity.Id, &value), NativeFunction.TransformGetWorld);
            return value;
        }
        set
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->TransformSetWorld, NativeFunction.TransformSetWorld);
            NativeCheck.ThrowIfFailed(api->TransformSetWorld(Entity.Id, &value), NativeFunction.TransformSetWorld);
        }
    }

    /// <summary>
    /// The propagated world matrix (scene.world_transform).
    /// </summary>
    public unsafe Matrix4x4 WorldMatrix
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->TransformGetWorldMatrix, NativeFunction.TransformGetWorldMatrix);
            Matrix4x4 matrix = Matrix4x4.Identity;
            NativeCheck.ThrowIfFailed(api->TransformGetWorldMatrix(Entity.Id, &matrix), NativeFunction.TransformGetWorldMatrix);
            return matrix;
        }
    }

    /// <summary>
    /// The pose physics interpolated for this frame (scene.interpolated_transform); throws
    /// ConjureNativeException(ComponentMissing) for an entity physics does not drive.
    /// </summary>
    public unsafe TransformData Interpolated
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->TransformGetInterpolated, NativeFunction.TransformGetInterpolated);
            TransformData value = TransformData.Identity;
            NativeCheck.ThrowIfFailed(api->TransformGetInterpolated(Entity.Id, &value), NativeFunction.TransformGetInterpolated);
            return value;
        }
    }

    /// <summary>
    /// The local -Z axis in parent space (the engine is right-handed, Y up, -Z forward).
    /// </summary>
    public Vector3 Forward => Vector3.Transform(-Vector3.UnitZ, Rotation);

    /// <summary>
    /// The local +X axis in parent space.
    /// </summary>
    public Vector3 Right => Vector3.Transform(Vector3.UnitX, Rotation);

    /// <summary>
    /// The local +Y axis in parent space.
    /// </summary>
    public Vector3 Up => Vector3.Transform(Vector3.UnitY, Rotation);

    /// <summary>
    /// The parent entity, or Entity.Null for a root (and for a dead entity).
    /// </summary>
    public unsafe Entity Parent
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->EntityGetParent, NativeFunction.EntityGetParent);
            return new Entity(api->EntityGetParent(Entity.Id));
        }
    }

    /// <summary>
    /// Reparents the entity (Entity.Null detaches it to a root) at sibling position 'index' (-1 appends),
    /// keeping its world pose when 'keepWorld' is set and its local pose otherwise. Inside a query scope
    /// the edit is deferred to the scope end (R5).
    /// </summary>
    public unsafe void SetParent(Entity parent, bool keepWorld = true, int index = -1)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->TransformSetParentEx, NativeFunction.TransformSetParentEx);
        NativeCheck.ThrowIfFailed(
            api->TransformSetParentEx(Entity.Id, parent.Id, keepWorld ? 1 : 0, index),
            NativeFunction.TransformSetParentEx);
    }

    /// <summary>
    /// The number of children (0 for a dead entity, B11).
    /// </summary>
    public unsafe int ChildCount
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->EntityGetChildren, NativeFunction.EntityGetChildren);
            int count = api->EntityGetChildren(Entity.Id, null, 0);
            return count < 0 ? 0 : count;
        }
    }

    /// <summary>
    /// Copies up to 'children.Length' children in sibling order and returns the total number of children
    /// (more than written when the span is short; 0 for a dead entity).
    /// </summary>
    public unsafe int GetChildren(Span<Entity> children)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->EntityGetChildren, NativeFunction.EntityGetChildren);
        fixed (Entity* pinned = children)
        {
            int count = api->EntityGetChildren(Entity.Id, (ulong*)pinned, children.Length);
            return count < 0 ? 0 : count;
        }
    }

    /// <summary>
    /// Moves the local position by 'delta'.
    /// </summary>
    public void Translate(Vector3 delta)
    {
        TransformData local = Local;
        local.Position += delta;
        Local = local;
    }

    /// <summary>
    /// Applies 'rotation' after the current local rotation.
    /// </summary>
    public void Rotate(Quaternion rotation)
    {
        TransformData local = Local;
        local.Rotation = Quaternion.Normalize(rotation * local.Rotation);
        Local = local;
    }

    /// <summary>
    /// Rotates the entity in world space so its forward axis points at 'target' (no-op when the target is
    /// the entity's own position).
    /// </summary>
    public void LookAt(Vector3 target, Vector3? up = null)
    {
        TransformData world = World;
        Vector3 direction = target - world.Position;
        if (direction.LengthSquared() <= 1e-12f)
        {
            return;
        }
        Vector3 upHint = up ?? Vector3.UnitY;
        if (Vector3.Cross(Vector3.Normalize(direction), Vector3.Normalize(upHint)).LengthSquared() <= 1e-8f)
        {
            upHint = MathF.Abs(upHint.Y) > 0.9f ? Vector3.UnitZ : Vector3.UnitY;
        }
        Matrix4x4 basis = Matrix4x4.CreateWorld(Vector3.Zero, direction, upHint);
        world.Rotation = Quaternion.Normalize(Quaternion.CreateFromRotationMatrix(basis));
        World = world;
    }
}
