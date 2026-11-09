# Assets

📘 **Public API wiki** · 🛠 Handbook: [Assets module](../../handbook/modules/assets.md)

The asset pipeline turns the files in your project (textures, models, sounds, scenes, scripts, raw data) into
things the engine can load quickly and safely, and keeps references to them stable when files move. Include
`conjure/assets/assets.h` for everything, or just the header of the piece you need; the types live in
`namespace conjure::assets`. This page explains how assets work from the outside. The pages below cover the
API in detail.

| Page | Covers |
|---|---|
| [Referencing and loading](referencing-and-loading.md) | `asset_ref<T>`, `asset_handle`, `asset_manager`, the load state machine, load batches, events |
| [Database, importers and hot reload](database-and-import.md) | `asset_database`, `.meta` sidecars, writing an importer and a loader, registering an asset type, hot reload |
| [Cooking, caches and packs](cooking-and-packs.md) | the cooked blob format, the cache, `asset_source` backends, `asset_cooker`, packs, plugin extension records |

## At a glance

| You want to... | Use | Page |
|---|---|---|
| Point a component at an asset | `asset_ref<T>` (24 bytes, copy freely) | [Referencing and loading](referencing-and-loading.md) |
| Keep an asset resident while you use it | `asset_handle` from `asset_manager::load()` | [Referencing and loading](referencing-and-loading.md) |
| Read the loaded data each frame | `ref.get()`, or `manager.get_or_fallback(ref)` | [Referencing and loading](referencing-and-loading.md) |
| Wait for a group of assets (a scene) | `asset_manager::load_batch()` and `asset_load_batch::is_complete()` | [Referencing and loading](referencing-and-loading.md) |
| Turn a uuid into a path, or back | `asset_manager::find(path)`, `asset_database::asset_path_of(id)` | [Database, importers and hot reload](database-and-import.md) |
| Add a file type of your own | an `asset_importer` plus a `typed_asset_loader` | [Database, importers and hot reload](database-and-import.md) |
| Ship the game | `asset_cooker::cook()` then `pack_asset_source` at runtime | [Cooking, caches and packs](cooking-and-packs.md) |

## How assets work

### Sources and the `.meta` sidecar

A **source** is any regular file under a mounted directory, such as `assets/models/robot.gltf`. The first time the
editor scans it, the database writes `robot.gltf.meta` next to it: a small JSON file holding the asset's
**uuid**, the importer that handles it and that importer's settings. Commit `.meta` files to version control;
they are what keeps references stable. Delete one and the asset gets a new identity, which breaks every scene
that referenced it.

A source can produce several assets. A glTF yields a model plus meshes, materials and textures; each
**sub-asset** gets its own uuid, recorded in the owner's `.meta` under a stable sub path such as `mesh/Body`.

### Identity: uuids and asset paths

Every asset is identified by a `uuid` (the alias `asset_id`), never by its path. Renaming or moving a file keeps
its uuid, so nothing that referenced it breaks. Paths exist for people and tools and always carry a mount:

```
assets://models/robot.gltf             the primary asset of a source in the project's assets directory
assets://models/robot.gltf#mesh/Body   a sub-asset of it
engine://shaders/default.vert          an engine built-in
```

`asset_manager::find("assets://...")` resolves a path to its uuid wherever the asset came from (editor cache
or shipped pack).

### Import and the cooked cache

Loading never parses a `.png` or a `.gltf`. An **importer** converts the source into one **cooked blob** per
asset (a binary payload the owning module understands, with a header that names its type, its dependencies and
the hashes of everything that went into it). In the editor the blobs live in `.conjure/cache/<platform>/blobs/`;
the database re-imports a blob only when its source bytes, side files, importer settings, importer version or
type version changed. Timestamps are never trusted on their own.

### Loading

`asset_manager` reads blobs through an `asset_source` and turns them into **payloads** in two stages: a worker
job decodes the bytes, then the main thread finalises the result (GPU upload, device buffers) under a time
budget, once every dependency the blob lists is loaded. A finished payload is **published**: from then on
`asset_ref<T>::get()` returns a pointer to it. Loading is asynchronous; `load_blocking()` exists for tools and
for the moments where a hitch is acceptable.

### Hot reload

