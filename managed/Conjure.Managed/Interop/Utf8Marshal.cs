using System;
using System.Text;

namespace Conjure.Interop;

/// <summary>
/// UTF-8 bytes of a string for a (const char *, int32_t) argument: a slice of the caller's scratch when
/// they fit, a fresh array otherwise. Pin Bytes with 'fixed' for the call.
/// </summary>
public readonly ref struct Utf8Encoded
{
    public Utf8Encoded(Span<byte> bytes)
    {
        Bytes = bytes;
    }

    public Span<byte> Bytes { get; }

    public int Length => Bytes.Length;
}

/// <summary>
/// String marshalling for the bindings entries (docs/design/15_bindings.md 2.1): UTF-8 both ways, no
/// terminator, and the needed-length protocol of the entries that fill a caller buffer (the result is the
/// length required; a result above the capacity means "call again with a bigger buffer").
/// </summary>
public static unsafe class Utf8Marshal
{
    /// <summary>
    /// Scratch size the facades put on the stack before falling back to the heap.
    /// </summary>
    public const int ScratchSize = 512;

    /// <summary>
    /// The UTF-8 bytes of 'text' ("" for null) in 'scratch' when they fit, else in a new array.
    /// </summary>
    public static Utf8Encoded Encode(string? text, Span<byte> scratch)
    {
        if (string.IsNullOrEmpty(text))
        {
            return new Utf8Encoded(Span<byte>.Empty);
        }
        int byteCount = Encoding.UTF8.GetByteCount(text);
        if (byteCount <= scratch.Length)
        {
            int written = Encoding.UTF8.GetBytes(text, scratch);
            return new Utf8Encoded(scratch.Slice(0, written));
        }
        return new Utf8Encoded(Encoding.UTF8.GetBytes(text));
    }

    /// <summary>
    /// A (pointer, length) pair as a string; "" for a null pointer or a non-positive length.
    /// </summary>
    public static string Read(byte* utf8, int length)
    {
        if (utf8 == null || length <= 0)
        {
            return "";
        }
        return Encoding.UTF8.GetString(utf8, length);
    }

    /// <summary>
    /// One call of the needed-length protocol: fills up to 'capacity' bytes and returns the length needed,
    /// or a negative status.
    /// </summary>
    public delegate int NeededLengthReader<TState>(TState state, byte* buffer, int capacity);

    /// <summary>
    /// Runs the needed-length protocol: one call into a stack scratch, a second into an exact heap buffer
    /// when the text is longer. A negative status is returned in 'status' and the result is "".
    /// </summary>
    public static string ReadNeeded<TState>(TState state, NeededLengthReader<TState> reader, out int status)
    {
        byte* scratch = stackalloc byte[ScratchSize];
        int needed = reader(state, scratch, ScratchSize);
        status = needed;
        if (needed <= 0)
        {
            return "";
        }
        if (needed <= ScratchSize)
        {
            return Encoding.UTF8.GetString(scratch, needed);
        }
        byte[] heap = new byte[needed];
        fixed (byte* pinned = heap)
        {
            int again = reader(state, pinned, needed);
            status = again;
            if (again <= 0)
            {
                return "";
            }
            int length = again < needed ? again : needed;
            return Encoding.UTF8.GetString(pinned, length);
        }
    }
}
