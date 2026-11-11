using System;
using System.Collections.Generic;
using System.Numerics;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using Conjure.Interop;

namespace Conjure.Runtime;

/// <summary>
/// One reflected field of a component mirror or [EcsComponent] struct.
/// </summary>
public readonly struct ComponentFieldLayout
{
    public ComponentFieldLayout(string name, string typeName, byte kind, int offset, int size, int alignment = 0)
    {
        Name = name;
        TypeName = typeName;
        Kind = kind;
        Offset = offset;
        Size = size;
        Alignment = alignment > 0 ? alignment : LayoutHash.AlignmentOfKind(kind);
    }

    /// <summary>
    /// The native field name: the C# name with its first letter lower-cased (R31/B10).
    /// </summary>
    public string Name { get; }

    /// <summary>
    /// The core::reflect type name ("f32", "vec3", "conjure.ecs.entity").
    /// </summary>
    public string TypeName { get; }

    /// <summary>
    /// core::reflect::field_kind value.
    /// </summary>
    public byte Kind { get; }

    public int Offset { get; }

    public int Size { get; }

    /// <summary>
    /// Natural alignment of the field in the managed layout (a nested struct's is its strictest field's).
    /// </summary>
    public int Alignment { get; }
}

/// <summary>
/// The layout of an unmanaged struct as core::reflect describes it, and its layout hash: FNV-1a 64 over
/// (name bytes, kind byte, offset u32 LE, size u32 LE) per field in declaration order, exactly as
/// type_info::layout_hash() (10_core.md 2.11, R61). Fields named Pad<digits> are padding and excluded.
///
/// Offsets and sizes are those of the managed (blittable) layout that Span&lt;T&gt;, sizeof(T) and the
/// component_get/set copies use, never the marshalled one: Marshal.OffsetOf/SizeOf widen bool to 4 bytes
/// and would disagree with the memory the native side reads for every mirror with a bool field.
/// </summary>
public static class LayoutHash
{
    private static readonly MethodInfo SizeOfMethod = typeof(Unsafe).GetMethod(nameof(Unsafe.SizeOf))!;
    // core::reflect::field_kind enumerator values.
    private const byte KindBool = 0;
    private const byte KindI8 = 1;
    private const byte KindI16 = 2;
    private const byte KindI32 = 3;
    private const byte KindI64 = 4;
    private const byte KindU8 = 5;
    private const byte KindU16 = 6;
    private const byte KindU32 = 7;
    private const byte KindU64 = 8;
    private const byte KindF32 = 9;
    private const byte KindF64 = 10;
    private const byte KindStringId = 12;
    private const byte KindUuid = 13;
    private const byte KindVec2 = 14;
    private const byte KindVec3 = 15;
    private const byte KindVec4 = 16;
    private const byte KindQuat = 17;
    private const byte KindMat4 = 18;
    private const byte KindTransform = 19;
    private const byte KindEnum = 20;
    private const byte KindStruct = 21;
    private const byte KindEntity = 23;
    private const byte KindAssetRef = 24;

    /// <summary>
    /// The core type name and kind of a field type, or false when the type cannot sit in a mirror.
    /// </summary>
    public static bool TryDescribe(Type type, out string typeName, out byte kind, out int size)
    {
        return TryDescribe(type, out typeName, out kind, out size, out _);
    }

