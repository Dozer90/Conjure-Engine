using System;
using System.Buffers.Binary;
using System.Numerics;
using System.Runtime.InteropServices;
using System.Text;
using Conjure.Interop;

namespace Conjure.Runtime;

/// <summary>
/// Writes the field blob of docs/design/03_scripting.md 2.2 into a caller-provided span: two passes, no
/// allocation. Writes past the end set Overflowed and keep counting so RequiredSize is always right.
///   header : u32 magic 'CSFB' | u16 version | u16 field_count
///   field  : u64 hash | u8 kind | u8 element_kind | u16 reserved | u32 payload_size | payload (padded to 4)
/// </summary>
public ref struct FieldBlobWriter
{
    public const uint Magic = 0x42465343;
    public const ushort Version = 1;
    public const int HeaderSize = 8;
    public const int FieldHeaderSize = 16;

    private readonly Span<byte> _output;
    private int _cursor;
    private ushort _fieldCount;
    private int _openSizeOffset;
    private int _openPayloadStart;
    private bool _fieldOpen;

    public FieldBlobWriter(Span<byte> output)
    {
        _output = output;
        _cursor = 0;
        _fieldCount = 0;
        _openSizeOffset = 0;
        _openPayloadStart = 0;
        _fieldOpen = false;
        Overflowed = false;
    }

    public bool Overflowed { get; private set; }

    public int RequiredSize => _cursor;

    public int FieldCount => _fieldCount;

    public void Begin()
    {
        WriteU32(Magic);
        WriteU16(Version);
        WriteU16(0);
    }

    public void BeginField(ulong hash, ScriptFieldKind kind, ScriptFieldKind elementKind)
    {
        WriteU64(hash);
        WriteU8((byte)kind);
        WriteU8((byte)elementKind);
        WriteU16(0);
        _openSizeOffset = _cursor;
        WriteU32(0);
        _openPayloadStart = _cursor;
        _fieldOpen = true;
    }

    public void EndField()
    {
        if (!_fieldOpen)
        {
            return;
        }
        uint payloadSize = (uint)(_cursor - _openPayloadStart);
        if (_openSizeOffset + 4 <= _output.Length)
        {
            BinaryPrimitives.WriteUInt32LittleEndian(_output.Slice(_openSizeOffset, 4), payloadSize);
        }
        PadTo4();
        _fieldOpen = false;
        ++_fieldCount;
    }

    /// <summary>
    /// Patches the field count and returns the bytes needed.
    /// </summary>
    public int Finish()
    {
        if (_fieldOpen)
        {
            EndField();
        }
        if (6 + 2 <= _output.Length)
        {
            BinaryPrimitives.WriteUInt16LittleEndian(_output.Slice(6, 2), _fieldCount);
        }
        return _cursor;
    }

    public void WriteBytes(scoped ReadOnlySpan<byte> bytes)
    {
        if (bytes.Length == 0)
        {
            return;
        }
        if (_cursor + bytes.Length <= _output.Length)
        {
            bytes.CopyTo(_output.Slice(_cursor));
        }
        else
        {
            Overflowed = true;
        }
        _cursor += bytes.Length;
    }

    public void WriteU8(byte value)
    {
        Span<byte> scratch = stackalloc byte[1];
        scratch[0] = value;
        WriteBytes(scratch);
    }

    public void WriteU16(ushort value)
    {
        Span<byte> scratch = stackalloc byte[2];
        BinaryPrimitives.WriteUInt16LittleEndian(scratch, value);
        WriteBytes(scratch);
    }

    public void WriteU32(uint value)
    {
        Span<byte> scratch = stackalloc byte[4];
        BinaryPrimitives.WriteUInt32LittleEndian(scratch, value);
        WriteBytes(scratch);
    }

    public void WriteI32(int value)
    {
        Span<byte> scratch = stackalloc byte[4];
        BinaryPrimitives.WriteInt32LittleEndian(scratch, value);
        WriteBytes(scratch);
    }

    public void WriteU64(ulong value)
    {
        Span<byte> scratch = stackalloc byte[8];
        BinaryPrimitives.WriteUInt64LittleEndian(scratch, value);
        WriteBytes(scratch);
    }

    public void WriteI64(long value)
    {
        Span<byte> scratch = stackalloc byte[8];
        BinaryPrimitives.WriteInt64LittleEndian(scratch, value);
        WriteBytes(scratch);
    }

    public void WriteF32(float value)
    {
        Span<byte> scratch = stackalloc byte[4];
        BinaryPrimitives.WriteSingleLittleEndian(scratch, value);
        WriteBytes(scratch);
    }

    public void WriteF64(double value)
    {
        Span<byte> scratch = stackalloc byte[8];
        BinaryPrimitives.WriteDoubleLittleEndian(scratch, value);
        WriteBytes(scratch);
    }

    /// <summary>
    /// UTF-8 bytes of a string (no length prefix; the field header carries the size).
    /// </summary>
    public void WriteStringBytes(string text)
    {
        int byteCount = Encoding.UTF8.GetByteCount(text);
        if (byteCount == 0)
        {
            return;
        }
        if (_cursor + byteCount <= _output.Length)
        {
            Encoding.UTF8.GetBytes(text, _output.Slice(_cursor, byteCount));
        }
        else
        {
            Overflowed = true;
        }
        _cursor += byteCount;
    }

    /// <summary>
    /// A list element string: u32 byte length, bytes, padding to 4.
    /// </summary>
    public void WriteLengthPrefixedString(string text)
    {
        WriteU32((uint)Encoding.UTF8.GetByteCount(text));
        WriteStringBytes(text);
        PadTo4();
    }

    public void PadTo4()
    {
        int padded = (_cursor + 3) & ~3;
        Span<byte> zeros = stackalloc byte[4];
        zeros.Clear();
        WriteBytes(zeros.Slice(0, padded - _cursor));
    }

    /*-----------------------------------------------------------------*/
    // Whole-field helpers

    public void WriteField(ulong hash, ScriptFieldKind kind, ReadOnlySpan<byte> payload)
    {
        BeginField(hash, kind, kind);
        WriteBytes(payload);
        EndField();
    }

    public void WriteBool(ulong hash, bool value)
    {
        BeginField(hash, ScriptFieldKind.Bool, ScriptFieldKind.Bool);
        WriteU8(value ? (byte)1 : (byte)0);
        EndField();
    }

    public void WriteInt32(ulong hash, int value, ScriptFieldKind kind = ScriptFieldKind.Int32)
    {
        BeginField(hash, kind, kind);
        WriteI32(value);
        EndField();
    }

    public void WriteInt64(ulong hash, long value)
    {
        BeginField(hash, ScriptFieldKind.Int64, ScriptFieldKind.Int64);
        WriteI64(value);
        EndField();
    }

    public void WriteFloat(ulong hash, float value)
    {
        BeginField(hash, ScriptFieldKind.Float, ScriptFieldKind.Float);
        WriteF32(value);
        EndField();
    }

    public void WriteDouble(ulong hash, double value)
    {
        BeginField(hash, ScriptFieldKind.Double, ScriptFieldKind.Double);
        WriteF64(value);
        EndField();
    }

    public void WriteString(ulong hash, string value)
    {
        BeginField(hash, ScriptFieldKind.String, ScriptFieldKind.String);
        WriteStringBytes(value);
        EndField();
    }

    public void WriteEntity(ulong hash, ulong entityId)
    {
        BeginField(hash, ScriptFieldKind.Entity, ScriptFieldKind.Entity);
        WriteU64(entityId);
        EndField();
    }
}

