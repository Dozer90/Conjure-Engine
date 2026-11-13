using System;
using System.Numerics;
using System.Runtime.InteropServices;

namespace Conjure.Components;

// The physics component mirrors (docs/design/15_bindings.md 2.6, 14_physics.md 2.3, R48). Fields named Pad*
// only make the managed layout agree with the C++ one and take no part in the layout hash (R61).

/// <summary>
/// physics::motion_type.
/// </summary>
public enum MotionType : byte
{
    Static,
    Kinematic,
    Dynamic,
}

/// <summary>
/// physics::rigid_body_flags.
/// </summary>
[Flags]
public enum RigidBodyFlags : ushort
{
    None = 0,
    ContinuousCollision = 1 << 0,
    KinematicMovesWithTransform = 1 << 1,
    AllowSleeping = 1 << 2,
    StartAsleep = 1 << 3,
    LockRotation = 1 << 4,
    LockRotationX = 1 << 5,
    LockRotationY = 1 << 6,
    LockRotationZ = 1 << 7,
    LockTranslationX = 1 << 8,
    LockTranslationY = 1 << 9,
    LockTranslationZ = 1 << 10,
    UseGyroscopicForce = 1 << 11,
}

/// <summary>
/// physics::ground_state.
/// </summary>
public enum GroundState : byte
{
    OnGround,
    OnSteepGround,
    NotSupported,
    InAir,
}

/// <summary>
/// physics::character_flags.
/// </summary>
[Flags]
public enum CharacterFlags : byte
{
    None = 0,
    StickToFloor = 1 << 0,
    WalkStairs = 1 << 1,
    ApplyGravity = 1 << 2,
}

/// <summary>
/// Mirror of physics.rigid_body (52 bytes). LinearVelocity, AngularVelocity and IsSleeping are runtime
/// fields the step writes back; Rigidbody is the facade for changing them.
/// </summary>
[NativeComponent("conjure.physics.rigid_body")]
[StructLayout(LayoutKind.Sequential)]
public struct RigidBodyData : IComponent
{
    public MotionType MotionType;
    public byte Layer;
    public RigidBodyFlags Flags;
    public float Mass;
    public float LinearDamping;
    public float AngularDamping;
    public float GravityFactor;
    public float MaxLinearVelocity;
    public Vector3 LinearVelocity;
    public Vector3 AngularVelocity;
    public NativeBool IsSleeping;
    public byte Pad0;
    public byte Pad1;
    public byte Pad2;

    /// <summary>
    /// The native defaults: a dynamic 1 kg body that may sleep.
    /// </summary>
    public static RigidBodyData Default => new RigidBodyData
    {
        MotionType = MotionType.Dynamic,
        Flags = RigidBodyFlags.AllowSleeping,
        Mass = 1.0f,
        LinearDamping = 0.05f,
        AngularDamping = 0.05f,
        GravityFactor = 1.0f,
        MaxLinearVelocity = 500.0f,
    };
}

/// <summary>
/// Mirror of physics.box_collider (44 bytes).
/// </summary>
[NativeComponent("conjure.physics.box_collider")]
[StructLayout(LayoutKind.Sequential)]
public struct BoxColliderData : IComponent
{
    public Vector3 HalfExtents;
    public Vector3 Offset;
    public Quaternion Rotation;
    public NativeBool IsTrigger;
    public byte Pad0;
    public byte Pad1;
    public byte Pad2;

    public static BoxColliderData Default => new BoxColliderData
    {
        HalfExtents = new Vector3(0.5f),
        Rotation = Quaternion.Identity,
    };
}

/// <summary>
/// Mirror of physics.sphere_collider (20 bytes).
/// </summary>
[NativeComponent("conjure.physics.sphere_collider")]
[StructLayout(LayoutKind.Sequential)]
public struct SphereColliderData : IComponent
{
    public float Radius;
    public Vector3 Offset;
    public NativeBool IsTrigger;
    public byte Pad0;
    public byte Pad1;
    public byte Pad2;

    public static SphereColliderData Default => new SphereColliderData { Radius = 0.5f };
}

/// <summary>
/// Mirror of physics.capsule_collider (40 bytes).
/// </summary>
[NativeComponent("conjure.physics.capsule_collider")]
[StructLayout(LayoutKind.Sequential)]
public struct CapsuleColliderData : IComponent
{
    public float Radius;
    public float HalfHeight;
    public Vector3 Offset;
    public Quaternion Rotation;
    public NativeBool IsTrigger;
    public byte Pad0;
    public byte Pad1;
    public byte Pad2;

    public static CapsuleColliderData Default => new CapsuleColliderData
    {
        Radius = 0.5f,
        HalfHeight = 0.5f,
        Rotation = Quaternion.Identity,
    };
}

/// <summary>
/// Mirror of physics.convex_collider (64 bytes).
/// </summary>
[NativeComponent("conjure.physics.convex_collider")]
[StructLayout(LayoutKind.Sequential)]
public struct ConvexColliderData : IComponent
{
    public AssetRef<CollisionMeshAsset> Mesh;
    public Vector3 Offset;
    public Quaternion Rotation;
    public float ConvexRadius;
    public NativeBool IsTrigger;
    public byte Pad0;
    public byte Pad1;
    public byte Pad2;
    public uint Pad3;

    public static ConvexColliderData Default => new ConvexColliderData
    {
        Rotation = Quaternion.Identity,
        ConvexRadius = 0.05f,
    };
}

/// <summary>
/// Mirror of physics.mesh_collider (32 bytes).
/// </summary>
[NativeComponent("conjure.physics.mesh_collider")]
[StructLayout(LayoutKind.Sequential)]
public struct MeshColliderData : IComponent
{
    public AssetRef<CollisionMeshAsset> Mesh;
    public NativeBool IsTrigger;
    public byte Pad0;
    public byte Pad1;
    public byte Pad2;
    public uint Pad3;
}

/// <summary>
/// Mirror of physics.physics_material (8 bytes).
/// </summary>
[NativeComponent("conjure.physics.physics_material")]
[StructLayout(LayoutKind.Sequential)]
public struct PhysicsMaterialData : IComponent
{
    public float Friction;
    public float Restitution;

    public static PhysicsMaterialData Default => new PhysicsMaterialData { Friction = 0.5f };
}

/// <summary>
/// Mirror of physics.character_controller (68 bytes): the capsule and the step's ground report.
/// </summary>
[NativeComponent("conjure.physics.character_controller")]
[StructLayout(LayoutKind.Sequential)]
public struct CharacterControllerData : IComponent
{
    public float Radius;
    public float Height;
    public float MaxSlopeAngleDegrees;
    public float StepHeight;
    public float Mass;
    public float MaxStrength;
    public float Padding;
    public byte Layer;
    public CharacterFlags Flags;
    public byte Pad0;
    public byte Pad1;
    public Vector3 Velocity;
    public GroundState Ground;
    public byte Pad2;
    public byte Pad3;
    public byte Pad4;
    public Vector3 GroundNormal;
    public EntityHandle GroundEntity;

    public static CharacterControllerData Default => new CharacterControllerData
    {
        Radius = 0.3f,
        Height = 1.8f,
        MaxSlopeAngleDegrees = 45.0f,
        StepHeight = 0.4f,
        Mass = 70.0f,
        MaxStrength = 100.0f,
        Padding = 0.02f,
        Flags = CharacterFlags.StickToFloor | CharacterFlags.WalkStairs | CharacterFlags.ApplyGravity,
        Ground = GroundState.InAir,
        GroundNormal = Vector3.UnitY,
    };
}
