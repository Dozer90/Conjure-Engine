using System;
using System.Collections.Generic;
using Conjure.Interop;

namespace Conjure.Dod;

/// <summary>
/// A structural-locked scope over every chunk holding the requested components (docs/design/03_scripting.md
/// 3.9). Use with 'using': the world stays locked until Dispose; structural requests made inside are
/// deferred to the end of the scope, and a scope still open at the end of the phase is force-closed with a
/// warning. Entity.Create inside the scope returns a reserved entity that is not alive until the scope closes
/// (IsAlive, HasComponent and GetComponent answer as for a dead entity); its AddComponent, SetComponent,
/// RemoveComponent, AddScript and Destroy calls and its Name, Tag, Layer and Enabled setters are recorded behind
/// the create and applied when the scope closes (the component requests in call order), so the entity is created
/// with its components and scripts then. The writes of the component facades (Transform, Rigidbody,
/// CharacterController, AudioSource) on it are kept in call order and made once the scope closes, right after the
/// flush that created it, so after every component request of the scope; their reads throw as for a dead entity.
/// The query is a mutable ref struct: a copy keeps the scope's chunk pointers after the original's Dispose, and
/// the spans of its chunks point into native memory. Neither a copy nor a span may be used after Dispose
/// (undefined; outside Distribution the native side overwrites the chunk views at the end of the scope, which
/// catches some of these uses, never all).
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
        ReservedWrites.ScopeOpened(scope.ScopeId);
        return query;
    }

    // The statuses are negated script_error codes (engine/include/conjure/scripting/scripting_error.h).
    private static string BeginFailure(int status) => (-status) switch
    {
        13 => "ComponentQuery.Begin must run on the main thread (WRONG_THREAD)",
        14 => "ComponentQuery.Begin is only available inside OnFixedUpdate, OnUpdate, OnLateUpdate, the collision " +
            "callbacks and OnCreate/OnDestroy run by the flush at the end of those phases; OnCreate/OnDestroy at " +
            "scene start, frame end or a mode change, and OnReload after a hot reload, run outside a script phase " +
            "(NOT_IN_PHASE)",
        16 => "ComponentQuery.Begin needs a running script runtime (NOT_INITIALISED)",
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
        // After query_end: closing the last scope flushed the deferred requests, so the reserved entities exist.
        ReservedWrites.ScopeClosed(_scopeId);
    }
}

/// <summary>
/// The facade writes made on entities a ComponentQuery scope reserved (owner ruling 2026-11-24, code sweep
/// OWN-P2: an entity accepts work as soon as it has an id). Entity.Create, Entity.CreateMany,
/// Prefab.Instantiate and Prefab.InstantiateMany note the ids they return while a scope is open and not alive
/// yet; a facade write on such an id is kept here instead of failing, and made in call order once the last open
/// scope closed (its query_end flushed the requests that create the entity). A write whose entity is not alive
/// then (a Destroy recorded in the scope won) is dropped; one that throws is logged at ERROR and the rest still
/// run. A scope left open is force-closed by native when the managed call returns, so the next top-level
/// managed call (a phase, the collision dispatch, a create or destroy batch) makes the writes still kept. Main
/// thread only, like every native entry.
/// </summary>
internal static class ReservedWrites
{
    private static readonly HashSet<uint> _openScopes = new HashSet<uint>();
    private static readonly HashSet<ulong> _reserved = new HashSet<ulong>();
    private static readonly List<IPendingWrite> _pending = new List<IPendingWrite>();
    private static int _managedCallDepth;

    private interface IPendingWrite
    {
        Entity Target { get; }

        void Run();
    }

    private sealed class PendingWrite<TState> : IPendingWrite
    {
        private readonly TState _state;
        private readonly Action<TState> _write;

        public PendingWrite(Entity target, TState state, Action<TState> write)
        {
            Target = target;
            _state = state;
            _write = write;
        }

        public Entity Target { get; }

        public void Run() => _write(_state);
    }