/// <summary>
/// One field as the reader yields it.
/// </summary>
public ref struct FieldBlobField
{
    public ulong Hash;
    public ScriptFieldKind Kind;
    public ScriptFieldKind ElementKind;
    public ReadOnlySpan<byte> Payload;
}

/// <summary>
/// Bounds-checked reader of a field blob. An empty span is a valid, empty blob.
/// </summary>
public ref struct FieldBlobReader
{
    private readonly ReadOnlySpan<byte> _blob;
    private int _cursor;
    private readonly ushort _fieldCount;
    private int _fieldsRead;

    public FieldBlobReader(ReadOnlySpan<byte> blob)
    {
        _blob = blob;
        _cursor = 0;
        _fieldCount = 0;
        _fieldsRead = 0;
        IsValid = false;
        if (blob.Length == 0)
        {
            IsValid = true;
            return;
        }
        if (blob.Length < FieldBlobWriter.HeaderSize)
        {
            return;
        }
        uint magic = BinaryPrimitives.ReadUInt32LittleEndian(blob);
        ushort version = BinaryPrimitives.ReadUInt16LittleEndian(blob.Slice(4));
        if (magic != FieldBlobWriter.Magic || version != FieldBlobWriter.Version)
        {
            return;
        }
        _fieldCount = BinaryPrimitives.ReadUInt16LittleEndian(blob.Slice(6));
        _cursor = FieldBlobWriter.HeaderSize;
        IsValid = true;
    }

    public bool IsValid { get; }

    public int FieldCount => _fieldCount;

    /// <summary>
    /// Reads the next field; false at the end. Throws InvalidDataException for a truncated field.
    /// </summary>
    public bool Next(out FieldBlobField field)
    {
        field = default;
        if (!IsValid || _blob.Length == 0 || _fieldsRead >= _fieldCount)
        {
            return false;
        }
        if (_cursor + FieldBlobWriter.FieldHeaderSize > _blob.Length)
        {
            throw new System.IO.InvalidDataException("field blob: field header past the end");
        }
        field.Hash = BinaryPrimitives.ReadUInt64LittleEndian(_blob.Slice(_cursor));
        byte kind = _blob[_cursor + 8];
        byte elementKind = _blob[_cursor + 9];
        uint payloadSize = BinaryPrimitives.ReadUInt32LittleEndian(_blob.Slice(_cursor + 12));
        if (kind >= (byte)ScriptFieldKind.Count || elementKind >= (byte)ScriptFieldKind.Count)
        {
            throw new System.IO.InvalidDataException("field blob: unknown field kind");
        }
        int payloadStart = _cursor + FieldBlobWriter.FieldHeaderSize;
        if (payloadStart + (long)payloadSize > _blob.Length)
        {
            throw new System.IO.InvalidDataException("field blob: payload past the end");
        }
        field.Kind = (ScriptFieldKind)kind;
        field.ElementKind = (ScriptFieldKind)elementKind;
        field.Payload = _blob.Slice(payloadStart, (int)payloadSize);
        int next = (payloadStart + (int)payloadSize + 3) & ~3;
        _cursor = next > _blob.Length ? _blob.Length : next;
        ++_fieldsRead;
        return true;
    }
}

