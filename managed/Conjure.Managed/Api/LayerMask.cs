using System;

namespace Conjure;

/// <summary>
/// A set of physics layers as a 32-bit mask (bit i = layer i); the filter of the physics queries.
/// </summary>
public readonly struct LayerMask : IEquatable<LayerMask>
{
    public static readonly LayerMask All = new LayerMask(0xFFFFFFFFu);
    public static readonly LayerMask None = default;

    public readonly uint Bits;

    public LayerMask(uint bits)
    {
        Bits = bits;
    }

    /// <summary>
    /// The mask of one layer index (0..31); ArgumentOutOfRangeException otherwise.
    /// </summary>
    public static LayerMask FromIndex(int layer)
    {
        if (layer < 0 || layer > 31)
        {
            throw new ArgumentOutOfRangeException(nameof(layer), "layer indices run 0..31");
        }
        return new LayerMask(1u << layer);
    }

    public bool Includes(int layer) => layer >= 0 && layer <= 31 && (Bits & (1u << layer)) != 0;

    public bool IsEmpty => Bits == 0;

    public bool Equals(LayerMask other) => Bits == other.Bits;

    public override bool Equals(object? obj) => obj is LayerMask other && Equals(other);

    public override int GetHashCode() => Bits.GetHashCode();

    public override string ToString() => "0x" + Bits.ToString("X8");

    public static LayerMask operator |(LayerMask left, LayerMask right) => new LayerMask(left.Bits | right.Bits);

    public static LayerMask operator &(LayerMask left, LayerMask right) => new LayerMask(left.Bits & right.Bits);

    public static LayerMask operator ~(LayerMask mask) => new LayerMask(~mask.Bits);

    public static bool operator ==(LayerMask left, LayerMask right) => left.Bits == right.Bits;

    public static bool operator !=(LayerMask left, LayerMask right) => left.Bits != right.Bits;
}
