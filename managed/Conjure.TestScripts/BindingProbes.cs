using System;
using System.Numerics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Conjure;
using Conjure.Components;
using Conjure.Dod;
using Conjure.Interop;

namespace Conjure.TestScripts;

// Probes of the bindings facades (docs/design/15_bindings.md 2.5, 2.6, 10), driven by
// tests/bindings/test_bindings_managed.cpp inside a headless application: every probe records what it saw in
// public fields the test reads back as JSON. Probes never let an exception escape (the guard would log an
// error the tests forbid); Catch records the exception type name instead.

internal static class ProbeSupport
{
    /// <summary>
    /// A packed id no live entity carries.
    /// </summary>
    public static readonly Entity Dead = new Entity((7UL << 32) | 123456UL);

    /// <summary>
    /// Runs 'action' and returns the exception type name, or "none".
    /// </summary>
    public static string Catch(Action action)
    {
        try
        {
            action();
            return "none";
        }
        catch (Exception exception)
        {
            return exception.GetType().Name;
        }
    }
}

/// <summary>
/// Calls one entry of every facade group and records the results; the counters on the native side go up.
/// </summary>
public sealed class BindingFacadeProbe : ScriptBehaviour
{
    public string Errors = "";
    public int Ran;
    public int PositionY;
    public int ChildCount;
    public int TagMatches;
    public int Layer;
    public int WasDisabled;
    public int LoadedScenes;
    public int GravityNegative;
    public int MasterGainPercent;
    public int CursorMode;
    public int ModuleCount;
    public int SetLocalCallsSeen;

    protected override void OnUpdate(float deltaTime)
    {
        if (Ran > 0)
        {
            return;
        }
        Ran = 1;
        try
        {
            // The facades are readonly structs: a property set needs a local copy (CS1612 on the rvalue).
            Transform transform = Transform;
            Entity self = Entity;
            transform.Position = new Vector3(0.0f, 3.0f, 0.0f);
            transform.Rotation = Quaternion.Identity;
            PositionY = (int)MathF.Round(transform.Position.Y);

            Span<Entity> children = stackalloc Entity[2];
            Conjure.Entity.CreateMany(2, children, self);
            ChildCount = transform.ChildCount;

            self.Tag = "player";
            TagMatches = self.HasTag("player") ? 1 : 0;

            self.Layer = 5;
            Layer = self.Layer;

            self.Enabled = false;
            WasDisabled = self.Enabled ? 0 : 1;
            self.Enabled = true;

            LoadedScenes = Scene.LoadedCount;
            GravityNegative = Physics.Gravity.Y < 0.0f ? 1 : 0;
            MasterGainPercent = (int)MathF.Round(Audio.MasterGain * 100.0f);

            Input.CursorMode = Conjure.CursorMode.Hidden;
            CursorMode = (int)Input.CursorMode;

            ModuleCount = Modules.Registry.Count;

            Diagnostics.Check("bindings.facade_probe", true, "every facade answered");
            SetLocalCallsSeen = Diagnostics.CallCount(NativeFunction.TransformSetLocal);
        }
        catch (Exception exception)
        {
            Errors += exception.GetType().Name + ": " + exception.Message + "\n";
        }
    }
}

/// <summary>
/// Provokes one failure per status family and records the exception type each one maps to (3.4).
/// </summary>
public sealed class BindingExceptionProbe : ScriptBehaviour
{
    public string DeadEntity = "";
    public string UnknownLayer = "";
    public string UnknownScene = "";
    public string NullAsset = "";
    public string UnloadedPrefab = "";
    public string MissingComponent = "";
    public string LastErrorMessage = "";
    public int LastErrorStatus;
    public int Ran;

    protected override void OnUpdate(float deltaTime)
    {
        if (Ran > 0)
        {
            return;
        }
        Ran = 1;
        Entity dead = ProbeSupport.Dead;
        DeadEntity = ProbeSupport.Catch(() => { _ = dead.Transform.Local; });
        LastErrorMessage = Diagnostics.LastError(out int status);
        LastErrorStatus = status;
        UnknownLayer = ProbeSupport.Catch(() => Physics.Layer("no_such_layer"));
        UnknownScene = ProbeSupport.Catch(() => Scene.Unload("assets://scenes/no_such_scene.scene"));
        NullAsset = ProbeSupport.Catch(() => new AssetRef<PrefabAsset>(AssetId.Null).Load());
        UnloadedPrefab = ProbeSupport.Catch(() => new Prefab(new AssetId(0x1234UL, 0x5678UL)).Instantiate());
        Entity self = Entity;
        MissingComponent = ProbeSupport.Catch(() => { _ = self.Rigidbody.LinearVelocity; });
    }
}

/// <summary>
/// Every predicate on a dead entity answers false (B11) while the setters throw EntityNotAliveException.
/// </summary>
public sealed class BindingPredicateProbe : ScriptBehaviour
{
    public int IsAlive;
    public int Enabled;
    public int IsSleeping;
    public int IsPlaying;
    public int IsInstanceRoot;
    public int ChildCount;
    public string SetterThrew = "";
    public string EnabledSetterThrew = "";
    public int Ran;

    protected override void OnUpdate(float deltaTime)
    {
        if (Ran > 0)
        {
            return;
        }
        Ran = 1;
        Entity dead = ProbeSupport.Dead;
        IsAlive = dead.IsAlive ? 1 : 0;
        Enabled = dead.Enabled ? 1 : 0;
        IsSleeping = dead.Rigidbody.IsSleeping ? 1 : 0;
        IsPlaying = dead.AudioSource.IsPlaying ? 1 : 0;
        IsInstanceRoot = Prefab.IsInstanceRoot(dead) ? 1 : 0;
        ChildCount = dead.ChildCount;
        SetterThrew = ProbeSupport.Catch(() =>
        {
            Transform transform = dead.Transform;
            transform.Position = Vector3.One;
        });
        EnabledSetterThrew = ProbeSupport.Catch(() => dead.Enabled = true);
    }
}

