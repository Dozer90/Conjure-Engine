using System;
using Conjure.Interop;

namespace Conjure.Dod;

/// <summary>
/// A structural-locked scope over every chunk holding the requested components (docs/design/03_scripting.md
/// 3.9). Use with 'using': the world stays locked until Dispose; structural requests made inside are
/// deferred to the end of the scope, and a scope still open at the end of the phase is force-closed
/// with a warning. The query is a mutable ref struct: a copy keeps the scope's chunk pointers after the
/// original's Dispose, and the spans of its chunks point into native memory. Neither a copy nor a span may be
/// used after Dispose (undefined; outside Distribution the native side overwrites the chunk views at the end of
/// the scope, which catches some of these uses, never all).
/// </summary>
public unsafe ref struct ComponentQuery
{
    private uint _scopeId;
    private int _chunkCount;
    private ChunkViewData* _chunks;
    private int _next;
    private bool _open;
    private ColumnIds _ids;

    public static ComponentQuery Begin<T1>() where T1 : unmanaged
    {
        uint* ids = stackalloc uint[1];
        ids[0] = ComponentType<T1>.RequireId();
        return Begin(ids, 1);
    }

    public static ComponentQuery Begin<T1, T2>() where T1 : unmanaged where T2 : unmanaged
    {
        uint* ids = stackalloc uint[2];
        ids[0] = ComponentType<T1>.RequireId();
        ids[1] = ComponentType<T2>.RequireId();
        return Begin(ids, 2);
    }

    public static ComponentQuery Begin<T1, T2, T3>() where T1 : unmanaged where T2 : unmanaged where T3 : unmanaged
    {
        uint* ids = stackalloc uint[3];
        ids[0] = ComponentType<T1>.RequireId();
        ids[1] = ComponentType<T2>.RequireId();
        ids[2] = ComponentType<T3>.RequireId();
        return Begin(ids, 3);
    }

    public static ComponentQuery Begin<T1, T2, T3, T4>()
        where T1 : unmanaged where T2 : unmanaged where T3 : unmanaged where T4 : unmanaged
    {
        uint* ids = stackalloc uint[4];
        ids[0] = ComponentType<T1>.RequireId();
        ids[1] = ComponentType<T2>.RequireId();
        ids[2] = ComponentType<T3>.RequireId();
        ids[3] = ComponentType<T4>.RequireId();
        return Begin(ids, 4);
    }

    /// <summary>
    /// Opens a scope over explicit component ids (1..8). A refused scope throws InvalidOperationException
    /// naming the reason, for example NOT_IN_PHASE outside the script phases and their flushes.
    /// </summary>
    public static ComponentQuery Begin(uint* ids, int count)
    {
        NativeApi* api = NativeBridge.RequireApi();
        NativeBridge.Require(api->QueryBegin, NativeFunction.QueryBegin);
        QueryScope scope = default;
        int status = api->QueryBegin(ids, count, &scope);
        if (status < 0)
        {
            throw new InvalidOperationException(BeginFailure(status));
        }
        ComponentQuery query = default;
        // Kept so ChunkView.Get can check that its T is the component of the column it asks for.
        for (int index = 0; index < count && index < 8; ++index)
        {
            query._ids[index] = ids[index];
        }
        query._scopeId = scope.ScopeId;
        query._chunkCount = scope.ChunkCount;
        query._chunks = scope.Chunks;
        query._next = 0;
        query._open = true;
        return query;
    }

    // The statuses are negated script_error codes (engine/include/conjure/scripting/scripting_error.h).
    private static string BeginFailure(int status) => (-status) switch
    {
        13 => "ComponentQuery.Begin must run on the main thread (WRONG_THREAD)",
        14 => "ComponentQuery.Begin is only available inside OnFixedUpdate, OnUpdate, OnLateUpdate, the collision " +
            "callbacks and OnCreate/OnDestroy run by the flush at the end of those phases; OnCreate/OnDestroy at " +
            "scene start, frame end or a mode change run outside a script phase (NOT_IN_PHASE)",
        16 => "ComponentQuery.Begin needs a running script runtime (NOT_INITIALIZED)",
        17 => "ComponentQuery.Begin was refused: it needs 1..8 registered component ids and an attached world " +
            "(NOT_SUPPORTED, see the log)",
        _ => "ComponentQuery.Begin failed with scripting status " + status,
    };

    public int ChunkCount => _chunkCount;

    public bool IsOpen => _open;

    /// <summary>
    /// The next chunk; false when every chunk was visited.
    /// </summary>
    public bool NextChunk(out ChunkView chunk)
    {
        if (!_open || _next >= _chunkCount)
        {
            chunk = default;
            return false;
        }
        chunk = new ChunkView(_chunks + _next, _ids);
        ++_next;
        return true;
    }

    /// <summary>
    /// Total rows over every chunk.
    /// </summary>
    public int EntityCount
    {
        get
        {
            int total = 0;
            for (int index = 0; index < _chunkCount; ++index)
            {
                total += _chunks[index].Count;
            }
            return total;
        }
    }

    public void Dispose()
    {
        if (!_open)
        {
            return;
        }
        _open = false;
        NativeApi* api = NativeBridge.Api;
        if (api != null && api->QueryEnd != null)
        {
            api->QueryEnd(_scopeId);
        }
        _chunks = null;
        _chunkCount = 0;
    }
}
