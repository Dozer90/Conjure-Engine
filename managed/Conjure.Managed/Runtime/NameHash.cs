using System;
using System.Text;

namespace Conjure.Runtime;

/// <summary>
/// FNV-1a 64 over UTF-8 bytes, identical to core::hash::fnv1a64 (the class, field and component name
/// hashes that cross the ABI). Test vectors: "" = 0xCBF29CE484222325, "a" = 0xAF63DC4C8601EC8C.
/// </summary>
public static class NameHash
{
    public const ulong OffsetBasis = 0xCBF29CE484222325UL;
    public const ulong Prime = 0x00000100000001B3UL;

    public static ulong Fnv1a64(string text)
    {
        ulong hash = OffsetBasis;
        if (string.IsNullOrEmpty(text))
        {
            return hash;
        }
        int byteCount = Encoding.UTF8.GetByteCount(text);
        if (byteCount <= 256)
        {
            Span<byte> buffer = stackalloc byte[256];
            int written = Encoding.UTF8.GetBytes(text, buffer);
            return Fnv1a64(buffer.Slice(0, written), hash);
        }
        return Fnv1a64(Encoding.UTF8.GetBytes(text), hash);
    }

    public static ulong Fnv1a64(ReadOnlySpan<byte> bytes, ulong seed = OffsetBasis)
    {
        ulong hash = seed;
        foreach (byte value in bytes)
        {
            hash = (hash ^ value) * Prime;
        }
        return hash;
    }

    public static ulong Step(ulong hash, byte value) => (hash ^ value) * Prime;
}