/// <summary>
/// Finds the test clip, loads it, watches it become resident, reads its duration and releases it on the
/// fifth update.
/// </summary>
public sealed class BindingAssetRefProbe : ScriptBehaviour
{
    public string Errors = "";
    public int Found;
    public int LoadedBeforeLoad;
    public int LoadedAfterFrames;
    public int TypeIdNonZero;
    public int MissingIsInvalid;
    public string IdText = "";
    public int DurationMs;
    public int Released;
    public int Updates;

    private AssetRef<AudioClipAsset> _clip;

    protected override void OnUpdate(float deltaTime)
    {
        ++Updates;
        try
        {
            if (Updates == 1)
            {
                _clip = AssetRef<AudioClipAsset>.Find("assets://audio/tone.wav");
                Found = _clip.IsValid ? 1 : 0;
                IdText = _clip.Id.ToString();
                LoadedBeforeLoad = _clip.IsLoaded ? 1 : 0;
                TypeIdNonZero = AssetRef<AudioClipAsset>.TypeId != 0 ? 1 : 0;
                MissingIsInvalid = AssetRef<AudioClipAsset>.Find("assets://audio/missing.wav").IsValid ? 0 : 1;
                _clip.Load();
                return;
            }
            if (Updates <= 4)
            {
                if (_clip.IsLoaded)
                {
                    LoadedAfterFrames = 1;
                    DurationMs = (int)MathF.Round(Audio.ClipDuration(_clip) * 1000.0f);
                }
                return;
            }
            if (Updates == 5)
            {
                Released = _clip.Release() ? 1 : 0;
            }
        }
        catch (Exception exception)
        {
            Errors += exception.GetType().Name + ": " + exception.Message + "\n";
        }
    }
}

/// <summary>
/// Resolves every [NativeComponent] mirror against the native registry (layout hash included), reads the
/// entity's LocalTransform and adds a RigidBodyData.
/// </summary>
public sealed class BindingMirrorProbe : ScriptBehaviour
{
    public int Total;
    public int Resolved;
    public string Unresolved = "";
    public int LocalTransformRead;
    public int RigidBodyAdded;
    public int Ran;

    protected override void OnUpdate(float deltaTime)
    {
        if (Ran > 0)
        {
            return;
        }
        Ran = 1;
        Check<LocalTransform>();
        Check<WorldTransform>();
        Check<InterpolatedTransform>();
        Check<Parent>();
        Check<Name>();
        Check<Tag>();
        Check<Layer>();
        Check<EntityGuid>();
        Check<Disabled>();
        Check<TransformDirty>();
        Check<PrefabInstance>();
        Check<RigidBodyData>();
        Check<BoxColliderData>();
        Check<SphereColliderData>();
        Check<CapsuleColliderData>();
        Check<ConvexColliderData>();
        Check<MeshColliderData>();
        Check<PhysicsMaterialData>();
        Check<CharacterControllerData>();
        Check<AudioSourceData>();
        Check<AudioListenerData>();
        LocalTransformRead = HasComponent<LocalTransform>() && GetComponent<LocalTransform>().Scale == Vector3.One ? 1 : 0;
        RigidBodyData body = RigidBodyData.Default;
        body.MotionType = MotionType.Static;
        Entity.AddComponent(in body);
        RigidBodyAdded = HasComponent<RigidBodyData>() ? 1 : 0;
    }

    private void Check<T>() where T : unmanaged
    {
        ++Total;
        if (ComponentType<T>.Id != 0)
        {
            ++Resolved;
            return;
        }
        Unresolved += typeof(T).Name + " ";
    }
}

/// <summary>
/// Adds an AudioSourceData whose Clip is an AssetRef: AddComponent binds the slot before the bytes land.
/// </summary>
public sealed class BindingAssetBindProbe : ScriptBehaviour
{
    public string Errors = "";
    public int Added;
    public int VolumeReadPercent;
    public int Ran;

    protected override void OnUpdate(float deltaTime)
    {
        if (Ran > 0)
        {
            return;
        }
        Ran = 1;
        try
        {
            AudioSourceData source = AudioSourceData.Default;
            source.Clip = AssetRef<AudioClipAsset>.Find("assets://audio/tone.wav");
            source.Volume = 0.5f;
            source.Loop = true;
            source.PlayOnStart = false;
            Entity.AddComponent(in source);
            Added = HasComponent<AudioSourceData>() ? 1 : 0;
            VolumeReadPercent = (int)MathF.Round(Entity.AudioSource.Volume * 100.0f);
        }
        catch (Exception exception)
        {
            Errors += exception.GetType().Name + ": " + exception.Message + "\n";
        }
    }
}

/// <summary>
/// Records the managed size of every ABI record of 15_bindings.md 2.4 (and of AssetRef&lt;T&gt;) plus the
/// NativeFunction enum's extent, so the native test compares them with the C++ static_asserts: the
/// BindingRecordSizesMatchDocumented and NativeFunctionEnumMatchesManifest cases of design 10. Offsets lists
/// every field of every record as "Record.Field=offset;", so a swap of two same-size fields shows too.
/// </summary>
public sealed class BindingRecordSizeProbe : ScriptBehaviour
{
    public int RayQuery;
    public int ShapeQuery;
    public int RaycastHit;
    public int AssetInfo;
    public int ModuleRecord;
    public int FrameStatsBlock;
    public int TransformData;
    public int UuidData;
    public int CollisionRecord;
    public int AssetRefPrefab;
    public int NativeFunctionCount;
    public int EntryCount;
    public int LastFunction;
    public string Offsets = "";
    public int Ran;

