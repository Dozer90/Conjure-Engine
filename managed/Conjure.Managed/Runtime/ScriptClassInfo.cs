using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.Numerics;
using System.Reflection;
using System.Text;
using Conjure.Interop;

namespace Conjure.Runtime;

/// <summary>
/// One serialised field of a script class: how to read and write it through reflection and the blob
/// codec (docs/design/03_scripting.md 3.6, 3.7). Boxing is accepted at load and inspector time.
/// </summary>
public sealed class FieldAccessor
{
    public FieldAccessor(FieldInfo field, ScriptFieldKind kind, ScriptFieldKind elementKind, Type elementType)
        : this(field, field.Name, kind, elementKind, elementType)
    {
    }

    /// <summary>
    /// An accessor serialised under 'name' (the bare field name, or "&lt;DeclaringType&gt;.&lt;name&gt;" for a
    /// base field hidden by a derived one).
    /// </summary>
    public FieldAccessor(FieldInfo field, string name, ScriptFieldKind kind, ScriptFieldKind elementKind, Type elementType)
    {
        Field = field;
        Name = name;
        Hash = NameHash.Fnv1a64(name);
        Kind = kind;
        ElementKind = elementKind;
        ElementType = elementType;
    }

    public FieldInfo Field { get; }

    /// <summary>
    /// The serialised name: the JSON key and the source of Hash.
    /// </summary>
    public string Name { get; }

    public ulong Hash { get; }

    public ScriptFieldKind Kind { get; }

    public ScriptFieldKind ElementKind { get; }

    /// <summary>
    /// The value type: the element type for lists and arrays, the field type otherwise.
    /// </summary>
    public Type ElementType { get; }

    public bool IsArray => Kind == ScriptFieldKind.List && Field.FieldType.IsArray;

    /// <summary>
    /// Writes the field's current value as one blob field.
    /// </summary>
    public void Write(ScriptBehaviour instance, ref FieldBlobWriter writer)
    {
        object? value = Field.GetValue(instance);
        writer.BeginField(Hash, Kind, ElementKind);
        if (Kind == ScriptFieldKind.List)
        {
            int count = 0;
            if (value is ICollection collection)
            {
                count = collection.Count;
            }
            writer.WriteU32((uint)count);
            if (value is IEnumerable sequence)
            {
                foreach (object? element in sequence)
                {
                    if (ElementKind == ScriptFieldKind.String)
                    {
                        writer.WriteLengthPrefixedString(element as string ?? "");
                    }
                    else
                    {
                        FieldPayload.Encode(ref writer, ElementKind, element);
                    }
                }
            }
        }
        else
        {
            FieldPayload.Encode(ref writer, Kind, value);
        }
        writer.EndField();
    }

    /// <summary>
    /// Sets the field from a blob payload; false when the payload does not decode.
    /// </summary>
    public bool Read(ScriptBehaviour instance, ScriptFieldKind kind, ReadOnlySpan<byte> payload)
    {
        if (kind != Kind)
        {
            return false;
        }
        if (Kind != ScriptFieldKind.List)
        {
            object? value = FieldPayload.Decode(Kind, payload, Field.FieldType);
            if (value == null)
            {
                return false;
            }
            Field.SetValue(instance, value);
            return true;
        }

        if (payload.Length < 4)
        {
            return false;
        }
        uint count = System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(payload);
        // The count is validated against the payload before anything is allocated: a corrupt or hostile
        // blob must fail the field, not try to allocate gigabytes first. Every element needs at least its
        // fixed size, or a 4-byte length prefix for strings.
        int elementSize = ElementKind == ScriptFieldKind.String ? 4 : FieldPayload.FixedSize(ElementKind);
        if (elementSize == 0 || count > (uint)((payload.Length - 4) / elementSize))
        {
            return false;
        }
        int cursor = 4;
        IList list = IsArray
            ? Array.CreateInstance(ElementType, (int)count)
            : (IList)Activator.CreateInstance(Field.FieldType)!;
        for (int index = 0; index < count; ++index)
        {
            object? element;
            if (ElementKind == ScriptFieldKind.String)
            {
                if (cursor + 4 > payload.Length)
                {
                    return false;
                }
                int length = (int)System.Buffers.Binary.BinaryPrimitives.ReadUInt32LittleEndian(payload.Slice(cursor));
                cursor += 4;
                if (cursor + length > payload.Length)
                {
                    return false;
                }
                element = Encoding.UTF8.GetString(payload.Slice(cursor, length));
                cursor = (cursor + length + 3) & ~3;
            }
            else
            {
                int size = FieldPayload.FixedSize(ElementKind);
                if (size == 0 || cursor + size > payload.Length)
                {
                    return false;
                }
                element = FieldPayload.Decode(ElementKind, payload.Slice(cursor, size), ElementType);
                cursor += size;
                if (element == null)
                {
                    return false;
                }
            }
            if (IsArray)
            {
                list[index] = element;
            }
            else
            {
                list.Add(element);
            }
        }
        Field.SetValue(instance, list);
        return true;
    }

