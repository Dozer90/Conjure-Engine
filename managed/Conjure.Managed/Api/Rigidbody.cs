using System.Numerics;
using Conjure.Components;
using Conjure.Dod;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// The rigid body facade of an entity (docs/design/15_bindings.md 2.5): velocities, forces and kinematic
/// moves through the physics_* entries, which need a physics.rigid_body on the entity
/// (ConjureNativeException(ComponentMissing) otherwise) and a physics world (NotSupportedException when
/// physics is off). A request the entity's kind of body cannot take is an ArgumentException (native
/// WRONG_BODY_KIND): a force or torque on a body that is not dynamic, a velocity on a static body,
/// MoveKinematic on anything but a kinematic body that does not follow its transform, and all of these on an
/// entity that also carries a character controller. The kind is the one the rigid_body component describes
/// now, so a motion type set this frame already counts; a body the engine has not created yet (added since the
/// last fixed step) accepts every request and drops, with a count, what it cannot take. IsSleeping never
/// throws (B11).
/// </summary>
public readonly struct Rigidbody
{
    public readonly Entity Entity;

    public Rigidbody(Entity entity)
    {
        Entity = entity;
    }

    /// <summary>
    /// True when the entity carries a rigid body; false in a world without the physics components (a build
    /// without physics), where Entity.HasComponent&lt;RigidBodyData&gt; throws.
    /// </summary>
    public bool Exists => ComponentType<RigidBodyData>.Id != 0 && Entity.HasComponent<RigidBodyData>();

    public unsafe Vector3 LinearVelocity
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->PhysicsGetLinearVelocity, NativeFunction.PhysicsGetLinearVelocity);
            Vector3 value = Vector3.Zero;
            NativeCheck.ThrowIfFailed(api->PhysicsGetLinearVelocity(Entity.Id, &value), NativeFunction.PhysicsGetLinearVelocity);
            return value;
        }
        set
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->PhysicsSetLinearVelocity, NativeFunction.PhysicsSetLinearVelocity);
            NativeCheck.ThrowIfFailed(api->PhysicsSetLinearVelocity(Entity.Id, &value), NativeFunction.PhysicsSetLinearVelocity);
        }
    }

    public unsafe Vector3 AngularVelocity
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->PhysicsGetAngularVelocity, NativeFunction.PhysicsGetAngularVelocity);
            Vector3 value = Vector3.Zero;
            NativeCheck.ThrowIfFailed(api->PhysicsGetAngularVelocity(Entity.Id, &value), NativeFunction.PhysicsGetAngularVelocity);
            return value;
        }
        set
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->PhysicsSetAngularVelocity, NativeFunction.PhysicsSetAngularVelocity);
            NativeCheck.ThrowIfFailed(api->PhysicsSetAngularVelocity(Entity.Id, &value), NativeFunction.PhysicsSetAngularVelocity);
        }
    }

    /// <summary>
    /// Applies a force (or an impulse, velocity change or acceleration) at the centre of mass; takes
    /// effect in the next fixed step. Dynamic bodies only (ArgumentException otherwise).
    /// </summary>
    public unsafe void AddForce(Vector3 force, ForceMode mode = ForceMode.Force)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PhysicsAddForce, NativeFunction.PhysicsAddForce);
        NativeCheck.ThrowIfFailed(api->PhysicsAddForce(Entity.Id, &force, (byte)mode), NativeFunction.PhysicsAddForce);
    }

    /// <summary>
    /// Applies a force at a world point, which also produces torque. Dynamic bodies only (ArgumentException
    /// otherwise).
    /// </summary>
    public unsafe void AddForceAtPosition(Vector3 force, Vector3 worldPoint, ForceMode mode = ForceMode.Force)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PhysicsAddForceAt, NativeFunction.PhysicsAddForceAt);
        NativeCheck.ThrowIfFailed(
            api->PhysicsAddForceAt(Entity.Id, &force, &worldPoint, (byte)mode),
            NativeFunction.PhysicsAddForceAt);
    }

    /// <summary>
    /// Applies a torque (Force or Impulse only; the other modes are ArgumentException). Dynamic bodies only
    /// (ArgumentException otherwise).
    /// </summary>
    public unsafe void AddTorque(Vector3 torque, ForceMode mode = ForceMode.Force)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PhysicsAddTorque, NativeFunction.PhysicsAddTorque);
        NativeCheck.ThrowIfFailed(api->PhysicsAddTorque(Entity.Id, &torque, (byte)mode), NativeFunction.PhysicsAddTorque);
    }

    /// <summary>
    /// Moves a kinematic body to 'target' over the next fixed step (velocity-driven, pushes dynamic bodies).
    /// ArgumentException for a static or dynamic body and for a kinematic body with
    /// KINEMATIC_MOVES_WITH_TRANSFORM, which is moved by writing its Transform.
    /// </summary>
    public unsafe void MoveKinematic(in TransformData target)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PhysicsMoveKinematic, NativeFunction.PhysicsMoveKinematic);
        fixed (TransformData* pinned = &target)
        {
            NativeCheck.ThrowIfFailed(api->PhysicsMoveKinematic(Entity.Id, pinned), NativeFunction.PhysicsMoveKinematic);
        }
    }

    /// <summary>
    /// Sets the body pose at once, without sweeping. Works for every kind of body.
    /// </summary>
    public unsafe void Teleport(in TransformData target)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PhysicsTeleport, NativeFunction.PhysicsTeleport);
        fixed (TransformData* pinned = &target)
        {
            NativeCheck.ThrowIfFailed(api->PhysicsTeleport(Entity.Id, pinned), NativeFunction.PhysicsTeleport);
        }
    }

    /// <summary>
    /// True when the body is asleep; false for a dead entity or one without a body.
    /// </summary>
    public unsafe bool IsSleeping
    {
        get
        {
            NativeApi* api = NativeBridge.RequireApi();
            NativeBridge.Require(api->PhysicsIsSleeping, NativeFunction.PhysicsIsSleeping);
            return api->PhysicsIsSleeping(Entity.Id) > 0;
        }
    }

    /// <summary>
    /// Wakes a sleeping body; does nothing for a static body, which cannot be woken (IsSleeping stays true).
    /// </summary>
    public unsafe void WakeUp()
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->PhysicsWake, NativeFunction.PhysicsWake);
        NativeCheck.ThrowIfFailed(api->PhysicsWake(Entity.Id), NativeFunction.PhysicsWake);
    }

    /// <summary>
    /// The rigid_body component itself (mass, damping, flags, runtime velocities); the setter writes it back
    /// through component_set.
    /// </summary>
    public RigidBodyData Data
    {
        get => Entity.GetComponent<RigidBodyData>();
        set => Entity.SetComponent(in value);
    }
}

/// <summary>
/// How Rigidbody.AddForce, AddForceAtPosition and AddTorque apply their vector (conjure_force_mode): Force
/// and Acceleration act over the next fixed step, Impulse and VelocityChange at once. The effect of Force and
/// Impulse depends on the body's mass; Acceleration and VelocityChange ignore it and need an explicit
/// rigid_body mass. AddTorque takes Force and Impulse only.
/// </summary>
public enum ForceMode : byte
{
    Force,
    Impulse,
    VelocityChange,
    Acceleration,
}
