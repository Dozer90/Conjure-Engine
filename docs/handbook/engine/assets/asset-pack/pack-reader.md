---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_pack.h](../asset-pack.md)

# pack_reader
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_pack.h` |
| Source File Path | `engine/src/assets/asset_pack.cpp` |
| API ↗ | [Cooking, caches and packs](../../../../api/assets/cooking-and-packs.md), [Assets](../../../../api/assets/index.md) |

## Syntax

```cpp
class CONJURE_API pack_reader
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">pack_reader();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">pack_reader(
    <a href="pack-reader.html">pack_reader</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~pack_reader();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mBuffer</code></td><td><code class="signature">std::vector&lt;std::byte&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Dependencies</code></td><td><code class="signature">std::vector&lt;<a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFile</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../../core/vfs/file-system/vfs-file.html">core::<wbr>vfs::<wbr>file</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mFileSize</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mHeader</code></td><td><code class="signature"><a href="pack-header.html">pack_header</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mOpen</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mStrings</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mToc</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="pack-toc-entry.html">pack_<wbr>toc_<wbr>entry</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `content_hash` {#function-content-hash}

<div class="function-declaration" id="function-content-hash-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> content_hash() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `dependencies_of` {#function-dependencies-of}

<div class="function-declaration" id="function-dependencies-of-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a>&gt; dependencies_of(
    <span class="k">const</span> <a href="pack-toc-entry.html">pack_toc_entry</a> &amp;entry) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The dependencies of an entry.</p>

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="pack-toc-entry.html">pack_toc_entry</a> *find(
    <span class="k">const</span> <a href="../asset-types.html#symbol-asset-id">asset_id</a> &amp;id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Binary search over the sorted TOC.</p>

#### `find_by_name` {#function-find-by-name}

<div class="function-declaration" id="function-find-by-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="pack-toc-entry.html">pack_toc_entry</a> *find_by_name(
    std::string_view assetPath) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The entry whose recorded asset path equals <code>assetPath</code>, or <code>nullptr</code>.</p>

#### `header` {#function-header}

<div class="function-declaration" id="function-header-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="pack-header.html">pack_header</a> &amp;header() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_open` {#function-is-open}

<div class="function-declaration" id="function-is-open-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_open() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `name_of` {#function-name-of}

<div class="function-declaration" id="function-name-of-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view name_of(
    <span class="k">const</span> <a href="pack-toc-entry.html">pack_toc_entry</a> &amp;entry) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The asset path recorded for an entry.</p>

#### `open` {#function-open}

<div class="function-declaration" id="function-open-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; open(
    <a href="../../core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> &amp;fileSystem,
    std::string_view packPath);</code></pre></div>

<p>Opens and validates a pack: magic, format version, table bounds, TOC order and the content hash of everything after the header. <code>PACK_CORRUPT</code> on any failure, <code>NOT_FOUND</code> (assets) when the file is missing.</p>

#### `open_bytes` {#function-open-bytes}

<div class="function-declaration" id="function-open-bytes-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; open_bytes(
    std::vector&lt;std::byte&gt; bytes);</code></pre></div>

<p>Parses an in-memory pack the same way; payloads are then served from the buffer.</p>

#### `read` {#function-read}

<div class="function-declaration" id="function-read-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../cooked-blob/cooked-blob.html">cooked_blob</a>&gt; read(
    <span class="k">const</span> <a href="pack-toc-entry.html">pack_toc_entry</a> &amp;entry) <span class="k">const</span>;</code></pre></div>

<p>Reads an entry&#x27;s payload and rebuilds the blob (header fields the pack does not store are zero). <code>PACK_CORRUPT</code> when the payload hash does not match.</p>

#### `toc` {#function-toc}

<div class="function-declaration" id="function-toc-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="pack-toc-entry.html">pack_toc_entry</a>&gt; toc() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

### Private

#### `parse_tables` {#function-parse-tables}

<div class="function-declaration" id="function-parse-tables-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; parse_tables();</code></pre></div>

#### `read_range` {#function-read-range}

<div class="function-declaration" id="function-read-range-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; read_range(
    <a href="../../core/types.html#symbol-u64">u64</a> offset,
    std::span&lt;std::byte&gt; out) <span class="k">const</span>;</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="pack-reader.html">pack_reader</a> &amp;<span class="k">operator</span>=(
    <a href="pack-reader.html">pack_reader</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
