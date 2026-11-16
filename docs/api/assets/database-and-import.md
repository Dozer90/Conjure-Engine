# Database, importers and hot reload

📘 **Public API wiki** · 🛠 Handbook: [Assets module](../../handbook/modules/assets.md)

The editor side of the pipeline: how sources become records with stable uuids, how a module adds a file type
(an importer that cooks it and a loader that reads the cooked result), and how changed files are picked up
while the editor runs. Headers: `conjure/assets/asset_database.h`, `conjure/assets/asset_meta.h`,
`conjure/assets/asset_importer.h`, `conjure/assets/asset_loader.h`, `conjure/assets/asset_payload.h`,
`conjure/assets/asset_type_registry.h`, `conjure/assets/binary_asset.h`, `conjure/assets/file_watcher.h` and
`conjure/assets/hot_reload.h`. Everything is in `namespace conjure::assets`. None of this runs in a shipped
game; the runtime only reads packs ([Cooking, caches and packs](cooking-and-packs.md)).

## At a glance

| You want to... | Use |
|---|---|
| Add a file type the engine does not know | derive `asset_importer` and `typed_asset_loader<T, Intermediate>`, declare the type with `CONJURE_DECLARE_ASSET_TYPE` |
| Register the type and importer | `types.register_type(make_asset_type_desc<T>(loader))`, `importers.register_importer(importer)` |
| Find out what the project contains | `asset_database::records()`, `find(id)`, `find_by_path(path)`, `asset_path_of(id)` |
| Re-import what changed | `database.scan()` then `database.import_stale(jobs)` |
| Change an asset's import settings | `read_meta(id)`, edit `settings`, `write_meta(id, meta)` |
| Import a file with an importer other than its extension's | `set_importer(id, importerId)`, then `import` |
| Rename, create or delete an asset from a tool | `move_asset`, `create_asset`, `delete_asset` |
| Pick up file changes while the editor runs | `polling_file_watcher` plus `hot_reload_controller::update()` each frame |

## Quick example: a project database

Copied from the fixture in `tests/assets/assets_test_support.cpp`. The manager owns the type registry; the
database reads it to know each type's version.

```cpp
#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_source.h"
#include "conjure/assets/binary_asset.h"

#include <memory>
#include <utility>

using namespace conjure;
using namespace conjure::assets;

core::result<void> open_project(
  core::vfs::file_system &files,
  core::jobs::job_system &jobs,
  importer_registry &importers,
  asset_manager &manager,
  std::unique_ptr<asset_database> &outDatabase
) {
    CONJURE_TRY(register_binary_asset_type(manager.types(), &importers));

    outDatabase = std::make_unique<asset_database>(files, importers, manager.types(), &jobs);
    CONJURE_TRY(outDatabase->add_mount("assets", "assets"));
    CONJURE_TRY(outDatabase->add_mount("engine", "engine"));
    CONJURE_TRY(outDatabase->open(".conjure/cache", "windows"));      // loads the db.bin snapshot if valid
    manager.swap_source(std::make_unique<cache_asset_source>(*outDatabase->cache(), *outDatabase));

    CONJURE_TRY(outDatabase->scan());                                 // metas, uuids, the stale set
    CONJURE_TRY(outDatabase->import_stale(jobs));                     // cooks every stale source into the cache
    return {};
}
```

> [!IMPORTANT]
> Register every importer before `scan()`: a source whose extension has no importer gets a `.meta` and a uuid
> (so it is browsable) but its record is `NO_IMPORTER` and it never loads until an importer exists and the
> source is scanned again.

## The database

`asset_database` knows every source under its mounts, the uuid and sub-asset uuids of each, the importer and
settings it uses, the hashes of its inputs and the dependency graph between assets. Construct it with the file
system, the importer registry, the type registry and an optional job system (used to hash sources in parallel
during `scan()`); then `add_mount()` the roots (`"assets"`, `"engine"`), `open()` the cache for a platform and
`scan()`.

