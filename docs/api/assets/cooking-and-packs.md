# Cooking, caches and packs

📘 **Public API wiki** · 🛠 Handbook: [Assets module](../../handbook/modules/assets.md)

Where the bytes the manager loads come from: the cooked blob format, the editor's cache, the `asset_source`
interface that hides cache, pack and memory behind one door, the cooker that exports a pack and the records a
plugin module uses to offer importers and asset types. Headers: `conjure/assets/cooked_blob.h`,
`conjure/assets/asset_cache.h`, `conjure/assets/asset_source.h`, `conjure/assets/asset_cooker.h`,
`conjure/assets/asset_pack.h` and `conjure/assets/asset_extension_desc.h`. Everything is in
`namespace conjure::assets`.

## At a glance

| You want to... | Use |
|---|---|
| Export the game's content | `asset_cooker::cook(settings)` |
| Load from the exported pack at runtime | `pack_reader::open()` then `pack_asset_source` as the manager's source |
| Load from the editor cache | `cache_asset_source(cache, database)` |
| Feed hand-made blobs to a manager in a test | `memory_asset_source::add(blob)` |
| Search several packs (the game's, then plugins') | `composite_asset_source`, or `asset_manager::add_source()` |
| Inspect what is in a pack | `pack_reader::toc()`, `find(id)`, `name_of(entry)`, `dependencies_of(entry)` |
| Offer an importer or asset type from a plugin module | `importer_extension_desc`, `asset_type_extension_desc` |

## Quick example: cook, then load the pack

Copied from `tests/assets/test_export_roundtrip.cpp`. The cooker needs the database and the cache the editor
already has; the runtime needs only the pack file.

```cpp
#include "conjure/assets/asset_cooker.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_pack.h"
#include "conjure/assets/asset_source.h"

#include <memory>
#include <span>
#include <utility>

using namespace conjure;
using namespace conjure::assets;

core::result<cook_report> export_game(
  asset_database &database,
  core::vfs::file_system &files,
  core::jobs::job_system &jobs,
  std::span<const asset_id> sceneRoots
) {
    asset_cooker cooker(database, *database.cache(), files, jobs);
    cook_settings settings = {};
    settings.outputDirectory = "export";                        // export/content.cpak + export/manifest.json
    settings.roots.assign(sceneRoots.begin(), sceneRoots.end()); // the closure of these is shipped
    return cooker.cook(settings);                                // all or nothing
}

core::result<void> open_runtime(core::vfs::file_system &files, core::jobs::job_system &jobs, pack_reader &reader) {
    CONJURE_TRY(reader.open(files, "export/content.cpak"));     // validates the header and the content hash
    asset_manager_desc desc = {};
    desc.jobs = &jobs;
    desc.source = std::make_unique<pack_asset_source>(reader);  // the reader must outlive the manager
    asset_manager runtime(std::move(desc));
    // register types, then load exactly as in the editor
    return {};
}
```

> [!WARNING]
> `pack_asset_source` holds a pointer to the `pack_reader` and `cache_asset_source` holds pointers to the
> `asset_cache` and the `asset_database`; they own none of them. Keep those objects alive for as long as the
> manager that uses the source, and construct them first.

## The cooked blob

A cooked blob (`conjure/assets/cooked_blob.h`) is what an importer produces, the cache stores and a loader
consumes: an 80-byte little-endian `cooked_blob_header` (magic `CJAB`, `formatVersion`, `type`, `typeVersion`,
`importerVersion`, `sourceHash`, `settingsHash`, `payloadHash`, `payloadSize`, `dependencyCount`, `flags`,
`id`), then the dependency uuids, then the payload at the next 16-byte boundary. `cooked_blob` holds the three
parts and owns its bytes.

| Signature (short) | Purpose |
|---|---|
| `cooked_blob make_cooked_blob(header, dependencies, payload)` | fills `payloadHash`, `payloadSize` and `dependencyCount` from the parts |
| `vector<byte> write_cooked_blob(const cooked_blob &)` | serialise (derived fields recomputed first) |
| `result<cooked_blob_header> parse_cooked_blob_header(bytes)` | header only; `CACHE_CORRUPT` on bad magic or format |
| `result<cooked_blob> parse_cooked_blob(bytes)` | header, sizes against the buffer and the payload hash; anything wrong is `CACHE_CORRUPT` |
| `u64 hash_bytes(bytes)` | the 64-bit content hash used everywhere in the module |
| `header.payload_offset()` / `header.total_size()` | layout helpers for a consistent header |

The payload layout belongs to the module that owns the type; the convention is a fixed header struct followed
by raw arrays. You only build blobs by hand in tests (`memory_asset_source`); importers return payload bytes
and the database writes the blob.

## The cache

