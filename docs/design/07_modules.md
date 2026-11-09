# Design G - Module / Plugin System (`conjure::modules`)

Status: final. Judge's synthesis of the performance-first and robustness-first proposals. Everything
here is binding for implementers; nothing architectural is left open.

## 1. Overview and decisions

The module system does five things: find modules (manifests), prove a binary is safe to load before
any of its code runs, load/unload/reload in dependency order on the main thread between frames,
collect what a module registers as plain data, and hand that data to *consumers* (ECS, assets,
scripting, editor) which apply it atomically. Nothing in `conjure::modules` runs per frame except one
relaxed atomic load. A module's components live in the same archetype chunks as engine components and
its systems sit in the same parallel schedule; the boundary is invisible to the scheduler.

### D1. Linkage: one shared engine library `conjure_shared`; hosts and plugins link it

| Option | Verdict |
|---|---|
| Plugins statically link the `conjure_<module>` libraries; process state injected via a runtime table | **Rejected.** Every mutable static, template static, ImGui/Jolt global and CRT-level state is duplicated per plugin. Enforcement would be a grep over code written by parallel implementers in every wave; one miss is a silent second logger, type registry or component-id counter. Also multiplies plugin size by the engine size. |
| Executables export engine symbols (`ENABLE_EXPORTS`) | **Rejected.** A Windows import library is bound to one image name, so a module linked against `conjure_editor.exe` cannot load into `conjure_runtime.exe`. |
| C function-pointer `engine_api` table; plugins never link engine code | **Rejected.** Doubles the public surface (`void*` mirrors of ECS, reflection, assets, jobs), cannot express templated DOD helpers (`query.each`, `span<T>` over chunks), and buys compiler independence the fixed decisions already exclude. |
| **Umbrella shared library `conjure_shared`** built by whole-archiving every enabled `conjure_<module>` static library; `conjure_editor`, `conjure_runtime` and all plugins link it | **Chosen.** One copy of every engine symbol and global. The static libraries exist exactly as the brief fixes (tests link them directly). Plugins are built once and load into either host. |

Compliance: each engine module is still a static library created by `conjure_add_module()` and game
modules/plugins are shared libraries; only how the two executables link the engine changes, which the
customer's "static library for core engine ... depending on chosen design" permits. The lead architect
confirms this reading (cross-area resolution R17). The umbrella is named `conjure_shared` (alias
`conjure::shared`) because `conjure_engine` is the static library of the wave-4 `engine` module. Binding
consequences for every engine module:
- Every public class with out-of-line member definitions and every public free function defined in a
  `.cpp` carries `CONJURE_API` (`conjure/core/api.h`). Templates, `inline`, `constexpr`, enums and
  pure data structs are not annotated.
- No mutable namespace-scope `inline` variables and no mutable function-local statics in templates
  defined in public headers; process-wide services are reached through exported accessor functions.
  (One documented exception: `ecs::component_type_cache<T>` caches a name lookup keyed by registry serial
  and carries no identity, so a duplicate per DLL is harmless; `01_ecs.md` D4, R17.)
- Component type identity is resolved by name (`core::string_id`) at attach/query time, never through
  a template static counter.
- Third-party libraries with global state (ImGui, Jolt, miniaudio, GLFW) are linked exactly once, into
  `conjure_shared`; modules reach them only through engine APIs. ImGui is the exception: it is exported
  from `conjure_shared` via `IMGUI_API` so editor, engine backend and plugin panels share one context.

### D2. ABI boundary: a C front door, then plain C++

A shared module exports exactly two C-linkage symbols. `conjure_module_describe()` returns a pure-C
`conjure_module_info` holding the **ABI signature** (struct size, ABI version, compiler, standard
library, build config, architecture, runtime flags, feature flags, exact engine version, public-header
hash) and the module name; it is inspected before any C++ object crosses. `conjure_module_entry()` runs
only after the signature matches the host field by field; from there the module uses ordinary C++
engine headers. The *refusal path* must not depend on C++ mangling or layout (a C symbol and struct can
always be read); once identical toolchain, config and headers are proven, C++ types are safe across the
boundary and a C mirror would only add surface and drift. Binary compatibility is **exact** (compiler
major, STL, config, architecture, sanitizers, feature set, engine `major.minor.patch`, header hash);
source compatibility is a **range** (`module.json` `engine`). The error says "rebuild the module"; the
editor builds the game module, so exact equality costs nothing.

### D3. Registration is data; consumers validate, then attach

The entry records into a `module_registration_set` through `module_registrar`: typed extension records
(trivially copyable descriptors appended to one aligned byte buffer under a `string_id` extension
point), managed assembly paths, lifecycle callbacks, failure reasons. After the entry returns, every
`module_consumer` runs `validate()`; only when all pass does the manager `attach()` in order, so a
failed load leaves the engine untouched. Components, systems, importers, asset types, editor panels,
editor commands and native bindings are all extension points whose descriptor types are defined by the
consuming area (convenience wrappers such as `ecs::register_component<T>(module_registrar &)` live
there). `conjure_modules` therefore depends on **`core` only**, keeping the wave-2 siblings independent.

### D4. Ownership is per type, never per instance

Consumers record `(what they attached -> module_id)`. No per-entity tags, refcounts or indirection on
any hot path. "Is this module in use?" is answered from archetype metadata, O(archetypes).

### D5. Main thread, between frames, jobs idle

`load/unload/reload/apply_pending` assert `core::jobs::is_main_thread()` and run at the engine loop's
structural sync point. The editor never mutates from a UI callback; it enqueues a request that
`apply_pending()` executes at the next frame start. The registry is immutable between sync points.

### D6. Reload = cascade unload + load with consumer-owned snapshots

Each consumer stores keyed binary blobs into a `reload_context` on detach and restores after attach.
The ECS consumer snapshots whole columns per archetype with a field table so an unchanged layout
restores with `memcpy`. A failed reload keeps the snapshot so fix-rebuild-reload is lossless.
`reload_scope::MANAGED_ONLY` runs only opted-in consumers (scripting): the editor's "C# hot reload".

### D7. Built-in engine libraries register through the same mechanism, explicitly

`engine` (wave 4) declares one **manifest-only** static module per enabled engine library (`conjure.ecs`,
`conjure.renderer`, `conjure.physics`, `conjure.audio`, `conjure.scripting`, `conjure.scene`) with
`CONJURE_DECLARE_STATIC_MODULE` (null entry function) and adds them with `module_manager::add_static_module()`
in fixed order, so game modules can depend on them and feature flags resolve uniformly. The built-in libraries
register their components, systems, asset types and importers through the engine's subsystem adapters in the
documented startup order (`06_engine_loop.md` 3.4), never through a module entry (R18). No static-initialiser
lists: the linker drops unreferenced objects from static libraries and
init order is untestable. Game modules can depend on `conjure.physics` like any other module, and the
whole system is unit-testable with static modules and zero `dlopen`.

### D8. The game project is the single `game` module, loaded last