    protected override void OnUpdate(float deltaTime)
    {
        if (Ran > 0)
        {
            return;
        }
        Ran = 1;
        RecordSizes();
        RecordOffsets();
    }

    private void RecordSizes()
    {
        RayQuery = Unsafe.SizeOf<RayQuery>();
        ShapeQuery = Unsafe.SizeOf<ShapeQuery>();
        RaycastHit = Unsafe.SizeOf<Interop.RaycastHit>();
        AssetInfo = Unsafe.SizeOf<AssetInfo>();
        ModuleRecord = Unsafe.SizeOf<ModuleRecord>();
        FrameStatsBlock = Unsafe.SizeOf<FrameStatsBlock>();
        TransformData = Unsafe.SizeOf<TransformData>();
        UuidData = Unsafe.SizeOf<UuidData>();
        CollisionRecord = Unsafe.SizeOf<CollisionRecord>();
        AssetRefPrefab = Unsafe.SizeOf<AssetRef<PrefabAsset>>();
        NativeFunctionCount = Enum.GetValues(typeof(NativeFunction)).Length;
        EntryCount = NativeApi.EntryCount;
        LastFunction = (int)NativeFunction.EntityGetTagText;
    }

    // Field names are string literals, not nameof: several are pointers or fixed buffers, which this project
    // (no unsafe blocks) cannot name. Marshal offsets equal the managed ones: these records hold no bool or char.
    private void RecordOffsets()
    {
        Add<RayQuery>("RayQuery", "Origin", "Direction", "MaxDistance", "LayerMask", "IgnoreEntity");
        Add<ShapeQuery>(
            "ShapeQuery", "Ray", "Shape", "Pad0", "Pad1", "Pad2", "HalfExtents", "Radius", "Rotation");
        Add<Interop.RaycastHit>("RaycastHit", "Entity", "Point", "Normal", "Distance", "Layer");
        Add<AssetInfo>("AssetInfo", "State", "Pad0", "Pad1", "Pad2", "Version", "TypeId");
        Add<ModuleRecord>("ModuleRecord",
            "VersionMajor", "VersionMinor", "VersionPatch", "State", "Kind", "Pad0", "Pad1", "FeatureFlags");
        Add<FrameStatsBlock>("FrameStatsBlock",
            "FrameMs", "PhaseMs", "SubmitMs", "FixedSteps", "JobsExecuted", "FrameMemoryHighWater");
        Add<TransformData>("TransformData", "Position", "Rotation", "Scale");
        Add<UuidData>("UuidData", "High", "Low");
        Add<CollisionRecord>("CollisionRecord", "Slot", "Other", "Point", "Normal", "Impulse", "Kind");
        Add<CreateRequest>("CreateRequest", "Entity", "ClassHash", "Fields", "FieldsSize", "Flags", "OutSlot");
        Add<ScriptClassDesc>("ScriptClassDesc", "Hash", "FullName", "NameLength", "CallbackMask", "FieldCount");
        Add<ScriptFieldDesc>("ScriptFieldDesc", "Hash", "Name", "NameLength", "Kind", "ElementKind",
            "DefaultJson", "DefaultLength", "MetaJson", "MetaLength");
        Add<ManagedComponentDesc>("ManagedComponentDesc", "NameHash", "FullName", "NameLength", "Size",
            "Alignment", "LayoutHash", "SchemaJson", "SchemaLength");
        Add<QueryScope>("QueryScope", "ScopeId", "ChunkCount", "Chunks");
        Add<ChunkViewData>("ChunkViewData", "Entities", "Columns", "Count");
        Add<GamepadState>("GamepadState", "ButtonsDown", "ButtonsPressed", "ButtonsReleased", "Axes", "Connected");
        Add<InputBlock>("InputBlock", "KeysDown", "KeysPressed", "KeysReleased", "MouseX", "MouseY", "MouseDx",
            "MouseDy", "ScrollX", "ScrollY", "MouseButtonsDown", "MouseButtonsPressed", "MouseButtonsReleased",
            "Gamepads");
        Add<FrameBlock>("FrameBlock", "TimeSeconds", "UnscaledTimeSeconds", "DeltaTime", "FixedDeltaTime",
            "TimeScale", "FrameIndex", "FixedStepIndex", "CurrentPhase", "InPlayMode", "Input");
    }

    private void Add<T>(string record, params string[] fields)
    {
        foreach (string field in fields)
        {
            Offsets += record + "." + field + "=" + Marshal.OffsetOf<T>(field).ToInt32() + ";";
        }
    }
}

/// <summary>
/// Reads the module registry: the count, the state of conjure.ecs, an unknown name and the first record.
/// </summary>
public sealed class BindingModulesProbe : ScriptBehaviour
{
    public string Errors = "";
    public int Count;
    public int EcsLoaded;
    public int UnknownLoaded;
    public string FirstName = "";
    public int EcsIsEngine;
    public int Ran;

    protected override void OnUpdate(float deltaTime)
    {
        if (Ran > 0)
        {
            return;
        }
        Ran = 1;
        try
        {
            Count = Modules.Registry.Count;
            EcsLoaded = Modules.Registry.IsLoaded("conjure.ecs") ? 1 : 0;
            UnknownLoaded = Modules.Registry.IsLoaded("conjure.no_such_module") ? 1 : 0;
            FirstName = Count > 0 ? Modules.Registry.Get(0).Name : "";
            ModuleInfo? ecs = Modules.Registry.Find("conjure.ecs");
            EcsIsEngine = ecs.HasValue && ecs.Value.Kind == ModuleKind.Engine ? 1 : 0;
        }
        catch (Exception exception)
        {
            Errors += exception.GetType().Name + ": " + exception.Message + "\n";
        }
    }
}

