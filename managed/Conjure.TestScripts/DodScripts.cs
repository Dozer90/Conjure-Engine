using System;
using System.Runtime.InteropServices;
using Conjure;
using Conjure.Dod;
using Conjure.Interop;
using Conjure.Runtime;

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
/// Spawns entities inside one query scope and changes them before it closes (17_demo_games.md P2, code sweep
/// OWN-P2): two components, add then remove, a second add, a set after an add, a set without an add, a script
/// and a destroy. Records what the reads inside the scope answered; Completed is set only when no call threw.
/// </summary>
public sealed class DeferredSpawner : ScriptBehaviour
{
    public Entity TwoComponents;
    public Entity AddRemove;
    public Entity Overwritten;
    public Entity SetAfterAdd;
    public Entity SetOnly;
    public Entity Scripted;
    public Entity Destroyed;
    public int AliveInside = -1;
    public int HasInside = -1;
    public int TryGetInside = -1;
    public int NameInside = -1;
    public int GetThrewNotAlive;
    public int RemovedAdded;
    public int RemovedMissing;
    public int DestroyAnswer;
    public int Completed;

    protected override void OnUpdate(float deltaTime)
    {
        if (Completed != 0)
        {
            return;
        }
        using ComponentQuery query = ComponentQuery.Begin<TestPosition>();

        TwoComponents = Entity.Create("two");
        TwoComponents.AddComponent(new TestPosition { X = 1.0f, Y = 2.0f, Z = 3.0f });
        TwoComponents.AddComponent(new Score { Value = 7, Bonus = 0.5f });
        AliveInside = TwoComponents.IsAlive ? 1 : 0;
        HasInside = TwoComponents.HasComponent<TestPosition>() ? 1 : 0;
        TryGetInside = TwoComponents.TryGetComponent(out TestPosition _) ? 1 : 0;
        NameInside = TwoComponents.Name.Length;
        try
        {
            TwoComponents.GetComponent<TestPosition>();
        }
        catch (EntityNotAliveException)
        {
            GetThrewNotAlive = 1;
        }

        AddRemove = Entity.Create();
        AddRemove.AddComponent(new TestPosition { X = 4.0f });
        AddRemove.AddComponent(new Score { Value = 1 });
        RemovedAdded = AddRemove.RemoveComponent<Score>() ? 1 : 0;
        RemovedMissing = AddRemove.RemoveComponent<Toggle>() ? 1 : 0;

        Overwritten = Entity.Create();
        Overwritten.AddComponent(new Score { Value = 1 });
        Overwritten.AddComponent(new Score { Value = 2 });

        SetAfterAdd = Entity.Create();
        SetAfterAdd.AddComponent(new Score { Value = 1 });
        SetAfterAdd.SetComponent(new Score { Value = 3, Bonus = 1.5f });

        SetOnly = Entity.Create();
        SetOnly.SetComponent(new Score { Value = 9 });

        Scripted = Entity.Create("scripted");
        Scripted.AddComponent(new TestPosition { X = 5.0f });
        Scripted.AddScript<SpawnedMarker>();

        Destroyed = Entity.Create("destroyed");
        Destroyed.AddComponent(new TestPosition { X = 6.0f });
        Destroyed.AddScript<SpawnedMarker>();
        DestroyAnswer = Destroyed.Destroy() ? 1 : 0;

        Completed = 1;
    }
}

/// <summary>
/// The script DeferredSpawner adds to an entity it reserved: OnCreate records that the instance came to life.
/// </summary>
public sealed class SpawnedMarker : ScriptBehaviour
{
    public int Created;

