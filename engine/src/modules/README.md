# modules

The module / plugin system (`conjure::modules`, target `conjure_modules`, alias `conjure::modules`).
Binding design: `docs/design/07_modules.md` (with `docs/ARCHITECTURE.md` 2.1 and the cross-area
resolutions R14, R17-R20, R36, R37, R64). Depends on `conjure::core` only.

It does five things: finds modules through their `module.json` manifests, proves a shared binary is
safe to load before any of its code runs (the C ABI signature), loads / unloads / reloads modules in
dependency order on the main thread between frames, collects what a module registers as plain data,
and hands that data to *consumers* (the ECS, assets, scripting and editor adapters of later waves)
which validate first and attach only when every consumer agreed. Nothing here runs per frame except
one relaxed atomic load in `apply_pending()`.

## Public headers (`conjure/modules/`)

| Header | Main types |
|---|---|
| `module_error.h` | `module_error` (32 codes of section 7 of the design), `module_error_name`, `make_module_error`; category `"modules"` |
| `module_version.h` | `semantic_version`, `version_range`, `parse_version`, `parse_version_range` (exact, `^`, `~`, `>=`, `<`, `*`), `to_string`, `host_engine_version()` |
| `module_types.h` | `module_id`, `module_kind`, `module_host_kind`, `module_state`, `unload_policy`, `reload_scope`, `build_config`, `module_entry_status`, `module_platform`, `module_architecture`, `module_platform_flags`, `module_feature_flags`, `binary_triplet`, the `host_*()` facts, names and parsers |
| `module_manifest.h` | `module_manifest`, `module_dependency`, `parse_module_manifest`, `load_module_manifest`, `write_module_manifest`, `is_valid_module_name`, `decorate_binary_name`, `module_binary_path`, `module_binary_directory` |
| `module_abi.h` | `conjure_module_abi_signature`, `conjure_module_info`, the two C entry typedefs, `CONJURE_MODULE_ABI_VERSION`, `CONJURE_MODULE_EXPORT`, `make_abi_signature`, `make_module_info`, `host_abi_signature`, `compare_abi` |
| `module_entry.h` | `static_module_desc`, `module_entry_function`, `CONJURE_DECLARE_MODULE`, `CONJURE_DECLARE_STATIC_MODULE`, `detail::entry_thunk` |
| `module_registration_set.h` | `extension_record`, `module_lifecycle`, `module_registration_set`, `extensions_of<T>` |
| `module_registrar.h` | `module_registrar` (`register_extension<T>`, `register_managed_assembly`, `set_lifecycle`, `fail`) |
| `module_service.h` | `service_id`, `make_service_id`, `CONJURE_MODULE_SERVICE`, `service_table` |
| `module_context.h` | `module_context` (id, host, engine version, manifest, root, registrar during the entry, `try_service` / `require_service`, `retain` for subscriptions) |
| `module_consumer.h` | `reload_context`, `module_consumer` |
| `module_events.h` | `module_loaded_event`, `module_unloading_event`, `module_unloaded_event`, `module_load_failed_event` |
| `module_resolver.h` | `resolution_input`, `resolution_failure`, `resolution_result`, `resolve_load_order` |
| `module_registry.h` | `module_registry` (read-only view plus the ownership table) |
| `module_manager.h` | `module_manager_desc`, `module_manager` |
| `modules.h` | umbrella for module authors (`module_entry.h`, `module_context.h`, `module_registrar.h`, `module_service.h`) |

Private (`engine/src/modules/`): `log_category.{h,cpp}` (`LOG_MODULES`), `module_record.h` (the
per-module state the registry owns), `module_loader.{h,cpp}` (the shared-library half of a load:
settings, binary lookup, shadow copy, `dlopen`, symbols, `describe()`, ABI and name check),
`shadow_copy.{h,cpp}`, and one `.cpp` per public header with out-of-line code.

## How a load works

```
load_one(record):
  static module      -> entry = desc.entry (may be null: manifest-only module)
  manifest-only      -> entry = null
  shared binary      -> allowSharedLibraries? exists? shadow copy? dynamic_library::load,
                        conjure_module_describe + conjure_module_entry, compare_abi(describe()->abi,
                        host), describe()->name == manifest.name          (each a distinct module_error)
  context = module_context(id, host, engineVersion, manifest, root, services)
  entry(context)      -> ENTRY_FAILED when the status is not OK or fail() was called (all reasons listed)
  consumers.validate  -> REGISTRATION_CONFLICT (nothing was applied)
  consumers.attach    (add order)
  LOADED; load order; link dependents; restore a pending reload snapshot; onActivated; module_loaded_event
```