/// <summary>
/// Injects a key press, sees it in the next frame's snapshot, releases it, and round-trips the clipboard.
/// </summary>
public sealed class BindingInputProbe : ScriptBehaviour
{
    public string Errors = "";
    public int PressedSameFrame;
    public int PressedNextFrame;
    public int DownNextFrame;
    public int ReleasedLater;
    public string Clipboard = "";
    public int GamepadNameEmpty;
    public int Updates;

    protected override void OnUpdate(float deltaTime)
    {
        ++Updates;
        try
        {
            if (Updates == 1)
            {
                Input.Inject.Key(Key.Space, ButtonAction.Press);
                PressedSameFrame = Input.IsKeyPressed(Key.Space) ? 1 : 0;
                Input.Clipboard = "managed clipboard";
                Clipboard = Input.Clipboard;
                GamepadNameEmpty = Input.GamepadName(0).Length == 0 ? 1 : 0;
                return;
            }
            if (Updates == 2)
            {
                PressedNextFrame = Input.IsKeyPressed(Key.Space) ? 1 : 0;
                DownNextFrame = Input.IsKeyDown(Key.Space) ? 1 : 0;
                Input.Inject.Key(Key.Space, ButtonAction.Release);
                return;
            }
            if (Updates == 3)
            {
                ReleasedLater = Input.IsKeyReleased(Key.Space) ? 1 : 0;
            }
        }
        catch (Exception exception)
        {
            Errors += exception.GetType().Name + ": " + exception.Message + "\n";
        }
    }
}

/// <summary>
/// Casts the same ray down onto the test's static box without a mask and with LayerMask.None.
/// </summary>
public sealed class BindingLayerMaskProbe : ScriptBehaviour
{
    public string Errors = "";
    public int HitDefault = -1;
    public int HitNone = -1;
    public int Ran;

    protected override void OnUpdate(float deltaTime)
    {
        if (Ran > 0)
        {
            return;
        }
        Ran = 1;
        try
        {
            Vector3 origin = new Vector3(0.0f, 5.0f, 0.0f);
            HitDefault = Physics.Raycast(origin, -Vector3.UnitY, 10.0f, out _) ? 1 : 0;
            HitNone = Physics.Raycast(origin, -Vector3.UnitY, 10.0f, out _, LayerMask.None) ? 1 : 0;
        }
        catch (Exception exception)
        {
            Errors += exception.GetType().Name + ": " + exception.Message + "\n";
        }
    }
}

/// <summary>
/// Sets Volume, Pitch and Loop of its entity's audio source once and counts the asset_ref_bind calls they
/// cost.
/// </summary>
public sealed class BindingAudioFieldsProbe : ScriptBehaviour
{
    public string Errors = "";
    public int BindCalls = -1;
    public int Ran;

    protected override void OnUpdate(float deltaTime)
    {
        if (Ran > 0)
        {
            return;
        }
        Ran = 1;
        try
        {
            int before = Diagnostics.CallCount(NativeFunction.AssetRefBind);
            AudioSource source = Entity.AudioSource;
            source.Volume = 0.25f;
            source.Pitch = 1.5f;
            source.Loop = true;
            BindCalls = Diagnostics.CallCount(NativeFunction.AssetRefBind) - before;
        }
        catch (Exception exception)
        {
            Errors += exception.GetType().Name + ": " + exception.Message + "\n";
        }
    }
}

/// <summary>
/// Compares colors with the == and != operators (compiling this is half the test).
/// </summary>
public sealed class BindingColorProbe : ScriptBehaviour
{
    public int EqualWhite = -1;
    public int DifferentBlack = -1;

    protected override void OnUpdate(float deltaTime)
    {
        EqualWhite = Color.White == new Color(1.0f, 1.0f, 1.0f) ? 1 : 0;
        DifferentBlack = Color.White != Color.Black ? 1 : 0;
    }
}

/// <summary>
/// Records the EnginePhase names in declaration order and whether there is one per FrameStats phase.
/// </summary>
public sealed class BindingEnginePhaseProbe : ScriptBehaviour
{
    public string Names = "";
    public int MatchesPhaseCount = -1;

    protected override void OnUpdate(float deltaTime)
    {
        Names = string.Join(",", Enum.GetNames(typeof(EnginePhase))).ToUpperInvariant();
        MatchesPhaseCount = Enum.GetValues(typeof(EnginePhase)).Length == FrameStats.PhaseCount ? 1 : 0;
    }
}

/// <summary>
/// Reads back a bound audio source, swaps only the clip id for a prefab's (keeping the stale slot pointer) and
/// writes it again: the mismatched reference must land unbound.
/// </summary>
public sealed class BindingAssetMismatchProbe : ScriptBehaviour
{
    public string Errors = "";
    public int BoundBefore = -1;
    public int StillBound = -1;
    public int Ran;

    protected override void OnUpdate(float deltaTime)
    {
        if (Ran > 0)
        {
            return;
        }
        Ran = 1;
        try
        {
            AudioSourceData source = AudioSourceData.Default;
            source.Clip = AssetRef<AudioClipAsset>.Find("assets://audio/tone.wav");
            source.PlayOnStart = false;
            Entity.AddComponent(in source);
            AudioSourceData readBack = GetComponent<AudioSourceData>();
            BoundBefore = readBack.Clip.IsBound ? 1 : 0;
            AssetId prefab = AssetRef<PrefabAsset>.Find("assets://prefabs/other.prefab").Id;
            Unsafe.As<AssetRef<AudioClipAsset>, AssetId>(ref readBack.Clip) = prefab;
            SetComponent(in readBack);
            StillBound = GetComponent<AudioSourceData>().Clip.IsBound ? 1 : 0;
        }
        catch (Exception exception)
        {
            Errors += exception.GetType().Name + ": " + exception.Message + "\n";
        }
    }
}

