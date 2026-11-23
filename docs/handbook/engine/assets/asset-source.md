---
kind: header
---

[Engine](../../engine.md) → [Assets](../assets.md)

# asset_source.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/assets/asset_source.h` |
| API ↗ | [Cooking, caches and packs](../../../api/assets/cooking-and-packs.md), [Database, importers and hot reload](../../../api/assets/database-and-import.md), [Assets](../../../api/assets/index.md) |

<p><code>A</code> provider of validated cooked blobs. <code>read()</code> may be called from any thread.</p>

## Structs

| File | Description |
|---|---|
| [`memory_asset_source::entry`](asset-source/memory-asset-source-entry.md) |  |

## Classes

| File | Description |
|---|---|
| [`asset_source`](asset-source/asset-source.md) | <code>A</code> provider of validated cooked blobs. <code>read()</code> may be called from any thread. |
| [`cache_asset_source`](asset-source/cache-asset-source.md) | Editor/dev source: identities from the database, blobs from the cache. <code>A</code> record whose status prevents loading (<code>META_INVALID</code>, <code>NO_IMPORTER</code>, <code>CYCLIC_DEPENDENCY</code>, <code>MISSING_SOURCE</code>) fails <code>read()</code> with that code; a missing blob is <code>CACHE_MISS</code>. |
| [`composite_asset_source`](asset-source/composite-asset-source.md) | Resolves through several sources in insertion order (the project pack first, then plugin packs). Sources are added on the main thread before loads start. |
| [`memory_asset_source`](asset-source/memory-asset-source.md) | Blobs held in memory. Tests add blobs directly; reads copy. |
| [`pack_asset_source`](asset-source/pack-asset-source.md) | Runtime source over an opened pack. |

## Forward Declarations

### `asset_cache` {#symbol-asset-cache}

```cpp
class asset_cache;
```

### `asset_database` {#symbol-asset-database}

```cpp
class asset_database;
```

### `pack_reader` {#symbol-pack-reader}

```cpp
class pack_reader;
```

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="asset_cache"></a>
<a id="asset_database"></a>
<a id="asset_source"></a>
<a id="asset_sourceasset_source"></a>
<a id="asset_sourceasset_source_1"></a>
<a id="asset_sourcecontains"></a>
<a id="asset_sourcefind_by_path"></a>
<a id="asset_sourceoperator"></a>
<a id="asset_sourceread"></a>
<a id="asset_sourcetype_of"></a>
<a id="cache_asset_source"></a>
<a id="cache_asset_sourcecache_asset_source"></a>
<a id="cache_asset_sourcecache_asset_source_1"></a>
<a id="cache_asset_sourcecontains"></a>
<a id="cache_asset_sourcefind_by_path"></a>
<a id="cache_asset_sourcemcache"></a>
<a id="cache_asset_sourcemdatabase"></a>
<a id="cache_asset_sourceread"></a>
<a id="cache_asset_sourcetype_of"></a>
<a id="composite_asset_source"></a>
<a id="composite_asset_sourceadd"></a>
<a id="composite_asset_sourcecomposite_asset_source"></a>
<a id="composite_asset_sourcecomposite_asset_source_1"></a>
<a id="composite_asset_sourcecontains"></a>
<a id="composite_asset_sourcefind_by_path"></a>
<a id="composite_asset_sourcemsources"></a>
<a id="composite_asset_sourceread"></a>
<a id="composite_asset_sourcesize"></a>
<a id="composite_asset_sourcetype_of"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="memory_asset_source"></a>
<a id="memory_asset_sourceadd"></a>
<a id="memory_asset_sourcecontains"></a>
<a id="memory_asset_sourceentry"></a>
<a id="memory_asset_sourceentryassetpath"></a>
<a id="memory_asset_sourceentryblob"></a>
<a id="memory_asset_sourcefind_by_path"></a>
<a id="memory_asset_sourcemblobs"></a>
<a id="memory_asset_sourcememory_asset_source"></a>
<a id="memory_asset_sourcememory_asset_source_1"></a>
<a id="memory_asset_sourcemidsbypath"></a>
<a id="memory_asset_sourcemmutex"></a>
<a id="memory_asset_sourceread"></a>
<a id="memory_asset_sourceremove"></a>
<a id="memory_asset_sourcesize"></a>
<a id="memory_asset_sourcetype_of"></a>
<a id="pack_asset_source"></a>
<a id="pack_asset_sourcecontains"></a>
<a id="pack_asset_sourcefind_by_path"></a>
<a id="pack_asset_sourcemreader"></a>
<a id="pack_asset_sourcepack_asset_source"></a>
<a id="pack_asset_sourcepack_asset_source_1"></a>
<a id="pack_asset_sourceread"></a>
<a id="pack_asset_sourcetype_of"></a>
<a id="pack_reader"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
