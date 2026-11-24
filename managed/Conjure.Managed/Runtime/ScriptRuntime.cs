using System;
using System.Collections.Generic;
using System.Numerics;
using Conjure.Dod;
using Conjure.Interop;

namespace Conjure.Runtime;

/// <summary>
/// The managed half of script execution (docs/design/03_scripting.md 2.3, 3.2-3.6): the instance
/// table, the per-class phase lists, batched create/destroy/collision calls and the field codecs.
/// Everything runs on the main thread inside the native exclusive systems.
/// </summary>
public static unsafe class ScriptRuntime
{
    private static readonly InstanceTable _instances = new InstanceTable();
    private static readonly List<ScriptBehaviour> _pendingStart = new List<ScriptBehaviour>();

    /// <summary>
    /// Live instances.
    /// </summary>
    public static int LiveCount => _instances.LiveCount;

    public static ScriptCatalog? Catalog => AssemblyLoader.CurrentCatalog;

    public static ScriptBehaviour? InstanceAt(int slot) => _instances[slot];

    /// <summary>
    /// Loads a game assembly into a fresh collectible context; the previous one stays until native
    /// destroyed its instances and calls UnloadGameAssembly. 0, or -ASSEMBLY_LOAD_FAILED.
    /// </summary>
    public static int LoadGameAssembly(string path)
    {
        try
        {
            AssemblyLoader.Load(path);
            return ScriptStatus.Ok;
        }
        catch (Exception exception)
        {
            NativeBridge.Log(LogLevel.Error, "game assembly '" + path + "' did not load: " + exception.Message);
            return ScriptStatus.LoadFailed;
        }
    }

    /// <summary>
    /// 0 when the previous context was collected, 1 when it leaked (D8). Native destroyed every instance of
    /// that context first, so the destroyed ones still waiting for OnStart are pruned here: they would keep
    /// its types reachable and turn the unload into a zombie.
    /// </summary>
    public static int UnloadGameAssembly()
    {
        _pendingStart.RemoveAll(instance => instance.Slot < 0);
        return AssemblyLoader.UnloadPrevious();
    }

    /// <summary>
    /// Creates one instance per request, writes its slot (-1 when the class is unknown or its constructor threw),
    /// applies the initial fields and then runs OnCreate, or OnReload for a request with FlagAlreadyCreated (an
    /// instance a hot reload recreated). An instance without FlagInactive joins its phases and, after OnCreate (not
    /// after OnReload), runs OnEnable; one with FlagInactive joins no phase until SetActive activates it.
    /// FlagAlreadyStarted marks it started; any other active instance waits for its OnStart, and an inactive one
    /// waits for its first activation. Returns the number of instances created.
    /// </summary>
    public static int CreateInstances(CreateRequest* requests, int count)
    {
        // The managed calls that run callbacks: what a closed (or force-closed) scope left for its reserved
        // entities is made first.
        ReservedWrites.EnterManagedCall();
        try
        {
            return CreateInstancesCore(requests, count);
        }
        finally
        {
            ReservedWrites.LeaveManagedCall();
        }
    }

    private static int CreateInstancesCore(CreateRequest* requests, int count)
    {
        ScriptCatalog? catalog = Catalog;
        int created = 0;
        for (int index = 0; index < count; ++index)
        {
            CreateRequest* request = requests + index;
            request->OutSlot = -1;
            ScriptClassInfo? info = catalog?.Find(request->ClassHash);
            if (info == null)
            {
                continue;
            }

            ScriptBehaviour instance;
            try
            {
                instance = info.CreateInstance();
            }
            catch (Exception exception)
            {
                NativeBridge.Log(LogLevel.Error, info.FullName + " constructor threw: " + exception);
                continue;
            }
            instance.EntityValue = new Entity(request->Entity);
            _instances.Add(instance);
            request->OutSlot = instance.Slot;
            ++created;

            if (request->FieldsSize > 0 && request->Fields != null)
            {
                try
                {
                    info.ApplyFields(instance, new ReadOnlySpan<byte>(request->Fields, request->FieldsSize), out int unknown);
                    if (unknown > 0)
                    {
                        NativeBridge.Log(LogLevel.Debug, info.FullName + ": " + unknown + " field(s) of the initial blob were unknown");
                    }
                }
                catch (Exception exception)
                {
                    NativeBridge.Log(LogLevel.Warn, info.FullName + ": initial fields rejected (" + exception.Message + "); defaults kept");
                }
            }

            // An instance of an inactive entity sleeps: no phase list, no OnEnable, no OnStart until it activates.
            bool active = (request->Flags & CreateRequest.FlagInactive) == 0;
            instance.Active = active;
            if (active)
            {
                AddToLists(instance);
            }

            // OnCreate runs once per entity lifetime: an instance a hot reload recreated gets OnReload instead, and
            // no OnEnable either (its state carries over).
            bool alreadyCreated = (request->Flags & CreateRequest.FlagAlreadyCreated) != 0;
            instance.Run(alreadyCreated ? ScriptCallback.Reload : ScriptCallback.Create, 0.0f);
            if (active && !alreadyCreated)
            {
                instance.Run(ScriptCallback.Enable, 0.0f);
            }
            if ((request->Flags & CreateRequest.FlagAlreadyStarted) != 0)
            {
                instance.Started = true;
            }
            else if (active)
            {
                _pendingStart.Add(instance);
            }
        }
        return created;
    }

