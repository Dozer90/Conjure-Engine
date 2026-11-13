using System;
using System.Runtime.CompilerServices;
using Conjure.Interop;

namespace Conjure.Dod;

/// <summary>
/// The component ids a ComponentQuery was opened with, one per column (at most 8, like conjure_chunk_view).
/// </summary>
[InlineArray(8)]
internal struct ColumnIds
{
    private uint _element0;
}

/// <summary>
/// One chunk of a ComponentQuery: spans over the native columns, valid only until the query's Dispose. Being a
/// ref struct, a ChunkView and its spans cannot be stored in a field, boxed or captured, so the compiler keeps
/// them on the stack; it does not keep them inside the scope. A span taken before Dispose, or a ChunkView read
/// from a copy of the query, must not be used after Dispose: doing so is undefined. Outside Distribution the
/// native side overwrites the chunk views at the end of the scope, which catches some of these uses, never all
/// (docs/design/03_scripting.md 3.9, D12).
/// </summary>
public readonly unsafe ref struct ChunkView
{
    private readonly ChunkViewData* _data;
    private readonly ColumnIds _ids;

    internal ChunkView(ChunkViewData* data, ColumnIds ids)
    {
        _data = data;
        _ids = ids;
    }

    /// <summary>
    /// Rows in the chunk.
    /// </summary>
    public int Count => _data == null ? 0 : _data->Count;

    /// <summary>
    /// The entity of every row.
    /// </summary>
    public ReadOnlySpan<Entity> Entities => _data == null ? default : new ReadOnlySpan<Entity>(_data->Entities, _data->Count);

    /// <summary>
    /// The column of the component at position 'column' of the Begin call, writable. Throws
    /// InvalidOperationException when T is not the component requested at that position of Begin (a wrong
    /// type or index would read and write another component's memory).
    /// </summary>
    public Span<T> Get<T>(int column) where T : unmanaged
    {
        if (_data == null || column < 0 || column >= 8)
        {
            throw new ArgumentOutOfRangeException(nameof(column));
        }
        void* pointer = (void*)_data->Columns[column];
        if (pointer == null)
        {
            throw new InvalidOperationException("the chunk has no column " + column);
        }
        uint expected = _ids[column];
        if (ComponentType<T>.Id != expected)
        {
            throw new InvalidOperationException(
                typeof(T).FullName + " is not the component at column " + column + " of this query");
        }
        return new Span<T>(pointer, _data->Count);
    }

    /// <summary>
    /// The column at 'column', read-only; throws like Get when T is not the component of that column.
    /// </summary>
    public ReadOnlySpan<T> GetReadOnly<T>(int column) where T : unmanaged => Get<T>(column);
}