    /// <summary>
    /// The canonical JSON of the field's value on 'defaults' (a throwaway instance).
    /// </summary>
    public string DefaultJson(ScriptBehaviour defaults)
    {
        return DefaultJson(defaults, out _);
    }

    /// <summary>
    /// DefaultJson, and whether a NaN or infinite float in the value was replaced (JSON has neither): NaN by 0,
    /// an infinity by the largest finite value of its sign.
    /// </summary>
    public string DefaultJson(ScriptBehaviour defaults, out bool clamped)
    {
        return JsonText.Value(Field.GetValue(defaults), Kind, ElementKind, out clamped);
    }

    /// <summary>
    /// {"tooltip","min","max","enumNames","enumValues","enumFlags","assetType","hidden"} from the attributes and
    /// the field type (3.7); a key with nothing to say is left out. A [Range] bound that is not finite is left out:
    /// the slider is open on that side. An enum field (or a list of enums) gets enumNames and the parallel
    /// enumValues, the int32 the blob carries for each name (a uint enum's 0xFFFFFFFF is -1), and enumFlags: true
    /// when the enum is [Flags]. An asset field (or a list of them) gets assetType, the registered asset type name of
    /// the AssetRef tag (AssetTypeId&lt;T&gt;.TypeName, for example "conjure.audio.clip"), left out for a tag that
    /// is not an IAssetType (an untyped reference).
    /// </summary>
    public string MetaJson()
    {
        StringBuilder builder = new StringBuilder();
        builder.Append('{');
        bool any = false;
        void Separator()
        {
            if (any)
            {
                builder.Append(',');
            }
            any = true;
        }

        TooltipAttribute? tooltip = Field.GetCustomAttribute<TooltipAttribute>();
        if (tooltip != null)
        {
            Separator();
            builder.Append("\"tooltip\":");
            JsonText.AppendString(builder, tooltip.Text);
        }
        RangeAttribute? range = Field.GetCustomAttribute<RangeAttribute>();
        bool boundClamped = false;      // stays false: only finite bounds are written
        if (range != null && double.IsFinite(range.Min))
        {
            Separator();
            builder.Append("\"min\":").Append(JsonText.Number(range.Min, ref boundClamped));
        }
        if (range != null && double.IsFinite(range.Max))
        {
            Separator();
            builder.Append("\"max\":").Append(JsonText.Number(range.Max, ref boundClamped));
        }
        Type enumType = Kind == ScriptFieldKind.Enum ? Field.FieldType : ElementKind == ScriptFieldKind.Enum ? ElementType : typeof(void);
        if (enumType.IsEnum)
        {
            string[] names = Enum.GetNames(enumType);
            Separator();
            builder.Append("\"enumNames\":[");
            for (int index = 0; index < names.Length; ++index)
            {
                if (index > 0)
                {
                    builder.Append(',');
                }
                JsonText.AppendString(builder, names[index]);
            }
            builder.Append("],\"enumValues\":[");
            for (int index = 0; index < names.Length; ++index)
            {
                if (index > 0)
                {
                    builder.Append(',');
                }
                builder.Append(EnumBlobValue(enumType, names[index]).ToString(CultureInfo.InvariantCulture));
            }
            builder.Append(']');
            if (enumType.IsDefined(typeof(FlagsAttribute), false))
            {
                builder.Append(",\"enumFlags\":true");
            }
        }
        Type assetType = Kind == ScriptFieldKind.Asset ? Field.FieldType : ElementKind == ScriptFieldKind.Asset ? ElementType : typeof(void);
        string assetTypeName = assetType.IsGenericType
            ? RegisteredAssetTypeName(assetType.GetGenericArguments()[0])
            : "";
        if (assetTypeName.Length > 0)
        {
            Separator();
            builder.Append("\"assetType\":");
            JsonText.AppendString(builder, assetTypeName);
        }
        if (Field.GetCustomAttribute<HideInInspectorAttribute>() != null)
        {
            Separator();
            builder.Append("\"hidden\":true");
        }
        builder.Append('}');
        return builder.ToString();
    }

