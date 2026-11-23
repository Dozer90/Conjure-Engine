---
kind: class
---

[Engine](../../../engine.md) → [RHI](../../rhi.md) → [shader_library.h](../shader-library.md)

# shader_library
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::rhi` |
| Include File Path | `engine/include/conjure/rhi/shader_library.h` |
| Source File Path | `engine/src/rhi/shader_library.cpp` |
| API ↗ | [Shaders](../../../../api/rhi/shaders.md) |

## Syntax

```cpp
class CONJURE_API shader_library
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> shader_library(
    create_tag);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~shader_library();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mImpl</code></td><td><code class="signature">std::unique_ptr&lt;<a href="../shader-library.html">impl</a>&gt;</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `backend` {#function-backend}

<div class="function-declaration" id="function-backend-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../backend.html#symbol-graphics-backend">graphics_backend</a> backend() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The backend whose blob column was selected.</p>

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <span class="k">const</span> <a href="shader-key.html">shader_key</a> &amp;key) <span class="k">const</span>;</code></pre></div>

<p>True when an entry with a blob for this backend matches the key (no file access; the lookup key is composed in a heap string, so the call may allocate).</p>

#### `directory` {#function-directory}

<div class="function-declaration" id="function-directory-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> std::filesystem::path &amp;directory() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The directory <code>index.json</code> was opened from.</p>

#### `entry_count` {#function-entry-count}

<div class="function-declaration" id="function-entry-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> entry_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of entries in the index, including entries without a blob for this backend.</p>

#### `get` {#function-get}

<div class="function-declaration" id="function-get-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="shader-blob.html">shader_blob</a>&gt; get(
    <span class="k">const</span> <a href="shader-key.html">shader_key</a> &amp;key);</code></pre></div>

<p>Returns the compiled blob, loading it lazily. Fails with <code>core::error_code::NOT_FOUND</code> when no entry matches the key or the entry has no blob for this backend, and <code>IO_FAILURE</code> when the blob file cannot be read or is empty. <code>A</code> returned blob is never empty.</p>

#### `loaded_blob_count` {#function-loaded-blob-count}

<div class="function-declaration" id="function-loaded-blob-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> loaded_blob_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of blobs currently loaded in memory.</p>

#### `open` {#function-open}

<div class="function-declaration" id="function-open-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;std::unique_ptr&lt;<a href="shader-library.html">shader_library</a>&gt;&gt; open(
    <a href="../device/device.html">device</a> &amp;device,
    <span class="k">const</span> std::filesystem::path &amp;shaderDirectory);</code></pre></div>

<p>Opens &lt;<code>shaderDirectory</code>&gt;/<code>index.json</code> and selects the blob set for the device&#x27;s backend. Fails with <code>core::error_code::NOT_FOUND</code> when the index is missing, <code>IO_FAILURE</code> when it cannot be read and <code>PARSE_FAILURE</code> when it is not valid JSON or not a valid index (the message names the entry).</p>

#### `reload` {#function-reload}

<div class="function-declaration" id="function-reload-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; reload();</code></pre></div>

<p>Re-reads <code>index.json</code> and drops every cached blob (development hot reload). On failure the previous index stays in effect and the error is returned.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
