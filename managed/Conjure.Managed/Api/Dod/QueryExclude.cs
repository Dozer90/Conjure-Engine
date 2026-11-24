using System;
using System.Runtime.InteropServices;
using Conjure.Components;

namespace Conjure.Dod;

/// <summary>
/// The components a ComponentQuery skips: an entity holding any of them is not visited (the query_begin
/// exclusion list, docs/design/03_scripting.md 3.9). Holds up to MaxCount component ids inline, so building one
/// allocates nothing; each id is resolved when it is added, which throws InvalidOperationException for a type
/// that is not a registered component (as ComponentType&lt;T&gt;.RequireId does). Excluding a component the query
/// also requires is refused by ComponentQuery.Begin with ArgumentException.
/// </summary>
public readonly unsafe struct QueryExclude
{
    /// <summary>
    /// The most component ids one exclusion list holds (QUERY_MAX_COMPONENTS).
    /// </summary>
    public const int MaxCount = 8;

    private readonly ColumnIds _ids;
    private readonly int _count;

    private QueryExclude(ColumnIds ids, int count)
    {
        _ids = ids;
        _count = count;
    }

    /// <summary>
    /// The number of component ids in the list (a repeated component counts each time it was added).
    /// </summary>
    public int Count => _count;

    /// <summary>
    /// The component id at 'index' (0..Count-1); ArgumentOutOfRangeException otherwise.
    /// </summary>
    public uint this[int index]
    {
        get
        {
            if (index < 0 || index >= _count)
            {
                throw new ArgumentOutOfRangeException(nameof(index), index, "QueryExclude holds " + _count + " ids");
            }
            return _ids[index];
        }
    }

    /// <summary>
    /// Skips the entities holding scene.disabled or scene.inactive: a disabled entity and every descendant of
    /// one, the same "not active" meaning the script callbacks use.
    /// </summary>
    public static QueryExclude Inactive => Of<Disabled>().And<InactiveTag>();

    /// <summary>
    /// A list that skips the entities holding T.
    /// </summary>
    public static QueryExclude Of<T>() where T : unmanaged => default(QueryExclude).And<T>();

    /// <summary>
    /// This list with T added; ArgumentException when it already holds MaxCount ids.
    /// </summary>
    public QueryExclude And<T>() where T : unmanaged
    {
        if (_count >= MaxCount)
        {
            throw new ArgumentException(
                "a ComponentQuery excludes at most " + MaxCount + " components; "
                + typeof(T).Name + " is one too many");
        }
        ColumnIds ids = _ids;
        ids[_count] = ComponentType<T>.RequireId();
        return new QueryExclude(ids, _count + 1);
    }

    /// <summary>
    /// Copies the ids to 'destination', which holds at least MaxCount ids; returns Count.
    /// </summary>
    internal int CopyTo(uint* destination)
    {
        for (int index = 0; index < _count; ++index)
        {
            destination[index] = _ids[index];
        }
        return _count;
    }

    /// <summary>
    /// The scene.inactive tag (13_scene.md: set on every descendant of a disabled entity), resolved by name for
    /// Inactive.
    /// </summary>
    [NativeComponent("conjure.scene.inactive")]
    [StructLayout(LayoutKind.Sequential)]
    private struct InactiveTag : IComponent
    {
    }
}