| Signature (short) | Purpose |
|---|---|
| `result<void> add_mount(name, rootPath)` / `span<const asset_mount> mounts()` | roots, scanned in the order added |
| `result<void> open(cacheRoot, platform)` / `bool is_open()` / `const asset_cache *cache()` | opens the blob cache and the snapshot |
| `result<scan_report> scan()` | walk every mount, create missing `.meta` files, hash changed sources, compute the stale set |
| `vector<asset_id> stale_ids()` / `result<import_report> import_stale(jobs)` / `result<import_report> import(ids, jobs)` | cook sources into the cache, one job per source |
| `result<asset_id> add_file(assetPath)` / `result<asset_id> create_asset(assetPath, importerId, bytes)` | register a new source (and write it); `create_asset` removes the source again when its `.meta` cannot be written |
| `result<void> move_asset(id, newAssetPath)` / `result<void> delete_asset(id)` | rename keeps every uuid; delete removes source, `.meta`, blobs and records |
| `result<asset_meta> read_meta(id)` / `result<void> write_meta(id, meta)` | the sidecar; a changed uuid is refused; a new importer or new settings make the asset stale |
| `result<void> set_importer(id, importerId)` | "import as": the `.meta` names another registered importer and takes its default settings, the uuid is kept and the asset is stale; `NO_IMPORTER` for an unknown id. For an importer that reads a format without claiming its extension (a `.glb` as `conjure.physics.collision_mesh_importer`) |
| `result<void> save_snapshot()` | persists `db.bin` (a cache; `scan()` saves it too) |
| `vector<asset_record> records()` / `usize record_count()` / `optional<asset_record> find(id)` | browse |
| `bool contains(id)` / `optional<asset_type_id> type_of(id)` / `optional<asset_record_status> status_of(id)` | allocation-free lookups |
| `optional<asset_id> find_by_path(assetPath)` / `find_by_file_path(filePath)` / `optional<string> asset_path_of(id)` / `file_path_of(id)` | path conversions |
| `vector<asset_id> dependencies_of(id)` / `dependents_of(id)` / `owners_of_side_file(assetPath)` | the graph |
| `result<vector<asset_id>> dependency_closure(roots)` / `void collect_dependents(roots, outOrdered)` | transitive closure (uuid order); transitive dependents (depth order, roots first) |

`add_file`, `create_asset` and `move_asset` refuse a path that leaves its mount (`"assets://../x"`,
`"assets:///abs"`, `"assets://C:/abs"`) with `INVALID_ARGUMENT`, before touching any file. `create_asset` and
`move_asset` refuse the same way a path one of whose names is not portable (`"assets://rock."`,
`"assets://what?.png"`; `core::vfs::path_utils::is_portable_name`, see [Files](../core/files.md)).

Lookups take a shared lock and are safe from any thread; every mutating call is main-thread only.

An `asset_record` carries `id`, `parentId` (the primary of a sub-asset), `type` (known once imported),
`mountIndex`, `relativePath`, `subPath`, `importerId`, `displayName`, `importerVersion`, `sourceSize`,
`sourceWriteTime`, `sourceHash`, `settingsHash`, `payloadHash` and a `status`:

| `asset_record_status` | Meaning |
|---|---|
| `OK` | loadable |
| `MISSING_SOURCE` | the file is gone; a loaded payload stays loaded, new loads fail `NOT_FOUND` |
| `META_INVALID` | the sidecar does not parse (never overwritten, fix it by hand) or could not be created (read-only file or folder; the next `scan()` tries again) |
| `DUPLICATE_UUID` | another source (first in sorted path order) owns the uuid; this one is excluded |
| `IMPORT_FAILED` | the last import failed; the previous blob is kept and `import_log.txt` has the message |
| `NO_IMPORTER` | no importer for the extension |
| `CYCLIC_DEPENDENCY` | part of a dependency cycle; every member is flagged |

Asset paths are `"mount://relative/path#sub/path"`; `split_asset_path()` and `make_asset_path()` convert.

> [!WARNING]
> Copying a source together with its `.meta` creates a duplicate uuid. The database keeps the first file in
> sorted path order and flags the other `DUPLICATE_UUID`; it never rewrites a `.meta` to fix this. Delete the
> copied `.meta` so the copy gets a fresh identity.

