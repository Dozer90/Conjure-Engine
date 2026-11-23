---
kind: header
---

[Engine](../../engine.md) → [Assets](../assets.md)

# asset_types.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/assets/asset_types.h` |
| API ↗ | [Referencing and loading assets](../../../api/assets/referencing-and-loading.md) |

<p>Identity, states and constants shared by every asset header.</p>

## Enumerations

### `asset_load_priority` {#symbol-asset-load-priority}

```cpp
enum class asset_load_priority : u8 {
    LOW = 0,
    NORMAL,
    HIGH
};
```

Dispatch order of queued loads; FIFO within a level.

### `asset_state` {#symbol-asset-state}

```cpp
enum class asset_state : u8 {
    UNLOADED = 0,
    QUEUED,
    LOADING,
    FINALISING,
    LOADED,
    RELOADING,
    FAILED
};
```

Lifecycle of a slot. <code>LOADED</code> and <code>FAILED</code> are terminal for dependency gating.

### `asset_type_flags` {#symbol-asset-type-flags}

```cpp
enum class asset_type_flags : u32 {
    NONE = 0,
    RELOAD_WITH_DEPENDENCIES = 1 << 0,   // hot reload reloads instances when a dependency changed
    EXCLUDE_FROM_PACK = 1 << 1,          // never cooked into content.cpak (script sources)
    PINNED_FALLBACK = 1 << 2             // the fallback is created at registration, not lazily
};
```

Per-type behaviour switches set at registration.

### `asset_type_id` {#symbol-asset-type-id}

```cpp
enum class asset_type_id : u64 {
    INVALID = 0
};
```

Identity of an asset type: <code>make_asset_type_id(&quot;conjure.renderer.texture&quot;)</code>. Stored in blob headers and pack tables, so the value never depends on compiler or platform.

## Type Aliases

### `asset_id` {#symbol-asset-id}

```cpp
using asset_id = core::uuid;
```

Stable identity of an asset or sub-asset. <code><a href="asset-types.html#symbol-asset-id">asset_id</a></code>{} (the <code>null</code> uuid) means &quot;no asset&quot;.

## Concepts

### `declared_asset_type` {#symbol-declared-asset-type}

```cpp
template <typename AssetType> concept declared_asset_type = requires { { asset_type_traits<AssetType>::NAME } -> std::convertible_to<std::string_view>; { asset_type_traits<AssetType>::id() } -> std::convertible_to<asset_type_id>; };
```

True when AssetType was declared with <code><a href="binary-asset.html#symbol-conjure-declare-asset-type">CONJURE_DECLARE_ASSET_TYPE</a></code>.

## Forward Declarations

### `asset_type_traits` {#symbol-asset-type-traits}

```cpp
template <typename AssetType> struct asset_type_traits;
```

Compile-time description of an asset type, specialised through <code><a href="binary-asset.html#symbol-conjure-declare-asset-type">CONJURE_DECLARE_ASSET_TYPE</a></code>. Provides <code>static constexpr std::string_view NAME</code> and <code>static constexpr <a href="asset-types.html#symbol-asset-type-id">asset_type_id</a> id()</code>.

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-ASSET_LOAD_PRIORITY_COUNT"></a><code>ASSET_LOAD_PRIORITY_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-usize">usize</a></code> | <code class="signature"><span class="mi">3</span></code> | Number of priority levels. |
| <a id="variable-COOKED_BLOB_FORMAT_VERSION"></a><code>COOKED_BLOB_FORMAT_VERSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">1</span></code> | Format versions written into blobs, packs and .meta files. Bumping one invalidates every artefact of that kind. |
| <a id="variable-DEFAULT_MAX_JOBS_IN_FLIGHT"></a><code>DEFAULT_MAX_JOBS_IN_FLIGHT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">8</span></code> | Default number of load jobs the manager keeps in flight. |
| <a id="variable-DEFAULT_WATCH_INTERVAL_SECONDS"></a><code>DEFAULT_WATCH_INTERVAL_SECONDS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f64">f64</a></code> | <code class="signature"><span class="mi">0.5</span></code> | Default interval of the polling file watcher, in seconds. |
| <a id="variable-detail::FNV1A64_OFFSET_BASIS"></a><code>detail::FNV1A64_OFFSET_BASIS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u64">u64</a></code> | <code class="signature"><span class="mi">0</span>xCBF29CE484222325ull</code> |  |
| <a id="variable-detail::FNV1A64_PRIME"></a><code>detail::FNV1A64_PRIME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u64">u64</a></code> | <code class="signature"><span class="mi">0</span>x00000100000001B3ull</code> |  |
| <a id="variable-MAX_ASSET_TYPES"></a><code>MAX_ASSET_TYPES</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-usize">usize</a></code> | <code class="signature"><span class="mi">256</span></code> | Upper bound on registered asset types. |
| <a id="variable-META_SCHEMA_VERSION"></a><code>META_SCHEMA_VERSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">1</span></code> |  |
| <a id="variable-PACK_FORMAT_VERSION"></a><code>PACK_FORMAT_VERSION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">1</span></code> |  |
| <a id="variable-RETIRE_FRAMES"></a><code>RETIRE_FRAMES</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u64">u64</a></code> | <code class="signature"><span class="mi">2</span></code> | Frames a retired payload stays alive after the publish that replaced it. |

