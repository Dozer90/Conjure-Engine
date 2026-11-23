# Writing a module
<!-- doc-template:wiki -->
[API](../index.md) → [Modules](index.md)

<!-- doc-content:purpose:start -->
How to write the native side of a module: the manifest, the entry point, what you can register and what the host
gives you in return. Include `conjure/modules/modules.h`; it pulls in `conjure/modules/module_entry.h`,
`conjure/modules/module_context.h`, `conjure/modules/module_registrar.h` and `conjure/modules/module_service.h`.
Everything is in `namespace conjure::modules`. The build side (`conjure_add_game_module()`, directory layout) is
in the handbook's [Creating a module](../../handbook/extensions/creating-a-module.md).
<!-- doc-content:purpose:end -->

|---|---|
| Name | Writing a module |
| Module | Modules |
| `#include` | `conjure/modules/module_context.h`, `conjure/modules/module_entry.h`, `conjure/modules/module_events.h`, `conjure/modules/module_manifest.h`, `conjure/modules/module_registrar.h`, `conjure/modules/module_registration_set.h`, `conjure/modules/module_service.h`, `conjure/modules/module_types.h`, `conjure/modules/module_version.h`, `conjure/modules/modules.h` |
| Engineering Handbook | [Engineering handbook](../../handbook/engine/modules.md) |

## Syntax

<!-- doc-content:syntax:start -->
<a id="quick-example"></a>

The entry registers a component and a lifecycle callback, then returns `OK`. Only `CONJURE_DECLARE_MODULE` differs
between a plugin and the game module.

```cpp
#include "conjure/modules/modules.h"

#include "conjure/ecs/component_type_desc.h"

#include "conjure/core/types.h"

using namespace conjure;
using namespace conjure::modules;

namespace game {
    struct spinner {
        f32 radiansPerSecond = 1.0f;
    };
}
CONJURE_COMPONENT(game::spinner, "game.spinner");

namespace {
    void on_activated(module_context &) {}
    void on_deactivating(module_context &) {}

    module_entry_status game_entry(module_context &context) {
        module_registrar &registrar = context.registrar();          // valid only while the entry runs
        registrar.register_extension(ecs::describe_component<game::spinner>());
        registrar.set_lifecycle(module_lifecycle{&on_activated, &on_deactivating});
        return module_entry_status::OK;
    }
}

CONJURE_DECLARE_MODULE(game_entry)
```

`tests/modules/fixtures/module_alpha/module_alpha.cpp` is the same shape with a system, a retained event
subscription and a log line through the host logger.
<!-- doc-content:syntax:end -->

## At a glance

<!-- doc-content:at_a_glance:start -->
| You want to... | Use |
|---|---|
| Declare the module binary's entry | `CONJURE_DECLARE_MODULE(entry_function)` |
| Compile a module into a host or test instead | `CONJURE_DECLARE_STATIC_MODULE(identifier, manifestJson, &entry)` |
| Add a component, system, importer, panel... | `context.registrar().register_extension(desc)` |
| Ship C# code | `"managed"` in the manifest, or `registrar.register_managed_assembly("managed/Game.dll")` |
| Run code once everything is applied / before it is removed | `registrar.set_lifecycle(module_lifecycle{&on_activated, &on_deactivating})` |
| Refuse to load with a reason | `registrar.fail("needs the renderer")` |
| Ask the host for a service | `context.try_service<T>()` or `context.require_service<T>()` |
| Subscribe to a host event safely | `context.retain(bus.subscribe<E>(&callback))` |
| Know which host you run in | `context.host()` is `EDITOR`, `RUNTIME` or `TEST` |
<!-- doc-content:at_a_glance:end -->

## Variables and Properties

<!-- doc-content:variables:start -->
Types, fields, and state are described in the reference sections below.
<!-- doc-content:variables:end -->

<!-- doc-content:body:start -->
## The manifest

`module.json` sits in the module root; `conjure/modules/module_manifest.h` parses it into a `module_manifest`.

| Key | Field | Meaning | Default |
|---|---|---|---|
| `schema` | `schemaVersion` | always `1` | required |
| `name` | `name` | `[a-z0-9_.]{1,64}`; reverse-domain names avoid clashes (`com.studio.game`) | required |
| `version` | `version` | `major.minor.patch` | required |
| `engine` | `engineVersion` | engine versions this module's *source* works with (a range, below) | required |
| `displayName`, `description` | same | shown in the editor | `""` |
| `kind` | `kind` | `plugin`, `game` (one per project, loaded last) or `engine` (built-ins only) | `plugin` |
| `platforms` | `platforms` | any of `windows`, `linux`, `macos`; elsewhere the module is skipped | all |
| `requires` | `requiredFeatures` | `renderer`, `physics`, `audio`, `scripting`, `editor`; the host must have them compiled in | none |
| `binary` | `binary` | base name of the native library (no directory part); empty means no native code | `""` |
| `dependencies` | `dependencies` | `{ "name", "version" (range), "optional" }` entries | none |
| `managed` | `managedAssemblies` | C# assemblies, relative to the module root and staying inside it | none |
| `content` | `contentDirectories` | content directories, relative to the module root and staying inside it | none |
| `editorOnly` | `editorOnly` | never exported into a shipped game | `false` |

