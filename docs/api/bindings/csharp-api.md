# The C# API

📘 **Public API wiki** · 🛠 Handbook: [Bindings module](../../handbook/modules/bindings.md)

What a script sees, by topic. The types live in `Conjure.Managed` (namespace `Conjure`); the native entries
behind them are declared by `conjure/bindings/native_api_entries.h` and counted by
`conjure/bindings/call_counters.h`. Your game project references `Conjure.Managed.dll` (the template
`managed/Conjure.Templates/Game/Game.csproj` does; `ConjureManagedDir` points at the engine's `bin/managed`).

## At a glance

| You want to... | Use |
|---|---|
| Create, find, destroy an entity | `Entity.Create(name)`, `Entity.Find(name)`, `Entity.FindByGuid`, `FindByTag`, `FindByLayer`, `entity.Destroy()` |
| Move something | `Transform.Position / Rotation / Scale / Local / World`, `Translate`, `Rotate`, `LookAt` |
| Parent and children | `Transform.SetParent(parent, keepWorld, index)`, `Transform.Parent`, `GetChildren(span)`, `Entity.Root` |
| Read or write a native component | `GetComponent<T>()`, `SetComponent(in value)`, `AddComponent`, `RemoveComponent`, `HasComponent`, `TryGetComponent` |
| Apply forces, move a kinematic body | `new Rigidbody(entity).AddForce(force, mode)`, `AddTorque`, `MoveKinematic`, `Teleport`, `LinearVelocity` |
| Walk a character | `new CharacterController(entity).Move(velocity)`, `IsGrounded`, `Velocity` |
| Query the world | `Physics.Raycast`, `RaycastAll`, `SphereCast`, `BoxCast`, `CapsuleCast`, `OverlapSphere`, `OverlapBox`, `Physics.Gravity`, `LayerMask` |
| Play a sound | `new AudioSource(entity).Play()`, `Stop`, `Pause`, `Volume`, `Pitch`, `Loop`, `Clip`; `Audio.PlayOneShot`, `MasterGain`, group gains |
| Read input | `Input.IsKeyDown / Pressed / Released`, `MousePosition`, `MouseDelta`, gamepads, `Input.CursorMode`, `Input.Clipboard` |
| Spawn a prefab, load a scene | `Prefab.Find(path).Instantiate(parent)`, `InstantiateMany`, `Scene.Load(path, additive)`, `Scene.Unload`, `Scene.IsLoaded` |
| Reference an asset | `AssetRef<T>`: `Load`, `LoadBlocking`, `Release`, `IsLoaded`, `State`, `AssetRef<T>.Find(path)` |
| Report a self-check, read frame statistics | `Diagnostics.Check(name, passed, detail)`, `Diagnostics.Stats`, `Diagnostics.LastError` |
| See which modules are loaded | `Modules.Registry` (read-only `ModuleInfo` list) |
| Iterate thousands of components without per-entity calls | `ComponentQuery.Begin<T1, T2>()`, `NextChunk`, `ChunkView.Get<T>(column)` |

## Scripts and their lifecycle

A script is a class deriving from `ScriptBehaviour`. The engine instantiates it when its entity gets the
script component (from a scene file, a prefab or `entity.AddScript<T>()`) and calls, in order, `OnCreate` (once,
also after a hot reload), `OnStart` (once, before the first `OnUpdate`), then per fixed step `OnFixedUpdate`,
per frame `OnUpdate` and `OnLateUpdate`, and `OnDestroy` when the entity or the script goes away. Collision
callbacks (`OnCollisionEnter(in Collision)`, `OnCollisionExit(in Collision)`, `OnTriggerEnter(Entity other)`,
`OnTriggerExit(Entity other)`) deliver the contacts of the previous fixed step, one step after they happened. `Entity`, `Transform`, `GetComponent<T>()`, `HasComponent<T>()`
and `Destroy()` are available on the base class.

Public fields and `[SerializeField]` private fields are saved with the scene, shown in the inspector
(`[Range]`, `[Tooltip]`, `[HideInInspector]`) and preserved across hot reloads; everything else resets.

```csharp
public sealed class Spinner : ScriptBehaviour
{
    [Range(0.0, 720.0)] public float DegreesPerSecond = 90.0f;

    protected override void OnUpdate(float deltaTime)
    {
        Transform transform = Transform;
        transform.Rotate(Quaternion.CreateFromAxisAngle(Vector3.UnitY, DegreesPerSecond * deltaTime * MathF.PI / 180.0f));
    }
}
```

> [!WARNING]
> `Transform`, `Rigidbody`, `AudioSource` and the other facades are `readonly struct`s returned by value. Write
> `Transform transform = Transform; transform.Position = p;` — `Transform.Position = p` does not compile (CS1612),
> and a property setter on a temporary would be lost anyway.

## Entities, transforms and the hierarchy

`Entity` is an 8-byte handle (`Id`, `IsNull`, `IsAlive`, `Name`). `Entity.Create(name)` makes a scene entity with
a transform and a guid; `Destroy()` queues it for the end of the phase (`OnDestroy` sees the entity alive).
`Transform` reads and writes `LocalTransform` and the world pose: `Position`, `Rotation`, `Scale` and `Local`
(one call each way), `World` and `WorldMatrix`, `Forward`, `Right`, `Up`, `Translate`, `Rotate`, `LookAt`,
`Interpolated` (the physics-interpolated pose the renderer draws). The hierarchy half: `Parent`,
`SetParent(parent, keepWorld = true, index = -1)`, `ChildCount`, `GetChildren(Span<Entity>)`, `Entity.Root`,
`Enabled`, `Tag`, `HasTag`, `Layer`, `Guid`, `Duplicate()`, `Entity.CreateMany(count, span, parent)`.

> [!TIP]
> `Position` is a get or a set, one native call each. For a batch of entities use `Local` once per entity, or a
> `ComponentQuery` over `LocalTransform` and write the spans.

## Components and mirrors

A `[NativeComponent("conjure.scene.local_transform")]` struct mirrors a native component field for field;
the engine verifies the layout hash at start-up and refuses a mismatched assembly. The mirrors shipped:
`LocalTransform`, `WorldTransform`, `InterpolatedTransform`, `Parent`, `Name`, `Tag`, `Layer`, `EntityGuid`,
`Disabled`, `TransformDirty`, `PrefabInstance` (scene); `RigidBodyData`, `BoxColliderData`, `SphereColliderData`,
`CapsuleColliderData`, `ConvexColliderData`, `MeshColliderData`, `PhysicsMaterialData`, `CharacterControllerData`
(physics); `AudioSourceData`, `AudioListenerData` (audio). `GetComponent<T>()` copies the struct out,
`SetComponent(in value)` copies it back, `AddComponent` / `RemoveComponent` change the archetype at once outside a
query scope and at the end of the phase inside one. A struct of your own marked `[EcsComponent]` becomes a
dynamic ECS component the editor can show and scene files can save.

> [!CAUTION]
> `WorldTransform`, `InterpolatedTransform` and the `[runtime]` fields of `RigidBodyData` (velocities,
> `IsSleeping`) are written by the engine every frame. Writing them back with `SetComponent` is overwritten at
> the next step; use `Transform`, `Rigidbody` and the facades to change state.

## Physics, audio, input

`Rigidbody` (over `RigidBodyData`): `LinearVelocity`, `AngularVelocity`, `AddForce(force, ForceMode)`,
`AddForceAtPosition`, `AddTorque`, `MoveKinematic(in TransformData)`, `Teleport`, `IsSleeping`, `WakeUp`.
`ForceMode` is `Force`, `Impulse`, `VelocityChange` or `Acceleration`, mapped with the body's mass.
`CharacterController`: `Move(velocity)`, `IsGrounded`, `Velocity`. The static `Physics` class queries the world:
`Raycast(origin, direction, maxDistance, out RaycastHit, LayerMask)`, `RaycastAll`, the shape casts,
`OverlapSphere`, `OverlapBox`, `Gravity` (get and set), `Layer(name)`, `LayerIndex`, `LayerName`, `LayersCollide`.

`AudioSource` plays the clip of an `audio_source` component: `Play`, `Stop`, `Pause`, `Resume`, `IsPlaying`,
`Volume`, `Pitch`, `Loop`, `Clip`. The static `Audio` class offers `PlayOneShot(clip, position, volume, group)`,
`MasterGain`, `GetGroupGain` / `SetGroupGain` and `ClipDuration`. Playback requests are consumed by the engine's
`engine.audio_sync` system in the same frame; `IsPlaying` is one frame old.

`Input` reads the frame's snapshot: `IsKeyDown` (held), `IsKeyPressed` (this frame), `IsKeyReleased`,
`MousePosition`, `MouseDelta`, `ScrollDelta`, `IsMouseButtonDown`, the gamepad buttons and axes; the bindings
half adds `CursorMode`, `GamepadName(index)`, `Clipboard` and `Input.Inject.*` (keys, buttons, mouse moves and
gamepad events a script or a test feeds into the next frame).

## Assets, scenes and prefabs

`AssetRef<T>` is a uuid plus a lazily bound slot: `Load(priority)`, `LoadBlocking()`, `Release()`, `IsValid`,
`IsLoaded`, `State`, `AssetRef<T>.Find(path)`; `T` is one of the asset tags (`PrefabAsset`, `SceneAsset`,
`AudioClipAsset`, `TextureAsset`, `MeshAsset`, `MaterialAsset`, `ModelAsset`, `EnvironmentAsset`,
`CollisionMeshAsset`, `BinaryAsset`). A public `AssetRef<T>` field is serialised as the uuid and bound when the
scene loads. `Prefab` wraps `AssetRef<PrefabAsset>`: `Instantiate(parent, local)`, `InstantiateMany(count, roots,
parent)`, `Prefab.SourceOf(entity)`, `IsInstanceRoot`. The static `Scene` class loads and unloads scene assets by
path or reference (`Load(path, additive)`, `Unload`, `IsLoaded`, `IsLoadPending`, `ActivePath`, `LoadedCount`,
`LoadedPath(index)`, `EntityCount`, `StructureVersion`, `GetRoots`); loads complete at the next `FRAME_BEGIN`.

## Diagnostics and modules

`Diagnostics.Check(name, passed, detail)` records a verdict in the engine's check registry (📘 API: [Export and
diagnostics](../engine/export-and-diagnostics.md)); the test harness and the feature-test scene read them back.
`Diagnostics.Stats` is the last frame's `FrameStats` (`FrameMs`, `SubmitMs`, `FixedSteps`, `JobsExecuted`,
`FrameMemoryHighWater`, `PhaseMs(phase)`); `Diagnostics.LastError(out status)` reads the native last-error text.
`Diagnostics.Draw.*` (lines, boxes, spheres, text) reaches the debug-draw overlay when the renderer runs.
`Modules.Registry` lists every discovered module as `ModuleInfo { Name, Version, State, Kind, FeatureFlags,
IsLoaded }`; it is read-only, loading is the editor's and the project's business.