    /// <summary>
    /// Destroys the instances of 'slots' (a free slot is skipped). With 'callOnDestroy' an active instance runs
    /// OnDisable and then OnDestroy, an inactive one OnDestroy only; without it (hot reload, world swap) neither
    /// runs. Returns the number of instances destroyed.
    /// </summary>
    public static int DestroyInstances(int* slots, int count, bool callOnDestroy)
    {
        // The managed calls that run callbacks: what a closed (or force-closed) scope left for its reserved
        // entities is made first.
        ReservedWrites.EnterManagedCall();
        try
        {
            return DestroyInstancesCore(slots, count, callOnDestroy);
        }
        finally
        {
            ReservedWrites.LeaveManagedCall();
        }
    }

    private static int DestroyInstancesCore(int* slots, int count, bool callOnDestroy)
    {
        int destroyed = 0;
        for (int index = 0; index < count; ++index)
        {
            ScriptBehaviour? instance = _instances[slots[index]];
            if (instance == null)
            {
                continue;
            }
            // Cleared before the callbacks, as SetActive does: IsActive reads false inside OnDisable and OnDestroy.
            bool wasActive = instance.Active;
            instance.Active = false;
            if (callOnDestroy)
            {
                if (wasActive)
                {
                    instance.Run(ScriptCallback.Disable, 0.0f);
                }
                instance.Run(ScriptCallback.Destroy, 0.0f);
            }
            RemoveFromLists(instance);
            // Not removed from _pendingStart (an O(n) scan per instance): DrainPendingStart skips instances
            // whose slot was freed, which Remove below marks with Slot = -1, and UnloadGameAssembly prunes them
            // in one pass before a reload retires their load context.
            _instances.Remove(slots[index]);
            instance.EntityValue = default;
            ++destroyed;
        }
        return destroyed;
    }

    /// <summary>
    /// One call per phase (D5): pending OnStart first (FIXED_UPDATE and UPDATE), then every class's list, which
    /// holds the active instances only. Instances not yet started (created by the flush after this frame's
    /// UPDATE) are skipped until their OnStart ran. Returns the number of instances visited.
    /// </summary>
    public static int RunPhase(ScriptPhase phase, float deltaTime)
    {
        // The managed calls that run callbacks: what a closed (or force-closed) scope left for its reserved
        // entities is made first.
        ReservedWrites.EnterManagedCall();
        try
        {
            return RunPhaseCore(phase, deltaTime);
        }
        finally
        {
            ReservedWrites.LeaveManagedCall();
        }
    }

    private static int RunPhaseCore(ScriptPhase phase, float deltaTime)
    {
        if (phase >= ScriptPhase.Count)
        {
            return ScriptStatus.NotSupported;
        }
        if (phase == ScriptPhase.FixedUpdate || phase == ScriptPhase.Update)
        {
            DrainPendingStart();
        }
        ScriptCatalog? catalog = Catalog;
        if (catalog == null)
        {
            return 0;
        }
        ScriptCallback callback = PhaseCallback((int)phase);
        int visited = 0;
        IReadOnlyList<ScriptClassInfo> classes = catalog.Classes;
        for (int classIndex = 0; classIndex < classes.Count; ++classIndex)
        {
            PhaseList list = classes[classIndex].Lists[(int)phase];
            for (int index = 0; index < list.Count; ++index)
            {
                ScriptBehaviour instance = list.Items[index];
                if (!instance.Started)
                {
                    // Created after this frame's FIXED_UPDATE/UPDATE drain: OnStart runs first (03_scripting.md 3.2).
                    continue;
                }
                instance.Run(callback, deltaTime);
                ++visited;
                // A swap-remove of the current instance moved another one into this index: visit it too.
                if (index < list.Count && !ReferenceEquals(list.Items[index], instance))
                {
                    --index;
                }
            }
        }
        return visited;
    }

