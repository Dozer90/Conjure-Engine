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
