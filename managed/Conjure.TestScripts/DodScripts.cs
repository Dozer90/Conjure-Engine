using System.Runtime.InteropServices;
using Conjure;
using Conjure.Dod;

namespace Conjure.TestScripts;

/// <summary>
/// Mirror of the native test component "test.scripting.position" {f32 x, y, z}.
/// </summary>
[NativeComponent("test.scripting.position")]
[StructLayout(LayoutKind.Sequential)]
public struct TestPosition
{
    public float X;
    public float Y;
    public float Z;
}

/// <summary>
/// A mirror whose layout does not match the native type (an int where a float is expected).
/// </summary>
[NativeComponent("test.scripting.position")]
[StructLayout(LayoutKind.Sequential)]
public struct BadPosition
{
    public float X;
    public int Y;
    public float Z;
}

/// <summary>
/// A user-defined ECS component registered at assembly load (R4).
/// </summary>
[EcsComponent]
[StructLayout(LayoutKind.Sequential)]
public struct Score
{
    public int Value;
    public float Bonus;
}

/// <summary>
/// Mirror of the native test component "test.scripting.flags" {bool active; u8 layer; u16 mask; f32 weight}:
/// 8 bytes in the managed layout, which the marshalled layout (bool widened to 4) does not match.
/// </summary>
[NativeComponent("test.scripting.flags")]
[StructLayout(LayoutKind.Sequential)]
public struct TestFlags
{
    public bool Active;
    public byte Layer;
    public ushort Mask;
    public float Weight;
}

/// <summary>
/// A user-defined ECS component with a bool field: registered with the managed size and offsets.
/// </summary>
[EcsComponent]
[StructLayout(LayoutKind.Sequential)]
public struct Toggle
{
    public bool On;
    public byte Level;
    public ushort Mask;
    public float Weight;
}

/// <summary>
/// Adds 1 to every TestPosition.X through a Span over the native columns.
/// </summary>
public sealed class PositionWriter : ScriptBehaviour
{
    public int Chunks;
    public int Rows;

    protected override void OnUpdate(float deltaTime)
    {
        using ComponentQuery query = ComponentQuery.Begin<TestPosition>();
        while (query.NextChunk(out ChunkView chunk))
        {
            ++Chunks;
            System.Span<TestPosition> positions = chunk.Get<TestPosition>(0);
            for (int index = 0; index < chunk.Count; ++index)
            {
                positions[index].X += 1.0f;
                ++Rows;
            }
        }
    }
}

/// <summary>
/// Opens a query over the mismatched mirror: Begin throws, the guard logs.
/// </summary>
public sealed class BadMirrorUser : ScriptBehaviour
{
    public int Attempts;

    protected override void OnUpdate(float deltaTime)
    {
        ++Attempts;
        using ComponentQuery query = ComponentQuery.Begin<BadPosition>();
        query.NextChunk(out _);
    }
}

/// <summary>
/// Forgets to dispose its query: native force-closes the scope at the end of the phase.
/// </summary>
public sealed class ForgetfulQuery : ScriptBehaviour
{
    public int Opened;

    protected override void OnUpdate(float deltaTime)
    {
        ComponentQuery query = ComponentQuery.Begin<TestPosition>();
        query.NextChunk(out _);
        ++Opened;
    }
}

/// <summary>
/// Creates an entity from inside a query scope: the structural request is deferred to the scope end.
/// </summary>
public sealed class DeferredCreator : ScriptBehaviour
{
    public Entity Created;
    public int AliveInsideScope;

    protected override void OnUpdate(float deltaTime)
    {
        if (!Created.IsNull)
        {
            return;
        }
        using ComponentQuery query = ComponentQuery.Begin<TestPosition>();
        Created = Entity.Create("deferred");
        AliveInsideScope = Created.IsAlive ? 1 : 0;
    }
}

/// <summary>
/// Resolves the bool-carrying mirror, flips its flag and bumps its weight through Get/SetComponent.
/// </summary>
public sealed class FlagsMirrorUser : ScriptBehaviour
{
    public int Resolved;
    public int ActiveSeen;
    public int LayerSeen;
    public int MaskSeen;
    public float WeightSeen;

    protected override void OnUpdate(float deltaTime)
    {
        Resolved = ComponentType<TestFlags>.Id != 0 ? 1 : 0;
        if (!HasComponent<TestFlags>())
        {
            return;
        }
        TestFlags flags = GetComponent<TestFlags>();
        ActiveSeen = flags.Active ? 1 : 0;
        LayerSeen = flags.Layer;
        MaskSeen = flags.Mask;
        WeightSeen = flags.Weight;
        flags.Active = !flags.Active;
        flags.Weight += 1.0f;
        SetComponent(in flags);
    }
}

/// <summary>
/// Adds, reads and updates the bool-carrying [EcsComponent] Toggle on its entity.
/// </summary>
public sealed class ToggleUser : ScriptBehaviour
{
    public int LastOn;
    public int LastLevel;
    public int LastMask;
    public float LastWeight;

    protected override void OnCreate()
    {
        AddComponent(new Toggle { On = true, Level = 2, Mask = 0x0101, Weight = 1.5f });
    }

    protected override void OnUpdate(float deltaTime)
    {
        Toggle toggle = GetComponent<Toggle>();
        LastOn = toggle.On ? 1 : 0;
        LastLevel = toggle.Level;
        LastMask = toggle.Mask;
        LastWeight = toggle.Weight;
        toggle.On = !toggle.On;
        ++toggle.Level;
        SetComponent(in toggle);
    }
}

/// <summary>
/// Adds, reads and updates the [EcsComponent] Score on its entity.
/// </summary>
public sealed class ScoreUser : ScriptBehaviour
{
    public int HasScore;
    public int LastValue;
    public float LastBonus;

    protected override void OnCreate()
    {
        AddComponent(new Score { Value = 1, Bonus = 0.5f });
    }

    protected override void OnUpdate(float deltaTime)
    {
        HasScore = HasComponent<Score>() ? 1 : 0;
        Score score = GetComponent<Score>();
        ++score.Value;
        score.Bonus += 0.5f;
        SetComponent(in score);
        LastValue = score.Value;
        LastBonus = score.Bonus;
    }
}