    protected override void OnCreate()
    {
        Created = 1;
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
        // A copy made by Entity.Duplicate, or a prefab instance, can already carry a Toggle: AddComponent would throw.
        if (HasComponent<Toggle>())
        {
            return;
        }
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
        // A copy made by Entity.Duplicate, or a prefab instance, can already carry a Score: AddComponent would throw.
        if (HasComponent<Score>())
        {
            return;
        }
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

/// <summary>
/// ScoreUser without the HasComponent guard (code sweep F631). A hot reload does not run OnCreate again (code
/// sweep OWN-RELOAD), so the recreated instance keeps working. A copy of the entity (Entity.Duplicate, a prefab
/// instance) runs OnCreate on an entity that already carries the Score: the add throws, and the fault guard
/// disables every callback of the copy's instance.
/// </summary>
public sealed class UnguardedScoreUser : ScriptBehaviour
{
    public int Updates;

    protected override void OnCreate()
    {
        AddComponent(new Score { Value = 10, Bonus = 0.0f });
    }

    protected override void OnUpdate(float deltaTime)
    {
        ++Updates;
    }
}

/// <summary>
/// Opens a query over TestPosition and asks its first column for a Score span: Get throws instead of
/// handing out a span of the wrong type over the position column.
/// </summary>
public sealed class WrongColumnUser : ScriptBehaviour
{
    public string Caught = "";

    protected override void OnUpdate(float deltaTime)
    {
        using ComponentQuery query = ComponentQuery.Begin<TestPosition>();
        if (!query.NextChunk(out ChunkView chunk))
        {
            return;
        }
        try
        {
            Span<Score> wrong = chunk.Get<Score>(0);
            wrong[0].Value = 99;
            Caught = "none";
        }
        catch (InvalidOperationException exception)
        {
            Caught = exception.GetType().Name;
        }
    }
}

/// <summary>
/// Opens a query from OnCreate: created by a flush outside any script phase, Begin is refused (NOT_IN_PHASE)
/// and the exception message names the reason.
/// </summary>
public sealed class QueryInCreate : ScriptBehaviour
{
    public string Message = "";

    protected override void OnCreate()
    {
        try
        {
            using ComponentQuery query = ComponentQuery.Begin<TestPosition>();
            Message = "opened";
        }
        catch (InvalidOperationException exception)
        {
            Message = exception.Message;
        }
    }
}

/// <summary>
/// A mirror the layout rule cannot describe (an auto layout may be reordered): its id is always 0.
/// </summary>
[NativeComponent("test.scripting.position")]
[StructLayout(LayoutKind.Auto)]
public struct AutoLayoutPosition
{
    public float X;
    public float Y;
    public float Z;
}

/// <summary>
/// Reads the id of the undescribable mirror three times per update and counts the zeros.
/// </summary>
public sealed class UndescribableMirrorUser : ScriptBehaviour
{
    public int Zeros;

    protected override void OnUpdate(float deltaTime)
    {
        for (int read = 0; read < 3; ++read)
        {
            if (ComponentType<AutoLayoutPosition>.Id == 0)
            {
                ++Zeros;
            }
        }
    }
}

/// <summary>
/// An [EcsComponent] with an enum field: registered with the enum's underlying integer.
/// </summary>
[EcsComponent]
[StructLayout(LayoutKind.Sequential)]
public struct Squad
{
    public int Size;
    public Mode Team;
}

/// <summary>
/// Adds a Squad in OnCreate and reads its team back every update.
/// </summary>
public sealed class SquadUser : ScriptBehaviour
{
    public int LastTeam = -1;

    protected override void OnCreate()
    {
        // A copy made by Entity.Duplicate, or a prefab instance, can already carry a Squad: AddComponent would throw.
        if (HasComponent<Squad>())
        {
            return;
        }
        AddComponent(new Squad { Size = 4, Team = Mode.Jump });
    }

    protected override void OnUpdate(float deltaTime)
    {
        LastTeam = (int)GetComponent<Squad>().Team;
    }
}

/// <summary>
/// A struct with an AssetRef field. Not an [EcsComponent] (that would warn at every load): DynamicSchemaProbe
/// asks LayoutHash for the schema it would register.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct AssetHolder
{
    public AssetRef<AudioClipAsset> Clip;
}

/// <summary>
/// Records why AssetHolder cannot become a dynamic component.
/// </summary>
public sealed class DynamicSchemaProbe : ScriptBehaviour
{
    public string Refusal = "";
    public int SchemaIsNull;

    protected override void OnUpdate(float deltaTime)
    {
        string? schema = LayoutHash.DynamicSchemaJson(typeof(AssetHolder), LayoutHash.Describe(typeof(AssetHolder)),
            out string refusal);
        SchemaIsNull = schema == null ? 1 : 0;
        Refusal = refusal;
    }
}

/// <summary>
/// An [EcsComponent] with a user field named like mirror padding: it is a real field, stored and aligned.
/// </summary>
[EcsComponent]
[StructLayout(LayoutKind.Sequential)]
public struct Seats
{
    public byte Row;
    public long Pad1;
}