An unknown key, at the top level or inside a `dependencies` entry, logs a warning naming it and is otherwise
ignored, so check the log after a typo such as `"versoin"`. A `binary` with a directory part (`/`, `\` or `:`, or
`.` / `..`) and a `managed` or `content` entry that would leave the module root (a `..` segment, a leading `/` or
`\`, a drive such as `C:`, or an empty string) are `MANIFEST_INVALID`, with the JSON pointer of the entry
(`/managed/0`). The check is textual, so a manifest is accepted or refused the same way on every platform.

The binary's location is never written down. For `"binary": "space_shooter"` the engine loads
`<root>/bin/<platform>-<arch>-<config>/space_shooter.dll` (`libspace_shooter.so`, `libspace_shooter.dylib`), with
`<config>` one of `debug`, `development`, `distribution`. `module_binary_path(manifest, root, triplet)` and
`decorate_binary_name(base, platform)` compute the same names if you need them in a tool.

> [!WARNING]
> `"engine"` is a *source* compatibility range and is checked at resolve time. Binary compatibility is exact: a
> native module must be built with the same compiler, standard library, configuration and engine version as the
> host, against the same public headers. When any of those differ the load fails before the module's code runs and
> the message says to rebuild the module. The editor builds the game module, so in practice this costs nothing.

### Versions and ranges

`conjure/modules/module_version.h` holds `semantic_version` (`majorVersion`, `minorVersion`, `patchVersion`, each at
most 65535, ordered) and `version_range`, a half-open range `[minInclusive, maxExclusive)`. The manifest range
grammar, as `parse_version_range()` reads it:

| Text | Range |
|---|---|
| `"1.2.3"` | exactly 1.2.3 |
| `"^1.2.3"` | `[1.2.3, 2.0.0)`; for `0.x` it is `[0.2.3, 0.3.0)`, for `0.0.x` only that patch |
| `"~1.2.3"` | `[1.2.3, 1.3.0)` |
| `">=1.0.0 <2.0.0"`, `">=1.0.0"`, `"<2.0.0"` | explicit bounds, either may be omitted |
| `"*"` | every version |

Whitespace (spaces, tabs, line breaks) separates the two comparators and may follow any operator
(`"^ 1.2.3"`, `">= 1.0.0 < 2.0.0"`). Each component is decimal without leading zeros: `"1.02.3"` is a
`PARSE_FAILURE`, `"0.0.0"` and `"10.0.0"` are fine.

```cpp
#include "conjure/modules/module_version.h"

using namespace conjure;
using namespace conjure::modules;

bool accepts(std::string_view rangeText, semantic_version version) {
    const core::result<version_range> range = parse_version_range(rangeText);
    return range.has_value() && range->contains(version);         // PARSE_FAILURE for bad or reversed text
}
```

`host_engine_version()` is the version of the engine your code was compiled against; `to_string()` and
`std::format("{}", version)` print both types.

## The entry point

`conjure/modules/module_entry.h` defines the two macros and the entry signature
`module_entry_status fn(module_context &)` (`module_entry_function`).

- `CONJURE_DECLARE_MODULE(entry)` exports the two C symbols a shared module must have: `conjure_module_describe`
  (returns the ABI signature and the module name) and `conjure_module_entry` (calls your function). Use it once, at
  namespace scope, in one source file. It needs `CONJURE_MODULE_NAME`, which `conjure_add_game_module()` defines
  from the manifest's `"name"`.
- `CONJURE_DECLARE_STATIC_MODULE(identifier, manifestJsonText, entry)` defines
  `const static_module_desc &conjure_static_module_<identifier>()` for a module compiled into the host (built-in
  engine libraries, tests). `entry` may be `nullptr` for a manifest-only module.

Return `module_entry_status::OK` or `FAILED`. Returning `FAILED`, or calling `fail()` even once, rejects the load
with `ENTRY_FAILED` listing every reason; nothing you registered is applied.

> [!WARNING]
> The entry runs on the main thread between frames and is the only place `context.registrar()` works. Keep it to
> registration: no engine objects exist for your module yet. Do the real set-up in `onActivated`, which runs after
> every subsystem has applied your registrations.

## Registering extensions

`module_registrar` (`conjure/modules/module_registrar.h`) fills a `module_registration_set`
(`conjure/modules/module_registration_set.h`), plain data the host reads after your entry returned:

| Call | Effect |
|---|---|
| `register_extension(desc)` | appends a trivially copyable descriptor under `DescType::EXTENSION_POINT` (`"ecs.component"`, `"ecs.system"`, `"editor.panel"`,...) |
| `register_managed_assembly("managed/X.dll")` | adds a C# assembly path relative to the module root; duplicates are ignored; a path that leaves the root (as for `managed` in the manifest) asserts and rejects the load |
| `set_lifecycle(module_lifecycle{&on_activated, &on_deactivating})` | installs the callbacks; the last call wins; either may be null |
| `fail("reason")` | records why the module cannot load; reasons accumulate |

Descriptor types belong to the subsystem that consumes them (the ECS defines `component_type_desc` and
`system_extension_desc`); a descriptor is a trivially copyable struct with a `static constexpr core::string_id
EXTENSION_POINT`. Both properties are `static_assert`ed. Strings inside a descriptor may point at literals in your
module because every subsystem copies what it keeps.

A system's `name` must be unique within its phase, so prefix it with your module's name. A name its phase already
holds, or one your module registers twice in one phase, fails the load with `REGISTRATION_CONFLICT`. Never use an
engine prefix (`scene.`, `physics.`, `scripting.`, `engine.`): a module loaded at startup that takes an engine
system's name still loads, and the engine's own system is then the one refused, with an error in the log. A name
in `reads` or `writes` that nothing has registered does not fail the load. The engine logs one warning when your
module attaches, and the access is added when a component of that name is registered
(📘 API: [Queries, views and systems](../ecs/queries-views-and-systems.md)).

`module_lifecycle::onActivated` runs after the last subsystem attached; `onDeactivating` runs before the first one
detaches. Both receive your `module_context`.

> [!TIP]
> Register everything your module offers and let the host decide: an `editor.panel` record in a runtime host is
> simply never read. Check `context.host()` only when a registration would be wrong (not merely unused) elsewhere.

## The context and host services

`module_context` (`conjure/modules/module_context.h`) is what your module sees of the host. One exists per loaded
module, from the entry until after `onDeactivating`:

| Accessor | Gives |
|---|---|
| `id()` | the `module_id` of this load (changes after an unload and reload) |
| `host()` | `module_host_kind::EDITOR`, `RUNTIME` or `TEST` |
| `engine_version()` | the host's `semantic_version` |
| `manifest()` | your parsed `module_manifest` |
| `root_directory()` | the directory holding `module.json` (empty for static modules) |
| `registrar()` | the registrar, only while `is_entry_running()` |
| `try_service<T>()` | `T *` or `nullptr` |
| `require_service<T>()` | `result<T *>`; `SERVICE_MISSING` names the interface and your module |
| `retain(subscription)` | owns an `event_subscription` so it dies before your binary is unmapped |

Services are host objects (the world, the asset database, the engine context) offered under an identity declared
with `CONJURE_MODULE_SERVICE(qualified::name)` inside the interface (`conjure/modules/module_service.h`). The host
provides them before anything loads; the table is read-only afterwards and safe to read from any thread.

```cpp
#include "conjure/modules/modules.h"

#include "conjure/core/event_bus.h"

using namespace conjure;
using namespace conjure::modules;

struct host_event_service {                                   // declared by the host, seen by the module
    CONJURE_MODULE_SERVICE(game::host_event_service);

    core::event_bus *bus = nullptr;
};

struct level_loaded {};

namespace {
    void on_level_loaded(const level_loaded &) {}

    module_entry_status entry(module_context &context) {
        const host_event_service *events = context.try_service<host_event_service>();
        if (events != nullptr && events->bus != nullptr) {
            context.retain(events->bus->subscribe<level_loaded>(&on_level_loaded));
        }
        return module_entry_status::OK;
    }
}
```

> [!CAUTION]
> Any callback the host could call after your module is unloaded is a crash. Hand the host function pointers only
> inside descriptors (which subsystems remove on detach) or through `context.retain()` (released before the binary
> closes). Never store a subscription or a `std::function` to module code anywhere else on the host side.

## Module events

The host publishes four events on its `core::event_bus` (`conjure/modules/module_events.h`) around every load and
unload; tools and other modules can watch them:

| Event | When | Payload |
|---|---|---|
| `module_loaded_event` | every subsystem attached and `onActivated` ran | `id` |
| `module_unloading_event` | about to detach | `id`, `reloading` (true inside a reload) |
| `module_unloaded_event` | unloaded; the id is now stale | `id`, `name` (a `string_id`, for listeners keyed by name) |
| `module_load_failed_event` | a load failed and applied nothing | `id`, `error` |

All four are published synchronously on the main thread. A module listening to them retains its subscription
through its context like any other.

## Handles, states and flags

`conjure/modules/module_types.h` holds the small value types every signature uses:

| Type | Values / meaning |
|---|---|
| `module_id` | 16-bit `index` + 16-bit `generation`; `is_valid()`; the default id is invalid; prints as `"index:generation"` |
| `module_kind` | `ENGINE`, `PLUGIN`, `GAME` |
| `module_host_kind` | `EDITOR`, `RUNTIME`, `TEST` |
| `module_state` | `DISCOVERED`, `SKIPPED`, `RESOLVED`, `LOADED`, `FAILED` (retry allowed), `UNLOADED`, `FAULTED` |
| `unload_policy` | `REFUSE_IF_IN_USE`, `REMOVE_INSTANCES` |
| `reload_scope` | `FULL`, `MANAGED_ONLY` (C# only) |
| `build_config` | `DEBUG`, `DEVELOPMENT`, `DISTRIBUTION` |
| `module_entry_status` | `OK`, `FAILED` |
| `module_platform_flags`, `module_feature_flags` | flag enumerations behind `platforms` and `requires` |
| `binary_triplet` | platform + architecture + config, prints as `"windows-x64-debug"` |

`host_platform()`, `host_architecture()`, `host_build_config()`, `host_feature_flags()` and `host_binary_triplet()`
are `constexpr` facts about the build your code is in; `to_string()` and the `parse_*()` functions convert to and
from the manifest spellings (`"windows"`, `"renderer"`, `"plugin"`,...).
<!-- doc-content:body:end -->

## Functions

<!-- doc-content:functions:start -->
<a id="api-summary"></a>

| Signature (short) | Purpose |
|---|---|
| `CONJURE_DECLARE_MODULE(entry)` | exports `conjure_module_describe` / `conjure_module_entry` from a shared module |
| `CONJURE_DECLARE_STATIC_MODULE(id, json, entry)` | defines `conjure_static_module_<id>()` returning a `static_module_desc` |
| `module_entry_status entry(module_context &)` | the entry signature (`module_entry_function`) |
| `struct static_module_desc { const char *identifier; const char *manifestJson; conjure_module_entry_fn entry; }` | a module compiled into the host |
| `void module_registrar::register_extension(const Desc &)` | append a descriptor under `Desc::EXTENSION_POINT` |
| `void register_managed_assembly(std::string_view)` / `void set_lifecycle(const module_lifecycle &)` / `void fail(std::string_view)` | assemblies, callbacks, refusal |
| `struct module_lifecycle { void (*onActivated)(module_context &); void (*onDeactivating)(module_context &); }` | optional callbacks |
| `small_vector<const Desc *, 8> extensions_of<Desc>(const module_registration_set &)` | read descriptors back (hosts) |
| `module_id id()` / `module_host_kind host()` / `semantic_version engine_version()` / `const module_manifest &manifest()` / `const path &root_directory()` | context facts |
| `module_registrar &registrar()` / `bool is_entry_running()` | registration, entry only |
| `T *try_service<T>()` / `result<T *> require_service<T>()` / `void retain(event_subscription &&)` | services and subscriptions |
| `CONJURE_MODULE_SERVICE(qualified::type)` | declares `SERVICE_NAME` and `SERVICE_ID` inside a service interface |
| `result<module_manifest> parse_module_manifest(text, sourceName)` / `load_module_manifest(path)` / `json::document write_module_manifest(manifest, includeEditorOnly)` | manifest text and files |
| `bool is_valid_module_name(name)` / `std::string decorate_binary_name(base, platform)` / `path module_binary_path(manifest, root, triplet)` / `path module_binary_directory(root, triplet)` | names and paths |
| `result<semantic_version> parse_version(text)` / `result<version_range> parse_version_range(text)` / `to_string(...)` / `host_engine_version()` | versions |
| `bool version_range::contains(semantic_version)` / `bool is_unbounded()` | range tests |
<!-- doc-content:functions:end -->

------------------------------------------

**See also:**

<!-- doc-content:related:start -->
- 📘 API: [Modules and plugins](index.md) (overview and lifecycle), [Loading and managing modules](loading-and-managing.md)
- 📘 API: [Errors and results](../core/errors-and-results.md), [Time, events and random](../core/time-and-events.md)
  (`event_bus`, `event_subscription`), [Strings and identifiers](../core/strings-and-identifiers.md) (`string_id`)
- 🛠 Handbook: [Creating a module](../../handbook/extensions/creating-a-module.md) (CMake, layout, test host),
  [Modules module](../../handbook/engine/modules.md)
<!-- doc-content:related:end -->

<!-- doc-legacy-anchors:start -->
<a id="see-also"></a>
<!-- doc-legacy-anchors:end -->
