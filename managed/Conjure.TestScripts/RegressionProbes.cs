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

/// <summary>
/// Uses the gameplay enums and TransformData by their bare names in a file without 'using Conjure.Interop':
/// it only compiles while they live in namespace Conjure, and records the namespace of each.
/// </summary>
public sealed class PublicTypesProbe : ScriptBehaviour
{
    public string Namespaces = "";
    public int Sum;

    protected override void OnUpdate(float deltaTime)
    {
        ForceMode force = ForceMode.Impulse;
        CursorMode cursor = CursorMode.Normal;
        ButtonAction action = ButtonAction.Press;
        AssetLoadPriority priority = AssetLoadPriority.High;
        TransformData pose = TransformData.Identity;
        Sum = (int)force + (int)cursor + (int)action + (int)priority + (int)pose.Scale.X;
        Namespaces = string.Join(",", typeof(ForceMode).Namespace, typeof(CursorMode).Namespace,
            typeof(ButtonAction).Namespace, typeof(AssetLoadPriority).Namespace, typeof(TransformData).Namespace);
    }
}

public enum WideMask : ulong
{
    None = 0,
    Top = 1UL << 63,
}

public enum WideLong : long
{
    Small = 1,
    Big = 1L << 40,
}

public enum HighUnsigned : uint
{
    Low = 1,
    High = 0xFFFFFFFF,
}

/// <summary>
/// Enum fields beyond the int32 ENUM payload: the 64-bit ones are not serialised, the uint one is, as the
/// same 32 bits in its default JSON and in its blob.
/// </summary>
public sealed class WideEnumFields : ScriptBehaviour
{
    public WideMask Mask = WideMask.Top;
    public WideLong Long = WideLong.Big;
    public HighUnsigned Unsigned = HighUnsigned.High;
    public int Plain = 3;
}

/// <summary>
/// Defaults and a range bound JSON cannot spell: they reach native as the largest finite values, and the
/// open range bound is left out of the metadata.
/// </summary>
public sealed class NonFiniteDefaults : ScriptBehaviour
{
    public float MaxRange = float.PositiveInfinity;
    public double Floor = double.NegativeInfinity;

    [Range(0.0, double.PositiveInfinity)]
    public float Reach = 1.0f;
}