    /// <summary>
    /// Runs one collision callback per record (OnCollisionEnter, OnCollisionStay, OnCollisionExit, OnTriggerEnter
    /// or OnTriggerExit by its kind). A free slot, an inactive instance and an unknown kind are skipped. Returns
    /// the number of callbacks run.
    /// </summary>
    public static int DispatchCollisions(CollisionRecord* records, int count)
    {
        // The managed calls that run callbacks: what a closed (or force-closed) scope left for its reserved
        // entities is made first.
        ReservedWrites.EnterManagedCall();
        try
        {
            return DispatchCollisionsCore(records, count);
        }
        finally
        {
            ReservedWrites.LeaveManagedCall();
        }
    }

    private static int DispatchCollisionsCore(CollisionRecord* records, int count)
    {
        int dispatched = 0;
        for (int index = 0; index < count; ++index)
        {
            CollisionRecord* record = records + index;
            ScriptBehaviour? instance = _instances[record->Slot];
            // Native builds no record for an inactive instance; this is the second guard.
            if (instance == null || !instance.Active)
            {
                continue;
            }
            Collision collision = new Collision(new Entity(record->Other), record->Point, record->Normal, record->Impulse);
            ScriptCallback callback = record->Kind switch
            {
                (byte)CollisionKind.CollisionEnter => ScriptCallback.CollisionEnter,
                (byte)CollisionKind.CollisionExit => ScriptCallback.CollisionExit,
                (byte)CollisionKind.TriggerEnter => ScriptCallback.TriggerEnter,
                (byte)CollisionKind.TriggerExit => ScriptCallback.TriggerExit,
                (byte)CollisionKind.CollisionStay => ScriptCallback.CollisionStay,
                _ => ScriptCallback.Count,
            };
            if (callback == ScriptCallback.Count)
            {
                continue;
            }
            instance.RunCollision(callback, in collision);
            ++dispatched;
        }
        return dispatched;
    }

    public static int ApplyFields(int slot, ReadOnlySpan<byte> blob)
    {
        ScriptBehaviour? instance = _instances[slot];
        if (instance == null || instance.ClassInfo == null)
        {
            return ScriptStatus.InstanceNotFound;
        }
        instance.ClassInfo.ApplyFields(instance, blob, out int unknown);
        return unknown;
    }

    /// <summary>
    /// Writes the instance's fields; returns the bytes needed (retry with a bigger buffer when larger).
    /// </summary>
    public static int CollectFields(int slot, Span<byte> buffer)
    {
        ScriptBehaviour? instance = _instances[slot];
        if (instance == null || instance.ClassInfo == null)
        {
            return ScriptStatus.InstanceNotFound;
        }
        return instance.ClassInfo.CollectFields(instance, buffer);
    }

    /// <summary>
    /// A full blocking collection; returns the number of zombie load contexts.
    /// </summary>
    public static int CollectGarbage()
    {
        GC.Collect();
        GC.WaitForPendingFinalizers();
        GC.Collect();
        return AssemblyLoader.ZombieCount;
    }

    /// <summary>
    /// The managed heap counters native samples once per frame (conjure_managed_memory): the bytes the calling
    /// thread (the main thread) allocated since the runtime started, the heap size without forcing a collection,
    /// and the gen 0 and gen 2 collections so far. Allocates nothing; threads that scripts start are not counted.
    /// </summary>
    public static ManagedMemory SampleMemory()
    {
        ManagedMemory memory = default;
        memory.ThreadAllocatedBytes = (ulong)GC.GetAllocatedBytesForCurrentThread();
        memory.HeapBytes = (ulong)GC.GetTotalMemory(false);
        memory.Gen0Collections = (uint)GC.CollectionCount(0);
        memory.Gen2Collections = (uint)GC.CollectionCount(2);
        return memory;
    }

