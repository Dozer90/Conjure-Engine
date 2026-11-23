---
kind: header
---

[Engine](../../engine.md) → [Assets](../assets.md)

# asset_meta.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/assets/asset_meta.h` |
| API ↗ | [Database, importers and hot reload](../../../api/assets/database-and-import.md) |

<p>The parsed sidecar. settings is opaque to the module; unknown top-level keys survive a read/write round trip in <code>unknown</code>.</p>

## Structs

| File | Description |
|---|---|
| [`asset_meta`](asset-meta/asset-meta.md) | The parsed sidecar. settings is opaque to the module; unknown top-level keys survive a read/write round trip in <code>unknown</code>. |
| [`sub_asset_entry`](asset-meta/sub-asset-entry.md) | One sub-asset uuid, keyed by the stable sub path the importer uses (&quot;mesh/Body&quot;). |

## Functions

### Public

#### `canonical_settings_hash` {#function-canonical-settings-hash}

<div class="function-declaration" id="function-canonical-settings-hash-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-u64">u64</a> canonical_settings_hash(
    <span class="k">const</span> <a href="../core/json/value/json-value.html">core::json::value</a> &amp;settings);</code></pre></div>

<p>Hash of the canonical form of a settings value (sorted keys, minified): independent of key order and whitespace. An invalid value hashes like an empty object.</p>

#### `is_meta_path` {#function-is-meta-path}

<div class="function-declaration" id="function-is-meta-path-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <span class="k">bool</span> is_meta_path(
    std::string_view path) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the path ends in &quot;.meta&quot;.</p>

#### `meta_path_of` {#function-meta-path-of}

<div class="function-declaration" id="function-meta-path-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string meta_path_of(
    std::string_view sourcePath);</code></pre></div>

<p>Path of the sidecar of a source file: &quot;&lt;source&gt;.meta&quot;.</p>

#### `parse_asset_meta` {#function-parse-asset-meta}

<div class="function-declaration" id="function-parse-asset-meta-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="asset-meta/asset-meta.html">asset_meta</a>&gt; parse_asset_meta(
    std::string_view text);</code></pre></div>

<p>Parses sidecar text. <code>META_INVALID</code> for malformed JSON, a non-object root, a missing or malformed uuid, an unsupported schema or a malformed sub-asset table.</p>

#### `read_asset_meta` {#function-read-asset-meta}

<div class="function-declaration" id="function-read-asset-meta-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="asset-meta/asset-meta.html">asset_meta</a>&gt; read_asset_meta(
    <a href="../core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> &amp;fileSystem,
    std::string_view metaPath);</code></pre></div>

<p>Reads and parses the sidecar at <code>metaPath</code>. <code>NOT_FOUND</code> (assets category) when absent, <code>IO_ERROR</code> for other read failures, <code>META_INVALID</code> for bad content.</p>

#### `serialize_asset_meta` {#function-serialize-asset-meta}

<div class="function-declaration" id="function-serialize-asset-meta-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string serialize_asset_meta(
    <span class="k">const</span> <a href="asset-meta/asset-meta.html">asset_meta</a> &amp;meta);</code></pre></div>

<p>Serialises a sidecar: sorted keys, sub-assets sorted by sub path, pretty printed, newline terminated. Deterministic for the same content.</p>

#### `write_asset_meta` {#function-write-asset-meta}

<div class="function-declaration" id="function-write-asset-meta-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; write_asset_meta(
    <a href="../core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> &amp;fileSystem,
    std::string_view metaPath,
    <span class="k">const</span> <a href="asset-meta/asset-meta.html">asset_meta</a> &amp;meta);</code></pre></div>

<p>Writes the sidecar atomically (tmp + rename). <code>IO_ERROR</code> when the file system refuses.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="asset_meta"></a>
<a id="asset_metafind_sub_asset"></a>
<a id="asset_metaid"></a>
<a id="asset_metaimporterid"></a>
<a id="asset_metaimporterversion"></a>
<a id="asset_metasettings"></a>
<a id="asset_metasubassets"></a>
<a id="asset_metaunknown"></a>
<a id="canonical_settings_hash"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="is_meta_path"></a>
<a id="meta_path_of"></a>
<a id="parse_asset_meta"></a>
<a id="read_asset_meta"></a>
<a id="serialize_asset_meta"></a>
<a id="structs-and-types"></a>
<a id="sub_asset_entry"></a>
<a id="sub_asset_entryid"></a>
<a id="sub_asset_entryoperator"></a>
<a id="sub_asset_entrysubpath"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="write_asset_meta"></a>
<!-- doc-legacy-anchors:end -->