    /// <summary>
    /// The int32 the ENUM payload carries for the constant 'name' of 'enumType' (FieldPayload.Encode): the raw
    /// constant, so an alias names the same value as its original. 64-bit enums never get here (TryClassifyScalar).
    /// </summary>
    private static int EnumBlobValue(Type enumType, string name)
    {
        FieldInfo? constant = enumType.GetField(name, BindingFlags.Public | BindingFlags.Static);
        object? raw = constant?.GetRawConstantValue();
        if (raw == null)
        {
            return 0;
        }
        return unchecked((int)Convert.ToInt64(raw, CultureInfo.InvariantCulture));
    }

    /// <summary>
    /// AssetTypeId&lt;tag&gt;.TypeName, the registered asset type name of an AssetRef tag; "" for a tag that is
    /// not an IAssetType, and for one whose TypeName threw (logged at INFO with the field, like the other fields the
    /// catalog cannot describe fully; the reference itself stays serialised).
    /// </summary>
    private string RegisteredAssetTypeName(Type tag)
    {
        FieldInfo? typeName = typeof(AssetTypeId<>).MakeGenericType(tag)
            .GetField(nameof(AssetTypeId<object>.TypeName), BindingFlags.Public | BindingFlags.Static);
        try
        {
            return typeName?.GetValue(null) as string ?? "";
        }
        catch (Exception exception)
            when (exception is TypeInitializationException || exception is TargetInvocationException)
        {
            // The type initializer's failure reaches here wrapped more than once: the innermost exception is the
            // one TypeName threw.
            Exception cause = exception;
            while (cause.InnerException != null)
            {
                cause = cause.InnerException;
            }
            string reason = cause.Message;
            NativeBridge.Log(
                LogLevel.Info,
                (Field.DeclaringType?.FullName ?? "") + "." + Field.Name + ": asset type tag " + tag.FullName
                    + " threw from TypeName (" + reason + "); its assetType is left out of the field meta");
            return "";
        }
    }

    /// <summary>
    /// Classifies a field type; false for types that are not serialised (3.7).
    /// </summary>
    public static bool TryClassify(Type type, out ScriptFieldKind kind, out ScriptFieldKind elementKind, out Type elementType)
    {
        elementKind = ScriptFieldKind.Int32;
        elementType = type;
        if (TryClassifyScalar(type, out kind))
        {
            return true;
        }
        Type? element = null;
        if (type.IsArray && type.GetArrayRank() == 1)
        {
            element = type.GetElementType();
        }
        else if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(List<>))
        {
            element = type.GetGenericArguments()[0];
        }
        if (element == null || !TryClassifyScalar(element, out elementKind))
        {
            return false;
        }
        kind = ScriptFieldKind.List;
        elementType = element;
        return true;
    }

    private static bool TryClassifyScalar(Type type, out ScriptFieldKind kind)
    {
        kind = ScriptFieldKind.Int32;
        if (type == typeof(bool)) { kind = ScriptFieldKind.Bool; return true; }
        if (type == typeof(int)) { kind = ScriptFieldKind.Int32; return true; }
        if (type == typeof(long)) { kind = ScriptFieldKind.Int64; return true; }
        if (type == typeof(float)) { kind = ScriptFieldKind.Float; return true; }
        if (type == typeof(double)) { kind = ScriptFieldKind.Double; return true; }
        if (type == typeof(string)) { kind = ScriptFieldKind.String; return true; }
        if (type == typeof(Vector2)) { kind = ScriptFieldKind.Vec2; return true; }
        if (type == typeof(Vector3)) { kind = ScriptFieldKind.Vec3; return true; }
        if (type == typeof(Vector4)) { kind = ScriptFieldKind.Vec4; return true; }
        if (type == typeof(Quaternion)) { kind = ScriptFieldKind.Quat; return true; }
        if (type == typeof(Color)) { kind = ScriptFieldKind.Color; return true; }
        if (type == typeof(Entity)) { kind = ScriptFieldKind.Entity; return true; }
        if (type.IsEnum)
        {
            // The ENUM payload is an int32 (03_scripting.md 3.6): a 64-bit enum would be truncated, so it is
            // not serialised and the catalog logs the skipped field at INFO.
            Type underlying = Enum.GetUnderlyingType(type);
            if (underlying == typeof(long) || underlying == typeof(ulong))
            {
                return false;
            }
            kind = ScriptFieldKind.Enum;
            return true;
        }
        if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(AssetRef<>)) { kind = ScriptFieldKind.Asset; return true; }
        return false;
    }
}

