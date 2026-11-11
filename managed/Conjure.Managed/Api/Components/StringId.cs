using System;
using System.Runtime.InteropServices;
using Conjure.Runtime;

namespace Conjure.Components;

/// <summary>
/// Mirror of core::string_id (the STRING_ID reflection kind): the FNV-1a 64 hash of the name, 8 bytes. The
/// interned text lives on the native side; managed code compares hashes and builds ids from strings with Of.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public readonly struct StringId : IEquatable<StringId>
{
    public static readonly StringId Null = default;

    public readonly ulong Hash;

    public StringId(ulong hash)
    {
        Hash = hash;
    }

    /// <summary>
    /// The id of a name: the hash native computes for string_id(name) (an empty name is the null id).
    /// </summary>
    public static StringId Of(string? name)
    {
        return string.IsNullOrEmpty(name) ? default : new StringId(NameHash.Fnv1a64(name));
    }

    public bool IsNull => Hash == 0;

    public bool Equals(StringId other) => Hash == other.Hash;

    public override bool Equals(object? obj) => obj is StringId other && Equals(other);

    public override int GetHashCode() => Hash.GetHashCode();

    public override string ToString() => "#" + Hash.ToString("x16");

    public static bool operator ==(StringId left, StringId right) => left.Hash == right.Hash;

    public static bool operator !=(StringId left, StringId right) => left.Hash != right.Hash;
}
