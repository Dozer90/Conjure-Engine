# Scripting from C#: the bindings

📘 **Public API wiki** · 🛠 Handbook: [Bindings module](../../handbook/modules/bindings.md)

The bindings module is the bridge between a C# script and the engine. The scripting module hosts the .NET
runtime and owns the core of the native table (logging, entities, components, queries); the bindings fill the
rest of it: transforms and the hierarchy, scenes and prefabs, physics, audio, input, assets, diagnostics and the
module registry, and they ship the C# facades (`Transform`, `Rigidbody`, `Prefab`, `Scene`, `Physics`, `Audio`,
`Input`, `Diagnostics`, `Modules`) and the `[NativeComponent]` mirrors through which a script reads and writes
the engine's own components. Native code lives in `namespace conjure::bindings`; the C# side is
`Conjure.Managed`, namespace `Conjure`.

## The mental model in five sentences

1. A script is a `ScriptBehaviour` subclass in your game assembly. The engine creates it when its entity gets the
   component, calls `OnCreate`, `OnStart`, `OnFixedUpdate`, `OnUpdate`, `OnLateUpdate`, `OnDestroy` and the
   collision callbacks, and destroys it with the entity.
2. Everything a script touches goes through one native table: `Entity.Create`, `Transform.Position`,
   `Rigidbody.AddForce`, `Prefab.Instantiate`, `Physics.Raycast`, `Audio.PlayOneShot` are thin calls into
   entries the bindings module filled before the game assembly loaded.
3. Component data crosses as plain structs: `LocalTransform`, `RigidBodyData`, `AudioSourceData` and the other
   mirrors have exactly the native layout (size and layout hash are checked the first time a mirror is used), so
   `Entity.GetComponent<T>()` is a copy, `SetComponent` a copy back, and `ComponentQuery` hands out `Span<T>` over
   whole chunks.
4. Structural changes (create, destroy, add, remove, reparent) are applied at once from an ordinary callback and
   deferred to the end of the phase from inside a `ComponentQuery` scope, where the world is locked.
5. Failures are exceptions on the C# side (`EntityNotAliveException`, `AssetNotLoadedException`,
   `NotSupportedException` for a compiled-out subsystem, `ConjureNativeException` otherwise); predicates such
   as `IsAlive` and `IsPlaying` never throw.

## Where to go next

| Page | Read it when you want to... |
|---|---|
| [The C# API](csharp-api.md) | write scripts: entities, transforms and the hierarchy, components and mirrors, physics, audio, input, assets, scenes and prefabs, diagnostics, DOD queries, hot reload |
| [The native table](native-table.md) | understand or extend the native side: `scripting_bindings_subsystem`, filling and counting the table, marshalling, the status protocol, the call counters tests read |

## Headers

| Header | What it holds |
|---|---|
| `conjure/bindings/bindings.h` | umbrella include |
| `conjure/bindings/bindings_subsystem.h` | `scripting_bindings_subsystem` (the engine subsystem that fills the table at startup step 16), `script_residency` |
| `conjure/bindings/native_api_entries.h` | `binding_context`, `fill_native_table()`, `wrap_with_counters()`, `group_available()`, `group_compiled()`, the entry functions |
| `conjure/bindings/call_counters.h` | `native_function_id`, `native_entry_descriptors()`, `call_counters()`, `call_count()`, `reset_call_counters()` |
| `conjure/bindings/marshal.h` | the conversions between the C ABI records and the engine's types, `structural_guard` |
| `conjure/bindings/bindings_error.h`, `conjure/bindings/bindings_error.inl` | `bindings_error`, the status protocol, the last-error buffer |
| `conjure/bindings/log_category.h` | `LOG_BINDINGS` |

## Quick tour

```csharp
using System.Numerics;
using Conjure;

namespace Game;

public sealed class Bouncer : ScriptBehaviour
{
    public float Impulse = 5.0f;
    public AssetRef<AudioClipAsset> Boing;

    protected override void OnStart()
    {
        Transform transform = Transform;       // a readonly struct: copy before writing a property
        transform.Position = new Vector3(0.0f, 2.0f, 0.0f);
    }

    protected override void OnCollisionEnter(in Collision collision)
    {
        new Rigidbody(Entity).AddForce(Vector3.UnitY * Impulse, ForceMode.Impulse);
        Audio.PlayOneShot(Boing, Transform.Position);
        Diagnostics.Check("bouncer.hit", collision.Other.IsAlive, "the other entity is alive");
    }
}
```

## Local terms

| Term | Meaning here |
|---|---|
| native table | the `conjure_native_api` struct of function pointers the managed side calls; filled by scripting (core entries) and bindings (the ten groups) |
| mirror | a C# struct with the exact layout of a native component (`[NativeComponent("conjure.scene.local_transform")]`) |
| facade | a C# wrapper struct or static class over native entries (`Transform`, `Rigidbody`, `Physics`) |
| DOD scope | a `ComponentQuery` between `Begin` and `Dispose`: the world is locked, spans are live, structural requests are deferred |
| stub | the entry installed for a compiled-out or disabled group: answers `NOT_SUPPORTED` and names the entry |
| call counter | one `u32` per entry, incremented outside Distribution; the feature-test scene proves every entry is exercised |

## See also

- 📘 API: [Scripting](../scripting/index.md) (the host, the runtime, the script component, hot reload)
- 📘 API: [Engine: the application and the frame](../engine/index.md), [Scenes, hierarchy and prefabs](../scene/index.md), [Physics](../physics/index.md), [Audio](../audio/index.md)
- 🛠 Handbook: [Bindings module](../../handbook/modules/bindings.md)