/// <summary>
/// Everything the runtime knows about one ScriptBehaviour subclass (docs/design/03_scripting.md 2.3).
/// Owned by the catalog of its load context and dropped at unload (D9).
/// </summary>
public sealed class ScriptClassInfo
{
    internal ScriptClassInfo(Type type, ConstructorInfo constructor, FieldAccessor[] fields, ushort callbackMask)
    {
        Type = type;
        FullName = type.FullName ?? type.Name;
        Hash = NameHash.Fnv1a64(FullName);
        Constructor = constructor;
        Fields = fields;
        CallbackMask = callbackMask;
        Lists = new PhaseList[(int)ScriptPhase.Count];
        for (int index = 0; index < Lists.Length; ++index)
        {
            Lists[index] = new PhaseList();
        }
    }

    public Type Type { get; }

    public string FullName { get; }

    public ulong Hash { get; }

    public ConstructorInfo Constructor { get; }

    /// <summary>
    /// Sorted by Hash.
    /// </summary>
    public FieldAccessor[] Fields { get; }

    public ushort CallbackMask { get; }

    internal PhaseList[] Lists { get; }

    public bool Overrides(ScriptCallback callback) => (CallbackMask & Guard.Bit(callback)) != 0;

    public ScriptBehaviour CreateInstance()
    {
        ScriptBehaviour instance = (ScriptBehaviour)Constructor.Invoke(null);
        instance.ClassInfo = this;
        return instance;
    }

    /// <summary>
    /// The accessor with this hash (binary search), or null.
    /// </summary>
    public FieldAccessor? FindField(ulong hash)
    {
        int low = 0;
        int high = Fields.Length - 1;
        while (low <= high)
        {
            int middle = (low + high) >> 1;
            ulong candidate = Fields[middle].Hash;
            if (candidate == hash)
            {
                return Fields[middle];
            }
            if (candidate < hash)
            {
                low = middle + 1;
            }
            else
            {
                high = middle - 1;
            }
        }
        return null;
    }

    /// <summary>
    /// Applies a blob to an instance; unknown hashes and mismatched kinds are counted.
    /// </summary>
    public void ApplyFields(ScriptBehaviour instance, ReadOnlySpan<byte> blob, out int unknown)
    {
        unknown = 0;
        FieldBlobReader reader = new FieldBlobReader(blob);
        if (!reader.IsValid)
        {
            throw new System.IO.InvalidDataException("bad field blob header");
        }
        while (reader.Next(out FieldBlobField field))
        {
            FieldAccessor? accessor = FindField(field.Hash);
            if (accessor == null || !accessor.Read(instance, field.Kind, field.Payload))
            {
                ++unknown;
            }
        }
    }

    /// <summary>
    /// Writes every field into 'buffer'; returns the bytes needed (more than the buffer when it was too
    /// small, in which case the caller retries).
    /// </summary>
    public int CollectFields(ScriptBehaviour instance, Span<byte> buffer)
    {
        FieldBlobWriter writer = new FieldBlobWriter(buffer);
        writer.Begin();
        foreach (FieldAccessor accessor in Fields)
        {
            accessor.Write(instance, ref writer);
        }
        return writer.Finish();
    }
}
