using Conjure;

namespace Conjure.TestScripts;

/// <summary>
/// Destroys its own entity on the first update; OnDestroy reports whether the entity was still alive.
/// </summary>
public sealed class DestroyProbe : ScriptBehaviour
{
    public int Updates;

    protected override void OnUpdate(float deltaTime)
    {
        ++Updates;
        Destroy();
    }

    protected override void OnDestroy()
    {
        Log.Info("DestroyProbe.OnDestroy alive=" + Entity.IsAlive);
    }
}