    /// <summary>
    /// Native reports that the entities of the instances in 'slots' became active or inactive (conjure_managed_api
    /// set_active). An instance that becomes inactive leaves its phase lists and runs OnDisable; one that becomes
    /// active joins them, runs OnEnable and, when its OnStart never ran, waits for it before the next FIXED_UPDATE
    /// or UPDATE. A free slot and an instance already in that state are skipped. Returns how many instances changed
    /// state.
    /// </summary>
    public static int SetActive(int* slots, int count, bool active)
    {
        // The managed calls that run callbacks: what a closed (or force-closed) scope left for its reserved
        // entities is made first.
        ReservedWrites.EnterManagedCall();
        try
        {
            return SetActiveCore(slots, count, active);
        }
        finally
        {
            ReservedWrites.LeaveManagedCall();
        }
    }

    private static int SetActiveCore(int* slots, int count, bool active)
    {
        int changed = 0;
        for (int index = 0; index < count; ++index)
        {
            ScriptBehaviour? instance = _instances[slots[index]];
            if (instance == null || instance.Active == active)
            {
                continue;
            }
            instance.Active = active;
            ++changed;
            if (!active)
            {
                RemoveFromLists(instance);
                instance.Run(ScriptCallback.Disable, 0.0f);
                continue;
            }
            AddToLists(instance);
            instance.Run(ScriptCallback.Enable, 0.0f);
            if (!instance.Started)
            {
                // DrainPendingStart skips an instance listed twice (disabled and enabled again before its OnStart).
                _pendingStart.Add(instance);
            }
        }
        return changed;
    }

    /// <summary>
    /// Destroys every instance (OnDisable first for an active one, then OnDestroy), unloads every load context,
    /// clears the tables and forgets the native table: the host that provided it may be freed right after this
    /// call, so nothing managed may dereference it until the next handshake (a later log from a background thread
    /// is dropped).
    /// </summary>
    public static int Shutdown()
    {
        foreach (ScriptBehaviour instance in _instances.Snapshot())
        {
            bool wasActive = instance.Active;
            instance.Active = false;
            if (wasActive)
            {
                instance.Run(ScriptCallback.Disable, 0.0f);
            }
            instance.Run(ScriptCallback.Destroy, 0.0f);
            RemoveFromLists(instance);
            instance.EntityValue = default;
        }
        _instances.Clear();
        _pendingStart.Clear();
        ReservedWrites.Reset();
        int leaked = AssemblyLoader.UnloadAll();
        NativeBridge.Reset();
        return leaked;
    }

    private static void DrainPendingStart()
    {
        if (_pendingStart.Count == 0)
        {
            return;
        }
        // OnStart may create more instances; those start on a later phase. A destroyed instance, one already started
        // (listed twice) and one disabled before its OnStart are dropped: SetActive lists the last again when it
        // activates.
        ScriptBehaviour[] starting = _pendingStart.ToArray();
        _pendingStart.Clear();
        foreach (ScriptBehaviour instance in starting)
        {
            if (instance.Slot < 0 || instance.Started || !instance.Active)
            {
                continue;
            }
            instance.Started = true;
            instance.Run(ScriptCallback.Start, 0.0f);
        }
    }

    private static void AddToLists(ScriptBehaviour instance)
    {
        ScriptClassInfo? info = instance.ClassInfo;
        if (info == null)
        {
            return;
        }
        for (int phase = 0; phase < (int)ScriptPhase.Count; ++phase)
        {
            if (info.Overrides(PhaseCallback(phase)))
            {
                info.Lists[phase].Add(instance, phase);
            }
        }
    }

    private static void RemoveFromLists(ScriptBehaviour instance)
    {
        ScriptClassInfo? info = instance.ClassInfo;
        if (info == null)
        {
            return;
        }
        for (int phase = 0; phase < (int)ScriptPhase.Count; ++phase)
        {
            info.Lists[phase].Remove(instance, phase);
        }
    }

    private static ScriptCallback PhaseCallback(int phase)
    {
        return phase switch
        {
            0 => ScriptCallback.FixedUpdate,
            1 => ScriptCallback.Update,
            _ => ScriptCallback.LateUpdate,
        };
    }
}
