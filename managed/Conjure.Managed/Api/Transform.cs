using System;
using System.Numerics;
using System.Runtime.InteropServices;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// The transform facade of an entity (docs/design/15_bindings.md 2.5): the local pose through
/// transform_get_local / set_local, the world pose through transform_get_world / set_world (composed up the
/// parent chain by native) and the hierarchy edits of transform_set_parent_ex. Position, Rotation and Scale
/// are two native calls each (a get and a set); batch edits through Local. Forward, Right and Up are the
/// local axes in parent space, WorldForward, WorldRight and WorldUp the same axes in world space. Every pose
/// getter and setter, the axes and SetParent throw EntityNotAliveException on a dead entity (3.4); Parent,
/// ChildCount and GetChildren answer Null / 0 (B11).
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
    /// The local -Z axis in parent space, from the local Rotation (the engine is right-handed, Y up, -Z forward).
    /// For a child of a rotated parent it is not the world direction; that is WorldForward.
    /// </summary>
    public Vector3 Forward => Vector3.Transform(-Vector3.UnitZ, Rotation);

    /// <summary>
    /// The local +X axis in parent space, from the local Rotation; WorldRight is the world direction.
    /// </summary>
    public Vector3 Right => Vector3.Transform(Vector3.UnitX, Rotation);

    /// <summary>
    /// The local +Y axis in parent space, from the local Rotation; WorldUp is the world direction.
    /// </summary>
    public Vector3 Up => Vector3.Transform(Vector3.UnitY, Rotation);

    /// <summary>
    /// The local -Z axis in world space: the -Z basis vector of WorldMatrix, normalised, so every rotation up
    /// the parent chain counts. It is the direction LookAt aims, except that a negative world scale on Z
    /// mirrors it; a zero world scale on Z gives Vector3.Zero (only an exact zero: a tiny scale still gives a
    /// unit axis). Throws EntityNotAliveException for a dead entity.
    /// </summary>
    public Vector3 WorldForward => WorldAxis(-Vector3.UnitZ);

    /// <summary>
    /// The local +X axis in world space (WorldMatrix, normalised; mirrored by a negative world scale on X,
    /// Vector3.Zero only for an exactly zero one). Throws EntityNotAliveException for a dead entity.
    /// </summary>
    public Vector3 WorldRight => WorldAxis(Vector3.UnitX);

    /// <summary>
    /// The local +Y axis in world space (WorldMatrix, normalised; mirrored by a negative world scale on Y,
    /// Vector3.Zero only for an exactly zero one). Throws EntityNotAliveException for a dead entity.
    /// </summary>
    public Vector3 WorldUp => WorldAxis(Vector3.UnitY);

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
    /// the entity's own position): WorldForward then points at 'target' unless a negative world scale on Z
    /// mirrors it (see WorldForward); Forward does so only for a root.
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

    /// <summary>
    /// 'localAxis' carried into world space by WorldMatrix (one native call) and normalised; Vector3.Zero only
    /// when the world scale collapses that axis to exactly zero. The axis is divided by its largest component
    /// before normalising, so a tiny scale (whose squared length would underflow) still gives a unit axis. The
    /// matrix is column-major natively, so as a System.Numerics (row-vector) Matrix4x4 TransformNormal applies
    /// it the right way round.
    /// </summary>
    private Vector3 WorldAxis(Vector3 localAxis)
    {
        Vector3 axis = Vector3.TransformNormal(localAxis, WorldMatrix);
        float largest = MathF.Max(MathF.Abs(axis.X), MathF.Max(MathF.Abs(axis.Y), MathF.Abs(axis.Z)));
        if (largest == 0.0f)
        {
            return Vector3.Zero;
        }
        return Vector3.Normalize(axis / largest);
    }
}

/// <summary>
/// A pose as the native side stores it (conjure_transform, 40 bytes): position, rotation as x,y,z,w, scale.
/// Transform.Local, World and Interpolated, Rigidbody.MoveKinematic and Teleport, and Prefab.Instantiate take
/// and return it.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct TransformData
{
    public Vector3 Position;
    public Quaternion Rotation;
    public Vector3 Scale;

    /// <summary>
    /// The origin, no rotation, unit scale.
    /// </summary>
    public static TransformData Identity => new TransformData
    {
        Position = Vector3.Zero,
        Rotation = Quaternion.Identity,
        Scale = Vector3.One,
    };
}
