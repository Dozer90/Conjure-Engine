---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_cache.h](../asset-cache.md)

# asset_cache
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_cache.h` |
| Source File Path | `engine/src/assets/asset_cache.cpp` |
| API ↗ | [Cooking, caches and packs](../../../../api/assets/cooking-and-packs.md) |

## Syntax

```cpp
class CONJURE_API asset_cache
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">asset_cache(
    <a href="../../core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> &amp;fileSystem,
    std::string_view cacheRoot,
    std::string_view platform);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~asset_cache();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mBlobRoot</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFileSystem</code></td><td><code class="signature"><a href="../../core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPlatform</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRoot</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `blob_path` {#function-blob-path}

<div class="function-declaration" id="function-blob-path-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string blob_path(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span>;</code></pre></div>

<p>Path of the blob of a uuid: blobs/&lt;first two hex chars&gt;/&lt;uuid&gt;.cab under the root.</p>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; clear() <span class="k">const</span>;</code></pre></div>

<p>Deletes every blob of this platform.</p>

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span>;</code></pre></div>

<p>True when a blob file exists for the uuid (not validated).</p>

#### `is_valid` {#function-is-valid}

<div class="function-declaration" id="function-is-valid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">bool</span> is_valid(
    <span class="k">const</span> <a href="../cooked-blob/cooked-blob-header.html">cooked_blob_header</a> &amp;header,
    <span class="k">const</span> <a href="cache-expectation.html">cache_expectation</a> &amp;expected) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the header matches the expectation and the current blob format.</p>

#### `platform` {#function-platform}

<div class="function-declaration" id="function-platform-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view platform() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `read` {#function-read}

<div class="function-declaration" id="function-read-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../cooked-blob/cooked-blob.html">cooked_blob</a>&gt; read(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span>;</code></pre></div>

<p>Reads and fully validates a blob. <code>CACHE_MISS</code> when absent, <code>CACHE_CORRUPT</code> on any validation failure (including a header that names another uuid).</p>

#### `read_header` {#function-read-header}

<div class="function-declaration" id="function-read-header-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../cooked-blob/cooked-blob-header.html">cooked_blob_header</a>&gt; read_header(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span>;</code></pre></div>

<p>Reads and validates only the header. <code>CACHE_MISS</code> when absent, <code>CACHE_CORRUPT</code> when the header is malformed or names another uuid.</p>

#### `remove` {#function-remove}

<div class="function-declaration" id="function-remove-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; remove(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span>;</code></pre></div>

<p>Deletes a blob. Succeeds when there was none.</p>

#### `remove_temporaries` {#function-remove-temporaries}

<div class="function-declaration" id="function-remove-temporaries-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt; remove_temporaries() <span class="k">const</span>;</code></pre></div>

<p>Deletes every *.tmp left by an interrupted write. Returns how many were removed.</p>

#### `root` {#function-root}

<div class="function-declaration" id="function-root-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view root() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `vfs` {#function-vfs}

<div class="function-declaration" id="function-vfs-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> &amp;vfs() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `write` {#function-write}

<div class="function-declaration" id="function-write-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; write(
    <span class="k">const</span> <a href="../cooked-blob/cooked-blob.html">cooked_blob</a> &amp;blob) <span class="k">const</span>;</code></pre></div>

<p>Writes a blob through &lt;path&gt;.tmp + rename. <code>IO_ERROR</code> when the file system refuses.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