/// <summary>
/// A script with no callbacks: EntityFailureProbe fills an entity's script set with it.
/// </summary>
public sealed class BindingIdleScript : ScriptBehaviour
{
}

/// <summary>
/// Drives every failure of the Entity component and script calls (code sweep F631): a dead entity throws
/// EntityNotAliveException, a present or missing component InvalidOperationException, RemoveComponent and
/// Destroy answer false, and a query scope defers an add instead of failing it (the flush then applies it like
/// a set) while an AddScript that needs a structural change is refused.
/// </summary>
public sealed class EntityFailureProbe : ScriptBehaviour
{
    public string Errors = "";
    public string AddDead = "";
    public string SetDead = "";
    public string GetDead = "";
    public string AddScriptDead = "";
    public string DataGetDead = "";
    public string DataSetDead = "";
    public string DeadMessage = "";
    public int RemoveDead = -1;
    public int DestroyDead = -1;
    public string AddFirst = "";
    public string AddPresent = "";
    public string PresentMessage = "";
    public string SetPresent = "";
    public string SetMissing = "";
    public string GetMissing = "";
    public string MissingMessage = "";
    public string WrapperAddPresent = "";
    public string WrapperSetMissing = "";
    public string DataSetMissing = "";
    public int RemovePresent = -1;
    public int RemoveMissing = -1;
    public int DestroyAlive = -1;
    public string ScriptsUpToMax = "";
    public string ScriptPastMax = "";
    public string ScopeAddPresent = "";
    public string ScopeAddReserved = "";
    public string ScopeAddScriptFresh = "";
    public string ScopeAddScriptHolder = "";
    public int ScopeLayerAfter = -1;
    public int ScopeReservedAlive = -1;
    public int ScopeReservedLayer = -1;
    public string ScopeReservedScripts = "";
    public string ScopeReservedPastMax = "";
    public long ScopeReservedId;
    public string ScopeReservedSetters = "";
    public string ScopeSpawnedTagInside = "unset";
    public int ScopeSpawnedLayerInside;
    public string ScopeSpawnedName = "";
    public string ScopeSpawnedTag = "";
    public int ScopeSpawnedLayer = -1;
    public int ScopeSpawnedEnabled = -1;
    public int Ran;

    protected override void OnUpdate(float deltaTime)
    {
        if (Ran > 0)
        {
            return;
        }
        Ran = 1;
        try
        {
            ProbeDead();
            ProbeAlive();
            ProbeScope();
        }
        catch (Exception exception)
        {
            Errors += exception.GetType().Name + ": " + exception.Message + "\n";
        }
    }

    private void ProbeDead()
    {
        Entity dead = ProbeSupport.Dead;
        AddDead = ProbeSupport.Catch(() => dead.AddComponent(new Layer { Index = 1 }));
        SetDead = ProbeSupport.Catch(() => dead.SetComponent(new Layer { Index = 1 }));
        GetDead = ProbeSupport.Catch(() => { _ = dead.GetComponent<Layer>(); });
        AddScriptDead = ProbeSupport.Catch(() => dead.AddScript<BindingIdleScript>());
        DataGetDead = ProbeSupport.Catch(() => { _ = dead.AudioSource.Data; });
        DataSetDead = ProbeSupport.Catch(() =>
        {
            AudioSource source = dead.AudioSource;
            source.Data = AudioSourceData.Default;
        });
        try
        {
            dead.SetComponent(new Layer { Index = 1 });
        }
        catch (EntityNotAliveException exception)
        {
            DeadMessage = exception.Message;
        }
        RemoveDead = dead.RemoveComponent<Layer>() ? 1 : 0;
        DestroyDead = dead.Destroy() ? 1 : 0;
    }

    private void ProbeAlive()
    {
        Entity target = Conjure.Entity.Create("failure_target");
        AddFirst = ProbeSupport.Catch(() => target.AddComponent(new Layer { Index = 2 }));
        AddPresent = ProbeSupport.Catch(() => target.AddComponent(new Layer { Index = 3 }));
        try
        {
            target.AddComponent(new Layer { Index = 3 });
        }
        catch (InvalidOperationException exception)
        {
            PresentMessage = exception.Message;
        }
        SetPresent = ProbeSupport.Catch(() => target.SetComponent(new Layer { Index = 4 }));
        SetMissing = ProbeSupport.Catch(() => target.SetComponent(new AudioListenerData { Gain = 1.0f }));
        GetMissing = ProbeSupport.Catch(() => { _ = target.GetComponent<AudioListenerData>(); });
        try
        {
            target.SetComponent(new AudioListenerData { Gain = 1.0f });
        }
        catch (InvalidOperationException exception)
        {
            MissingMessage = exception.Message;
        }
        WrapperAddPresent = ProbeSupport.Catch(() => AddComponent(GetComponent<LocalTransform>()));
        WrapperSetMissing = ProbeSupport.Catch(() => SetComponent(new AudioListenerData { Gain = 1.0f }));
        DataSetMissing = ProbeSupport.Catch(() =>
        {
            AudioSource source = target.AudioSource;
            source.Data = AudioSourceData.Default;
        });
        RemovePresent = target.RemoveComponent<Layer>() ? 1 : 0;
        RemoveMissing = target.RemoveComponent<Layer>() ? 1 : 0;

        Entity scripted = Conjure.Entity.Create("failure_scripted");
        ScriptsUpToMax = ProbeSupport.Catch(() =>
        {
            for (int index = 0; index < 8; ++index)
            {
                scripted.AddScript<BindingIdleScript>();
            }
        });
        ScriptPastMax = ProbeSupport.Catch(() => scripted.AddScript<BindingIdleScript>());

        Entity doomed = Conjure.Entity.Create("failure_doomed");
        DestroyAlive = doomed.Destroy() ? 1 : 0;
    }