The `.meta` is the identity. A uuid edited by hand in an existing `.meta` is refused by `scan()` (ERROR; the
record keeps its uuid), but two changes at a known path are not edits: a source re-created there without its
`.meta` gets a fresh uuid and starts over (the previous asset becomes `MISSING_SOURCE`, references to it break,
and a WARN names both uuids), and a source moved there together with its `.meta` from a place it has left keeps
its own uuid. Restore a `.meta` lost through version control before the next scan to keep its asset.

## The `.meta` sidecar

`asset_meta` (`conjure/assets/asset_meta.h`) is the parsed form of `<source>.meta`: `id`, `importerId`,
`importerVersion`, `settings` (a JSON object the importer defines, opaque to the module), `subAssets` (`subPath`
to uuid) and `unknown` (top-level keys the module does not know, preserved on write). Helpers:
`meta_path_of(sourcePath)`, `is_meta_path(path)`, `parse_asset_meta(text)`, `serialize_asset_meta(meta)`
(sorted keys, deterministic), `read_asset_meta(fs, path)`, `write_asset_meta(fs, path, meta)` (atomic) and
`canonical_settings_hash(value)`, which ignores key order and whitespace. Prefer `database.read_meta()` /
`write_meta()`: they also refresh the record, mark the source stale when its importer or settings change (even
for two importers of the same version) and refuse a uuid change.

## Adding an asset type

A type needs three things: a payload struct declared as an asset type, an **importer** (source bytes to cooked
payload, on a worker, deterministic) and a **loader** (cooked payload to published value, two stages).

```cpp
#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_loader.h"
#include "conjure/assets/asset_type_registry.h"

#include <array>
#include <cstring>
#include <memory>
#include <string>

using namespace conjure;
using namespace conjure::assets;

struct dialogue_asset {
    std::string text = {};
};

CONJURE_DECLARE_ASSET_TYPE(dialogue_asset, "mygame.dialogue");       // global scope, after the type

class dialogue_importer final : public asset_importer {
    public:
        static constexpr std::string_view ID = "mygame.dialogue";
        static constexpr std::array<std::string_view, 1> EXTENSIONS = {".dlg"};

    public:
        [[nodiscard]] std::string_view id() const noexcept override {
            return ID;
        }

        [[nodiscard]] u32 version() const noexcept override {
            return 1;                                                        // bump to re-import everything
        }

        [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override {
            return EXTENSIONS;
        }

        [[nodiscard]] core::result<import_output> import(import_context &context) const override {
            imported_asset primary = {};                                     // subPath "" = the primary
            primary.type = asset_type_traits<dialogue_asset>::id();
            primary.payload.assign(context.source_bytes().begin(), context.source_bytes().end());
            import_output output = {};
            output.assets.push_back(std::move(primary));
            return output;
        }
};

class dialogue_loader final : public typed_asset_loader<dialogue_asset, dialogue_asset> {
    protected:
        [[nodiscard]] core::result<dialogue_asset> load_cpu_typed(
          const load_context &,
          std::span<const std::byte> payloadBytes
        ) const override {                                                   // worker thread
            dialogue_asset asset = {};
            asset.text.resize(payloadBytes.size());
            std::memcpy(asset.text.data(), payloadBytes.data(), payloadBytes.size());
            return asset;
        }

        [[nodiscard]] core::result<dialogue_asset> finalise_typed(
          const finalise_context &,
          dialogue_asset &&intermediate
        ) const override {                                                   // main thread
            return std::move(intermediate);
        }
};

core::result<void> register_dialogue(asset_type_registry &types, importer_registry &importers) {
    CONJURE_TRY(types.register_type(make_asset_type_desc<dialogue_asset>(std::make_unique<dialogue_loader>())));
    return importers.register_importer(std::make_unique<dialogue_importer>());
}
```

### Importers

`asset_importer` is called on worker threads and must be deterministic: no clocks, no hash-map iteration order
in the output, no state beyond the `import_context`. It declares `id()` (recorded in every `.meta`),
`version()` (bump it and every blob it produced is stale), `extensions()` (lower case with the dot, matched
ignoring case), optional `default_settings()` (written into new `.meta` files) and `import()`, which returns an
`import_output` with exactly one `imported_asset` whose `subPath` is empty plus any sub-assets. Each
`imported_asset` has a `type`, a `displayName`, a sorted `dependencies` list and the `payload` bytes.

