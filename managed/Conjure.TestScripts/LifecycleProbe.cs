using Conjure;

namespace Conjure.TestScripts;

/// <summary>
/// Counts every lifecycle callback in public fields the native tests read back through collect_fields.
/// OnDestroy reports whether the entity was still alive through the log.
/// </summary>
public sealed class LifecycleProbe : ScriptBehaviour
{
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
        Log.Info("LifecycleProbe.OnDestroy alive=" + Entity.IsAlive);
    }
}
