using System;
using System.Numerics;
using System.Runtime.CompilerServices;
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

            self.SetTag("player");
            TagMatches = self.HasTag("player") ? 1 : 0;

            self.Layer = 5;
            Layer = self.Layer;

            self.Enabled = false;
            WasDisabled = self.Enabled ? 0 : 1;
            self.Enabled = true;

            LoadedScenes = Scene.LoadedCount;
            GravityNegative = Physics.Gravity.Y < 0.0f ? 1 : 0;
            MasterGainPercent = (int)MathF.Round(Audio.MasterGain * 100.0f);

            Input.CursorMode = Interop.CursorMode.Hidden;
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
        RigidBodyAdded = Entity.AddComponent(in body) && HasComponent<RigidBodyData>() ? 1 : 0;
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
            Added = Entity.AddComponent(in source) ? 1 : 0;
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
/// BindingRecordSizesMatchDocumented and NativeFunctionEnumMatchesManifest cases of design 10.
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
    public int Ran;

    protected override void OnUpdate(float deltaTime)
    {
        if (Ran > 0)
        {
            return;
        }
        Ran = 1;
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
        LastFunction = (int)NativeFunction.ModulesName;
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
