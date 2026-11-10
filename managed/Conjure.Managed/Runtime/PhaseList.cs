using System;

namespace Conjure.Runtime;

/// <summary>
/// Dense array of the live instances of one class that override one phase callback. Removal is swap-
/// remove O(1); each behaviour remembers its index per phase (docs/design/03_scripting.md 2.3).
/// </summary>
internal sealed class PhaseList
{
    private ScriptBehaviour[] _items = new ScriptBehaviour[8];

    public int Count { get; private set; }

    public ScriptBehaviour[] Items => _items;

    public void Add(ScriptBehaviour behaviour, int phase)
    {
        if (Count == _items.Length)
        {
            Array.Resize(ref _items, Count * 2);
        }
        _items[Count] = behaviour;
        behaviour.PhaseIndices[phase] = Count;
        ++Count;
    }

    public void Remove(ScriptBehaviour behaviour, int phase)
    {
        int index = behaviour.PhaseIndices[phase];
        if (index < 0 || index >= Count || !ReferenceEquals(_items[index], behaviour))
        {
            return;
        }
        int last = Count - 1;
        if (index != last)
        {
            ScriptBehaviour moved = _items[last];
            _items[index] = moved;
            moved.PhaseIndices[phase] = index;
        }
        _items[last] = null!;
        behaviour.PhaseIndices[phase] = -1;
        Count = last;
    }

    public void Clear()
    {
        Array.Clear(_items, 0, Count);
        Count = 0;
    }
}
