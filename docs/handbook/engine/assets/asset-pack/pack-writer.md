---
kind: class
---

[Engine](../../../engine.md) → [Assets](../../assets.md) → [asset_pack.h](../asset-pack.md)

# pack_writer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::assets` |
| Include File Path | `engine/include/conjure/assets/asset_pack.h` |
| Source File Path | `engine/src/assets/asset_pack.cpp` |
| API ↗ | [Cooking, caches and packs](../../../../api/assets/cooking-and-packs.md), [Assets](../../../../api/assets/index.md) |

## Syntax

```cpp
class CONJURE_API pack_writer
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">pack_writer();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">pack_writer(
    <a href="pack-writer.html">pack_writer</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~pack_writer();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mEntries</code></td><td><code class="signature">std::vector&lt;<a href="pack-writer-entry.html">entry</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `add` {#function-add}

<div class="function-declaration" id="function-add-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; add(
    <a href="../cooked-blob/cooked-blob.html">cooked_blob</a> blob,
    std::string_view assetPath);</code></pre></div>

<p>Adds a blob (re-verifying its payload hash) with the asset path recorded in the string table. <code>CACHE_CORRUPT</code> when the hash does not match, <code>ALREADY_EXISTS</code> for a uuid already added.</p>

#### `build` {#function-build}

<div class="function-declaration" id="function-build-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;std::byte&gt; build(
    <a href="pack-stats.html">pack_stats</a> &amp;outStats) <span class="k">const</span>;</code></pre></div>

<p>Serialises the pack: sorts by uuid, deduplicates payloads by (hash, size, bytes).</p>

#### `build_manifest` {#function-build-manifest}

<div class="function-declaration" id="function-build-manifest-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string build_manifest() <span class="k">const</span>;</code></pre></div>

<p>The manifest: a JSON array of {uuid, type, name, size, hash, dependencies[]} sorted by uuid, canonical form, newline terminated.</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `write` {#function-write}

<div class="function-declaration" id="function-write-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="pack-stats.html">pack_stats</a>&gt; write(
    <a href="../../core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> &amp;fileSystem,
    std::string_view packPath) <span class="k">const</span>;</code></pre></div>

<p>Writes the pack through tmp + rename.</p>

#### `write_manifest` {#function-write-manifest}

<div class="function-declaration" id="function-write-manifest-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; write_manifest(
    <a href="../../core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> &amp;fileSystem,
    std::string_view manifestPath) <span class="k">const</span>;</code></pre></div>

<p>Writes <code><a href="#function-build-manifest-1">build_manifest</a>()</code> through tmp + rename.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="pack-writer.html">pack_writer</a> &amp;<span class="k">operator</span>=(
    <a href="pack-writer.html">pack_writer</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
