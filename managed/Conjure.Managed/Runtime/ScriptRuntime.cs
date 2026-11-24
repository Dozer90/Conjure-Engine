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
    /// instance a hot reload recreated). FlagAlreadyStarted marks it started; any other instance waits for its
    /// OnStart. Returns the number of instances created.
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

            for (int phase = 0; phase < (int)ScriptPhase.Count; ++phase)
            {
                if (info.Overrides(PhaseCallback(phase)))
                {
                    info.Lists[phase].Add(instance, phase);
                }
            }

            // OnCreate runs once per entity lifetime: an instance a hot reload recreated gets OnReload instead.
            bool alreadyCreated = (request->Flags & CreateRequest.FlagAlreadyCreated) != 0;
            instance.Run(alreadyCreated ? ScriptCallback.Reload : ScriptCallback.Create, 0.0f);
            if ((request->Flags & CreateRequest.FlagAlreadyStarted) != 0)
            {
                instance.Started = true;
            }
            else
            {
                _pendingStart.Add(instance);
            }
        }
        return created;
    }

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
            if (callOnDestroy)
            {
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
    /// One call per phase (D5): pending OnStart first (FIXED_UPDATE and UPDATE), then every class's list.
    /// Instances not yet started (created by the flush after this frame's UPDATE) are skipped until their
    /// OnStart ran. Returns the number of instances visited.
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
            if (instance == null)
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
    /// Destroys every instance (OnDestroy), unloads every load context, clears the tables and forgets the
    /// native table: the host that provided it may be freed right after this call, so nothing managed may
    /// dereference it until the next handshake (a later log from a background thread is dropped).
    /// </summary>
    public static int Shutdown()
    {
        foreach (ScriptBehaviour instance in _instances.Snapshot())
        {
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
        // OnStart may create more instances; those start on a later phase.
        ScriptBehaviour[] starting = _pendingStart.ToArray();
        _pendingStart.Clear();
        foreach (ScriptBehaviour instance in starting)
        {
            if (instance.Slot < 0)
            {
                continue;
            }
            instance.Started = true;
            instance.Run(ScriptCallback.Start, 0.0f);
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