    /// <summary>
    /// Entities noted and not yet created (diagnostics and tests).
    /// </summary>
    internal static int ReservedCount => _reserved.Count;

    /// <summary>
    /// Writes kept for the scope end (diagnostics and tests).
    /// </summary>
    internal static int PendingCount => _pending.Count;

    internal static void ScopeOpened(uint scopeId)
    {
        _openScopes.Add(scopeId);
    }

    /// <summary>
    /// A scope closed; the last one makes the kept writes. A second Dispose (of a copy) finds the id gone.
    /// </summary>
    internal static void ScopeClosed(uint scopeId)
    {
        if (_openScopes.Remove(scopeId) && _openScopes.Count == 0)
        {
            Drain();
        }
    }

    /// <summary>
    /// Notes 'entity' as reserved when a scope is open and the entity is not alive (a deferred create returned
    /// it). Outside a scope it costs one count check and no native call.
    /// </summary>
    internal static void NoteCreated(Entity entity)
    {
        if (_openScopes.Count == 0 || entity.Id == 0 || entity.IsAlive)
        {
            return;
        }
        _reserved.Add(entity.Id);
    }

    /// <summary>
    /// NoteCreated for every entity of 'entities'.
    /// </summary>
    internal static void NoteCreated(ReadOnlySpan<Entity> entities)
    {
        if (_openScopes.Count == 0)
        {
            return;
        }
        foreach (Entity entity in entities)
        {
            NoteCreated(entity);
        }
    }

    /// <summary>
    /// True (and the write kept for the scope end) when 'target' is a reserved entity; false otherwise, and the
    /// caller makes the write now. Pass a static lambda and its state so the common path allocates nothing.
    /// </summary>
    internal static bool TryDefer<TState>(Entity target, TState state, Action<TState> write)
    {
        if (!IsReserved(target))
        {
            return false;
        }
        Keep(target, state, write);
        return true;
    }

    /// <summary>
    /// True for an entity noted as reserved whose scope has not closed yet.
    /// </summary>
    internal static bool IsReserved(Entity entity) => _reserved.Count != 0 && _reserved.Contains(entity.Id);

    /// <summary>
    /// Keeps a write on 'target' for the scope end whatever 'target' is (a write a reserved argument needs).
    /// </summary>
    internal static void Keep<TState>(Entity target, TState state, Action<TState> write)
    {
        _pending.Add(new PendingWrite<TState>(target, state, write));
    }

    /// <summary>
    /// Opens a top-level managed call: native closed every scope after the previous one returned (a scope left
    /// open is force-closed and flushed), so what is still kept is made now. Calls nested in a managed callback
    /// leave the open scopes alone.
    /// </summary>
    internal static void EnterManagedCall()
    {
        if (_managedCallDepth++ > 0 || (_openScopes.Count == 0 && _pending.Count == 0 && _reserved.Count == 0))
        {
            return;
        }
        _openScopes.Clear();
        Drain();
    }

    internal static void LeaveManagedCall()
    {
        if (_managedCallDepth > 0)
        {
            --_managedCallDepth;
        }
    }

    /// <summary>
    /// Forgets everything kept (runtime shutdown).
    /// </summary>
    internal static void Reset()
    {
        _openScopes.Clear();
        _reserved.Clear();
        _pending.Clear();
        _managedCallDepth = 0;
    }

    private static void Drain()
    {
        _reserved.Clear();
        if (_pending.Count == 0)
        {
            return;
        }
        IPendingWrite[] writes = _pending.ToArray();
        _pending.Clear();
        foreach (IPendingWrite write in writes)
        {
            if (!write.Target.IsAlive)
            {
                continue;
            }
            try
            {
                write.Run();
            }
            catch (Exception exception)
            {
                NativeBridge.Log(
                    LogLevel.Error,
                    "a facade write recorded on entity " + write.Target + " in a ComponentQuery scope failed when "
                        + "the scope closed: " + exception);
            }
        }
    }
}