    /// <summary>
    /// TryDescribe plus the field's natural alignment in the managed layout. Beyond the fixed kinds it
    /// accepts the mirror primitives of 15_bindings.md 2.6 (NativeBool, StringId, AssetId, EntityHandle,
    /// AssetRef&lt;T&gt;), enums (their underlying integer) and nested sequential structs of describable fields.
    /// </summary>
    public static bool TryDescribe(Type type, out string typeName, out byte kind, out int size, out int alignment)
    {
        typeName = "";
        kind = 0;
        size = 0;
        alignment = 1;
        if (type == typeof(Components.NativeBool)) { typeName = "bool"; kind = KindBool; size = 1; return true; }
        if (type == typeof(Components.StringId)) { typeName = "string_id"; kind = KindStringId; size = 8; alignment = 8; return true; }
        if (type == typeof(AssetId)) { typeName = "uuid"; kind = KindUuid; size = 16; alignment = 8; return true; }
        if (type == typeof(Components.EntityHandle)) { typeName = "conjure.ecs.entity"; kind = KindEntity; size = 8; alignment = 4; return true; }
        if (type.IsEnum)
        {
            if (!TryDescribe(Enum.GetUnderlyingType(type), out _, out _, out size, out alignment))
            {
                return false;
            }
            typeName = type.FullName ?? type.Name;
            kind = KindEnum;
            return true;
        }
        if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(AssetRef<>))
        {
            typeName = "conjure.assets.asset_ref";
            kind = KindAssetRef;
            size = 24;
            alignment = 8;
            return true;
        }
        if (TryDescribeFixed(type, out typeName, out kind, out size))
        {
            alignment = AlignmentOfKind(kind);
            return true;
        }
        if (type.IsValueType && !type.IsPrimitive && TryDescribeNested(type, out size, out alignment))
        {
            typeName = type.FullName ?? type.Name;
            kind = KindStruct;
            return true;
        }
        return false;
    }

    private static bool TryDescribeFixed(Type type, out string typeName, out byte kind, out int size)
    {
        typeName = "";
        kind = 0;
        size = 0;
        if (type == typeof(bool)) { typeName = "bool"; kind = KindBool; size = 1; return true; }
        if (type == typeof(sbyte)) { typeName = "i8"; kind = KindI8; size = 1; return true; }
        if (type == typeof(short)) { typeName = "i16"; kind = KindI16; size = 2; return true; }
        if (type == typeof(int)) { typeName = "i32"; kind = KindI32; size = 4; return true; }
        if (type == typeof(long)) { typeName = "i64"; kind = KindI64; size = 8; return true; }
        if (type == typeof(byte)) { typeName = "u8"; kind = KindU8; size = 1; return true; }
        if (type == typeof(ushort)) { typeName = "u16"; kind = KindU16; size = 2; return true; }
        if (type == typeof(uint)) { typeName = "u32"; kind = KindU32; size = 4; return true; }
        if (type == typeof(ulong)) { typeName = "u64"; kind = KindU64; size = 8; return true; }
        if (type == typeof(float)) { typeName = "f32"; kind = KindF32; size = 4; return true; }
        if (type == typeof(double)) { typeName = "f64"; kind = KindF64; size = 8; return true; }
        if (type == typeof(UuidData)) { typeName = "uuid"; kind = KindUuid; size = 16; return true; }
        if (type == typeof(Vector2)) { typeName = "vec2"; kind = KindVec2; size = 8; return true; }
        if (type == typeof(Vector3)) { typeName = "vec3"; kind = KindVec3; size = 12; return true; }
        if (type == typeof(Vector4)) { typeName = "vec4"; kind = KindVec4; size = 16; return true; }
        if (type == typeof(Quaternion)) { typeName = "quat"; kind = KindQuat; size = 16; return true; }
        if (type == typeof(Matrix4x4)) { typeName = "mat4"; kind = KindMat4; size = 64; return true; }
        if (type == typeof(TransformData)) { typeName = "transform"; kind = KindTransform; size = 40; return true; }
        if (type == typeof(Entity)) { typeName = "conjure.ecs.entity"; kind = KindEntity; size = 8; return true; }
        return false;
    }

    /// <summary>
    /// Size and alignment of a nested sequential struct whose fields are all describable (the strictest
    /// field alignment, capped by Pack); false otherwise.
    /// </summary>
    private static bool TryDescribeNested(Type structType, out int size, out int alignment)
    {
        size = 0;
        alignment = 1;
        StructLayoutAttribute? layoutAttribute = structType.StructLayoutAttribute;
        if (layoutAttribute != null && layoutAttribute.Value == LayoutKind.Auto)
        {
            return false;
        }
        int pack = layoutAttribute == null || layoutAttribute.Pack <= 0 ? int.MaxValue : layoutAttribute.Pack;
        FieldInfo[] fields = structType.GetFields(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
        foreach (FieldInfo field in fields)
        {
            if (!TryDescribe(field.FieldType, out _, out _, out _, out int fieldAlignment))
            {
                return false;
            }
            alignment = Math.Max(alignment, Math.Min(fieldAlignment, pack));
        }
        size = ManagedSizeOf(structType);
        return true;
    }

    /// <summary>
    /// The fields of an unmanaged struct in declaration order (Pad<digits> excluded). Throws
    /// InvalidOperationException for a field type that cannot be described.
    /// </summary>
    public static List<ComponentFieldLayout> Describe(Type structType)
    {
        return Describe(structType, out _);
    }

    /// <summary>
    /// Describe plus the managed size of the struct (Unsafe.SizeOf&lt;T&gt;). The offsets follow the
    /// sequential layout rule the runtime applies to blittable structs (each field at its natural
    /// alignment, capped by Pack) or the [FieldOffset] values of an explicit layout, and the size they add
    /// up to must equal the managed size: a struct the rule cannot describe (LayoutKind.Auto, a field type
    /// outside TryDescribe) throws InvalidOperationException.
    /// </summary>
    public static List<ComponentFieldLayout> Describe(Type structType, out int size)
    {
        if (!structType.IsValueType || structType.IsPrimitive || structType.IsEnum)
        {
            throw new InvalidOperationException(structType.FullName + " is not a struct");
        }
        StructLayoutAttribute? layoutAttribute = structType.StructLayoutAttribute;
        LayoutKind layoutKind = layoutAttribute?.Value ?? LayoutKind.Sequential;
        if (layoutKind == LayoutKind.Auto)
        {
            throw new InvalidOperationException(
                structType.FullName + " needs [StructLayout(LayoutKind.Sequential)]: an auto layout may be reordered");
        }
        int pack = layoutAttribute == null || layoutAttribute.Pack <= 0 ? int.MaxValue : layoutAttribute.Pack;

        FieldInfo[] fields = structType.GetFields(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
        Array.Sort(fields, (left, right) => left.MetadataToken.CompareTo(right.MetadataToken));
        List<ComponentFieldLayout> layout = new List<ComponentFieldLayout>(fields.Length);
        if (fields.Length == 0)
        {
            // A tag: no storage natively (size 0, no reflection), whatever the 1-byte managed placeholder says.
            size = 0;
            return layout;
        }
        int cursor = 0;
        int structAlignment = 1;
        foreach (FieldInfo field in fields)
        {
            if (!TryDescribe(field.FieldType, out string typeName, out byte kind, out int fieldSize, out int naturalAlignment))
            {
                throw new InvalidOperationException(
                    structType.FullName + "." + field.Name + " has a type that cannot be mirrored: " + field.FieldType.Name);
            }
            int alignment = Math.Min(naturalAlignment, pack);
            int offset;
            if (layoutKind == LayoutKind.Explicit)
            {
                FieldOffsetAttribute? explicitOffset = field.GetCustomAttribute<FieldOffsetAttribute>();
                if (explicitOffset == null)
                {
                    throw new InvalidOperationException(structType.FullName + "." + field.Name + " has no [FieldOffset]");
                }
                offset = explicitOffset.Value;
            }
            else
            {
                offset = AlignUp(cursor, alignment);
            }
            cursor = Math.Max(cursor, offset + fieldSize);
            structAlignment = Math.Max(structAlignment, alignment);
            if (!IsPaddingName(field.Name))
            {
                layout.Add(new ComponentFieldLayout(NativeName(field.Name), typeName, kind, offset, fieldSize, alignment));
            }
        }

        int computed = AlignUp(cursor, structAlignment);
        if (layoutAttribute != null && layoutAttribute.Size > computed)
        {
            computed = layoutAttribute.Size;
        }
        size = ManagedSizeOf(structType);
        if (computed != size)
        {
            throw new InvalidOperationException(
                structType.FullName + ": the sequential layout rule gives " + computed + " bytes but the managed size is " +
                size + "; only sequential structs of the supported field types can be mirrored");
        }
        return layout;
    }

    /// <summary>
    /// True for the padding fields excluded from the layout (R61): "Pad" followed by digits only, so a real
    /// field such as character_controller.padding keeps its place in the hash.
    /// </summary>
    public static bool IsPaddingName(string fieldName)
    {
        if (fieldName.Length < 4 || !fieldName.StartsWith("Pad", StringComparison.Ordinal))
        {
            return false;
        }
        for (int index = 3; index < fieldName.Length; ++index)
        {
            if (fieldName[index] < '0' || fieldName[index] > '9')
            {
                return false;
            }
        }
        return true;
    }

    /// <summary>
    /// Unsafe.SizeOf&lt;T&gt;() of a struct named at runtime: the size sizeof(T) and Span&lt;T&gt; use.
    /// </summary>
    public static int ManagedSizeOf(Type structType)
    {
        object? result = SizeOfMethod.MakeGenericMethod(structType).Invoke(null, null);
        return result is int value ? value : 0;
    }

    /// <summary>
    /// Natural alignment of a field kind in the managed layout (and in core::reflect::kind_alignment).
    /// </summary>
    public static int AlignmentOfKind(byte kind)
    {
        return kind switch
        {
            KindI64 or KindU64 or KindF64 or KindStringId or KindUuid or KindEntity or KindAssetRef => 8,
            KindI16 or KindU16 => 2,
            KindBool or KindI8 or KindU8 => 1,
            _ => 4,
        };
    }

    private static int AlignUp(int value, int alignment)
    {
        int remainder = value % alignment;
        return remainder == 0 ? value : value + (alignment - remainder);
    }

    /// <summary>
    /// type_info::layout_hash() of a described layout.
    /// </summary>
    public static ulong Compute(IReadOnlyList<ComponentFieldLayout> layout)
    {
        ulong hash = NameHash.OffsetBasis;
        Span<byte> scratch = stackalloc byte[4];
        foreach (ComponentFieldLayout field in layout)
        {
            hash = NameHash.Fnv1a64(Encoding.UTF8.GetBytes(field.Name), hash);
            hash = NameHash.Step(hash, field.Kind);
            System.Buffers.Binary.BinaryPrimitives.WriteUInt32LittleEndian(scratch, (uint)field.Offset);
            hash = NameHash.Fnv1a64(scratch, hash);
            System.Buffers.Binary.BinaryPrimitives.WriteUInt32LittleEndian(scratch, (uint)field.Size);
            hash = NameHash.Fnv1a64(scratch, hash);
        }
        return hash;
    }

    /// <summary>
    /// The strictest natural alignment among the fields (1 for an empty layout).
    /// </summary>
    public static int Alignment(IReadOnlyList<ComponentFieldLayout> layout)
    {
        int alignment = 1;
        foreach (ComponentFieldLayout field in layout)
        {
            int fieldAlignment = field.Alignment;
            if (fieldAlignment > alignment)
            {
                alignment = fieldAlignment;
            }
        }
        return alignment;
    }

    /// <summary>
    /// The schema JSON component_register_managed parses: {"fields":[{"name","type","offset"}]}.
    /// </summary>
    public static string SchemaJson(IReadOnlyList<ComponentFieldLayout> layout)
    {
        StringBuilder builder = new StringBuilder();
        builder.Append("{\"fields\":[");
        for (int index = 0; index < layout.Count; ++index)
        {
            if (index > 0)
            {
                builder.Append(',');
            }
            builder.Append("{\"name\":");
            JsonText.AppendString(builder, layout[index].Name);
            builder.Append(",\"type\":");
            JsonText.AppendString(builder, layout[index].TypeName);
            builder.Append(",\"offset\":").Append(layout[index].Offset).Append('}');
        }
        builder.Append("]}");
        return builder.ToString();
    }

    /// <summary>
    /// "LinearVelocity" -> "linearVelocity", "X" -> "x" (R31).
    /// </summary>
    public static string NativeName(string csharpName)
    {
        if (csharpName.Length == 0 || char.IsLower(csharpName[0]))
        {
            return csharpName;
        }
        return char.ToLowerInvariant(csharpName[0]) + csharpName.Substring(1);
    }
}
