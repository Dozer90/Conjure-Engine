using Conjure;

namespace Conjure.TestScripts;

/// <summary>
/// Creates an entity with a LifecycleProbe from OnUpdate; the probe's OnCreate runs in the same frame.
/// </summary>
public sealed class Spawner : ScriptBehaviour
{
    public Entity Spawned;
    public int SpawnedAlive;

    protected override void OnUpdate(float deltaTime)
    {
        if (!Spawned.IsNull)
        {
            return;
        }
        Spawned = Entity.Create("spawned");
        Spawned.AddScript<LifecycleProbe>();
        SpawnedAlive = Spawned.IsAlive ? 1 : 0;
    }
}