The context offers `source_path()`, `source_bytes()`, `settings()`, `platform()`, `primary_id()`,
`read_side_file(relativePath)` (a file next to the source; it becomes part of the source hash and is watched,
even when it is missing, so its appearance re-imports a source whose import failed for want of it),
`sub_asset_id(subPath)` (a uuid that stays the same on every import), `find_asset_by_path(path)` (the uuid of
another source, by asset path or relative to this source) and `add_warning(message)`.

`importer_registry`: `register_importer(unique_ptr)`, `unregister_importer(id)` (refused while imports run),
`find(id)`, `find_for_extension(ext)`, `importers()`.

> [!TIP]
> Declare dependencies from the importer (`find_asset_by_path` for an external texture a model cites,
> `sub_asset_id` for one it embeds). The manager then loads them before your asset finalises, hot reload
> propagates through them and the cooker ships them with it. Anything you resolve at load time instead is
> invisible to all three.

### Loaders and payloads

`asset_loader` (`conjure/assets/asset_loader.h`) runs `load_cpu(context, bytes)` on a worker (the bytes are
valid only during the call; return something that owns its data) and `finalise(context, cpuOutput)` on the main
thread (GPU upload, device buffers). `finalise_context::dependencies` holds a terminal handle per dependency in
blob order; a FAILED one reads `nullptr`, so use the manager's fallback. `create_fallback()` builds the type's
placeholder once. `typed_asset_loader<AssetType, Intermediate>` does the type erasure: override
`load_cpu_typed`, `finalise_typed` and optionally `create_fallback_typed` (`nullopt` means no fallback).

The erasure itself is `asset_payload` (`conjure/assets/asset_payload.h`): a `std::unique_ptr<void, void (*)(void *)>`
built with `make_payload(value)`, read with `payload_as<T>(payload)`, with `make_null_payload()` for "nothing".
You only meet it when writing a plain `asset_loader` by hand.

### Registering

`asset_type_registry` (`conjure/assets/asset_type_registry.h`) holds one `asset_type_desc` per type: `id`,
`name`, `version` (bump to invalidate every cooked blob of the type), `flags` and the `loader`.
`make_asset_type_desc<T>(loader, version = 1, flags = NONE)` fills it from the traits. `register_type(desc)` fails
with `TYPE_DUPLICATE` for a taken id and `OUT_OF_RANGE` past `MAX_ASSET_TYPES` (256); `unregister_type(id)` is
refused with `INVALID_STATE` while any slot of the type is loaded or in flight. `find(id)`, `find(name)`,
`find_loader(id)` and `types()` are safe from any thread between registrations.

| Flag | Effect |
|---|---|
| `RELOAD_WITH_DEPENDENCIES` | hot reload reloads instances of this type when one of their dependencies changed (materials) |
| `EXCLUDE_FROM_PACK` | never cooked into `content.cpak` (script sources) |
| `PINNED_FALLBACK` | the fallback is created at registration instead of on first use |

`binary_asset` (`conjure/assets/binary_asset.h`) is the one built-in type: `struct binary_asset { std::vector<std::byte> bytes; }`,
imported verbatim from `.bin`, `.dat`, `.bytes` and `.raw` by `binary_importer`, loaded by `binary_loader`
(fallback: an empty asset). `register_binary_asset_type(types, &importers)` registers both; pass `nullptr` for
the importers at runtime. It is also the simplest complete example of the three pieces above.

## Hot reload

`polling_file_watcher` (`conjure/assets/file_watcher.h`) stats every file under its roots on a job once per
interval and reports a change only after it has stayed the same for one whole interval, so a file written in
several steps is reported once. `hot_reload_controller` (`conjure/assets/hot_reload.h`) turns those changes into
re-imports and reloads.

