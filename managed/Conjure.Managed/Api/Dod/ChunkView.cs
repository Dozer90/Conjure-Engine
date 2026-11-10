using System;
using Conjure.Interop;

namespace Conjure.Dod;

/// <summary>
/// One chunk of a ComponentQuery: spans over the native columns, valid only inside the query scope
/// (a ref struct cannot be stored, boxed or captured, which enforces that at compile time; D12).
/// </summary>
public readonly unsafe ref struct ChunkView
{
    private readonly ChunkViewData* _data;

    internal ChunkView(ChunkViewData* data)
    {
        _data = data;
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
    /// The column of the component at position 'column' of the Begin call, writable.
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
        return new Span<T>(pointer, _data->Count);
    }

    /// <summary>
    /// The column at 'column', read-only.
    /// </summary>
    public ReadOnlySpan<T> GetReadOnly<T>(int column) where T : unmanaged => Get<T>(column);
}