    private void ProbeScope()
    {
        Entity target = Conjure.Entity.Create("scope_target");
        target.AddComponent(new Layer { Index = 5 });
        Entity fresh = Conjure.Entity.Create("scope_fresh");
        Entity holder = Conjure.Entity.Create("scope_holder");
        holder.AddScript<BindingIdleScript>();
        Entity reserved = Conjure.Entity.Null;
        Entity spawned = Conjure.Entity.Null;
        using (ComponentQuery query = ComponentQuery.Begin<LocalTransform>())
        {
            // Deferred to the scope end: the call cannot see that the entity already has a Layer.
            ScopeAddPresent = ProbeSupport.Catch(() => target.AddComponent(new Layer { Index = 6 }));
            // A reserved entity is not alive until the scope closes; the add is recorded behind its create and the
            // entity is created with the component (code sweep OWN-P2).
            reserved = Conjure.Entity.Create("scope_reserved");
            Entity reservedCopy = reserved;
            ScopeAddReserved = ProbeSupport.Catch(() => reservedCopy.AddComponent(new Layer { Index = 7 }));
            ScopeReservedScripts = ProbeSupport.Catch(() =>
            {
                for (int index = 0; index < 8; ++index)
                {
                    reservedCopy.AddScript<BindingIdleScript>();
                }
            });
            // The status picks the exception, not IsAlive (false for a reserved entity): the entity refused.
            ScopeReservedPastMax = ProbeSupport.Catch(() => reservedCopy.AddScript<BindingIdleScript>());
            // The Entity setters record on a reserved entity too, while its getters answer as for a dead one.
            spawned = Conjure.Entity.Create();
            Entity spawnedCopy = spawned;
            ScopeReservedSetters = ProbeSupport.Catch(() =>
            {
                spawnedCopy.Name = "scope_spawned";
                spawnedCopy.Tag = "scope_tag";
                spawnedCopy.Layer = 9;
                spawnedCopy.Enabled = false;
            });
            ScopeSpawnedTagInside = spawned.Tag;
            ScopeSpawnedLayerInside = spawned.Layer;
            // A first script needs the script component, a structural change the locked world refuses; a further
            // script only joins the set the entity already has.
            ScopeAddScriptFresh = ProbeSupport.Catch(() => fresh.AddScript<BindingIdleScript>());
            ScopeAddScriptHolder = ProbeSupport.Catch(() => holder.AddScript<BindingIdleScript>());
        }
        ScopeLayerAfter = target.GetComponent<Layer>().Index;
        ScopeReservedAlive = reserved.IsAlive ? 1 : 0;
        ScopeReservedLayer = reserved.TryGetComponent(out Layer reservedLayer) ? reservedLayer.Index : -1;
        ScopeReservedId = (long)reserved.Id;
        ScopeSpawnedName = spawned.Name;
        ScopeSpawnedTag = spawned.Tag;
        ScopeSpawnedLayer = spawned.Layer;
        ScopeSpawnedEnabled = spawned.Enabled ? 1 : 0;
    }
}

/// <summary>
/// The Name property's setter and the obsolete SetName forward (code sweep F631).
/// </summary>
public sealed class EntityNameProbe : ScriptBehaviour
{
    public string Errors = "";
    public string AfterSet = "";
    public int FoundByNewName = -1;
    public string AfterForward = "";
    public int ClearedByEmpty = -1;
    public int ClearedByNull = -1;
    public string DeadSetter = "";
    public string DeadGetter = "unset";
    public int Ran;

    protected override void OnUpdate(float deltaTime)
    {
        if (Ran > 0)
        {
            return;
        }
        Ran = 1;
        try
        {
            Entity target = Conjure.Entity.Create("before");
            target.Name = "renamed";
            AfterSet = target.Name;
            FoundByNewName = Conjure.Entity.Find("renamed") == target ? 1 : 0;
#pragma warning disable CS0618 // SetName is the obsolete forward under test.
            target.SetName("forwarded");
#pragma warning restore CS0618
            AfterForward = target.Name;
            target.Name = "";
            ClearedByEmpty = target.Name.Length == 0 ? 1 : 0;
            target.Name = "again";
            target.Name = null;
            ClearedByNull = target.Name.Length == 0 ? 1 : 0;
            Entity dead = ProbeSupport.Dead;
            DeadSetter = ProbeSupport.Catch(() => dead.Name = "ghost");
            DeadGetter = dead.Name;
        }
        catch (Exception exception)
        {
            Errors += exception.GetType().Name + ": " + exception.Message + "\n";
        }
    }
}

/// <summary>
/// The Tag property both ways, TagId and the obsolete SetTag forward (code sweep OWN-TAG).
/// </summary>
public sealed class EntityTagProbe : ScriptBehaviour
{
    /// <summary>
    /// A text no native code interns: written as a StringId it has a hash and no spelling.
    /// </summary>
    private const string UnspelledText = "probe.own_tag.never_interned";

