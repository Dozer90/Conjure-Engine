# Loading and managing modules

📘 **Public API wiki** · 🛠 Handbook: [Modules module](../../handbook/modules/modules.md)

The host side of the module system: finding manifests, loading modules in dependency order, unloading and
reloading them, and reading what is loaded. In a game the engine owns the `module_manager` and you only read its
`module_registry`; you build a manager yourself in a tool, a test host or a custom runtime. Include
`conjure/modules/module_manager.h`; it brings `conjure/modules/module_registry.h` and
`conjure/modules/module_consumer.h` with it. Everything is in `namespace conjure::modules`.

## At a glance

| You want to... | Use |
|---|---|
| Create the manager | `module_manager manager(desc, eventBus)` with a `module_manager_desc` |
| Find every module under a directory | `manager.discover(directory)` |
| Register one manifest or a built-in module | `manager.add_manifest(path)`, `manager.add_static_module(desc)` |
| Load everything in order | `manager.load_all()` |
| Load one module and what it depends on | `manager.load(id)` |
| Unload, refusing while in use / stripping instances | `manager.unload(id)`, `manager.unload(id, unload_policy::REMOVE_INSTANCES)` |
| Reload a rebuilt module keeping state | `manager.reload(id, reload_scope::FULL)` |
| Change modules from inside a frame | `request_load()` / `request_unload()` / `request_reload()`, then `apply_pending()` at frame start |
| Look a module up, read its state or error | `manager.registry().find(name)`, `state(id)`, `last_error(id)` |
| Apply module registrations in an engine system | derive from `module_consumer`, `manager.add_consumer(consumer)` |
| Offer host objects to modules | `manager.provide_service(object)` |
| Lay out loaded modules for a shipped build | `manager.export_layout(directory, build_config::DISTRIBUTION)` |

## Quick example

```cpp
#include "conjure/modules/module_manager.h"

#include "conjure/core/log.h"

#include <filesystem>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

CONJURE_DEFINE_LOG_CATEGORY(LOG_TOOL, "tool");

result<void> load_project_modules(module_manager &manager, const std::filesystem::path &modulesDirectory) {
    CONJURE_TRY(manager.discover(modulesDirectory));            // every <dir>/*/module.json, in name order
    const result<void> all = manager.load_all();                 // dependency order; continues past failures

    const module_registry &registry = manager.registry();
    for (const module_id id : registry.all()) {
        if (registry.state(id) == module_state::FAILED) {
            const std::string &name = registry.manifest(id).name;
            CONJURE_LOG_WARN(LOG_TOOL, "module '{}' failed: {}", name, registry.last_error(id).message());
        }
    }
    return all;                                                   // the first error, or success
}
```

The manager itself is built once by the host: `module_manager_desc desc = {}; desc.host = module_host_kind::RUNTIME;
module_manager manager(desc, events);`. Destroying it unloads everything in reverse order.

## Setting up the manager

`module_manager_desc` describes the host:

| Field | Default | Meaning |
|---|---|---|
| `host` | `RUNTIME` | what modules see in `context.host()` |
| `engineVersion`, `engineFeatures` | this build's | what manifests' `engine` and `requires` are checked against |
| `binaryConfig` | this build's | selects `bin/<platform>-<arch>-<config>/` |
| `allowSharedLibraries` | `true` | `false` makes every native module fail with `SHARED_LIBRARIES_DISABLED`; manifest-only and static modules still load |
| `shadowCopyBinaries`, `shadowCopyDirectory` | `false` | copy each binary before opening it so a rebuild can overwrite the original (the editor on Windows) |
| `keepBinariesResident` | `false` | debug aid: never close a binary |
| `defaultUnloadPolicy` | `REFUSE_IF_IN_USE` | what `unload(id)` without a policy uses |
| `isInsideFrame` | `nullptr` | engine hook; when set, mutators assert it returns false |

Before anything is loaded, add consumers (`add_consumer()`, applied in add order) and services
(`provide_service()`); both assert once a module is `LOADED`.

> [!IMPORTANT]
> Consumers and services are not owned by the manager. Declare them before the manager so they outlive it: the
> manager's destructor unloads what is still loaded and must find them alive.

## Registering and loading