## Data-oriented scopes

`ComponentQuery.Begin<T1, T2>()` locks the world and walks every chunk that holds the requested components:
`NextChunk(out ChunkView chunk)`, then `chunk.Get<T1>(0)`, `chunk.Get<T2>(1)` as `Span<T>` and
`chunk.Entities`. Writes through the spans are writes into the engine's memory, no copies. The scope ends with
`Dispose()` (a `using` block), which also flushes every structural request made meanwhile.

```csharp
using (ComponentQuery query = ComponentQuery.Begin<LocalTransform, RigidBodyData>())
{
    while (query.NextChunk(out ChunkView chunk))
    {
        Span<LocalTransform> transforms = chunk.Get<LocalTransform>(0);
        for (int i = 0; i < chunk.Count; ++i)
        {
            transforms[i].Position.Y = MathF.Max(transforms[i].Position.Y, 0.0f);
        }
    }
}
```

> [!WARNING]
> Inside a scope `Entity.Create`, `Destroy`, `AddComponent`, `RemoveComponent`, `SetParent` and
> `Prefab.Instantiate` are deferred to the end of the scope (the handles they return are valid at once); a
> `Duplicate()` cannot be deferred and throws `InvalidOperationException` (`STRUCTURE_LOCKED`).

## Hot reload