In the editor a `file_watcher` polls the mounted directories. When a source, its `.meta` or a side file changes,
the `hot_reload_controller` re-imports it and reloads the asset through the normal pipeline; types flagged
`RELOAD_WITH_DEPENDENCIES` (materials) are reloaded after the assets they depend on. Everything else notices a
change by comparing `ref.version()`.

### Packs

Exporting cooks the closure of your scenes into one `content.cpak` plus a `manifest.json`. The runtime opens
the pack with `pack_reader` and loads from it through exactly the same manager and loaders as the editor. Two
exports of identical content are byte-identical.

## Quick example

Load one raw data file with the built-in `binary_asset` type and read it when it arrives. The manager is given
a source of cooked blobs (here a pack; the editor uses `cache_asset_source`) and is ticked once per frame.

```cpp
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_pack.h"
#include "conjure/assets/asset_source.h"
#include "conjure/assets/binary_asset.h"

#include <memory>
#include <utility>

using namespace conjure;
using namespace conjure::assets;

core::result<void> run(core::vfs::file_system &files, core::jobs::job_system &jobs) {
    pack_reader reader = {};                                   // must outlive the manager
    CONJURE_TRY(reader.open(files, "content/content.cpak"));

    asset_manager_desc desc = {};
    desc.jobs = &jobs;
    desc.source = std::make_unique<pack_asset_source>(reader);
    asset_manager manager(std::move(desc));
    CONJURE_TRY(register_binary_asset_type(manager.types()));

    const std::optional<asset_id> id = manager.find("assets://data/loot.bin");
    if (!id.has_value()) {
        return make_asset_error(asset_error_code::NOT_FOUND, "loot table missing from the pack");
    }
    const asset_ref<binary_asset> table = manager.bind<binary_asset>(*id);   // store this in a component
    asset_handle keepResident = manager.load(*id);                           // keep this while in use

    for (u64 frame = 0; table.get() == nullptr && !keepResident.is_terminal(); ++frame) {
        manager.begin_frame(frame);
        manager.update();
        manager.end_frame();
    }
    return {};
}
```

> [!IMPORTANT]
> Register every asset type (and, in the editor, every importer) before the first load. Engine modules do this
> for their own types when they start; the only type the assets module registers itself is `binary_asset`.

> [!WARNING]
> A pointer from `get()` is valid until the end of the next frame. Anything that lives longer keeps the
> `asset_ref` and calls `get()` again. An `asset_handle` keeps the payload resident; a bare `asset_ref` does not.

## Where the code lives

| You are | Include | Why |
|---|---|---|
| A gameplay programmer with a component that references an asset | `conjure/assets/asset_ref.h` | `asset_ref<T>` only; no manager needed to declare the field |
| Code that loads, unloads or reacts to assets | `conjure/assets/asset_manager.h` | brings in refs, slots, sources and the type registry |
| A module adding a file type | `conjure/assets/asset_importer.h`, `conjure/assets/asset_loader.h`, `conjure/assets/asset_type_registry.h` | importer, loader and registration |
| Editor or tool code | `conjure/assets/asset_database.h`, `conjure/assets/hot_reload.h`, `conjure/assets/asset_cooker.h` | scanning, importing, reloading, exporting |

Errors from this module are `core::error` values in the `"assets"` category (`conjure/assets/assets_error.h`):
`NOT_FOUND`, `NO_IMPORTER`, `META_INVALID`, `CACHE_MISS`, `CACHE_CORRUPT`, `LOAD_FAILED`, `FINALISE_FAILED`,
`TYPE_MISMATCH`, `CYCLIC_DEPENDENCY`, `PACK_CORRUPT`, `SHUTTING_DOWN`, `TIMEOUT` and a few more. Test them with
`error.is(asset_error_code::CACHE_MISS)`; see 📘 API: [Errors and results](../core/errors-and-results.md).

## See also

- 📘 API: [Referencing and loading](referencing-and-loading.md)
- 📘 API: [Database, importers and hot reload](database-and-import.md)
- 📘 API: [Cooking, caches and packs](cooking-and-packs.md)
- 📘 API: [Data types](../core/data-types.md) for `uuid`, [Files and the virtual file system](../core/files.md),
  [Jobs and threading](../core/jobs.md)
- 📘 API: [Glossary](../glossary.md)
- 🛠 Handbook: [Assets module](../../handbook/modules/assets.md)