| Call | Behaviour |
|---|---|
| `add_manifest(path)` | parses `module.json`; the module root is the file's directory. `SKIPPED` when the manifest excludes this platform |
| `discover(directory)` | `add_manifest()` for every immediate subdirectory holding a `module.json`, in name order; returns the count, collects per-module errors into one error while still adding the valid ones; a missing directory is `MANIFEST_NOT_FOUND` |
| `add_static_module(desc)` | a module compiled into the host; the descriptor must outlive the manager |
| `load_all()` | resolves every registered module, loads in order, continues past failures and returns the first error |
| `load(id)` | loads the module and the transitive closure of its dependencies, stopping at the first failure (earlier successes stay loaded) |

Order comes from `resolve_load_order()` in `conjure/modules/module_resolver.h`, a pure function you can call
yourself to preview an order: fill a `resolution_input` (candidate ids, already loaded ids, the registry, engine
version and features) and read `resolution_result::order` and `failures`. Dependencies load first, ties are
broken by name so the order is identical on every machine, and the single `GAME` module goes as late as the graph
allows.

```cpp
#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_resolver.h"

#include <vector>

using namespace conjure;
using namespace conjure::modules;

std::vector<module_id> preview_order(const module_manager &manager) {
    const module_registry &registry = manager.registry();
    std::vector<module_id> candidates = {};
    for (const module_id id : registry.all()) {
        if (registry.state(id) == module_state::DISCOVERED) {
            candidates.push_back(id);
        }
    }
    resolution_input input = {};
    input.candidates = candidates;
    input.loaded = registry.load_order();
    input.registry = &registry;
    input.engineVersion = manager.desc().engineVersion;
    input.engineFeatures = manager.desc().engineFeatures;
    return resolve_load_order(input).order;                      // failures are in .failures, in name order
}
```

## Unloading and reloading

| Call | Behaviour |
|---|---|
| `unload(id, policy)` / `unload(id)` | refuses with `HAS_DEPENDENTS` while loaded modules depend on it, then asks every consumer; `REFUSE_IF_IN_USE` fails with `IN_USE` (count in the message) while instances exist, `REMOVE_INSTANCES` strips them. Pre-checks change nothing |
| `reload(id, scope)` | the module plus its loaded dependents: detach with snapshots in reverse load order, close and reopen the binaries (`FULL`), attach in order, restore. `MANAGED_ONLY` swaps only the C# side. The id stays valid across a reload |
| `unload_all()` | reverse load order, no `can_detach` pre-check (the destructor calls it) |

After an unload the id is stale: `registry().is_valid(id)` is false and operations on it return `STALE_ID`. Find
the module again by name to load it a second time. A reload that fails half-way leaves that module `FAILED` and
the rest of the cascade `UNLOADED` with their snapshots kept; the next successful load restores them, so
fix-rebuild-reload loses nothing.

> [!WARNING]
> Every mutator (`load`, `unload`, `reload`, `apply_pending`, `add_consumer`, ...) runs on the main thread outside a
> frame and asserts both. From a system, an editor command or any job, use the request functions instead:
> `request_load(id)`, `request_unload(id, policy)` and `request_reload(id, scope)` enqueue (main thread only),
> `apply_pending()` performs the queue at the next frame start and `pending_count()` tells how much is waiting.
> Requests for stale ids are dropped with a warning; a refused request leaves its error in `last_error(id)`, except
> on a `FAULTED` module, which keeps the fault that put it there.

## Reading the registry

`module_registry` (`conjure/modules/module_registry.h`) is the read-only view; `manager.registry()` returns it. It
changes only inside the manager's mutators, so between frames it may be read from any thread.

| You want to... | Use |
|---|---|
| Find a module by name | `find(string_id(name))`, an invalid id when unknown |
| Check an id is still current | `is_valid(id)` |
| Read state, manifest, error | `state(id)`, `manifest(id)`, `last_error(id)` |
| Read what it registered | `registrations(id)`, then `extensions_of<Desc>(set)` |
| Paths | `root_directory(id)`, `binary_path(id)` (empty for static and manifest-only modules) |
| Everything, or the loaded ones in load order | `all()`, `load_order()`, `size()` |
| The dependency graph of loaded modules | `dependencies(id)`, `dependents(id)` |
| How often a module loaded | `load_count(id)` |
| Who provided a component or asset type | `owner_of(point, key)`; consumers call `note_ownership()` in `attach()` |