Rebuild the game assembly (`script.compile` in the editor, or `dotnet build`) and the engine reloads it: it
collects every serialised field, destroys the instances, unloads the old assembly, loads the new one and recreates
the instances with the saved fields (`OnCreate` runs again, `OnStart` does not). A class that vanished keeps its
field JSON and is disabled with one warning; it comes back when a later build restores it. Private state resets to
its initialisers. Plugin assemblies share the game load context, so a leaky plugin assembly prevents unloading.

## API summary

| Member (short) | Purpose |
|---|---|
| `ScriptBehaviour`: `OnCreate`, `OnStart`, `OnFixedUpdate(float)`, `OnUpdate(float)`, `OnLateUpdate(float)`, `OnDestroy`, `OnCollisionEnter/Exit(in Collision)`, `OnTriggerEnter/Exit(Entity)` | the callbacks |
| `Entity`: `Create`, `Find`, `FindByGuid`, `FindByTag`, `FindByLayer`, `CreateMany`, `IsAlive`, `Name`, `Destroy`, `AddScript<T>`, `GetScript<T>`, component accessors, `Parent`, `Root`, `Enabled`, `Tag`, `Layer`, `Guid`, `Duplicate` | entities |
| `Transform`: `Position`, `Rotation`, `Scale`, `Local`, `World`, `WorldMatrix`, `Forward/Right/Up`, `SetParent`, `GetChildren`, `Translate`, `Rotate`, `LookAt`, `Interpolated` | transforms |
| `Rigidbody`, `CharacterController`, `Physics`, `LayerMask`, `RaycastHit` | physics |
| `AudioSource`, `Audio` | audio |
| `Input` (+ `Input.Inject`), `Key`, `MouseButton`, `GamepadButton`, `GamepadAxis`, `CursorMode` | input |
| `AssetRef<T>`, `AssetId`, `AssetState`, the asset tags, `Prefab`, `Scene` | assets, scenes, prefabs |
| `Diagnostics`, `FrameStats`, `Modules.Registry`, `ModuleInfo`, `Log`, `Time` | diagnostics and services |
| `ComponentQuery`, `ChunkView`, `ComponentType<T>` | the DOD escape hatch |

## See also

- 📘 API: [Scripting from C#: the bindings](index.md), [The native table](native-table.md)
- 📘 API: [Scripts, components and the runtime](../scripting/scripts-and-components.md), [Hosting, the native API and hot reload](../scripting/hosting-and-hot-reload.md)
- 📘 API: [Bodies, colliders and characters](../physics/bodies-and-colliders.md), [Playing sounds](../audio/playing-sounds.md), [Scene files, prefabs and snapshots](../scene/scene-files-and-prefabs.md)
