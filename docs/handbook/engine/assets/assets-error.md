---
kind: header
---

[Engine](../../engine.md) → [Assets](../assets.md)

# assets_error.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/assets/assets_error.h` |
| API ↗ | [Assets](../../../api/assets/index.md) |

<p>The assets error category. Every recoverable failure of the module is reported through <code><a href="../core/result/result.html">core::result</a></code> with one of these codes; the generic core codes are used only where the failure is not asset-specific (an invalid argument, a registry overflow).</p>

## Enumerations

### `asset_error_code` {#symbol-asset-error-code}

```cpp
enum class asset_error_code : u16 {
    NONE = 0,
    NOT_FOUND,              // unknown uuid, or the source of a record is gone
    NO_IMPORTER,            // no importer registered for the source's extension
    NO_LOADER,              // no loader registered for the blob's type
    META_INVALID,           // the .meta sidecar does not parse or lacks required fields
    DUPLICATE_UUID,         // two .meta files declare the same uuid
    IO_ERROR,               // the file system refused a read or write
    IMPORT_FAILED,          // the importer reported an error or produced invalid output
    CACHE_MISS,             // no cooked blob for the uuid at load time
    CACHE_CORRUPT,          // the cooked blob failed validation
    LOAD_FAILED,            // load_cpu failed
    FINALISE_FAILED,        // finalise failed
    TYPE_MISMATCH,          // bound type differs from the recorded or cooked type
    TYPE_UNKNOWN,           // the type id is not registered
    TYPE_DUPLICATE,         // register_type with an id that is taken
    CYCLIC_DEPENDENCY,      // the dependency graph contains a cycle through this asset
    PACK_CORRUPT,           // the pack header, TOC or content hash failed validation
    UNBOUND,                // the operation needs a bound slot
    SHUTTING_DOWN,          // the manager refuses new work after shutdown()
    TIMEOUT,                // load_blocking exceeded its timeout
    COUNT
};
```

Failure conditions of the asset pipeline.

## Macros

### `CONJURE_DECLARE_ERROR_CATEGORY` {#symbol-conjure-declare-error-category}

```cpp
CONJURE_DECLARE_ERROR_CATEGORY(conjure::assets::asset_error_code, "assets", conjure::assets::asset_error_code_name)
```

## Functions

### Public

#### `asset_error_code_name` {#function-asset-error-code-name}

<div class="function-declaration" id="function-asset-error-code-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view asset_error_code_name(
    <a href="assets-error.html#symbol-asset-error-code">asset_error_code</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>Stable upper-case name of a code (&quot;<code>CACHE_MISS</code>&quot;); &quot;&quot; for values outside the enumeration.</p>

#### `make_asset_error` {#function-make-asset-error}

<div class="function-declaration" id="function-make-asset-error-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/error/error.html">core::error</a> make_asset_error(
    <a href="assets-error.html#symbol-asset-error-code">asset_error_code</a> code,
    std::string message = {});</code></pre></div>

<p>Builds an error in the assets category.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="asset_error_code"></a>
<a id="asset_error_code_name"></a>
<a id="asset_error_codecache_corrupt"></a>
<a id="asset_error_codecache_miss"></a>
<a id="asset_error_codecount"></a>
<a id="asset_error_codecyclic_dependency"></a>
<a id="asset_error_codeduplicate_uuid"></a>
<a id="asset_error_codefinalise_failed"></a>
<a id="asset_error_codeimport_failed"></a>
<a id="asset_error_codeio_error"></a>
<a id="asset_error_codeload_failed"></a>
<a id="asset_error_codemeta_invalid"></a>
<a id="asset_error_codeno_importer"></a>
<a id="asset_error_codeno_loader"></a>
<a id="asset_error_codenone"></a>
<a id="asset_error_codenot_found"></a>
<a id="asset_error_codepack_corrupt"></a>
<a id="asset_error_codeshutting_down"></a>
<a id="asset_error_codetimeout"></a>
<a id="asset_error_codetype_duplicate"></a>
<a id="asset_error_codetype_mismatch"></a>
<a id="asset_error_codetype_unknown"></a>
<a id="asset_error_codeunbound"></a>
<a id="classes"></a>
<a id="conjure_declare_error_category"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="make_asset_error"></a>
<a id="other-declarations"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