## Macros

### `CONJURE_ENUM_FLAGS` {#symbol-conjure-enum-flags}

```cpp
CONJURE_ENUM_FLAGS(asset_type_flags)
```

## Functions

### Public

#### `detail::fnv1a64` {#function-detail-fnv1a64}

<div class="function-declaration" id="function-detail-fnv1a64-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../core/types.html#symbol-u64">u64</a> fnv1a64(std::string_view text) <span class="k">noexcept</span>;</code></pre></div>

<p>64-bit FNV-1a of the bytes of a string, usable in constant expressions.</p>

#### `is_terminal` {#function-is-terminal}

<div class="function-declaration" id="function-is-terminal-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_terminal(
    <a href="asset-types.html#symbol-asset-state">asset_state</a> state) <span class="k">noexcept</span>;</code></pre></div>

<p>True for <code>LOADED</code> and <code>FAILED</code>: the states a dependent may finalise against.</p>

#### `make_asset_type_id` {#function-make-asset-type-id}

<div class="function-declaration" id="function-make-asset-type-id-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="asset-types.html#symbol-asset-type-id">asset_type_id</a> make_asset_type_id(
    std::string_view name) <span class="k">noexcept</span>;</code></pre></div>

<p>Hashes a type name into its id. An empty name yields <code>INVALID</code>.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="asset-types.html#symbol-asset-state">asset_state</a> state) <span class="k">noexcept</span>;</code></pre></div>

<p>Upper-case name of a state (&quot;<code>LOADING</code>&quot;).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="asset_id"></a>
<a id="asset_load_priority"></a>
<a id="asset_load_priority_count"></a>
<a id="asset_load_priorityhigh"></a>
<a id="asset_load_prioritylow"></a>
<a id="asset_load_prioritynormal"></a>
<a id="asset_state"></a>
<a id="asset_statefailed"></a>
<a id="asset_statefinalising"></a>
<a id="asset_stateloaded"></a>
<a id="asset_stateloading"></a>
<a id="asset_statequeued"></a>
<a id="asset_statereloading"></a>
<a id="asset_stateunloaded"></a>
<a id="asset_type_flags"></a>
<a id="asset_type_flagsexclude_from_pack"></a>
<a id="asset_type_flagsnone"></a>
<a id="asset_type_flagspinned_fallback"></a>
<a id="asset_type_flagsreload_with_dependencies"></a>
<a id="asset_type_id"></a>
<a id="asset_type_idinvalid"></a>
<a id="asset_type_traits"></a>
<a id="classes"></a>
<a id="conjure_enum_flags"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="cooked_blob_format_version"></a>
<a id="declaration-summaries"></a>
<a id="declared_asset_type"></a>
<a id="default_max_jobs_in_flight"></a>
<a id="default_watch_interval"></a>
<a id="dependencies"></a>
<a id="detailfnv1a64"></a>
<a id="detailfnv1a64_offset_basis"></a>
<a id="detailfnv1a64_prime"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="fnv1a64"></a>
<a id="fnv1a64_offset_basis"></a>
<a id="fnv1a64_prime"></a>
<a id="function-default-watch-interval"></a>
<a id="function-default-watch-interval-1"></a>
<a id="internals-and-algorithms"></a>
<a id="is_terminal"></a>
<a id="make_asset_type_id"></a>
<a id="max_asset_types"></a>
<a id="meta_schema_version"></a>
<a id="other-declarations"></a>
<a id="pack_format_version"></a>
<a id="retire_frames"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