/// <summary>
/// Scalar payload codecs shared by the field accessors.
/// </summary>
internal static class FieldPayload
{
    public static int FixedSize(ScriptFieldKind kind)
    {
        switch (kind)
        {
            case ScriptFieldKind.Bool:
                return 1;
            case ScriptFieldKind.Int32:
            case ScriptFieldKind.Enum:
            case ScriptFieldKind.Float:
                return 4;
            case ScriptFieldKind.Int64:
            case ScriptFieldKind.Double:
            case ScriptFieldKind.Entity:
            case ScriptFieldKind.Vec2:
                return 8;
            case ScriptFieldKind.Vec3:
                return 12;
            case ScriptFieldKind.Vec4:
            case ScriptFieldKind.Quat:
            case ScriptFieldKind.Color:
            case ScriptFieldKind.Asset:
                return 16;
            default:
                return 0;
        }
    }

    public static object? Decode(ScriptFieldKind kind, ReadOnlySpan<byte> payload, Type targetType)
    {
        if (kind == ScriptFieldKind.String)
        {
            return Encoding.UTF8.GetString(payload);
        }
        int needed = FixedSize(kind);
        if (payload.Length < needed)
        {
            return null;
        }
        switch (kind)
        {
            case ScriptFieldKind.Bool:
                return payload[0] != 0;
            case ScriptFieldKind.Int32:
                return BinaryPrimitives.ReadInt32LittleEndian(payload);
            case ScriptFieldKind.Enum:
                return Enum.ToObject(targetType, (long)BinaryPrimitives.ReadInt32LittleEndian(payload));
            case ScriptFieldKind.Int64:
                return BinaryPrimitives.ReadInt64LittleEndian(payload);
            case ScriptFieldKind.Float:
                return BinaryPrimitives.ReadSingleLittleEndian(payload);
            case ScriptFieldKind.Double:
                return BinaryPrimitives.ReadDoubleLittleEndian(payload);
            case ScriptFieldKind.Vec2:
                return new Vector2(ReadF32(payload, 0), ReadF32(payload, 4));
            case ScriptFieldKind.Vec3:
                return new Vector3(ReadF32(payload, 0), ReadF32(payload, 4), ReadF32(payload, 8));
            case ScriptFieldKind.Vec4:
                return new Vector4(ReadF32(payload, 0), ReadF32(payload, 4), ReadF32(payload, 8), ReadF32(payload, 12));
            case ScriptFieldKind.Quat:
                return new Quaternion(ReadF32(payload, 0), ReadF32(payload, 4), ReadF32(payload, 8), ReadF32(payload, 12));
            case ScriptFieldKind.Color:
                return new Color(ReadF32(payload, 0), ReadF32(payload, 4), ReadF32(payload, 8), ReadF32(payload, 12));
            case ScriptFieldKind.Entity:
                return new Entity(BinaryPrimitives.ReadUInt64LittleEndian(payload));
            case ScriptFieldKind.Asset:
            {
                UuidData id = new UuidData(
                    BinaryPrimitives.ReadUInt64LittleEndian(payload),
                    BinaryPrimitives.ReadUInt64LittleEndian(payload.Slice(8)));
                return AssetRefFactory.Create(targetType, id);
            }
            default:
                return null;
        }
    }