A record is `RESOLVED` only for the instant between the dependency check below and its `load_one`;
a load that stops early leaves the modules it never reached in the state they had before. Before
`load_one` the manager checks every non-optional dependency named in the manifest: one that is not
`LOADED` (it failed while loading in this same pass, after resolution had accepted it) fails the
dependent with `DEPENDENCY_FAILED` naming the dependency and its error, so a module never ends up
`LOADED` without the dependency it declared. When a module becomes `LOADED` it is linked with the
`LOADED` modules it depends on and, in the other direction, with the already `LOADED` modules whose
manifests name it (an optional dependency that loaded after its dependent), so `HAS_DEPENDENTS` and
the reload cascade hold whichever side loaded first.

Every failure before the entry is side-effect free except for a closed handle; a failed entry or
validation closes the binary and applies nothing. `unload` pre-checks (`HAS_DEPENDENTS`, the
consumers' `can_detach`) are side-effect free; then `onDeactivating`, `module_unloading_event`, detach
in reverse order, context and registrations die, the binary closes, the generation increments and
`module_unloaded_event` carries the old id and the name. A consumer still reporting live uses after
detach puts the record in `FAULTED`: its binary stays resident and every later operation on the id
returns `FAULTED`. `reload` (FULL) is the cascade of the module plus its transitive LOADED dependents:
detach with `reload_context` snapshots in reverse load order, close, reopen (a fresh shadow copy),
attach in order, restore, `onActivated`; the id stays valid across a reload. A load failing inside the
cascade leaves that module `FAILED` (its error in `last_error()`) and the rest `UNLOADED`, every
snapshot kept in the record until the next successful load restores it. `MANAGED_ONLY` skips close
and open and runs only consumers with `participates_in_managed_reload()`, but the lifecycle
callbacks bracket it exactly like a full reload: `onDeactivating` before the participating detaches
and `onActivated` after the restores, per cascade member (07_modules.md 3.5).

`load_all()` resolves every registered module (Kahn's algorithm, name tie-breaks, a `GAME` module as
late as the graph allows) and continues past failures; `load(id)` resolves the transitive closure of
the module's unloaded dependencies and stops at the first failure. Resolution failures
(`ENGINE_VERSION_MISMATCH`, `FEATURE_UNAVAILABLE`, `DEPENDENCY_MISSING`, `DEPENDENCY_VERSION`,
`DEPENDENCY_CYCLE`, `DEPENDENCY_FAILED`, `MULTIPLE_GAME_MODULES`) mark the record `FAILED` like any
other load failure. `DEPENDENCY_MISSING` names the actual cause: a module that is not registered,
one `SKIPPED` on this platform, or one `FAULTED` (with its error). `discover()` returns
`MANIFEST_NOT_FOUND` for a search directory that does not exist or cannot be listed; an unreadable
directory never passes for an empty one. `request_load/unload/reload` enqueue; `apply_pending()`
performs the queue at the frame start, drops requests for stale ids with a warning and keeps
refusals in `last_error()`.

## Binaries, manifests and the ABI identity

- A manifest's binary location is derived, never declared: `<root>/bin/<platform>-<arch>-<config>/`
  plus the platform decoration (`game.dll`, `libgame.so`, `libgame.dylib`), with the config chosen by
  `module_manager_desc::binaryConfig`. `conjure_add_game_module()` (`cmake/ConjureModule.cmake`) puts a
  plugin exactly there.
- `make_abi_signature()` is `constexpr` and reads no compiler-defined macro itself: compiler and
  standard library ids come from `platform_detect.h`; the compiler major, the runtime facts
  (`CONJURE_ABI_RUNTIME_DYNAMIC_CRT`, `_ASAN`, `_UBSAN`) and the public-header hash come from the
  generated `conjure/modules/abi_identity.h` (`cmake/ConjureSharedLibrary.cmake`, custom target
  `conjure_abi_identity`, rewritten at build time whenever a public header of any engine module
  changes). The design (07_modules.md 2.2) wants `compilerMajor`, the CRT bit and the ASan bit to
  describe the translation unit being compiled (`_MSC_VER / 100`, `__GNUC__`, `__clang_major__`;
  `_DLL`; `__SANITIZE_ADDRESS__` / `__has_feature(address_sanitizer)`), because a module built
  against the installed SDK ships the generated header frozen and would otherwise report the
  engine's values for a toolchain that differs. Only `platform_detect.h` may read those macros, so
  `module_abi.h` prefers `CONJURE_COMPILER_VERSION_MAJOR`, `CONJURE_RUNTIME_DYNAMIC_CRT` and
  `CONJURE_SANITIZER_ADDRESS` whenever core defines them and falls back to the generated values
  until then (**open request to core**: add the three macros to `platform_detect.h`; nothing in
  this module changes when they land). UBSan has no reliable compile-time macro and always comes
  from the generated header. `ABI_SIGNATURE_COMPILER_MAJOR` and `ABI_SIGNATURE_RUNTIME_FLAGS` expose
  what a translation unit reports. The hash is the first 64 bits of the SHA-256 over every header's
  relative path and SHA-256 digest, sorted by path; the design's FNV-1a over the digests was
  replaced because CMake has no unsigned 64-bit arithmetic to compute it with.
  `CONJURE_TEST_ABI_VERSION_OVERRIDE` and
  `CONJURE_TEST_ENGINE_VERSION_OVERRIDE` (a major version) exist for the mismatch fixtures only.
- `compare_abi` checks `structSize` first so a module built against another layout is refused before
  any other field is read, then `magic`, `abiVersion` and the rest in declaration order; the error
  names the field, the expected and the actual value and asks for a rebuild.
- With `shadowCopyBinaries` the binary and its `.pdb` / `.dSYM` are copied to
  `<shadowCopyDirectory>/<name>/<loadCount + 1>/` and the copy is opened, so a rebuild can overwrite
  the original while the module is loaded (Windows locks a loaded DLL). The cache is emptied when the
  manager is constructed (locked entries are skipped with a WARN) and a copy is removed once its
  library closed.
- `keepBinariesResident` (a debug aid) never closes a binary: on unload the handle and its shadow
  copy stay, and when a later load opens a new image the previous handle moves into the record's
  `residentBinaries` list (plain assignment would unload it, `core::dynamic_library`'s move
  assignment closes the handle it replaces). Every resident image is released when the manager is
  destroyed; their shadow copies are left on disk for the next manager's cache clean.
- `export_layout` copies `registrations(id).managedAssemblies`, the manifest's assemblies plus the
  ones the entry added through `module_registrar::register_managed_assembly()`, and writes that full
  list into the exported manifest's `"managed"` array so the exported tree describes itself.

## The umbrella shared library and symbol export

`cmake/ConjureSharedLibrary.cmake` creates `conjure_shared` (alias `conjure::shared`) in
`conjure_finalize_engine()`, which the root `CMakeLists.txt` calls right after `add_subdirectory(engine)`
so every static library registered through `conjure_register_static_library` (every
`conjure_add_module` / `conjure_add_core_area` target, in the global property
`CONJURE_STATIC_LIBRARIES`: the 17 wave-1/2 libraries today) is whole-archived
(`$<LINK_LIBRARY:WHOLE_ARCHIVE,...>` with a `LINK_LIBRARY_OVERRIDE_<lib>` per library so the transitive
plain links agree). Third-party static libraries follow transitively and are not whole-archived.
`conjure_shared` compiles with `CONJURE_SHARED_EXPORTS`; its consumers get `CONJURE_SHARED_IMPORTS`, the
engine include directories, every engine library's interface compile definitions,
`conjure::platform_defines` (which carries `CONJURE_BUILD_EDITOR=1` outside Distribution, mirrored by
`host_feature_flags()`) and glm; the MSVC dll-interface noise `/wd4251 /wd4275` is part of
`conjure::warnings` for every target.

What is exported today:

- **MSVC.** A module definition file listing every external symbol of the whole-archived objects is
  generated at build time (`cmake -E __create_def`, the mechanism behind CMake's
  `WINDOWS_EXPORT_ALL_SYMBOLS`, which by itself does not look inside static libraries). Everything
  the engine's static libraries define is therefore exported, whether or not it is marked. The one
  limit is the 65535 exports a DLL may have: the 17 wave-1/2 libraries already produce 26,739 and
  `scene`, `renderer`, `physics`, `scripting` and `engine` are still to come, so the `.def`
  generation is followed by a `cmake -P` count that warns above 80 % of the limit and fails the
  build with the remedy instead of leaving the linker to fail with LNK1189. The remedy is the
  design's own (07_modules.md 5): `CONJURE_API`-marked exports only, with `CONJURE_SHARED_EXPORTS`
  on the static libraries and hidden visibility in `conjure_add_module`, once the core owner adds
  the defines.
- **GCC / Clang.** Every default-visibility symbol, the compiler default in this tree (the hidden
  visibility preset the design asks `conjure_add_module` to set is not in place yet).
- **`CONJURE_API`** is applied to every public class with out-of-line members and every out-of-line
  free function of *this* module (`tools/check_exports.py` verifies that); core carries none yet.
  Since the exported side needs nothing extra, the macro matters on the import side: it makes
  consumers call the module system through the import thunk directly and lets exported data resolve.
  No public data symbol is exported by `conjure_modules` (`LOG_MODULES` is private).

The static libraries themselves are compiled without `CONJURE_SHARED_EXPORTS`, so tests that link
them directly see an empty `CONJURE_API`, exactly as before.

`conjure_add_game_module(<target> MANIFEST module.json SOURCES ... [NAME <name>] [OUTPUT_ROOT <dir>]
[DEPS ...] [DEFINES ...] [EDITOR_UI])` builds a plugin: a SHARED library linking `conjure::shared`
privately, hidden visibility, `CONJURE_MODULE_NAME="<name>"` (the manifest's `"name"` unless `NAME`
says otherwise), output name = the manifest's `"binary"`, output directory
`<root>/bin/<platform>-<arch>-$<LOWER_CASE:$<CONFIG>>/` and a `$ORIGIN` run path. `OUTPUT_ROOT` is
an addition to the design so the test fixtures can be built into the build tree instead of beside
their manifests in the source tree.

## Threading and ownership

Every `module_manager` mutator and every consumer callback runs on the main thread with no jobs in
flight: `CONJURE_ASSERT(core::jobs::is_main_thread())` and, when the hook is set,
`CONJURE_ASSERT(!desc.isInsideFrame())`; a violated precondition returns
`core::error_code::INVALID_STATE` (or returns early for the `void` operations). `request_*` is main
thread only. `module_registry` and `service_table` are immutable between sync points and may be read
from job threads. `dlopen` runs module static initialisers on the main thread.

The manager owns records, registration sets, contexts, libraries, shadow copies, the request queue
and the service table. Consumers and services are non-owning pointers to host objects that outlive
the manager. Descriptor strings may point at module literals, so consumers copy what they keep; event
callbacks from module code are retained by the `module_context` and die before the binary is
unmapped.

## Testing

`tests/modules/` builds `conjure_modules_tests` plus seven fixture plugins (`module_alpha`,
`module_beta`, `module_bad_abi`, `module_bad_version`, `module_wrong_name`, `module_bad_entry`,
`module_no_symbol`) and four manifest-only fixtures (`module_managed_only`, `module_wrong_platform`,
`module_cycle_a/b`) under `${PROJECT_BINARY_DIR}/test_modules/<name>/` (`CONJURE_TEST_MODULE_DIR`).
The test executable is declared with `conjure_add_test_module(modules ... LINK_ENGINE_SHARED)`
(`cmake/ConjureTesting.cmake`): it links `conjure::shared` (not the static libraries) together with
`conjure::test_support_shared`, the support sources compiled against the shared library
(`tests/support/CMakeLists.txt`), because the fixtures link it too and the suite proves that host and
plugins share one engine copy (`module_alpha` logs through the host logger into the test's
`log_capture`). One file per public
header plus the topic files of the design (`test_module_manager_static`, `test_module_lifecycle_order`,
`test_module_reload`, `test_module_requests`, `test_dynamic_library`, `test_module_manager_shared`,
`test_module_shadow_copy`, `test_module_export_layout`). Besides the design's list the suite covers
the regressions found in review: a dependency failing at load time fails its dependents, resident
binaries across reload and unload, entry-registered assemblies in the exported layout and manifest,
a retained host subscription dying with the module (`module_alpha` subscribes through the
`fixtures::host_event_service` the test provides), a fault inside a reload cascade, the `unload_all`
error path, an optional dependency linked after its dependent and a `request_*` made off the main
thread (through `core::jobs::dedicated_thread`).

```
python tools/build.py --build-dir build/mod_modules --target conjure_modules_tests --test -R modules
python tools/check_exports.py modules
```

`LINK_ENGINE_SHARED` leaves the test support's global `operator new` replacement
(`tracked_global_new.cpp`) out of the executable: it releases through `_aligned_free`, which corrupts
the heap for memory allocated inside `conjure_shared` by the C runtime's `operator new` (the file
documents the CRT-compatible hooks core::memory would need). `memory_guard` and `allocation_scope`
therefore count nothing here, and the suite asserts "no growth" through the registry (record count,
generations) instead.
