using System;
using System.Collections;
using System.Globalization;
using System.Numerics;
using System.Text;
using Conjure.Interop;

namespace Conjure.Runtime;

/// <summary>
/// A minimal JSON writer for field defaults and inspector metadata. System.Text.Json is avoided on
/// purpose: its metadata caches root game-assembly types and defeat load-context unloading (D7).
/// </summary>
internal static class JsonText
{
    public static string Value(object? value, ScriptFieldKind kind, ScriptFieldKind elementKind)
    {
        StringBuilder builder = new StringBuilder();
        Append(builder, value, kind, elementKind);
        return builder.ToString();
    }

    public static void Append(StringBuilder builder, object? value, ScriptFieldKind kind, ScriptFieldKind elementKind)
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
                    AppendScalar(builder, element, elementKind);
                }
            }
            builder.Append(']');
            return;
        }
        AppendScalar(builder, value, kind);
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

    public static string Number(double value)
    {
        if (double.IsNaN(value) || double.IsInfinity(value))
        {
            return "0";
        }
        return value.ToString("R", CultureInfo.InvariantCulture);
    }

    public static string Number(float value)
    {
        if (float.IsNaN(value) || float.IsInfinity(value))
        {
            return "0";
        }
        return value.ToString("R", CultureInfo.InvariantCulture);
    }

    private static void AppendScalar(StringBuilder builder, object? value, ScriptFieldKind kind)
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
                builder.Append(value == null ? "0" : Convert.ToInt64(value, CultureInfo.InvariantCulture).ToString(CultureInfo.InvariantCulture));
                return;
            case ScriptFieldKind.Int64:
                builder.Append(value is long l ? l.ToString(CultureInfo.InvariantCulture) : "0");
                return;
            case ScriptFieldKind.Float:
                builder.Append(value is float f ? Number(f) : "0");
                return;
            case ScriptFieldKind.Double:
                builder.Append(value is double d ? Number(d) : "0");
                return;
            case ScriptFieldKind.String:
                AppendString(builder, value as string ?? "");
                return;
            case ScriptFieldKind.Vec2:
            {
                Vector2 v = value is Vector2 vector ? vector : Vector2.Zero;
                builder.Append('[').Append(Number(v.X)).Append(',').Append(Number(v.Y)).Append(']');
                return;
            }
            case ScriptFieldKind.Vec3:
            {
                Vector3 v = value is Vector3 vector ? vector : Vector3.Zero;
                builder.Append('[').Append(Number(v.X)).Append(',').Append(Number(v.Y)).Append(',').Append(Number(v.Z)).Append(']');
                return;
            }
            case ScriptFieldKind.Vec4:
            {
                Vector4 v = value is Vector4 vector ? vector : Vector4.Zero;
                builder.Append('[').Append(Number(v.X)).Append(',').Append(Number(v.Y)).Append(',').Append(Number(v.Z)).Append(',').Append(Number(v.W)).Append(']');
                return;
            }
            case ScriptFieldKind.Quat:
            {
                Quaternion q = value is Quaternion quaternion ? quaternion : Quaternion.Identity;
                builder.Append('[').Append(Number(q.X)).Append(',').Append(Number(q.Y)).Append(',').Append(Number(q.Z)).Append(',').Append(Number(q.W)).Append(']');
                return;
            }
            case ScriptFieldKind.Color:
            {
                Color c = value is Color color ? color : Color.White;
                builder.Append('[').Append(Number(c.R)).Append(',').Append(Number(c.G)).Append(',').Append(Number(c.B)).Append(',').Append(Number(c.A)).Append(']');
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
