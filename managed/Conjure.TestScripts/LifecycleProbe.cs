using Conjure;

namespace Conjure.TestScripts;

/// <summary>
/// Counts every lifecycle callback in public fields the native tests read back through collect_fields.
/// OnDestroy counts in static fields (one count per assembly load, shared by every probe) because the
/// destroyed instance can no longer be read: LifecycleWitness copies them into instance fields.
/// </summary>
public sealed class LifecycleProbe : ScriptBehaviour
{
    /// <summary>OnDestroy calls of every LifecycleProbe since the assembly was loaded.</summary>
    public static int Destroyed;

    /// <summary>The OnDestroy calls that saw Entity.IsAlive (the Entity.Destroy() path, 03_scripting.md 3.4).</summary>
    public static int DestroyedAlive;

    public int Created;
    public int Started;
    public int Updates;
    public int FixedUpdates;
    public int LateUpdates;
    public int StartedBeforeUpdate;
    public int UpdateBeforeLate;

    protected override void OnCreate()
    {
        ++Created;
    }

    protected override void OnStart()
    {
        ++Started;
    }

    protected override void OnFixedUpdate(float fixedDeltaTime)
    {
        ++FixedUpdates;
    }

    protected override void OnUpdate(float deltaTime)
    {
        if (Started > 0 && Updates == 0)
        {
            StartedBeforeUpdate = 1;
        }
        ++Updates;
    }

    protected override void OnLateUpdate(float deltaTime)
    {
        if (Updates > LateUpdates)
        {
            UpdateBeforeLate = 1;
        }
        ++LateUpdates;
    }

    protected override void OnDestroy()
    {
        ++Destroyed;
        bool alive = Entity.IsAlive;
        if (alive)
        {
            ++DestroyedAlive;
        }
        Log.Info("LifecycleProbe.OnDestroy alive=" + alive);
    }
}

/// <summary>
/// A long-lived observer of LifecycleProbe's static destroy counters: every update copies them into
/// instance fields, so a native test reads how many probes were destroyed (and how many saw a live
/// entity) after the destroyed probe itself is gone.
/// </summary>
public sealed class LifecycleWitness : ScriptBehaviour
{
    public int ProbeDestroyed;
    public int ProbeDestroyedAlive;

    protected override void OnUpdate(float deltaTime)
    {
        Record();
    }

    protected override void OnLateUpdate(float deltaTime)
    {
        Record();
    }

    private void Record()
    {
        ProbeDestroyed = LifecycleProbe.Destroyed;
        ProbeDestroyedAlive = LifecycleProbe.DestroyedAlive;
    }
}

/// <summary>
/// Logs the lifecycle in order as "Create,Enable,Start,Update,...": the native activity tests read Log through
/// collect_fields (the callback order with OnEnable and OnDisable, code sweep W3-CS-L4). Update is the only phase it
/// logs; FixedUpdates and LateUpdates count the two other phases. SawDisabledTag becomes 1 on an update that finds
/// TestPosition (the tests' disabling tag) on its entity. ActiveIn* hold what IsActive read in the last call of
/// that callback (1 or 0; -1 until it runs). OnDestroy leaves the final log and its IsActive in static fields,
/// which ActivityWitness copies into instance fields.
/// </summary>
public sealed class ActivityProbe : ScriptBehaviour
{
    /// <summary>
    /// The log of the last ActivityProbe destroyed since the assembly was loaded ("Destroy," included).
    /// </summary>
    public static string LastDestroyedLog = "";

    /// <summary>
    /// What IsActive read inside the OnDestroy of that probe (-1 until one was destroyed).
    /// </summary>
    public static int LastActiveInDestroy = -1;

    public string Log = "";
    public int ActiveDuringUpdate = 1;
    public int SawDisabledTag;
    public int FixedUpdates;
    public int LateUpdates;
    public int ActiveInCreate = -1;
    public int ActiveInReload = -1;
    public int ActiveInEnable = -1;
    public int ActiveInDisable = -1;

    protected override void OnCreate()
    {
        Log += "Create,";
        ActiveInCreate = IsActive ? 1 : 0;
    }

    protected override void OnReload()
    {
        Log += "Reload,";
        ActiveInReload = IsActive ? 1 : 0;
    }

    protected override void OnEnable()
    {
        Log += "Enable,";
        ActiveInEnable = IsActive ? 1 : 0;
    }

    protected override void OnStart()
    {
        Log += "Start,";
    }

    protected override void OnUpdate(float deltaTime)
    {
        Log += "Update,";
        if (!IsActive)
        {
            ActiveDuringUpdate = 0;
        }
        if (HasComponent<TestPosition>())
        {
            SawDisabledTag = 1;
        }
    }

    protected override void OnFixedUpdate(float fixedDeltaTime)
    {
        ++FixedUpdates;
    }

    protected override void OnLateUpdate(float deltaTime)
    {
        ++LateUpdates;
    }

    protected override void OnDisable()
    {
        Log += "Disable,";
        ActiveInDisable = IsActive ? 1 : 0;
    }

    protected override void OnDestroy()
    {
        Log += "Destroy,";
        LastDestroyedLog = Log;
        LastActiveInDestroy = IsActive ? 1 : 0;
    }
}

/// <summary>
/// Copies ActivityProbe.LastDestroyedLog and LastActiveInDestroy into fields on every update, so a native test reads
/// what a probe that is gone recorded.
/// </summary>
public sealed class ActivityWitness : ScriptBehaviour
{
    public string LastDestroyedLog = "";
    public int LastActiveInDestroy = -1;

    protected override void OnUpdate(float deltaTime)
    {
        LastDestroyedLog = ActivityProbe.LastDestroyedLog;
        LastActiveInDestroy = ActivityProbe.LastActiveInDestroy;
    }
}

/// <summary>
/// With DisableOnUpdate set, its first update adds a TestPosition to its entity, the component the native activity
/// tests name as the disabling tag, then clears the field: script A of the same-phase rule. Its class name sorts
/// before ActivityProbe, so on a shared entity it updates first.
/// </summary>
public sealed class ActivityDisabler : ScriptBehaviour
{
    public int DisableOnUpdate;

    protected override void OnUpdate(float deltaTime)
    {
        if (DisableOnUpdate == 0)
        {
            return;
        }
        DisableOnUpdate = 0;
        if (!HasComponent<TestPosition>())
        {
            AddComponent(new TestPosition());
        }
    }
}

/// <summary>
/// OnEnable always throws: the fault guard disables OnEnable alone after three consecutive faults. Updates and
/// the disabled-callback mask it saw on its last update are kept in fields.
/// </summary>
public sealed class EnableThrower : ScriptBehaviour
{
    public int EnableCalls;
    public int DisableCalls;
    public int Updates;
    public int DisabledSeen;

    protected override void OnEnable()
    {
        ++EnableCalls;
        throw new System.InvalidOperationException("EnableThrower faults in OnEnable " + EnableCalls);
    }

    protected override void OnDisable()
    {
        ++DisableCalls;
    }

    protected override void OnUpdate(float deltaTime)
    {
        ++Updates;
        DisabledSeen = DisabledCallbacks;
    }
}