    public string Errors = "";
    public string Untagged = "unset";
    public int UntaggedIdNull = -1;
    public string AfterSet = "";
    public int IdMatchesText = -1;
    public int HasTagAfterSet = -1;
    public int FoundByTag = -1;
    public int LongRoundTrip = -1;
    public string AfterForward = "";
    public int ClearedByEmpty = -1;
    public int ClearedByNull = -1;
    public string Unspelled = "unset";
    public int UnspelledIdKept = -1;
    public string DeadSetter = "";
    public string DeadForward = "";
    public string DeadGetter = "unset";
    public int DeadIdNull = -1;
    public int Ran;

    protected override void OnUpdate(float deltaTime)
    {
        if (Ran > 0)
        {
            return;
        }
        Ran = 1;
        try
        {
            Entity target = Conjure.Entity.Create("tagged");
            Untagged = target.Tag;
            UntaggedIdNull = target.TagId.IsNull ? 1 : 0;

            target.Tag = "Enemy";
            AfterSet = target.Tag;
            IdMatchesText = target.TagId == StringId.Of("Enemy") ? 1 : 0;
            HasTagAfterSet = target.HasTag("Enemy") ? 1 : 0;
            Span<Entity> found = stackalloc Entity[2];
            FoundByTag = Conjure.Entity.FindByTag("Enemy", found) == 1 && found[0] == target ? 1 : 0;

            // Longer than Utf8Marshal.ScratchSize in UTF-8 bytes: the getter's second call into a heap buffer.
            string longTag = new string('\u00e9', Utf8Marshal.ScratchSize) + "!";
            target.Tag = longTag;
            LongRoundTrip = target.Tag == longTag ? 1 : 0;

#pragma warning disable CS0618 // SetTag is the obsolete forward under test.
            target.SetTag("forwarded");
#pragma warning restore CS0618
            AfterForward = target.Tag;

            target.Tag = "";
            ClearedByEmpty = target.Tag.Length == 0 && target.TagId.IsNull ? 1 : 0;
            target.Tag = "again";
            target.Tag = null;
            ClearedByNull = target.Tag.Length == 0 && target.TagId.IsNull ? 1 : 0;

            Tag unspelled = new Tag { Value = StringId.Of(UnspelledText) };
            if (target.HasComponent<Tag>())
            {
                target.SetComponent(unspelled);
            }
            else
            {
                target.AddComponent(unspelled);
            }
            Unspelled = target.Tag;
            UnspelledIdKept = target.TagId == StringId.Of(UnspelledText) ? 1 : 0;

            Entity dead = ProbeSupport.Dead;
            DeadSetter = ProbeSupport.Catch(() => dead.Tag = "ghost");
#pragma warning disable CS0618 // SetTag is the obsolete forward under test.
            DeadForward = ProbeSupport.Catch(() => dead.SetTag("ghost"));
#pragma warning restore CS0618
            DeadGetter = dead.Tag;
            DeadIdNull = dead.TagId.IsNull ? 1 : 0;
        }
        catch (Exception exception)
        {
            Errors += exception.GetType().Name + ": " + exception.Message + "\n";
        }
    }
}

/// <summary>
/// Forward / Right / Up stay in parent space while WorldForward / WorldRight / WorldUp follow the parent chain
/// (code sweep F631). The probe's entity is the parent, turned 90 degrees about +Y and scaled by 2; its child
/// starts at identity. Axes are recorded as "x,y,z" in thousandths.
/// </summary>
public sealed class WorldAxisProbe : ScriptBehaviour
{
    public string Errors = "";
    public string Forward = "";
    public string WorldForward = "";
    public string WorldRight = "";
    public string WorldUp = "";
    public string MirroredForward = "";
    public string CollapsedForward = "";
    public string CollapsedRight = "";
    public string TinyForward = "";
    public string FaintForward = "";
    public string LookAtWorldForward = "";
    public string LookAtForward = "";
    public string DeadAxis = "";
    public int Ran;

    protected override void OnUpdate(float deltaTime)
    {
        if (Ran > 0)
        {
            return;
        }
        Ran = 1;
        try
        {
            Transform parent = Transform;
            parent.Rotation = Quaternion.CreateFromAxisAngle(Vector3.UnitY, MathF.PI * 0.5f);
            parent.Scale = new Vector3(2.0f, 2.0f, 2.0f);
            Span<Entity> children = stackalloc Entity[1];
            Conjure.Entity.CreateMany(1, children, Entity);
            Transform child = children[0].Transform;

            Forward = Axis(child.Forward);
            WorldForward = Axis(child.WorldForward);
            WorldRight = Axis(child.WorldRight);
            WorldUp = Axis(child.WorldUp);

            child.Scale = new Vector3(1.0f, 1.0f, -1.0f);
            MirroredForward = Axis(child.WorldForward);
            child.Scale = new Vector3(1.0f, 1.0f, 0.0f);
            CollapsedForward = Axis(child.WorldForward);
            CollapsedRight = Axis(child.WorldRight);
            child.Scale = new Vector3(1.0f, 1.0f, 1e-7f);
            TinyForward = Axis(child.WorldForward);
            child.Scale = new Vector3(1.0f, 1.0f, 1e-23f);   // its squared length underflows to zero
            FaintForward = Axis(child.WorldForward);
            child.Scale = Vector3.One;

            Vector3 origin = child.World.Position;
            child.LookAt(origin + new Vector3(3.0f, 0.0f, 4.0f));
            LookAtWorldForward = Axis(child.WorldForward);
            LookAtForward = Axis(child.Forward);

            Entity dead = ProbeSupport.Dead;
            DeadAxis = ProbeSupport.Catch(() => { _ = dead.Transform.WorldForward; });
        }
        catch (Exception exception)
        {
            Errors += exception.GetType().Name + ": " + exception.Message + "\n";
        }
    }