`asset_cache` (`conjure/assets/asset_cache.h`) stores one blob per asset under
`<cacheRoot>/<platform>/blobs/<xx>/<uuid>.cab` (`xx` = the first two hex characters of the uuid). Every write
goes to a `.tmp` sibling and is renamed into place; every read validates the whole blob. The database owns the
cache (`database.cache()`); tools read it directly.

| Signature (short) | Purpose |
|---|---|
| `asset_cache(fs, cacheRoot, platform)` / `root()` / `platform()` / `blob_path(id)` | layout |
| `bool contains(id)` | a file exists (not validated) |
| `result<cooked_blob_header> read_header(id)` / `result<cooked_blob> read(id)` | `CACHE_MISS` when absent, `CACHE_CORRUPT` on any validation failure, including a header that names another uuid |
| `result<void> write(blob)` | atomic |
| `static bool is_valid(header, cache_expectation)` | the staleness rule: `type`, `typeVersion`, `importerVersion`, `sourceHash`, `settingsHash` and the current format version must all match |
| `result<void> remove(id)` / `result<u32> remove_temporaries()` / `result<void> clear()` | maintenance (`clear()` empties this platform only) |

> [!NOTE]
> The cache is disposable. Delete `.conjure/cache` and the next `scan()` re-imports everything; nothing in it
> is committed to version control. Each platform has its own blob directory, so a Windows and a Linux build of
> the editor can share a checkout.

## Sources

`asset_source` (`conjure/assets/asset_source.h`) is the interface the load pipeline reads blobs through, so the
editor, the runtime, plugin packs and tests all run the same code: `contains(id)`, `type_of(id)`,
`find_by_path(assetPath)` and `read(id)`, which returns a validated blob or `NOT_FOUND`, `CACHE_MISS`,
`CACHE_CORRUPT` or `PACK_CORRUPT`. `read()` may be called from any thread.

| Source | Backed by | Use |
|---|---|---|
| `cache_asset_source(cache, database)` | the editor cache and database | editor and development builds; a record whose status forbids loading (`META_INVALID`, `NO_IMPORTER`, `CYCLIC_DEPENDENCY`, `MISSING_SOURCE`) fails `read()` with that code |
| `pack_asset_source(reader)` | an opened `pack_reader` | the shipped game |
| `memory_asset_source` | blobs added with `add(blob, assetPath)`; `remove(id)`, `size()` | tests and tools |
| `composite_asset_source` | other sources, searched in the order added with `add(source)` | the project pack first, then plugin packs |

`asset_manager::swap_source()` replaces the primary source (the editor switches from cache to a pack preview
this way; loaded payloads stay, and a read in flight finishes on the old source first) and `add_source()`
appends a secondary one searched after the primary.

## The cooker

`asset_cooker` (`conjure/assets/asset_cooker.h`) produces the export pack from the database and the cache.
`cook(settings)` scans, computes the closure (the `roots`, plus `alwaysInclude`, plus every `engine://` asset;
or every record when `includeAll` is set), drops types flagged `EXCLUDE_FROM_PACK`, imports whatever is stale,
re-verifies every blob and writes `content.cpak` (and `manifest.json` unless `writeManifest` is false) into
`outputDirectory`. It is **all or nothing**: if any member fails to import, the call fails with `IMPORT_FAILED`,
nothing is written and `last_report().failures` lists every failing asset with its path and message.

| `cook_settings` | Meaning |
|---|---|
| `outputDirectory` | where the two files go (required) |
| `platform` | informational; the database's cache platform is what gets cooked |
| `roots` / `alwaysInclude` | the assets to ship and the assets to ship even if nothing references them |
| `includeAll` | ship every record instead of a closure |
| `writeManifest` | also write `manifest.json` (default true) |

`cook_report`: `assetCount`, `packBytes`, `duplicatesRemoved`, `contentHash`, `failures` (`cook_failure { id,
assetPath, message }`) and `warnings`.

> [!TIP]
> Cook from the scenes you ship, not with `includeAll`. The closure follows the dependency lists importers
> declared, so unreferenced test content stays out of the pack, and two cooks of the same content are
> byte-identical (no timestamps, no absolute paths), which makes exports diffable.

## Packs

`content.cpak` (`conjure/assets/asset_pack.h`) is a 64-byte `pack_header` (magic `CPAK`, `formatVersion`,
`entryCount`, the offsets of the TOC, dependency table, string table and blobs, and a `contentHash` of every
byte after the header), a TOC of 72-byte `pack_toc_entry` records sorted by uuid, the dependency table, a string
table of asset paths and the payloads, 16-byte aligned with identical payloads stored once. The runtime never
needs `manifest.json`; it is a JSON array of `{uuid, type, name, size, hash, dependencies[]}` for tools.