Getters taking an id assert on an invalid or stale id and return an empty value, never another module's data.

## Consumers: applying registrations

Engine systems receive module registrations by implementing `module_consumer` (`conjure/modules/module_consumer.h`)
and adding one instance with `add_consumer()`. The manager calls, on the main thread between frames:

| Override | When | Contract |
|---|---|---|
| `name()` | in `REGISTRATION_CONFLICT` messages | a `string_id` such as `"ecs"` |
| `validate(id, set, registry)` | after the entry, before any attach | reject conflicts; an error from any consumer aborts the load with nothing applied |
| `attach(id, context, set)` | in add order | apply the descriptors; cannot fail |
| `can_detach(id, policy)` | before an unload | side-effect free veto: `IN_USE`, `UNLOAD_BLOCKED` |
| `detach(id, reload)` | in reverse order | remove everything the module registered; `reload` is null for a plain unload, a `reload_context` to fill during a reload |
| `restore(id, reload)` | after re-attach in a reload | read the blobs back |
| `live_use_count(id)` | diagnostics and faults | entities with its components, open assets, ... |
| `participates_in_managed_reload()` | `MANAGED_ONLY` | default false; the scripting consumer says true |

`reload_context` is a key-value store of byte blobs: `store(key, bytes)`, `fetch(key)` (empty when missing),
`contains(key)`, `size()`. Consumers own what modules register, tagged by `module_id`, and must remove all of it
in `detach()`.

```cpp
#include "conjure/modules/module_manager.h"

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

class counting_consumer final : public module_consumer {
    private:
        usize mRecordCount = 0;                                 // a real consumer keeps what it attached, by module_id

    public:
        [[nodiscard]] string_id name() const noexcept override {
            static const string_id NAME = string_id::intern("counting");
            return NAME;
        }
        [[nodiscard]] result<void> validate(
          module_id,
          const module_registration_set &,
          const module_registry &
        ) override {
            return {};                                          // an error here aborts the load, nothing applied
        }
        void attach(module_id, module_context &, const module_registration_set &set) override {
            mRecordCount += set.extensions.size();              // read descriptors with extensions_of<Desc>(set)
        }
        [[nodiscard]] result<void> can_detach(module_id, unload_policy) override { return {}; }
        void detach(module_id, reload_context *) override {}
        void restore(module_id, const reload_context &) override {}
        [[nodiscard]] u32 live_use_count(module_id) const override { return 0; }
};
```

> [!CAUTION]
> A consumer that still reports live uses after `detach()` puts the module in `FAULTED`: the binary stays mapped
> for the rest of the process and every later operation on that module fails. Strip everything you own.

## The ABI check

Before a native module's entry runs the manager calls its `conjure_module_describe()` and compares the returned
`conjure_module_abi_signature` with `host_abi_signature()` using `compare_abi()` (`conjure/modules/module_abi.h`).
The signature records the struct size, `CONJURE_MODULE_ABI_VERSION`, compiler and major version, standard library,
build configuration, architecture, runtime flags (dynamic CRT, ASan, UBSan), compiled-in features, the exact engine
version and a hash of every public engine header. Any difference is `ABI_MISMATCH` naming the field with the
expected and actual values; the module's code never ran.

> [!TIP]
> Read the field name in the message: `headerHash` means the module was built against other engine headers,
> `buildConfig` means a Debug module against a Development host, `engineVersionMajor` an older SDK. In every case the
> fix is to rebuild the module in the same tree and configuration as the host.

## Exporting

