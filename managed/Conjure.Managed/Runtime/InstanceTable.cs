using System;
using System.Collections.Generic;

namespace Conjure.Runtime;

/// <summary>
/// Slot-indexed storage of live instances with LIFO slot reuse. The slot is the only thing native keeps
/// (D9).
/// </summary>
internal sealed class InstanceTable
{
    private ScriptBehaviour?[] _instances = new ScriptBehaviour?[64];
    private readonly Stack<int> _freeList = new Stack<int>();
    private int _highWater;

    public int LiveCount { get; private set; }

    public int Capacity => _instances.Length;

    public ScriptBehaviour? this[int slot]
    {
        get
        {
            if (slot < 0 || slot >= _instances.Length)
            {
                return null;
            }
            return _instances[slot];
        }
    }

    public int Add(ScriptBehaviour instance)
    {
        int slot;
        if (_freeList.Count > 0)
        {
            slot = _freeList.Pop();
        }
        else
        {
            slot = _highWater++;
            if (slot >= _instances.Length)
            {
                Array.Resize(ref _instances, _instances.Length * 2);
            }
        }
        _instances[slot] = instance;
        instance.Slot = slot;
        ++LiveCount;
        return slot;
    }

    public ScriptBehaviour? Remove(int slot)
    {
        if (slot < 0 || slot >= _instances.Length)
        {
            return null;
        }
        ScriptBehaviour? instance = _instances[slot];
        if (instance == null)
        {
            return null;
        }
        _instances[slot] = null;
        _freeList.Push(slot);
        --LiveCount;
        instance.Slot = -1;
        return instance;
    }

    /// <summary>
    /// Every live instance (allocates a snapshot; used by bulk destruction only).
    /// </summary>
    public List<ScriptBehaviour> Snapshot()
    {
        List<ScriptBehaviour> result = new List<ScriptBehaviour>(LiveCount);
        for (int slot = 0; slot < _highWater; ++slot)
        {
            ScriptBehaviour? instance = _instances[slot];
            if (instance != null)
            {
                result.Add(instance);
            }
        }
        return result;
    }

    public void Clear()
    {
        Array.Clear(_instances, 0, _instances.Length);
        _freeList.Clear();
        _highWater = 0;
        LiveCount = 0;
    }
}
