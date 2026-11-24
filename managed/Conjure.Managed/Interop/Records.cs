using System.Numerics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Conjure.Interop;

/// <summary>
/// Kind of a serialised script field (conjure::scripting::script_field_kind).
/// </summary>
public enum ScriptFieldKind : byte
{
    Bool,
    Int32,
    Int64,
    Float,
    Double,
    String,
    Vec2,
    Vec3,
    Vec4,
    Quat,
    Color,
    Entity,
    Asset,
    Enum,
    List,
    Count,
}

/// <summary>
/// The phases run_phase takes (conjure::scripting::script_phase).
/// </summary>
public enum ScriptPhase : byte
{
    FixedUpdate,
    Update,
    LateUpdate,
    Count,
}

/// <summary>
/// Bit index of the callback mask and index of the fault counters (conjure::scripting::script_callback). Reload
/// (OnReload) comes last, so the older callbacks keep their bits.
/// </summary>
public enum ScriptCallback : byte
{
    Create,
    Start,
    FixedUpdate,
    Update,
    LateUpdate,
    Destroy,
    CollisionEnter,
    CollisionExit,
    TriggerEnter,
    TriggerExit,
    Reload,
    Count,
}

/// <summary>
/// conjure_collision_record::kind.
/// </summary>
public enum CollisionKind : byte
{
    CollisionEnter,
    CollisionExit,
    TriggerEnter,
    TriggerExit,
}

/// <summary>
/// conjure_cast_shape: the shape a ShapeQuery sweeps (internal to the Physics casts).
/// </summary>
public enum CastShape : byte
{
    Sphere,
    Box,
    Capsule,
}

/// <summary>
/// conjure_uuid (16 bytes): the two halves of a core::uuid, most significant first.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public readonly struct UuidData : System.IEquatable<UuidData>
{
    public readonly ulong High;
    public readonly ulong Low;

    public UuidData(ulong high, ulong low)
    {
        High = high;
        Low = low;
    }

    public bool IsNull => High == 0 && Low == 0;

    public bool Equals(UuidData other) => High == other.High && Low == other.Low;

    public override bool Equals(object? obj) => obj is UuidData other && Equals(other);

    public override int GetHashCode() => System.HashCode.Combine(High, Low);

    public override string ToString()
    {
        // Canonical lower-case 8-4-4-4-12 form of the 128-bit value, high half first.
        string hex = High.ToString("x16") + Low.ToString("x16");
        return hex.Substring(0, 8) + "-" + hex.Substring(8, 4) + "-" + hex.Substring(12, 4) + "-" +
               hex.Substring(16, 4) + "-" + hex.Substring(20, 12);
    }

    public static bool operator ==(UuidData left, UuidData right) => left.Equals(right);

    public static bool operator !=(UuidData left, UuidData right) => !left.Equals(right);
}

/// <summary>
/// conjure_create_request (40 bytes).
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct CreateRequest
{
    public const byte FlagAlreadyStarted = 1;

    /// <summary>
    /// CREATE_FLAG_ALREADY_CREATED: OnCreate already ran in the entity's lifetime (hot reload), so the new instance
    /// runs OnReload instead.
    /// </summary>
    public const byte FlagAlreadyCreated = 2;

    public ulong Entity;
    public ulong ClassHash;
    public byte* Fields;
    public int FieldsSize;
    public byte Flags;
    public int OutSlot;
}

/// <summary>
/// conjure_collision_record (48 bytes).
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct CollisionRecord
{
    public int Slot;
    public ulong Other;
    public Vector3 Point;
    public Vector3 Normal;
    public float Impulse;
    public byte Kind;
}

/// <summary>
/// conjure_script_class_desc (24 bytes).
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct ScriptClassDesc
{
    public ulong Hash;
    public byte* FullName;
    public int NameLength;
    public ushort CallbackMask;
    public ushort FieldCount;
}

/// <summary>
/// conjure_script_field_desc (56 bytes).
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct ScriptFieldDesc
{
    public ulong Hash;
    public byte* Name;
    public int NameLength;
    public byte Kind;
    public byte ElementKind;
    public byte* DefaultJson;
    public int DefaultLength;
    public byte* MetaJson;
    public int MetaLength;
}

/// <summary>
/// conjure_managed_component_desc (56 bytes).
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct ManagedComponentDesc
{
    public ulong NameHash;
    public byte* FullName;
    public int NameLength;
    public uint Size;
    public uint Alignment;
    public ulong LayoutHash;
    public byte* SchemaJson;
    public int SchemaLength;
}

