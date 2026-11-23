---
kind: class
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [gltf_importer.h](../gltf-importer.md)

# gltf_importer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/gltf_importer.h` |
| Source File Path | `engine/src/renderer/import/gltf_importer.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Renderer assets](../../../../api/renderer/renderer-assets.md) |

## Syntax

```cpp
class CONJURE_API gltf_importer
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">gltf_importer();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~gltf_importer();</code></td><td></td></tr>
</tbody>
</table>



## Functions

### Public

#### `import` {#function-import}

<div class="function-declaration" id="function-import-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="gltf-import-result.html">gltf_import_result</a>&gt; import(
    <span class="k">const</span> std::filesystem::path &amp;sourcePath,
    <span class="k">const</span> <a href="gltf-import-options.html">gltf_import_options</a> &amp;options) <span class="k">const</span>;</code></pre></div>

<p>Imports .gltf/.glb into CPU assets. Unsupported extensions are warned and skipped. Buffers and images are read only from the model&#x27;s directory and below: a URI that is absolute or climbs out of it with &#x27;..&#x27; fails with <code>NOT_FOUND</code>. Fails with <code>NOT_FOUND</code> when the file does not exist, <code>IO_FAILURE</code> when it cannot be read, then as <code>import_bytes</code>.</p>

#### `import_bytes` {#function-import-bytes}

<div class="function-declaration" id="function-import-bytes-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="gltf-import-result.html">gltf_import_result</a>&gt; import_bytes(
    std::span&lt;<span class="k">const</span> std::byte&gt; bytes,
    <span class="k">const</span> <a href="gltf-import-options.html">gltf_import_options</a> &amp;options,
    <span class="k">const</span> <a href="../gltf-importer.html#symbol-gltf-file-reader">gltf_file_reader</a> &amp;reader) <span class="k">const</span>;</code></pre></div>

<p>Imports from memory; external buffers and images are read through <code>reader</code> (a <code>null</code> reader fails them with <code>NOT_FOUND</code>). Fails with <code>PARSE_FAILURE</code> for malformed glTF (also an accessor or buffer view reaching past its buffer, a byteStride above 252, a node cycle, or a <code>COLOR_0</code> that is not <code>VEC3</code> or <code>VEC4</code>), <code>NOT_FOUND</code> for a missing buffer or image and <code>UNSUPPORTED</code> for points / lines topology or an accessor layout the importer cannot read. Node hierarchies of any depth import without recursion.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->
