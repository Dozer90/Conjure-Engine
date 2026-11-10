using Conjure;

namespace Conjure.TestScripts;

/// <summary>
/// Destroys its own entity from OnCreate: the instance is created, its entity dies in the same flush and
/// OnDestroy must still run once (no orphan keeps updating a dead entity).
/// </summary>
public sealed class DestroyInCreate : ScriptBehaviour
{
    public int Updates;

    protected override void OnCreate()
    {
        Destroy();
    }

    protected override void OnUpdate(float deltaTime)
    {
        ++Updates;
    }

    protected override void OnDestroy()
    {
        Log.Info("DestroyInCreate.OnDestroy alive=" + Entity.IsAlive);
    }
}

/// <summary>
/// Spawns scripted entities from OnDestroy, the "drop loot on death" pattern: the script store grows while
/// native is still inside destroy_instances.
/// </summary>
public sealed class LootDropper : ScriptBehaviour
{
    public int LootCount = 4;

    protected override void OnDestroy()
    {
        for (int index = 0; index < LootCount; ++index)
        {
            Entity loot = Entity.Create("loot");
            loot.AddScript<LifecycleProbe>();
        }
    }
}

/// <summary>
/// Base of ShadowChild with serialised fields the child redeclares under the same names.
/// </summary>
public class ShadowBase : ScriptBehaviour
{
    [SerializeField]
    private int _hidden = 1;

    public int Shared = 1;

    public int BaseView;

    protected override void OnUpdate(float deltaTime)
    {
        BaseView = _hidden + Shared;
    }
}

/// <summary>
/// Redeclares both fields of its base: only the most derived declarations are serialised (the hidden base
/// fields would alias them by name hash).
/// </summary>
public sealed class ShadowChild : ShadowBase
{
    [SerializeField]
    private int _hidden = 2;

    public new int Shared = 2;

    public int ChildView;

    protected override void OnUpdate(float deltaTime)
    {
        base.OnUpdate(deltaTime);
        ChildView = _hidden + Shared;
    }
}