```cpp
#include "conjure/assets/file_watcher.h"
#include "conjure/assets/hot_reload.h"

using namespace conjure;
using namespace conjure::assets;

class editor_reload {
    private:
        polling_file_watcher mWatcher;
        hot_reload_controller mController;

    public:
        editor_reload(
          core::vfs::file_system &files,
          core::jobs::job_system &jobs,
          asset_database &database,
          asset_manager &manager
        ) :
            mWatcher(files, jobs),                                   // DEFAULT_WATCH_INTERVAL, 500 ms
            mController(database, manager, mWatcher, jobs) {
            for (const asset_mount &mount : database.mounts()) {
                mWatcher.add_root(mount.rootPath);
            }
        }

        [[nodiscard]] core::result<void> start() {
            return mWatcher.snapshot();                              // the baseline; emits nothing
        }

        void tick(asset_manager &manager) {
            manager.update();
            mController.update();                                    // after the manager, every frame
        }
};
```

What one `update()` does: a modified source, `.meta` (settings or importer change) or side file marks its owners
dirty; a `.meta` written by the database itself (a new source's, or sub-asset uuids appended by an import) does
not; an added file is registered; a removed one becomes `MISSING_SOURCE` (its payload stays loaded) and is
restored when it comes back. Dirty sources plus slots `FAILED` with `CACHE_MISS` whose record is `OK` are
re-imported synchronously (a source whose import failed waits for a change to it, its `.meta` or a side file),
then every changed asset and every `RELOAD_WITH_DEPENDENCIES` dependent is reloaded in dependency order, each
once. `last_report()` says what happened (`changesSeen`, `sourcesReimported`, `reloaded`, `deferred`).

> [!WARNING]
> A reload swaps the pointer and bumps `version()`; unflagged dependents keep their payload and only see the new
> version. A failed re-import keeps the previous blob (record `IMPORT_FAILED`); a failed reload keeps the old
> payload and fires `RELOAD_FAILED`. An asset that changes while its slot is still loading is reloaded by a later
> `update()`, once it is `LOADED`.

`file_watcher` is an interface (`add_root`, `snapshot`, `poll(outChanges)`), so a native watcher can replace the
polling one later. `scan_now()` runs one pass synchronously for tests and tools. A pass that keeps failing (a
watched root was deleted) logs one WARN, again only when the failure changes, and one INFO when passes succeed
again.

## API summary

| Signature (short) | Purpose |
|---|---|
| `asset_database(fs, importers, types, jobs = nullptr)` | the project's sources; see the table in [The database](#the-database) |
| `struct asset_meta`, `read_asset_meta`, `write_asset_meta`, `canonical_settings_hash` | the sidecar |
| `class asset_importer`: `id`, `version`, `extensions`, `default_settings`, `import(import_context &)` | source to cooked payload |
| `class import_context`: `source_bytes`, `settings`, `read_side_file`, `sub_asset_id`, `find_asset_by_path`, `add_warning` | what an importer may touch |
| `class importer_registry`: `register_importer`, `unregister_importer`, `find`, `find_for_extension` | importers by id and extension |
| `class asset_loader` / `typed_asset_loader<T, Intermediate>`: `load_cpu[_typed]`, `finalise[_typed]`, `create_fallback[_typed]` | cooked payload to published value |
| `asset_payload`, `make_payload`, `make_null_payload`, `payload_as<T>` | the one type erasure |
| `asset_type_registry`: `register_type`, `unregister_type`, `find`, `find_loader`, `types`; `make_asset_type_desc<T>` | type registration |
| `binary_asset`, `binary_importer`, `binary_loader`, `register_binary_asset_type` | the built-in type |
| `polling_file_watcher(fs, jobs, interval)`: `add_root`, `snapshot`, `poll`, `scan_now` | debounced change detection |
| `hot_reload_controller(database, manager, watcher, jobs)`: `update`, `reloads_this_frame`, `last_report` | re-import and reload |

## See also

- 📘 API: [Assets](index.md), [Referencing and loading](referencing-and-loading.md)
- 📘 API: [Cooking, caches and packs](cooking-and-packs.md): the blob format importers produce and the cache that stores it
- 📘 API: [JSON](../core/json.md) for `settings`, [Files and the virtual file system](../core/files.md), [Glossary](../glossary.md)
- 🛠 Handbook: [Assets module](../../handbook/modules/assets.md): scan and hashing, the watcher job, determinism rules