/// <summary>
/// conjure_chunk_view (80 bytes): the entity column and up to eight component columns of one chunk.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct ChunkViewData
{
    public ulong* Entities;
    public fixed ulong Columns[8];
    public int Count;
}

/// <summary>
/// conjure_query_scope (16 bytes).
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct QueryScope
{
    public uint ScopeId;
    public int ChunkCount;
    public ChunkViewData* Chunks;
}

[StructLayout(LayoutKind.Sequential)]
public struct RayQuery
{
    public Vector3 Origin;
    public Vector3 Direction;
    public float MaxDistance;
    public uint LayerMask;
    public ulong IgnoreEntity;
}

[StructLayout(LayoutKind.Sequential)]
public struct ShapeQuery
{
    public RayQuery Ray;
    public byte Shape;
    public byte Pad0;
    public byte Pad1;
    public byte Pad2;
    public Vector3 HalfExtents;
    public float Radius;
    public Quaternion Rotation;
}

[StructLayout(LayoutKind.Sequential)]
public struct RaycastHit
{
    public ulong Entity;
    public Vector3 Point;
    public Vector3 Normal;
    public float Distance;
    public int Layer;
}

[StructLayout(LayoutKind.Sequential)]
public struct AssetInfo
{
    public byte State;
    public byte Pad0;
    public byte Pad1;
    public byte Pad2;
    public uint Version;
    public ulong TypeId;
}

[StructLayout(LayoutKind.Sequential)]
public struct ModuleRecord
{
    public uint VersionMajor;
    public uint VersionMinor;
    public uint VersionPatch;
    public byte State;
    public byte Kind;
    public byte Pad0;
    public byte Pad1;
    public uint FeatureFlags;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct FrameStatsBlock
{
    public float FrameMs;
    public fixed float PhaseMs[10];
    public float SubmitMs;
    public uint FixedSteps;
    public uint JobsExecuted;
    public ulong FrameMemoryHighWater;
}

/// <summary>
/// Expected sizes of the records above (15_bindings.md 2.4); checked once by NativeBridge.Initialize.
/// </summary>
internal static class RecordSizes
{
    public static unsafe bool AllMatch(out string mismatch)
    {
        mismatch = "";
        if (sizeof(TransformData) != 40) { mismatch = "TransformData"; return false; }
        if (sizeof(UuidData) != 16) { mismatch = "UuidData"; return false; }
        if (sizeof(CreateRequest) != 40) { mismatch = "CreateRequest"; return false; }
        if (sizeof(CollisionRecord) != 48) { mismatch = "CollisionRecord"; return false; }
        if (sizeof(ScriptClassDesc) != 24) { mismatch = "ScriptClassDesc"; return false; }
        if (sizeof(ScriptFieldDesc) != 56) { mismatch = "ScriptFieldDesc"; return false; }
        if (sizeof(ManagedComponentDesc) != 56) { mismatch = "ManagedComponentDesc"; return false; }
        if (sizeof(ChunkViewData) != 80) { mismatch = "ChunkViewData"; return false; }
        if (sizeof(QueryScope) != 16) { mismatch = "QueryScope"; return false; }
        if (sizeof(RayQuery) != 40) { mismatch = "RayQuery"; return false; }
        if (sizeof(ShapeQuery) != 80) { mismatch = "ShapeQuery"; return false; }
        if (sizeof(RaycastHit) != 40) { mismatch = "RaycastHit"; return false; }
        if (sizeof(AssetInfo) != 16) { mismatch = "AssetInfo"; return false; }
        if (sizeof(ModuleRecord) != 20) { mismatch = "ModuleRecord"; return false; }
        if (sizeof(FrameStatsBlock) != 64) { mismatch = "FrameStatsBlock"; return false; }
        if (sizeof(GamepadState) != 40) { mismatch = "GamepadState"; return false; }
        if (sizeof(InputBlock) != FrameBlock.ExpectedInputSize) { mismatch = "InputBlock"; return false; }
        if ((int)Marshal.OffsetOf<FrameBlock>(nameof(FrameBlock.FrameIndex)) != FrameBlock.ExpectedFrameIndexOffset)
        {
            mismatch = "FrameBlock.FrameIndex";
            return false;
        }
        if ((int)Marshal.OffsetOf<FrameBlock>(nameof(FrameBlock.Input)) != FrameBlock.ExpectedInputOffset)
        {
            mismatch = "FrameBlock.Input";
            return false;
        }
        return true;
    }
}
