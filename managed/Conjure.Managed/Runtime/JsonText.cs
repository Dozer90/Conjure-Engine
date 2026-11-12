using System;
using System.Collections;
using System.Globalization;
using System.Numerics;
using System.Text;
using Conjure.Interop;

namespace Conjure.Runtime;

/// <summary>
/// A minimal JSON writer for field defaults and inspector metadata. System.Text.Json is avoided on
/// purpose: its metadata caches root game-assembly types and defeat load-context unloading (D7). JSON has no
/// NaN or infinity: a NaN float is written as 0 and an infinite one as the largest finite value of its sign
/// and type, and the 'clamped' flag tells the caller a value was replaced.
/// </summary>
internal static class JsonText
{
    /// <summary>
    /// The value as JSON, and whether a NaN or infinite float in it was replaced by 0 or the largest finite value.
    /// </summary>
    public static string Value(object? value, ScriptFieldKind kind, ScriptFieldKind elementKind, out bool clamped)
    {
        StringBuilder builder = new StringBuilder();
        clamped = false;
        Append(builder, value, kind, elementKind, ref clamped);
        return builder.ToString();
    }

    public static void Append(
        StringBuilder builder,
        object? value,
        ScriptFieldKind kind,
        ScriptFieldKind elementKind,
        ref bool clamped)
    {
        if (kind == ScriptFieldKind.List)
        {
            builder.Append('[');
            bool first = true;
            if (value is IEnumerable sequence)
            {
                foreach (object? element in sequence)
                {
                    if (!first)
                    {
                        builder.Append(',');
                    }
                    first = false;
                    AppendScalar(builder, element, elementKind, ref clamped);
                }
            }
            builder.Append(']');
            return;
        }
        AppendScalar(builder, value, kind, ref clamped);
    }

    public static void AppendString(StringBuilder builder, string text)
    {
        builder.Append('"');
        foreach (char character in text)
        {
            switch (character)
            {
                case '"':
                    builder.Append("\\\"");
                    break;
                case '\\':
                    builder.Append("\\\\");
                    break;
                case '\n':
                    builder.Append("\\n");
                    break;
                case '\r':
                    builder.Append("\\r");
                    break;
                case '\t':
                    builder.Append("\\t");
                    break;
                default:
                    if (character < 0x20)
                    {
                        builder.Append("\\u").Append(((int)character).ToString("x4", CultureInfo.InvariantCulture));
                    }
                    else
                    {
                        builder.Append(character);
                    }
                    break;
            }
        }
        builder.Append('"');
    }

    /// <summary>
    /// A double as JSON; NaN becomes 0 and an infinity +/-double.MaxValue, with 'clamped' set.
    /// </summary>
    public static string Number(double value, ref bool clamped)
    {
        if (double.IsNaN(value))
        {
            clamped = true;
            return "0";
        }
        if (double.IsInfinity(value))
        {
            clamped = true;
            value = value > 0.0 ? double.MaxValue : -double.MaxValue;
        }
        return value.ToString("R", CultureInfo.InvariantCulture);
    }

    /// <summary>
    /// A float as JSON; NaN becomes 0 and an infinity +/-float.MaxValue, with 'clamped' set.
    /// </summary>
    public static string Number(float value, ref bool clamped)
    {
        if (float.IsNaN(value))
        {
            clamped = true;
            return "0";
        }
        if (float.IsInfinity(value))
        {
            clamped = true;
            // Written through double so the text parses back to exactly float.MaxValue, not one rounding above.
            double limit = value > 0.0f ? float.MaxValue : -float.MaxValue;
            return limit.ToString("R", CultureInfo.InvariantCulture);
        }
        return value.ToString("R", CultureInfo.InvariantCulture);
    }

    private static void AppendScalar(StringBuilder builder, object? value, ScriptFieldKind kind, ref bool clamped)
    {
        switch (kind)
        {
            case ScriptFieldKind.Bool:
                builder.Append(value is true ? "true" : "false");
                return;
            case ScriptFieldKind.Int32:
                builder.Append(value is int i ? i.ToString(CultureInfo.InvariantCulture) : "0");
                return;
            case ScriptFieldKind.Enum:
            {
                // The int32 the blob carries (FieldPayload.Encode): a uint enum's 0xFFFFFFFF is -1 here and in
                // the blob, and Enum.ToObject restores it. 64-bit enums are not serialised (TryClassifyScalar).
                int bits = value == null ? 0 : unchecked((int)Convert.ToInt64(value, CultureInfo.InvariantCulture));
                builder.Append(bits.ToString(CultureInfo.InvariantCulture));
                return;
            }
            case ScriptFieldKind.Int64:
                builder.Append(value is long l ? l.ToString(CultureInfo.InvariantCulture) : "0");
                return;
            case ScriptFieldKind.Float:
                builder.Append(value is float f ? Number(f, ref clamped) : "0");
                return;
            case ScriptFieldKind.Double:
                builder.Append(value is double d ? Number(d, ref clamped) : "0");
                return;
            case ScriptFieldKind.String:
                AppendString(builder, value as string ?? "");
                return;
            case ScriptFieldKind.Vec2:
            {
                Vector2 v = value is Vector2 vector ? vector : Vector2.Zero;
                builder.Append('[').Append(Number(v.X, ref clamped)).Append(',').Append(Number(v.Y, ref clamped));
                builder.Append(']');
                return;
            }
            case ScriptFieldKind.Vec3:
            {
                Vector3 v = value is Vector3 vector ? vector : Vector3.Zero;
                builder.Append('[').Append(Number(v.X, ref clamped)).Append(',').Append(Number(v.Y, ref clamped));
                builder.Append(',').Append(Number(v.Z, ref clamped)).Append(']');
                return;
            }
            case ScriptFieldKind.Vec4:
            {
                Vector4 v = value is Vector4 vector ? vector : Vector4.Zero;
                builder.Append('[').Append(Number(v.X, ref clamped)).Append(',').Append(Number(v.Y, ref clamped));
                builder.Append(',').Append(Number(v.Z, ref clamped)).Append(',').Append(Number(v.W, ref clamped));
                builder.Append(']');
                return;
            }
            case ScriptFieldKind.Quat:
            {
                Quaternion q = value is Quaternion quaternion ? quaternion : Quaternion.Identity;
                builder.Append('[').Append(Number(q.X, ref clamped)).Append(',').Append(Number(q.Y, ref clamped));
                builder.Append(',').Append(Number(q.Z, ref clamped)).Append(',').Append(Number(q.W, ref clamped));
                builder.Append(']');
                return;
            }
            case ScriptFieldKind.Color:
            {
                Color c = value is Color color ? color : Color.White;
                builder.Append('[').Append(Number(c.R, ref clamped)).Append(',').Append(Number(c.G, ref clamped));
                builder.Append(',').Append(Number(c.B, ref clamped)).Append(',').Append(Number(c.A, ref clamped));
                builder.Append(']');
                return;
            }
            case ScriptFieldKind.Entity:
                builder.Append(value is Entity entity ? entity.Id.ToString(CultureInfo.InvariantCulture) : "0");
                return;
            case ScriptFieldKind.Asset:
            {
                UuidData id = value is IAssetRef reference ? reference.Id : default;
                AppendString(builder, id.IsNull ? "" : id.ToString());
                return;
            }
            default:
                builder.Append("null");
                return;
        }
    }
}