    public static void Encode(ref FieldBlobWriter writer, ScriptFieldKind kind, object? value)
    {
        switch (kind)
        {
            case ScriptFieldKind.Bool:
                writer.WriteU8(value is true ? (byte)1 : (byte)0);
                return;
            case ScriptFieldKind.Int32:
                writer.WriteI32(value is int i ? i : 0);
                return;
            case ScriptFieldKind.Enum:
                writer.WriteI32(value == null ? 0 : unchecked((int)Convert.ToInt64(value, System.Globalization.CultureInfo.InvariantCulture)));
                return;
            case ScriptFieldKind.Int64:
                writer.WriteI64(value is long l ? l : 0);
                return;
            case ScriptFieldKind.Float:
                writer.WriteF32(value is float f ? f : 0.0f);
                return;
            case ScriptFieldKind.Double:
                writer.WriteF64(value is double d ? d : 0.0);
                return;
            case ScriptFieldKind.String:
                writer.WriteStringBytes(value as string ?? "");
                return;
            case ScriptFieldKind.Vec2:
            {
                Vector2 v = value is Vector2 vector ? vector : Vector2.Zero;
                writer.WriteF32(v.X);
                writer.WriteF32(v.Y);
                return;
            }
            case ScriptFieldKind.Vec3:
            {
                Vector3 v = value is Vector3 vector ? vector : Vector3.Zero;
                writer.WriteF32(v.X);
                writer.WriteF32(v.Y);
                writer.WriteF32(v.Z);
                return;
            }
            case ScriptFieldKind.Vec4:
            {
                Vector4 v = value is Vector4 vector ? vector : Vector4.Zero;
                writer.WriteF32(v.X);
                writer.WriteF32(v.Y);
                writer.WriteF32(v.Z);
                writer.WriteF32(v.W);
                return;
            }
            case ScriptFieldKind.Quat:
            {
                Quaternion q = value is Quaternion quaternion ? quaternion : Quaternion.Identity;
                writer.WriteF32(q.X);
                writer.WriteF32(q.Y);
                writer.WriteF32(q.Z);
                writer.WriteF32(q.W);
                return;
            }
            case ScriptFieldKind.Color:
            {
                Color c = value is Color color ? color : Color.White;
                writer.WriteF32(c.R);
                writer.WriteF32(c.G);
                writer.WriteF32(c.B);
                writer.WriteF32(c.A);
                return;
            }
            case ScriptFieldKind.Entity:
                writer.WriteU64(value is Entity entity ? entity.Id : 0);
                return;
            case ScriptFieldKind.Asset:
            {
                UuidData id = value is IAssetRef reference ? reference.Id : default;
                writer.WriteU64(id.High);
                writer.WriteU64(id.Low);
                return;
            }
            default:
                return;
        }
    }

    private static float ReadF32(ReadOnlySpan<byte> payload, int offset)
    {
        return BinaryPrimitives.ReadSingleLittleEndian(payload.Slice(offset));
    }
}