| Signature (short) | Purpose |
|---|---|
| `pack_writer::add(blob, assetPath)` | re-verifies the payload hash; `ALREADY_EXISTS` for a repeated uuid |
| `vector<byte> build(pack_stats &)` / `result<pack_stats> write(fs, packPath)` | serialise (sort, dedup); write atomically |
| `string build_manifest()` / `result<void> write_manifest(fs, path)` | the manifest |
| `pack_reader::open(fs, packPath)` / `open_bytes(bytes)` / `is_open()` | validates magic, version, table bounds, TOC order and the content hash; `PACK_CORRUPT` on any failure |
| `const pack_toc_entry *find(id)` / `find_by_name(assetPath)` | binary search over the TOC; by recorded path |
| `result<cooked_blob> read(entry)` | positional read of one payload, hash checked |
| `toc()` / `dependencies_of(entry)` / `name_of(entry)` / `content_hash()` / `header()` | inspection |

> [!CAUTION]
> A pack that fails validation at `open()` is refused as a whole (`PACK_CORRUPT`); the runtime does not start
> on a partially written or modified pack. Always let `pack_writer::write()` or the cooker produce the file; both
> write to a temporary and rename.

## Extension records for plugin modules

A plugin module cannot call `register_type()` itself at load time; it publishes trivially copyable records
(`conjure/assets/asset_extension_desc.h`) under the extension points `"assets.importer"` and `"assets.type"`,
and the engine's assets consumer instantiates and registers them on attach. Copied from
`tests/assets/test_asset_extension_desc.cpp`:

```cpp
#include "conjure/assets/asset_extension_desc.h"
#include "conjure/assets/binary_asset.h"

#include <memory>

using namespace conjure;
using namespace conjure::assets;

// A module offering the built-in binary type would publish these two records; your own importer
// and loader (see the previous page) slot in the same way.
std::unique_ptr<asset_importer> create_binary_importer() {
    return std::make_unique<binary_importer>();
}

std::unique_ptr<asset_loader> create_binary_loader() {
    return std::make_unique<binary_loader>();
}

constexpr importer_extension_desc BINARY_IMPORTER = {"conjure.assets.binary", &create_binary_importer};
constexpr asset_type_extension_desc BINARY_TYPE = {
  "conjure.assets.binary", binary_importer::VERSION, asset_type_flags::NONE, &create_binary_loader
};

core::result<void> attach(asset_type_registry &types, importer_registry &importers) {
    CONJURE_TRY(register_asset_type_extension(types, BINARY_TYPE));       // engine plumbing calls these
    return register_importer_extension(importers, BINARY_IMPORTER);
}
```

`importer_extension_desc { const char *id; unique_ptr<asset_importer> (*create)(); }` and
`asset_type_extension_desc { const char *name; u32 version; asset_type_flags flags; unique_ptr<asset_loader> (*createLoader)(); }`.
Registration fails with `INVALID_ARGUMENT` when the factory is missing, returns nothing or produces an importer
or loader whose id or type does not match the record. The module system chapter explains how records reach the
engine: 🛠 Handbook: [Module system](../../handbook/module-system/index.md).

## API summary

| Signature (short) | Purpose |
|---|---|
| `cooked_blob_header`, `cooked_blob`, `make_cooked_blob`, `write_cooked_blob`, `parse_cooked_blob[_header]`, `hash_bytes` | the cooked format |
| `asset_cache(fs, cacheRoot, platform)`: `read`, `read_header`, `write`, `contains`, `is_valid`, `remove`, `remove_temporaries`, `clear` | blob storage per platform |
| `asset_source`: `contains`, `type_of`, `find_by_path`, `read`; `memory_asset_source`, `cache_asset_source`, `pack_asset_source`, `composite_asset_source` | blob providers |
| `asset_cooker(database, cache, fs, jobs)`: `result<cook_report> cook(const cook_settings &)`, `last_report()` | export |
| `pack_writer`: `add`, `build`, `write`, `build_manifest`, `write_manifest`; `pack_reader`: `open`, `open_bytes`, `find`, `find_by_name`, `read`, `toc` | the pack |
| `register_importer_extension(importers, desc)` / `register_asset_type_extension(types, desc)` | plugin records |

## See also

- 📘 API: [Assets](index.md), [Referencing and loading](referencing-and-loading.md)
- 📘 API: [Database, importers and hot reload](database-and-import.md): the importers that produce blobs and the database that decides what is stale
- 📘 API: [Files and the virtual file system](../core/files.md), [Glossary](../glossary.md)
- 🛠 Handbook: [Assets module](../../handbook/modules/assets.md): cache validation, cooker determinism, pack layout
- 🛠 Handbook: [Module system](../../handbook/module-system/index.md): how extension records are collected
