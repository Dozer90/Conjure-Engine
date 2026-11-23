---
kind: header
---

[Engine](../../engine.md) → [Assets](../assets.md)

# asset_cache.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/assets/asset_cache.h` |
| API ↗ | [Cooking, caches and packs](../../../api/assets/cooking-and-packs.md) |

<p>Blob storage for one platform. Safe to call from any thread: the file system does the synchronisation, and every write is atomic.</p>

## Structs

| File | Description |
|---|---|
| [`cache_expectation`](asset-cache/cache-expectation.md) | What a blob must match to be considered up to date. |

## Classes

| File | Description |
|---|---|
| [`asset_cache`](asset-cache/asset-cache.md) | Blob storage for one platform. Safe to call from any thread: the file system does the synchronisation, and every write is atomic. |

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
<a id="asset_cacheasset_cache"></a>
<a id="asset_cacheasset_cache_1"></a>
<a id="asset_cacheblob_path"></a>
<a id="asset_cacheclear"></a>
<a id="asset_cachecontains"></a>
<a id="asset_cacheis_valid"></a>
<a id="asset_cachemblobroot"></a>
<a id="asset_cachemfilesystem"></a>
<a id="asset_cachemplatform"></a>
<a id="asset_cachemroot"></a>
<a id="asset_cacheoperator"></a>
<a id="asset_cacheplatform"></a>
<a id="asset_cacheread"></a>
<a id="asset_cacheread_header"></a>
<a id="asset_cacheremove"></a>
<a id="asset_cacheremove_temporaries"></a>
<a id="asset_cacheroot"></a>
<a id="asset_cachevfs"></a>
<a id="asset_cachewrite"></a>
<a id="cache_expectation"></a>
<a id="cache_expectationimporterversion"></a>
<a id="cache_expectationsettingshash"></a>
<a id="cache_expectationsourcehash"></a>
<a id="cache_expectationtype"></a>
<a id="cache_expectationtypeversion"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