`export_layout(outputDirectory, build_config::DISTRIBUTION)` writes `<out>/modules/<name>/` for every loaded module
that is not an engine module, not `editorOnly` and not static: the manifest (without `editorOnly`, with every
assembly the module registered), its `bin/<platform>-<arch>-distribution/` directory and its managed assemblies.
A module without a binary for that configuration is `BINARY_NOT_FOUND` ("build the project in Distribution
first"); a copy error is `EXPORT_FAILED`. Content directories are not copied: the asset pipeline cooks them.

## Failure modes

Every failure is a `core::error` in the `"modules"` category; `module_load_failed_event` carries the same error
and `registry().last_error(id)` keeps it.

| Stage | Codes |
|---|---|
| Manifest | `MANIFEST_NOT_FOUND`, `MANIFEST_INVALID` (file and JSON pointer), `INVALID_NAME`, `DUPLICATE_NAME`, `PLATFORM_UNSUPPORTED` (state `SKIPPED`) |
| Resolution | `ENGINE_VERSION_MISMATCH`, `FEATURE_UNAVAILABLE`, `DEPENDENCY_MISSING`, `DEPENDENCY_VERSION`, `DEPENDENCY_CYCLE` (lists the cycle), `DEPENDENCY_FAILED` (names the root cause), `MULTIPLE_GAME_MODULES` |
| Binary | `SHARED_LIBRARIES_DISABLED`, `BINARY_NOT_FOUND` (full path), `BINARY_COPY_FAILED`, `BINARY_LOAD_FAILED` (OS text), `ENTRY_SYMBOL_MISSING`, `ABI_MISMATCH`, `NAME_MISMATCH` |
| Entry | `ENTRY_FAILED` (every `fail()` reason), `REGISTRATION_CONFLICT` (consumer and key), `SERVICE_MISSING` |
| Unload | `HAS_DEPENDENTS`, `IN_USE` (count), `UNLOAD_BLOCKED` (consumer reason) |
| Handles and state | `NOT_LOADED`, `ALREADY_LOADED`, `INVALID_ID`, `STALE_ID`, `FAULTED`, `EXPORT_FAILED` |

Everything before the entry is side-effect free apart from a closed handle; a failed entry or validation closes the
binary and applies nothing; `FAILED` modules may be loaded again after the cause is fixed.

## API summary

| Signature (short) | Purpose |
|---|---|
| `module_manager(const module_manager_desc &, event_bus &)` | the host object; the destructor calls `unload_all()` |
| `void add_consumer(module_consumer &)` / `void remove_consumer(module_consumer &)` / `void provide_service<T>(T &)` | host set-up, before anything loads |
| `result<module_id> add_static_module(const static_module_desc &)` / `add_manifest(path)` / `result<u32> discover(directory)` | registration |
| `result<void> load_all()` / `load(id)` / `unload(id[, policy])` / `reload(id, scope)` / `void unload_all()` | synchronous operations |
| `void request_load(id)` / `request_unload(id, policy)` / `request_reload(id, scope)` / `void apply_pending()` / `usize pending_count()` | deferred operations |
| `const module_registry &registry()` / `const service_table &services()` / `const module_manager_desc &desc()` / `bool any_loaded()` | views |
| `result<void> export_layout(directory, build_config)` | shipped layout |
| `module_id find(string_id)` / `bool is_valid(id)` / `module_state state(id)` / `const module_manifest &manifest(id)` / `const module_registration_set &registrations(id)` / `const error &last_error(id)` | registry lookups |
| `const path &root_directory(id)` / `binary_path(id)` / `u32 load_count(id)` / `span<const module_id> all()` / `load_order()` / `dependents(id)` / `dependencies(id)` / `usize size()` | registry views |
| `void note_ownership(point, key, owner)` / `void forget_ownership(owner)` / `module_id owner_of(point, key)` | "provided by" table |
| `resolution_result resolve_load_order(const resolution_input &)` | pure dependency ordering |
| `class module_consumer` with `name`, `validate`, `attach`, `can_detach`, `detach`, `restore`, `live_use_count`, `participates_in_managed_reload` | the consumer interface |
| `reload_context::store(key, bytes)` / `span<const byte> fetch(key)` / `bool contains(key)` / `usize size()` | reload snapshots |
| `const conjure_module_abi_signature &host_abi_signature()` / `result<void> compare_abi(module, host)` / `constexpr make_abi_signature()` / `make_module_info(name)` | the ABI check |

## See also

- 📘 API: [Modules and plugins](index.md), [Writing a module](writing-a-module.md) (the module author's side)
- 📘 API: [Errors and results](../core/errors-and-results.md), [Time, events and random](../core/time-and-events.md)
  (the event bus), [Jobs and threading](../core/jobs.md) (the main thread)
- 🛠 Handbook: [Importing and distributing modules](../../handbook/module-system/importing-and-distributing.md),
  [Modules module](../../handbook/modules/modules.md) (load algorithm, shadow copies, ABI identity)