    private static string Axis(Vector3 value)
    {
        return Thousandths(value.X) + "," + Thousandths(value.Y) + "," + Thousandths(value.Z);
    }

    private static int Thousandths(float value)
    {
        return (int)MathF.Round(value * 1000.0f);
    }
}

/// <summary>
/// The component facades on entities a ComponentQuery scope reserved (owner ruling 2026-11-24, code sweep OWN-P2):
/// the writes never throw, are kept in call order and made once the scope closed; the reads throw as for a dead
/// entity; a Destroy recorded in the scope drops the kept writes. The second update leaks a scope, which native
/// force-closes: the next managed call makes its kept write.
/// </summary>
public sealed class ReservedFacadeProbe : ScriptBehaviour
{
    public string Errors = "";
    public string InsideWrites = "";
    public string InsidePositionRead = "";
    public string InsideVolumeRead = "";
    public int InsidePlaying = -1;
    public string DoomedWrites = "";
    public long SpawnedId;
    public long DoomedId;
    public long LeakedId;
    public float PositionX;
    public float PositionY;
    public float ScaleX;
    public float Volume;
    public int Updates;

    protected override void OnUpdate(float deltaTime)
    {
        ++Updates;
        try
        {
            if (Updates == 1)
            {
                RunClosedScope();
            }
            else if (Updates == 2)
            {
                RunLeakedScope();
            }
        }
        catch (Exception exception)
        {
            Errors += exception.GetType().Name + ": " + exception.Message + "\n";
        }
    }

    private void RunClosedScope()
    {
        Entity spawned = Conjure.Entity.Null;
        using (ComponentQuery query = ComponentQuery.Begin<LocalTransform>())
        {
            // CreateMany gives the entity a local transform (Entity.Create makes a bare one).
            Span<Entity> created = stackalloc Entity[1];
            Conjure.Entity.CreateMany(1, created);
            spawned = created[0];
            Entity copy = spawned;
            AudioSourceData source = AudioSourceData.Default;
            source.Clip = AssetRef<AudioClipAsset>.Find("assets://audio/tone.wav");
            source.PlayOnStart = false;
            InsideWrites = ProbeSupport.Catch(() =>
            {
                // In call order after the flush: the pose the flush gave the entity, then each write on top of it.
                Transform transform = copy.Transform;
                transform.Position = new Vector3(1.0f, 2.0f, 3.0f);
                transform.Translate(new Vector3(10.0f, 0.0f, 0.0f));
                transform.Scale = new Vector3(2.0f, 2.0f, 2.0f);
                // The component request lands at the flush, before the facade writes that need it.
                copy.AddComponent(in source);
                AudioSource speaker = copy.AudioSource;
                speaker.Volume = 0.5f;
                speaker.Play();
            });
            InsidePositionRead = ProbeSupport.Catch(() => _ = copy.Transform.Position);
            InsideVolumeRead = ProbeSupport.Catch(() => _ = copy.AudioSource.Volume);
            InsidePlaying = copy.AudioSource.IsPlaying ? 1 : 0;

            Entity doomed = Conjure.Entity.Create("facade_doomed");
            DoomedId = (long)doomed.Id;
            DoomedWrites = ProbeSupport.Catch(() =>
            {
                Transform transform = doomed.Transform;
                transform.Position = new Vector3(5.0f, 5.0f, 5.0f);
                doomed.AudioSource.Play();
                doomed.Destroy();
            });
        }
        SpawnedId = (long)spawned.Id;
        Vector3 position = spawned.Transform.Position;
        PositionX = position.X;
        PositionY = position.Y;
        ScaleX = spawned.Transform.Scale.X;
        Volume = spawned.AudioSource.Volume;
    }

    private void RunLeakedScope()
    {
        // Left open on purpose: native force-closes it with a WARN when this call returns, and flushes it.
        ComponentQuery leaked = ComponentQuery.Begin<LocalTransform>();
        Span<Entity> created = stackalloc Entity[1];
        Conjure.Entity.CreateMany(1, created);
        Entity spawned = created[0];
        Transform transform = spawned.Transform;
        transform.Position = new Vector3(7.0f, 0.0f, 0.0f);
        LeakedId = (long)spawned.Id;
        _ = leaked.ChunkCount;
    }
}

/// <summary>
/// Instantiates a prefab inside a ComponentQuery scope and destroys the root in the same scope (owner ruling
/// 2026-11-24, code sweep OWN-P2): Destroy returns true and no part of the instance is ever created.
/// </summary>
public sealed class ReservedPrefabDestroyProbe : ScriptBehaviour
{
    public string Errors = "";
    public string Path = "assets://prefabs/lamp.prefab";
    public int DestroyResult = -1;
    public int AliveInside = -1;
    public int AliveAfter = -1;
    public long RootId;
    public int Ran;

    protected override void OnUpdate(float deltaTime)
    {
        if (Ran > 0)
        {
            return;
        }
        Ran = 1;
        try
        {
            Entity root = Conjure.Entity.Null;
            using (ComponentQuery query = ComponentQuery.Begin<LocalTransform>())
            {
                root = Prefab.Find(Path).Instantiate();
                AliveInside = root.IsAlive ? 1 : 0;
                Transform transform = root.Transform;
                transform.Position = new Vector3(3.0f, 0.0f, 0.0f);
                DestroyResult = root.Destroy() ? 1 : 0;
            }
            RootId = (long)root.Id;
            AliveAfter = root.IsAlive ? 1 : 0;
        }
        catch (Exception exception)
        {
            Errors += exception.GetType().Name + ": " + exception.Message + "\n";
        }
    }
}