`<project>/module.json` sits beside `project.json`; a project may ship no native binary (pure C#). The
editor discovers `<engine>/plugins/*`, `<project>/modules/*` and `<project>`. Export copies
`modules/<name>/{module.json, bin/<triplet>/, managed/}` and the runtime runs the **same** discovery,
resolution and checks. No bypass path, one set of tests.

### D9. Windows shadow copies in the editor

Windows locks a loaded DLL. With `shadowCopyBinaries` the manager copies the binary and its `.pdb` to
`<project>/.conjure/module_cache/<name>/<n>/` and loads the copy so a rebuild can overwrite the
original. The runtime loads in place.

## 2. Data model

Struct fields use plain camelCase as in the style guide's struct example; class members use `m`.

```cpp
namespace conjure::modules {

    // 32-bit handle; generation increments on every unload so stale ids are detectable.
    struct module_id {
        std::uint16_t index = INVALID_INDEX;
        std::uint16_t generation = 0;
        static constexpr std::uint16_t INVALID_INDEX = 0xFFFF;

        [[nodiscard]] bool is_valid() const noexcept { return index != INVALID_INDEX; }
        auto operator<=>(const module_id &) const = default;
    };

    enum class module_kind : std::uint8_t { ENGINE, PLUGIN, GAME };
    enum class module_host_kind : std::uint8_t { EDITOR, RUNTIME, TEST };
    enum class module_state : std::uint8_t { DISCOVERED, SKIPPED, RESOLVED, LOADED, FAILED, UNLOADED, FAULTED };
    enum class unload_policy : std::uint8_t { REFUSE_IF_IN_USE, REMOVE_INSTANCES };
    enum class reload_scope : std::uint8_t { FULL, MANAGED_ONLY };
    enum class build_config : std::uint8_t { DEBUG = 1, DEVELOPMENT = 2, DISTRIBUTION = 3 };
    enum class module_entry_status : std::uint32_t { OK = 0, FAILED = 1 };

    enum class module_platform_flags : std::uint8_t {    // enum class + NONE per CONVENTIONS 3 (R31)
        NONE = 0,
        WINDOWS = 1 << 0,
        LINUX = 1 << 1,
        MACOS = 1 << 2,
        ALL = WINDOWS | LINUX | MACOS
    };
    CONJURE_ENUM_FLAGS(module_platform_flags);

    // Field names avoid `major`/`minor`, which glibc defines as macros.
    struct semantic_version {
        std::uint16_t majorVersion = 0;
        std::uint16_t minorVersion = 0;
        std::uint16_t patchVersion = 0;
        auto operator<=>(const semantic_version &) const = default;
    };

    // Half-open range [minInclusive, maxExclusive). "*" is [0.0.0, 65535.65535.65535).
    struct version_range {
        semantic_version minInclusive = {};
        semantic_version maxExclusive = { 0xFFFF, 0xFFFF, 0xFFFF };
        [[nodiscard]] bool contains(semantic_version version) const noexcept;
    };

    struct module_manifest {
        std::uint32_t schemaVersion = 1;
        std::string name = {};                              // [a-z0-9_.]{1,64}
        std::string displayName = {};
        std::string description = {};
        semantic_version version = {};
        module_kind kind = module_kind::PLUGIN;
        version_range engineVersion = {};                   // source compatibility
        module_platform_flags platforms = module_platform_flags::ALL;
        module_feature_flags requiredFeatures = {};
        std::string binary = {};                            // base name; empty = no native code
        std::vector<module_dependency> dependencies = {};
        std::vector<std::string> managedAssemblies = {};    // relative to module root
        std::vector<std::string> contentDirectories = {};   // relative to module root
        bool editorOnly = false;
    };

    struct module_registration_set {
        std::vector<std::string> managedAssemblies = {};
        std::vector<extension_record> extensions = {};
        std::vector<std::byte> extensionStorage = {};       // offsets aligned per record
        module_lifecycle lifecycle = {};
        std::vector<std::string> errors = {};               // non-empty => load rejected
    };
}
```

Also in `module_types.h`: `enum class module_feature_flags : std::uint32_t` with `NONE = 0`, `RENDERER = 1 << 0`,
`PHYSICS`, `AUDIO`, `SCRIPTING`, `EDITOR` (`CONJURE_ENUM_FLAGS`; R31), mirroring
the `CONJURE_WITH_*` / `CONJURE_BUILD_EDITOR` definitions. Small structs:
`module_dependency { std::string name; version_range version; bool optional; }`;
`extension_record { core::string_id point; std::uint32_t offset; std::uint32_t size; std::uint32_t alignment; }`;
`module_lifecycle { void (*onActivated)(module_context &); void (*onDeactivating)(module_context &); }`
(after all consumers attached / before any detaches);
`static_module_desc { const char *identifier; const char *manifestJson; conjure_module_entry_fn entry; }`.
The manager-private `module_record` (`engine/src/modules/module_record.h`) holds `id`, `state`, `manifest`,
`rootDirectory`, `binaryPath`, `loadedBinaryPath` (shadow copy), `staticDesc`, `dynamic_library library`,
`registrations`, `std::unique_ptr<module_context> context`, `std::unique_ptr<reload_context> pendingSnapshot`,
`dependencies`/`dependents` (`core::small_vector<module_id, 8>`), `lastError`, `loadOrder`, `loadCount`.
`module_registry` keeps `std::vector<module_record>` (slot reuse by index), the load-order vector, a name
map and an ownership table `(point, key) -> module_id` filled by consumers for tooling.

### 2.1 Manifest file `module.json`

```json
{
  "schema": 1,
  "name": "com.studio.space_shooter",
  "displayName": "Space Shooter",
  "version": "0.3.1",
  "kind": "game",
  "engine": "^0.1.0",
  "platforms": ["windows", "linux", "macos"],
  "requires": ["renderer", "physics", "scripting"],
  "binary": "space_shooter",
  "dependencies": [
    { "name": "conjure.physics", "version": "*" },
    { "name": "com.studio.gameplay_common", "version": ">=1.0.0 <2.0.0", "optional": false }
  ],
  "managed": ["managed/SpaceShooter.dll"],
  "content": ["assets"]
}
```

Required: `schema`, `name`, `version`, `engine`. Defaults: `kind=plugin`, `platforms=all`,
`requires=[]`, `binary=""`, `editorOnly=false`. Unknown keys warn. Validation: name regex, no
self-dependency, no duplicate dependency names. Parsed with `core::json`; errors carry file path and
JSON pointer. Range grammar: `"1.2.3"` exact; `"^1.2.3"` = `[1.2.3, 2.0.0)` (for `0.x`:
`[0.2.3, 0.3.0)`); `"~1.2.3"` = `[1.2.3, 1.3.0)`; `">=1.0.0 <2.0.0"`; `">=1.0.0"`; `"*"`. No
pre-release tags. Binary location is derived, never declared: `<root>/bin/<platform>-<arch>-<config>/`
with `platform` in `windows|linux|macos`, `arch` in `x64|arm64`, `config` in
`debug|development|distribution`, plus the platform decoration of `binary` (`space_shooter.dll`,
`libspace_shooter.so`, `libspace_shooter.dylib`).

### 2.2 C ABI (`module_abi.h`)

```cpp
#define CONJURE_MODULE_ABI_VERSION 1u

namespace conjure::modules { class module_context; }

extern "C" {
    struct conjure_module_abi_signature {
        std::uint32_t magic;                // 0x444D4A43 'CJMD'
        std::uint32_t structSize;           // sizeof(conjure_module_abi_signature)
        std::uint32_t abiVersion;           // CONJURE_MODULE_ABI_VERSION
        std::uint32_t compilerId;           // 1 = MSVC, 2 = GCC, 3 = Clang, 4 = AppleClang
        std::uint32_t compilerMajor;        // _MSC_VER / 100, __GNUC__, __clang_major__
        std::uint32_t standardLibraryId;    // 1 = MSVC STL, 2 = libstdc++, 3 = libc++
        std::uint32_t buildConfig;          // build_config
        std::uint32_t architecture;         // 1 = x64, 2 = arm64
        std::uint32_t runtimeFlags;         // bit0 dynamic CRT, bit1 ASan, bit2 UBSan
        std::uint32_t featureFlags;         // module_feature_flags compiled in
        std::uint32_t engineVersionMajor;   // exact match required
        std::uint32_t engineVersionMinor;
        std::uint32_t engineVersionPatch;
        std::uint64_t headerHash;           // CONJURE_ABI_HEADER_HASH, see 3.8
    };

    struct conjure_module_info {
        conjure_module_abi_signature abi;
        const char *name;                   // must equal manifest.name
    };

    typedef const conjure_module_info *(*conjure_module_describe_fn)(void);
    typedef std::uint32_t (*conjure_module_entry_fn)(conjure::modules::module_context *context);
}
```

The exported symbol names are exactly `conjure_module_describe` and `conjure_module_entry`.

## 3. Algorithms and flow

### 3.1 Discover

`discover(directory)` iterates immediate subdirectories containing `module.json`, sorted by name, and
calls `add_manifest(path)` on each; per-module errors are collected into one `core::error` while valid
modules are still added. `add_manifest`: parse and validate, reject `DUPLICATE_NAME`, resolve root and
binary path for the host triplet, state `SKIPPED` when `platforms` excludes the host (never an error
unless depended upon), else `DISCOVERED`. `add_static_module(desc)` parses the embedded manifest text.

### 3.2 Resolve (pure function, `module_resolver.h`)

Input: candidates (`DISCOVERED`/`UNLOADED`/`FAILED`), `LOADED` modules, host version and features.
1. Per module: `engineVersion.contains(host)` else `ENGINE_VERSION_MISMATCH`; `requiredFeatures ⊆ host`
   else `FEATURE_UNAVAILABLE`.
2. Per dependency: find by name among candidates + loaded (`SKIPPED` counts as missing); missing
   non-optional -> `DEPENDENCY_MISSING`; `version.contains(found.version)` else `DEPENDENCY_VERSION`.
3. Kahn's algorithm, ties broken by name: deterministic order on every machine, hence reproducible
   component-id assignment. Remaining in-degree > 0 -> `DEPENDENCY_CYCLE` (message lists the cycle).
4. Failure propagates to dependents as `DEPENDENCY_FAILED` naming the root cause.
5. `kind == GAME` is forced last; a second game module -> `MULTIPLE_GAME_MODULES`.

### 3.3 Load one module

```
load_one(record):
  assert main thread and outside a frame; state in {RESOLVED, UNLOADED, FAILED}
  if staticDesc:         entry = staticDesc->entry; info = synthetic (host signature, manifest name)
  else if binary empty:  entry = nullptr                                      (manifest-only module)
  else:
    !desc.allowSharedLibraries -> SHARED_LIBRARIES_DISABLED;  !exists(binaryPath) -> BINARY_NOT_FOUND (full path)
    path = shadowCopyBinaries ? shadow_copy(binaryPath + .pdb/.dSYM) : binaryPath   -> BINARY_COPY_FAILED
    library.open(path)  RTLD_NOW|RTLD_LOCAL / LoadLibraryExW(SEARCH_DEFAULT_DIRS|SEARCH_DLL_LOAD_DIR)
                                                                             -> BINARY_LOAD_FAILED (OS text)
    describe, entry = symbol(...) -> ENTRY_SYMBOL_MISSING;  info = describe()  (static-data getter, only code run)
    compare_abi(info->abi, host_abi_signature())                             -> ABI_MISMATCH (field, expected, actual)
    info->name == manifest.name                                              -> NAME_MISMATCH
  context = make_unique<module_context>(id, host, engineVersion, manifest, root, services)
  registrations = {}; registrations.managedAssemblies = manifest.managedAssemblies
  if entry: status = entry(context.get()); status != OK or errors non-empty  -> ENTRY_FAILED (all reasons); close
  for consumer in consumers: validate(id, registrations, registry)           -> REGISTRATION_CONFLICT; close
  for consumer in consumers: attach(id, *context, registrations)             (ecs first: assigns type ids)
  state = LOADED; loadOrder = next; loadCount++; update dependents; onActivated(*context); publish module_loaded_event
```
Any failure sets `FAILED`, stores `lastError`, publishes `module_load_failed_event`, and applies no
registration (validation precedes every attach; an attach failure is a programmer error,
`CONJURE_ASSERT`). `load(id)` resolves the transitive closure of `id` and loads in order, stopping at
the first failure (earlier successes stay loaded; they are valid alone). `load_all()` resolves
everything, continues past failures, returns the first error.

### 3.4 Unload

```
unload(id, policy):
  pre-checks, side-effect free: main thread; state LOADED; no LOADED dependents  -> HAS_DEPENDENTS
  for consumer in consumers: can_detach(id, policy)                     -> IN_USE(count) / UNLOAD_BLOCKED
  lifecycle.onDeactivating(*context); publish module_unloading_event{ id, reloading = false }
  for consumer in reverse: detach(id, nullptr)          (REMOVE_INSTANCES: ecs strips owned components)
  context.reset(); registrations = {}                   (subscriptions and descriptor strings die before the binary)
  if !desc.keepBinariesResident: library.close()
  generation++; state = UNLOADED; drop from load order and dependents; publish module_unloaded_event
```
A `CONJURE_VERIFY` failure between `onDeactivating` and `close` puts the record in `FAULTED`: the
library stays resident for the process lifetime and every later operation on the id returns
`FAULTED`. A leaked DLL beats a `dlclose` of unknown state.

### 3.5 Reload (editor)

`request_reload(id, scope)` enqueues; `apply_pending()` at the next frame start computes the cascade
(the module plus transitive `LOADED` dependents, reverse load order) and per module: pre-checks as
unload, `onDeactivating`, `module_unloading_event{ reloading = true }`, `detach(id, &snapshot)`, close.
Then forward: `load_one` (fresh shadow copy), `restore(id, snapshot)` per consumer, `onActivated`.
`MANAGED_ONLY` skips close/open and runs only consumers with `participates_in_managed_reload()`. If a
load in the cascade fails, that module and the rest stay `UNLOADED` with their snapshots kept in
`pendingSnapshot`; the next successful `load_one` of that id restores them.

### 3.6 ECS snapshot contract (owned by the ECS consumer; fixed here so reload stays fast)

Key: `string_id(component type name)`. Blob: `{ fieldCount, fields[]{ nameHash, offset, size,
typeHash } }` then, per archetype containing the type, `{ archetypeKey, entityCount, entityIds[],
rawColumn[] }`. Restore: identical field table -> one `memcpy` per chunk; otherwise field by field by
name hash, new fields default-initialised via reflection. 10,000 entities of a 64-byte component: 640 KiB.

### 3.7 Export layout

`export_layout(outputDirectory, build_config::DISTRIBUTION)`: for each `LOADED` module with
`kind != ENGINE` and `!editorOnly`, write `<out>/modules/<name>/module.json` (re-serialised,
`editorOnly` dropped), copy `bin/<platform>-<arch>-distribution/` (missing -> `BINARY_NOT_FOUND` with
"build the project in Distribution first"), copy `managedAssemblies`. Content directories are cooked by
the asset pipeline, not copied raw. Static modules are omitted.

### 3.8 Public-header hash

`cmake/ConjureSharedLibrary.cmake` (`conjure_finalize_engine()`, custom target `conjure_abi_identity`) generates
`${PROJECT_BINARY_DIR}/generated/include/conjure/modules/abi_identity.h` with
`#define CONJURE_ABI_HEADER_HASH 0x...ull`: the first 64 bits of a SHA-256 over the relative path and SHA-256 of
every public header of every library in the global property `CONJURE_STATIC_LIBRARIES` (each target's
`CONJURE_PUBLIC_HEADERS` property, set by `conjure_add_module` / `conjure_add_core_area`), sorted by path, as a
build-time custom command depending on those headers (SHA-256 instead of FNV-1a because CMake has no unsigned
64-bit arithmetic; wave 2 integration). Engine and modules compiled in one tree agree; a module built
against stale headers is refused with `ABI_MISMATCH(headerHash)`. The installed SDK ships it frozen.

## 4. Public API - `engine/include/conjure/modules/`

| Header | Main types |
|---|---|
| `log_category.h` | `CONJURE_LOG_CATEGORY(modules)` |
| `module_error.h` | `enum class module_error : std::uint16_t`, `make_module_error` |
| `module_version.h` | `semantic_version`, `version_range`, `parse_version`, `parse_version_range`, `to_string` |
| `module_types.h` | `module_id`, `module_kind`, `module_host_kind`, `module_state`, `unload_policy`, `reload_scope`, `build_config`, `module_entry_status`, platform/feature flags, `binary_triplet` |
| `module_manifest.h` | `module_manifest`, `module_dependency`, `parse_module_manifest`, `load_module_manifest`, `write_module_manifest`, `is_valid_module_name` |
| `module_abi.h` | C structs, `CONJURE_MODULE_ABI_VERSION`, `CONJURE_MODULE_EXPORT`, `make_abi_signature`, `host_abi_signature`, `compare_abi` |
| `module_entry.h` | `static_module_desc`, `CONJURE_DECLARE_MODULE`, `CONJURE_DECLARE_STATIC_MODULE` |
| `module_registration_set.h` | `extension_record`, `module_lifecycle`, `module_registration_set`, `extensions_of<T>` |
| `module_registrar.h` | `module_registrar` |
| `module_service.h` | `service_id`, `make_service_id`, `CONJURE_MODULE_SERVICE`, `service_table` |
| `module_context.h` | `module_context` |
| `module_consumer.h` | `reload_context`, `module_consumer` |
| `module_events.h` | `module_loaded_event`, `module_unloading_event`, `module_unloaded_event`, `module_load_failed_event` |
| `module_resolver.h` | `resolution_input`, `resolution_failure`, `resolution_result`, `resolve_load_order` |
| `module_registry.h` | `module_registry` (read-only view) |
| `module_manager.h` | `module_manager_desc`, `module_manager` |
| `modules.h` | Umbrella for module authors: `module_entry.h`, `module_context.h`, `module_registrar.h`, `module_service.h` |

The RAII `dynamic_library` wrapper described in 4.7 lives in `core` (`conjure/core/dynamic_library.h`,
`core::dynamic_library`) because `scripting` needs it for hostfxr as well; this module consumes it (R30).

Private (`engine/src/modules/`): `module_record.h`, `module_loader.{h,cpp}`, `shadow_copy.{h,cpp}`, one `.cpp`
per public header with out-of-line code, `README.md`, `CMakeLists.txt` (`conjure_add_module(modules SOURCES ...
PUBLIC_HEADERS ... PUBLIC_DEPS conjure::core)`).

### 4.1 `module_abi.h` and `module_entry.h`

```cpp
namespace conjure::modules {
    // Compile-time, from core/platform_detect.h, core/build_info.h and the generated abi_identity.h.
    // Honours CONJURE_TEST_ABI_VERSION_OVERRIDE / CONJURE_TEST_ENGINE_VERSION_OVERRIDE (fixtures only).
    [[nodiscard]] constexpr conjure_module_abi_signature make_abi_signature() noexcept;
    [[nodiscard]] constexpr conjure_module_info make_module_info(const char *name) noexcept;
    [[nodiscard]] CONJURE_API const conjure_module_abi_signature &host_abi_signature() noexcept;

    /// Compares every field, structSize first; the error names the first differing field, expected and actual.
    [[nodiscard]] CONJURE_API core::result<void> compare_abi(
      const conjure_module_abi_signature &moduleSignature,
      const conjure_module_abi_signature &hostSignature
    );
}

// CONJURE_MODULE_EXPORT is __declspec(dllexport) on Windows, __attribute__((visibility("default"))) elsewhere.
// Shared game module. CONJURE_MODULE_NAME is defined by conjure_add_game_module(). EntryFunction is
//   conjure::modules::module_entry_status fn(conjure::modules::module_context &context)
#define CONJURE_DECLARE_MODULE(EntryFunction)                                                        \
    extern "C" CONJURE_MODULE_EXPORT const conjure_module_info *conjure_module_describe(void) {      \
        static constexpr conjure_module_info INFO = conjure::modules::make_module_info(CONJURE_MODULE_NAME); \
        return &INFO;                                                                                \
    }                                                                                                \
    extern "C" CONJURE_MODULE_EXPORT std::uint32_t conjure_module_entry(                             \
      conjure::modules::module_context *context                                                      \
    ) {                                                                                              \
        return static_cast<std::uint32_t>(EntryFunction(*context));                                  \
    }

// Built-in library: defines  const static_module_desc &conjure_static_module_<Identifier>()  (entry_thunk-wrapped).
#define CONJURE_DECLARE_STATIC_MODULE(Identifier, ManifestJsonText, EntryFunction) /* ... */
```

### 4.2 `module_registrar.h`

```cpp
class CONJURE_API module_registrar {
    private:
        module_registration_set *mSet = nullptr;

    public:
        explicit module_registrar(module_registration_set &set) noexcept;
        ~module_registrar();

    public:
        /// Stores a trivially copyable descriptor under DescType::EXTENSION_POINT. The consuming area
        /// defines the descriptor; strings inside may point at module literals, consumers copy what they keep.
        template <typename DescType>
        void register_extension(const DescType &desc) {
            static_assert(std::is_trivially_copyable_v<DescType>);
            static_assert(alignof(DescType) <= alignof(std::max_align_t));
            append_extension(DescType::EXTENSION_POINT, &desc, sizeof(DescType), alignof(DescType));
        }

        void register_managed_assembly(std::string_view relativePath);     // relative to the module root
        void set_lifecycle(const module_lifecycle &lifecycle) noexcept;
        void fail(std::string_view reason);                                 // load rejected after the entry returns
        [[nodiscard]] const module_registration_set &registrations() const noexcept;

    private:
        void append_extension(core::string_id point, const void *data, std::size_t size, std::size_t alignment);
};

template <typename DescType>
[[nodiscard]] core::small_vector<const DescType *, 8> extensions_of(const module_registration_set &set);
```

### 4.3 `module_service.h` and `module_context.h`

```cpp
struct service_id {
    std::uint64_t hash = 0;     // core::hash::fnv1a64 of the qualified interface name
    auto operator<=>(const service_id &) const = default;
};
[[nodiscard]] constexpr service_id make_service_id(std::string_view qualifiedName) noexcept;

#define CONJURE_MODULE_SERVICE(InterfaceType) \
    static constexpr ::conjure::modules::service_id SERVICE_ID = ::conjure::modules::make_service_id(#InterfaceType);

// Host-owned, immutable while any module is LOADED. Services are provided by host code only.
class CONJURE_API service_table {
    public:
        template <typename ServiceType> void provide(ServiceType &service);              // asserts not already provided
        template <typename ServiceType> [[nodiscard]] ServiceType *find() const noexcept;
        void clear() noexcept;
};

class CONJURE_API module_context {
    public:
        module_context(
          module_id id,
          module_host_kind host,
          semantic_version engineVersion,
          const module_manifest &manifest,
          std::filesystem::path rootDirectory,
          const service_table &services
        );
        ~module_context();                                  // releases retained subscriptions

    public:
        [[nodiscard]] module_id id() const noexcept;
        [[nodiscard]] module_host_kind host() const noexcept;
        [[nodiscard]] semantic_version engine_version() const noexcept;
        [[nodiscard]] const module_manifest &manifest() const noexcept;
        [[nodiscard]] const std::filesystem::path &root_directory() const noexcept;
        [[nodiscard]] module_registrar &registrar() noexcept;                   // asserts outside the entry

        template <typename ServiceType>
        [[nodiscard]] ServiceType *try_service() const noexcept;                // nullptr when not provided
        template <typename ServiceType>
        [[nodiscard]] core::result<ServiceType *> require_service() const;      // SERVICE_MISSING names the type

        /// Owns an event subscription so its callback (module code) is released before the binary is
        /// unmapped. Module code never hands the host a callback any other way except through descriptors.
        void retain(core::event_subscription &&subscription);
};
```

Members: `mId`, `mHost`, `mEngineVersion`, `mManifest`, `mRootDirectory`, `mRegistrar` (bound for the entry
only), `mServices`, `std::vector<core::event_subscription> mSubscriptions`; `service_table` wraps a
`core::flat_map<service_id, void *>` behind `provide_raw`/`find_raw`.

### 4.4 `module_consumer.h`

`reload_context` (`CONJURE_API`, owns `core::flat_map<core::string_id, std::vector<std::byte>> mBlobs`):
`void store(core::string_id key, std::span<const std::byte> bytes)`,
`[[nodiscard]] std::span<const std::byte> fetch(core::string_id key) const noexcept` (empty when missing),
`[[nodiscard]] bool contains(core::string_id key) const noexcept`.

```cpp
class CONJURE_API module_consumer {
    public:
        module_consumer();
        virtual ~module_consumer();

    public:
        [[nodiscard]] virtual core::string_id name() const noexcept = 0;

        /// Rejects conflicting registrations before anything is applied.
        [[nodiscard]] virtual core::result<void> validate(
          module_id id,
          const module_registration_set &set,
          const module_registry &registry
        ) = 0;
        virtual void attach(module_id id, module_context &context, const module_registration_set &set) = 0;

        /// Side-effect free veto: IN_USE under REFUSE_IF_IN_USE, UNLOAD_BLOCKED in play mode.
        [[nodiscard]] virtual core::result<void> can_detach(module_id id, unload_policy policy) = 0;
        virtual void detach(module_id id, reload_context *reload) = 0;      // reload null for a plain unload
        virtual void restore(module_id id, const reload_context &reload) = 0;

        [[nodiscard]] virtual std::uint32_t live_use_count(module_id id) const = 0;
        [[nodiscard]] virtual bool participates_in_managed_reload() const noexcept;   // default false
};
```

### 4.5 `module_manager.h`

```cpp
struct module_manager_desc {
    module_host_kind host = module_host_kind::RUNTIME;
    semantic_version engineVersion = host_engine_version();
    module_feature_flags engineFeatures = host_feature_flags();
    build_config binaryConfig = host_build_config();        // selects bin/<platform>-<arch>-<config>
    bool allowSharedLibraries = true;
    bool shadowCopyBinaries = false;
    bool keepBinariesResident = false;                      // debug aid: never dlclose
    std::filesystem::path shadowCopyDirectory = {};
    unload_policy defaultUnloadPolicy = unload_policy::REFUSE_IF_IN_USE;
    bool (*isInsideFrame)() = nullptr;                      // engine loop hook; null = always outside
};

class CONJURE_API module_manager {
    public:
        module_manager(const module_manager_desc &desc, core::event_bus &events);
        ~module_manager();                                  // unload_all()

    public:
        void add_consumer(module_consumer &consumer);       // asserts nothing LOADED; order = attach order
        void remove_consumer(module_consumer &consumer);    // asserts nothing LOADED
        template <typename ServiceType>
        void provide_service(ServiceType &service);         // asserts nothing LOADED

        [[nodiscard]] core::result<module_id> add_static_module(const static_module_desc &desc);
        [[nodiscard]] core::result<module_id> add_manifest(const std::filesystem::path &manifestPath);
        [[nodiscard]] core::result<std::uint32_t> discover(const std::filesystem::path &searchDirectory);

        [[nodiscard]] core::result<void> load_all();
        [[nodiscard]] core::result<void> load(module_id id);
        [[nodiscard]] core::result<void> unload(module_id id, unload_policy policy);
        [[nodiscard]] core::result<void> unload(module_id id);
        [[nodiscard]] core::result<void> reload(module_id id, reload_scope scope);
        void unload_all();                                  // reverse load order, REMOVE_INSTANCES

        void request_load(module_id id);
        void request_unload(module_id id, unload_policy policy);
        void request_reload(module_id id, reload_scope scope);
        void apply_pending();                               // frame start; one relaxed atomic load when idle

        [[nodiscard]] const module_registry &registry() const noexcept;
        [[nodiscard]] const service_table &services() const noexcept;
        [[nodiscard]] core::result<void> export_layout(
          const std::filesystem::path &outputDirectory,
          build_config targetConfig
        ) const;
};
```

Members: `mDesc`, `mRegistry`, `mServices`, `std::vector<module_consumer *> mConsumers` (non-owning),
`std::vector<pending_operation> mPending`, `std::atomic<bool> mHasPending`, `core::event_bus *mEvents`.

### 4.6 `module_registry.h` (read-only; thread-safe between sync points)

`class CONJURE_API module_registry`; every getter is `[[nodiscard]] ... const noexcept`:
`module_id find(core::string_id name)`, `bool is_valid(module_id)`, `module_state state(module_id)`,
`const module_manifest &manifest(module_id)`, `const module_registration_set &registrations(module_id)`,
`const core::error &last_error(module_id)`, `const std::filesystem::path &root_directory(module_id)`,
`std::span<const module_id> all()`, `std::span<const module_id> load_order()`,
`std::span<const module_id> dependents(module_id)`. Consumers record what they attached so the inspector
can show "provided by": `void note_ownership(core::string_id point, core::string_id key, module_id owner)`,
`void forget_ownership(module_id owner)`,
`[[nodiscard]] module_id owner_of(core::string_id point, core::string_id key) const noexcept`.

### 4.7 `module_resolver.h`, `module_events.h`, `dynamic_library.h`

`[[nodiscard]] CONJURE_API resolution_result resolve_load_order(const resolution_input &input)` (pure) with
`resolution_input { std::span<const module_id> candidates; std::span<const module_id> loaded;
const module_registry *registry; semantic_version engineVersion; module_feature_flags engineFeatures; }`,
`resolution_failure { module_id id; core::error error; }` and
`resolution_result { std::vector<module_id> order; std::vector<resolution_failure> failures; }`.

```cpp
struct module_loaded_event { module_id id = {}; };
struct module_unloading_event { module_id id = {}; bool reloading = false; };
struct module_unloaded_event { module_id id = {}; core::string_id name = {}; };
struct module_load_failed_event { module_id id = {}; core::error error = {}; };
```

`core::dynamic_library` (`CONJURE_API`, move-only RAII over `LoadLibraryExW` / `dlopen`, `void *mHandle`,
`std::filesystem::path mPath`): `[[nodiscard]] core::result<void> open(const std::filesystem::path &path)`
(`RTLD_NOW | RTLD_LOCAL`), `void close() noexcept` (idempotent), `[[nodiscard]] bool is_open() const noexcept`,
`[[nodiscard]] void *symbol(const char *name) const noexcept` (nullptr when missing),
`[[nodiscard]] const std::filesystem::path &path() const noexcept`,
`[[nodiscard]] static std::string decorate_name(std::string_view baseName)` (`lib` prefix, `.dll/.so/.dylib`).

### 4.8 Module author view (`samples/feature_test/source/feature_test_module.cpp`)

```cpp
#include "conjure/modules/modules.h"
#include "conjure/ecs/component_type_desc.h"
#include "conjure/editor/panel_desc.h"

namespace {
    conjure::modules::module_entry_status feature_test_entry(conjure::modules::module_context &context) {
        conjure::modules::module_registrar &registrar = context.registrar();

        registrar.register_extension(conjure::ecs::describe_component<feature_test::spinner_component>());
        registrar.register_extension(feature_test::spinner_system_desc());

        if (context.host() == conjure::modules::module_host_kind::EDITOR) {
            registrar.register_extension(feature_test::debug_panel_desc());
        }
        return conjure::modules::module_entry_status::OK;
    }
}

CONJURE_DECLARE_MODULE(feature_test_entry)
```

## 5. Build system (`cmake/ConjureModule.cmake`, `cmake/ConjureSharedLibrary.cmake`)

Spellings follow the implementation (wave 2 integration, `09_cross_area_resolutions.md` 3): the global property is
`CONJURE_STATIC_LIBRARIES`, the macros are `CONJURE_SHARED_EXPORTS` / `CONJURE_SHARED_IMPORTS` (R17 kept the
`CONJURE_ENGINE_*` names, `conjure/core/api.h` shipped the `CONJURE_SHARED_*` ones first and the code wins), and the
umbrella lives in `cmake/ConjureSharedLibrary.cmake`.

- `conjure_add_module()` keeps its signature. Internally it appends the target to the global property
  `CONJURE_STATIC_LIBRARIES` and records its `PUBLIC_HEADERS` in the target property `CONJURE_PUBLIC_HEADERS`.
  The intended `PRIVATE CONJURE_SHARED_EXPORTS`, `CXX_VISIBILITY_PRESET hidden` and `VISIBILITY_INLINES_HIDDEN ON`
  are applied once every public class and function of core carries `CONJURE_API` (`tools/check_exports.py`); until
  then the generated module definition file below exports everything on Windows and GCC/Clang export by default.
- `conjure_finalize_engine()`, called by the root `CMakeLists.txt` right after `add_subdirectory(engine)`,
  creates `conjure_shared` (`SHARED`, alias `conjure::shared`) from one generated empty source; links
  `PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,<every CONJURE_STATIC_LIBRARIES>>"` (third-party dependencies
  follow transitively and are not whole-archived, except `conjure::thirdparty::imgui`, whole-archived
  when `CONJURE_BUILD_EDITOR`); links `PUBLIC conjure::platform_defines conjure::thirdparty::glm`;
  forwards each engine library's `INTERFACE_COMPILE_DEFINITIONS`; defines `PRIVATE CONJURE_SHARED_EXPORTS` and
  `INTERFACE CONJURE_SHARED_IMPORTS`; sets `BUILD_RPATH`/`INSTALL_RPATH` to `$ORIGIN` / `@loader_path`; creates the
  `conjure_abi_identity` custom target (3.8) on which `conjure_modules` and `conjure_shared` depend. On MSVC a
  module definition file listing every external symbol of the whole-archived objects is generated at build time
  and its export count is checked against the 65535 a DLL may hold.
- `conjure/core/api.h`: `CONJURE_API` = `dllexport`/`visibility("default")` under `CONJURE_SHARED_EXPORTS`,
  `dllimport`/`visibility("default")` under `CONJURE_SHARED_IMPORTS`, empty otherwise (tests linking
  static libraries). `cmake/ConjureWarnings.cmake` adds `/wd4251 /wd4275` (dll-interface warnings for
  standard-library members of exported classes, the standard remedy for DLL-exporting C++ projects).
- `conjure_thirdparty_imgui` compiles with `PRIVATE IMGUI_API=<export>` and exposes
  `conjure::thirdparty::imgui_interface` (include dirs, `IMGUI_API=<import>`, existing ImGui defines)
  for the editor executable and for modules built with `EDITOR_UI`.
- `conjure_editor` and `conjure_runtime` list `conjure::shared` in `DEPS` and nothing else from the engine.
  `conjure_add_test_module(<module> SOURCES ... [DEPS ...] [LINK_ENGINE_SHARED])` links `conjure::shared`
  instead of `conjure::<module>` with the option (used by `modules`, `integration`, `engine`, `editor` tests).
- `conjure_add_game_module(<target> MANIFEST <module.json> SOURCES ... [NAME <manifest name>] [OUTPUT_ROOT <dir>]
  [DEPS ...] [DEFINES ...] [EDITOR_UI])`: `add_library(SHARED)`, links `conjure::shared PRIVATE`, hidden
  visibility, defines `CONJURE_MODULE_NAME="<manifest name>"` (`NAME` defaults to the manifest's `"name"`), output
  name = the manifest's `"binary"`, output directory `<root>/bin/<platform>-<arch>-$<LOWER_CASE:$<CONFIG>>/` (both
  `LIBRARY_` and `RUNTIME_OUTPUT_DIRECTORY`; `<root>` is the manifest's directory unless `OUTPUT_ROOT` says
  otherwise, which the test fixtures use to build into the build tree), RPATH `$ORIGIN`, `/MD`. With `EDITOR_UI` links
  `conjure::thirdparty::imgui_interface` and the editor extension headers. `ConjureConfig.cmake`
  exports the same function for installed projects (`find_package(conjure CONFIG)`) and records the
  engine's options so the `CONJURE_WITH_*` macros match.
- `CONJURE_MODULE_ABI_VERSION` is bumped whenever `conjure_module_abi_signature`, `module_context`,
  `module_registrar` or `module_registration_set` change layout.

## 6. Threading and ownership

**Threading.** Every `module_manager` mutator and every consumer callback runs on the main thread with
no jobs in flight (`CONJURE_ASSERT(core::jobs::is_main_thread())` and `CONJURE_ASSERT(!desc.isInsideFrame())`
when the hook is set). `apply_pending()` costs one `mHasPending.load(std::memory_order_relaxed)` when
idle. `request_*` is main-thread only (the editor's command executor). `module_registry` and
`service_table` are read-only between sync points and may be read from job threads. `dlopen` runs
module static initialisers and is never put on a job. Module systems run wherever the ECS scheduler
places them; module code never creates threads (`core::jobs` only).

**Ownership.** `module_manager` owns records, registration sets, contexts, libraries, shadow copies,
the request queue and the service table. Consumers and services are non-owning pointers to host
objects whose lifetime exceeds the manager's: `engine` constructs them first and destroys the manager
first. Consumers own what modules register, tagged by `module_id`, and must remove all of it on detach.
The ECS owns component storage; the manager owns only descriptors. `module_context` lives while its
record is `LOADED` and is valid in both lifecycle callbacks. Nothing host-side points into module code
after `detach` completes, by construction: descriptors are copied by consumers, subscriptions are
retained by the context, and the registration set dies before `close()`.

## 7. Failure handling

`module_error` (each a `core::error` whose message names the module and, where relevant, expected vs
actual): `MANIFEST_NOT_FOUND`, `MANIFEST_INVALID` (file + JSON pointer), `INVALID_NAME`, `DUPLICATE_NAME`,
`PLATFORM_UNSUPPORTED` (-> `SKIPPED`), `ENGINE_VERSION_MISMATCH`, `FEATURE_UNAVAILABLE`,
`DEPENDENCY_MISSING`, `DEPENDENCY_VERSION`, `DEPENDENCY_CYCLE`, `DEPENDENCY_FAILED`,
`MULTIPLE_GAME_MODULES`, `SHARED_LIBRARIES_DISABLED`, `BINARY_NOT_FOUND`, `BINARY_COPY_FAILED`,
`BINARY_LOAD_FAILED` (OS text via `FormatMessageW` / `dlerror`), `ENTRY_SYMBOL_MISSING`, `ABI_MISMATCH`
(field, expected, actual), `NAME_MISMATCH`, `ENTRY_FAILED` (all `fail()` reasons), `REGISTRATION_CONFLICT`
(consumer + offending key), `SERVICE_MISSING`, `HAS_DEPENDENTS`, `IN_USE` (count), `UNLOAD_BLOCKED`
(consumer reason), `NOT_LOADED`, `ALREADY_LOADED`, `INVALID_ID`, `STALE_ID`, `FAULTED`, `EXPORT_FAILED`.

Rules: every failure before `conjure_module_entry` is side-effect free except a closed handle; a
failed entry or validation closes the library and applies nothing; unload pre-checks are side-effect
free; `FAULTED` is the only non-recoverable state and it is contained. Crashes inside module code are
not caught (engine never catches): the editor builds modules out of process, auto-saves the scene
before `apply_pending()` performs a reload, and stops play mode before requesting one (the editor
consumer also vetoes with `UNLOAD_BLOCKED`). Requests for stale ids are dropped with a warning.
Missing optional services are a normal `nullptr`.

## 8. Headless / null behaviour

Module loading has no device dependency; headless editor and runtime load modules fully. Tests of
other areas construct `module_manager_desc{ .host = module_host_kind::TEST, .allowSharedLibraries = false }`:
static modules work, any shared manifest returns `SHARED_LIBRARIES_DISABLED`. With no editor consumer,
`editor.panel` records are stored and never read. `CONJURE_WITH_SCRIPTING=OFF` removes
`FEATURE_SCRIPTING` from `host_feature_flags()`, so a module requiring it fails resolution with a clear
message instead of hitting a missing consumer. The shared-library fixtures are built and loaded in CI
on all three platforms without a GPU, window or audio device.

## 9. Integration points

**Needs from `core` (wave 1).** `result<T>`/`error` (enum code + message); `string_id` with a
`constexpr` constructor from a literal; `small_vector`, `flat_map`; `json` reader/writer; `event_bus`
with RAII `event_subscription`; `is_main_thread()`; constexpr `hash::fnv1a64`; `core/platform_detect.h`
macros `CONJURE_PLATFORM_{WINDOWS,LINUX,MACOS}`, `CONJURE_COMPILER_{MSVC,GCC,CLANG,APPLECLANG}`,
`CONJURE_ARCH_{X64,ARM64}`, `CONJURE_STDLIB_{MSVC,LIBSTDCXX,LIBCXX}`; generated `core/build_info.h` with
`CONJURE_VERSION_MAJOR/MINOR/PATCH`; `core/api.h` with `CONJURE_API`; reflection `type_info` reachable
by pointer from a trivially copyable descriptor and deep-copied by registries at attach.

**Needs from `ecs` (A).** `component_type_desc` (trivially copyable; `static constexpr core::string_id
EXTENSION_POINT{ "ecs.component" }`; name, size, alignment, flags, ops, `const core::reflect::type_info *`),
`describe_component<T>()`; `system_extension_desc` (`EXTENSION_POINT{ "ecs.system" }`; phase, flags,
`reads`/`writes`/`after`/`before` as `const core::string_id *` + counts, run function pointer; the
`std::function`-based `ecs::system_desc` stays the scheduler's own type, R2); `component_registry::register_component`
/ `register_dynamic` returning ids resolved by name and `remove_from_all`; `scheduler::add/remove_system`;
archetype enumeration for `live_use_count`, stripping owned components, and the column snapshot/restore of 3.6.
The ECS consumer rejects a system without a declared access set in `validate`.

**Needs from `assets` (E).** `importer_extension_desc { EXTENSION_POINT{ "assets.importer" }; const char *id;
std::unique_ptr<asset_importer> (*create)(); }` and `asset_type_extension_desc` (`"assets.type"`; name, version,
flags, loader factory) (R37; the `std::string`-bearing `assets::asset_type_desc` stays the registry's own type);
the assets consumer calls the factories, registers them, mounts `contentDirectories` as `modules://<name>/`
(R24) on attach and unmounts on detach; a file watcher on `bin/<triplet>/` the editor turns into
`request_reload` (editor setting, default on). Export cooks module content directories through the asset
pipeline rather than copying them raw.

**Needs from `scripting` (C).** `native_binding_desc { EXTENSION_POINT{ "scripting.binding" }; const char
*entryName; void *function; }` filling an existing null slot of the fixed native table (new slots need an
`.inl` line in the engine tree, R36); the scripting consumer adds `managedAssemblies` to the single game
`AssemblyLoadContext` (`03_scripting.md` D2 and 3.8) on attach, and on detach or a MANAGED_ONLY reload calls
`script_runtime::reload_game_assembly`, which preserves script fields itself (the `reload_context` is unused by
scripting); a leaked ALC is logged, never blocks; `participates_in_managed_reload() == true` (R36).

**Needs from `engine` (F, wave 4).** Owns `module_manager`, the consumers (`ecs_module_consumer`,
`assets_module_consumer`, `scripting_module_consumer`, plus `editor_module_consumer` registered by the editor
executable before `initialise()`) and `builtin_modules.cpp` with one manifest-only `CONJURE_DECLARE_STATIC_MODULE`
per enabled engine library (R18). Startup order (`06_engine_loop.md` 3.4 step 15): construct services and
consumers; `provide_service` for `engine::engine_context`, `engine::application`, `ecs::world`,
`assets::asset_database`, `scene::scene_manager` (R19); `add_consumer` in the order ecs, assets, scripting,
editor; `add_static_module` for built-ins; `discover(<engine>/plugins)` when `engine_plugins`, `discover` for
each `search_paths` entry, `add_manifest(<project>/module.json)`; `load_all()`. `apply_pending()` at frame
start (step 5 of 3.1) with `isInsideFrame` wired. `project.json` gains `"modules": { "search_paths":
["modules"], "engine_plugins": true, "required": [...] }` (R20). The engine's `export_pipeline` builds the
project in Distribution, then calls `export_layout(out, DISTRIBUTION)` as step 4 of `06_engine_loop.md` 3.6 (R21).

**Needs from `editor` (D).** Public extension headers `engine/include/conjure/editor/panel_desc.h` and
`command_desc.h` (installed with the SDK, no editor internals; R14): `panel_desc { EXTENSION_POINT{
"editor.panel" }; const char *title; panel_create_fn create; }`, `command_desc` (`"editor.command"`); an
`editor_module_consumer` that vetoes `can_detach` during play; a Modules panel (states, errors,
load/unload/reload buttons that only enqueue requests); protocol commands `module.list`, `module.load`,
`module.unload`, `module.reload`, `module.discover`, `module.build` (runs the project's CMake build for the
editor's config, then `request_reload`) (R15). Editor sets `shadowCopyBinaries = true` and
`shadowCopyDirectory = <project>/.conjure/module_cache`. Panels use ImGui through the single context owned by
`engine::imgui_layer` and exported from `conjure_shared` (R9).

**Needs from `bindings` and tests (H).** Read-only C# `Conjure.Modules.Registry` (list, state, version,
last error) for scripts and the AI agent, no load/unload from managed code; `LINK_ENGINE_SHARED` tests;
ASan/UBSan CI runs include `dlopen`/`dlclose` with leak suppressions for the loader's own allocations;
`tools/check_exports.py` flags public classes without `CONJURE_API`.

**Offers.** `CONJURE_DECLARE_MODULE`, `CONJURE_DECLARE_STATIC_MODULE`, `module_registrar` with typed
extension points, `module_consumer`, `module_context` services and retained subscriptions, module
events, `module_registry` lookups (`owner_of` for the inspector's "provided by"),
`conjure_add_game_module()`, exported module layout, deterministic load order, and exact compatibility
diagnostics before any module code runs.

## 10. Implementation plan

1. `core/api.h` (`CONJURE_API`), `/wd4251 /wd4275`, `conjure_add_module` additions (exports define,
   hidden visibility, global properties). Build everything; nothing changes behaviour yet.
2. `module_types.h`, `module_version.h/.cpp`, `module_error.h`, `log_category.h`, `module_manifest.h/.cpp`
   over `core::json` with binary triplet derivation. Tests `test_module_version`, `test_module_manifest`.
3. `module_abi.h/.cpp` and `cmake/ConjureSharedLibrary.cmake` generating `abi_identity.h`. Tests `test_module_abi`.
4. `module_registration_set.h`, `module_registrar.h/.cpp`, `extensions_of<T>`. Tests `test_module_registrar`.
5. `module_service.h/.cpp`, `module_context.h/.cpp`, `module_consumer.h/.cpp`, `module_events.h`,
   `module_registry.h/.cpp`, `module_resolver.h/.cpp`. Tests `test_module_context_services`, `test_module_resolver`.
6. `module_entry.h` (macros, `detail::entry_thunk`), `module_manager` with static modules only:
   `add_static_module`, `load/unload/reload/unload_all`, requests, `apply_pending`, events. Tests
   `test_module_manager_static`, `test_module_lifecycle_order`, `test_module_reload`, `test_module_requests`.
7. `dynamic_library.h` + `dynamic_library_win32.cpp` / `dynamic_library_posix.cpp`, `shadow_copy.h/.cpp`,
   `module_loader.h/.cpp` (the shared path of 3.3). Tests `test_dynamic_library`.
8. `conjure_finalize_engine()`, `conjure_shared`, `conjure_add_game_module()`, `LINK_ENGINE_SHARED`,
   ImGui export. `conjure_modules_tests` switches to `LINK_ENGINE_SHARED`.
9. Fixtures under `tests/modules/fixtures/` built with `conjure_add_game_module`, directory passed as
   `CONJURE_TEST_MODULE_DIR`. Tests `test_module_manager_shared`, `test_module_shadow_copy`.
10. `export_layout` (tests `test_module_export_layout`); `engine/src/modules/README.md`;
    `tools/check_exports.py`; `ConjureConfig.cmake` export of `conjure_add_game_module`. Hand the
    consumer contracts of section 9 to areas A, C, D, E, F.

## 11. Tests (`tests/modules/`, target `conjure_modules_tests`, `LINK_ENGINE_SHARED`)

Fixtures (`tests/modules/fixtures/<name>/module.json` + sources): `module_alpha` (one component desc,
one system desc, lifecycle callbacks incrementing an exported counter, logs one line through the host
logger), `module_beta` (depends on alpha `^1.0.0`; registers an `editor.panel` record and a managed
assembly path), `module_bad_abi` (`CONJURE_TEST_ABI_VERSION_OVERRIDE=999`), `module_bad_version`
(`CONJURE_TEST_ENGINE_VERSION_OVERRIDE`), `module_wrong_name`, `module_bad_entry` (returns FAILED after
two `fail()` calls), `module_no_symbol` (plain shared library), `module_managed_only` (no `binary`),
`module_wrong_platform` (manifest only), `module_cycle_a/b`.

- `test_module_version.cpp`: parse exact/caret/tilde/explicit/open/`*`; caret on `0.x`; invalid and
  reversed ranges; `contains` boundaries; `to_string` round trip; ordering.
- `test_module_manifest.cpp`: full and minimal manifests; each required field missing; bad name
  characters and length 65; unknown key warns; unknown platform/`requires` entry errors; self-dependency;
  duplicate dependency; write/parse equality; triplet derivation per platform and config; missing file.
- `test_module_abi.cpp`: `host_abi_signature() == make_abi_signature()`; each field mutated individually
  is rejected naming that field; smaller `structSize` rejected before other fields are read; `headerHash`
  mismatch rejected.
- `test_module_registrar.cpp`: extension alignment and retrieval with two interleaved descriptor types;
  managed assembly recording; `fail()` accumulates; lifecycle set; empty set.
- `test_module_context.cpp` (the design's `test_module_context_services`): `provide`/`try_service`/`require_service`
  miss names the interface;
  `provide_service` while loaded asserts (`assert_capture`); retained subscription fires while loaded and
  never after unload.
- `test_module_resolver.cpp`: linear chain; diamond; name tie-break; missing and optional-missing
  dependency; version out of range; two- and three-node cycles with message; failure propagation; game
  module forced last; two game modules; loaded modules satisfy dependencies; `SKIPPED` dependency treated
  as missing; engine version and feature checks.
- `test_module_manager_static.cpp`: fake consumer recording calls; load/unload/reload across three static
  modules; `validate` veto leaves the registry untouched; `IN_USE` and `REMOVE_INSTANCES`; `UNLOAD_BLOCKED`;
  `HAS_DEPENDENTS`; generation increments and stale id rejected; `unload_all` reverse order; entry failure
  -> `FAILED`, retry succeeds; `allowSharedLibraries = false`.
- `test_module_lifecycle_order.cpp`: consumers attach in add order and detach in reverse; `onActivated`
  after the last attach; `onDeactivating` before the first detach; events in order.
- `test_module_reload.cpp`: FULL cascade order (beta detached before alpha, alpha attached before beta);
  blob round trip through `reload_context`; `MANAGED_ONLY` touches only participating consumers; failed
  reload keeps `pendingSnapshot` and restores on the next successful load; `request_reload` is a no-op
  until `apply_pending`.
- `test_module_requests.cpp`: queue order; stale generation dropped with warning; `apply_pending` with
  `isInsideFrame` returning true asserts.
- `test_dynamic_library.cpp`: open missing path fails with OS text; open fixture; symbol found and missing;
  move semantics; close idempotent; `decorate_name` per platform.
- `test_module_manager_shared.cpp`: discover fixture dir (counts `SKIPPED`); alpha loads and logs through
  the host logger (one logger proves one engine copy); beta after alpha; bad ABI, bad version, wrong name,
  missing symbol never run the entry (counter unchanged); bad entry reports both reasons; managed-only
  module lists its assembly; `load_all` continues past failures; unloading alpha with beta loaded ->
  `HAS_DEPENDENTS`; load/unload 100 times without record growth.
- `test_module_shadow_copy.cpp`: copy path differs from source and source is writable while loaded
  (Windows lock regression); `.pdb` copied when present; cache cleaned at construction, locked files
  skipped with a warning.
- `test_module_export_layout.cpp`: output contains exactly non-engine, non-editorOnly modules, their
  manifests without `editorOnly`, distribution binaries and assemblies; missing distribution binary ->
  `BINARY_NOT_FOUND`.
- `tests/integration/test_module_ecs_roundtrip.cpp` (wave 4): real `ecs_module_consumer`; load alpha,
  spawn 10,000 entities with its component, run its system, FULL reload, all values survive, system runs
  again; `REFUSE_IF_IN_USE` reports 10,000; `REMOVE_INSTANCES` leaves entities without the component.
- `tests/integration/test_module_asset_importer.cpp` (wave 4): a module importer imports a fixture; after
  unload the importer is gone.
- `tests/benchmarks/bench_module_reload.cpp`: reload with 10,000 entities of three owned components;
  target < 5 ms excluding OS load/unload.

## 12. Out of scope / future

- `CONJURE_STATIC_MODULES`: a static aggregate `conjure_shared` and statically linked game modules for
  single-executable distribution. The macro shapes above already allow it (`CONJURE_DECLARE_STATIC_MODULE`
  with a generated manifest TU); not implemented in the first version.
- Relaxed binary compatibility (`major.minor` with a header-layout freeze) for a third-party binary
  plugin marketplace. Exact equality is correct while the editor builds the game module.
- Pre-release version tags, module signing, downloading plugins from a registry; containing crashes in
  module code (out-of-process modules); per-module memory accounting and allocator isolation.
- Module-provided shaders and render passes register through the renderer's own extension descriptors
  when area B defines them; the mechanism here needs no change.
