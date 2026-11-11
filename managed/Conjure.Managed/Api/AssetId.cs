using System;
using System.Runtime.InteropServices;
using Conjure.Interop;

namespace Conjure;

/// <summary>
/// The 128-bit id of an asset (core::uuid, the UUID reflection kind; 16 bytes, two ulongs most significant
/// first). Converts implicitly to and from the ABI's UuidData.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public readonly struct AssetId : IEquatable<AssetId>
{
    public static readonly AssetId Null = default;

    public readonly ulong High;
    public readonly ulong Low;

    public AssetId(ulong high, ulong low)
    {
        High = high;
        Low = low;
    }

    public AssetId(UuidData value)
    {
        High = value.High;
        Low = value.Low;
    }

    public bool IsNull => High == 0 && Low == 0;

    /// <summary>
    /// Parses the canonical 8-4-4-4-12 form (case-insensitive, hyphens optional); false for anything else.
    /// </summary>
    public static bool TryParse(string? text, out AssetId id)
    {
        id = default;
        if (text == null)
        {
            return false;
        }
        ulong high = 0;
        ulong low = 0;
        int digits = 0;
        foreach (char character in text)
        {
            if (character == '-')
            {
                continue;
            }
            int value = HexValue(character);
            if (value < 0 || digits >= 32)
            {
                return false;
            }
            if (digits < 16)
            {
                high = (high << 4) | (uint)value;
            }
            else
            {
                low = (low << 4) | (uint)value;
            }
            ++digits;
        }
        if (digits != 32)
        {
            return false;
        }
        id = new AssetId(high, low);
        return true;
    }

    public bool Equals(AssetId other) => High == other.High && Low == other.Low;

    public override bool Equals(object? obj) => obj is AssetId other && Equals(other);

    public override int GetHashCode() => HashCode.Combine(High, Low);

    public override string ToString() => ((UuidData)this).ToString();

    public static bool operator ==(AssetId left, AssetId right) => left.Equals(right);

    public static bool operator !=(AssetId left, AssetId right) => !left.Equals(right);

    public static implicit operator UuidData(AssetId id) => new UuidData(id.High, id.Low);

    public static implicit operator AssetId(UuidData value) => new AssetId(value.High, value.Low);

    private static int HexValue(char character)
    {
        if (character >= '0' && character <= '9')
        {
            return character - '0';
        }
        if (character >= 'a' && character <= 'f')
        {
            return character - 'a' + 10;
        }
        if (character >= 'A' && character <= 'F')
        {
            return character - 'A' + 10;
        }
        return -1;
    }
}
