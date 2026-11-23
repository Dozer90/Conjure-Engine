---
kind: class
---

[Engine](../../../engine.md) → [Renderer](../../renderer.md) → [model_asset.h](../model-asset.md)

# model_asset
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::renderer` |
| Include File Path | `engine/include/conjure/renderer/model_asset.h` |
| Source File Path | `engine/src/renderer/assets/model_asset.cpp` |
| API ↗ | [Rendering](../../../../api/renderer/index.md), [Renderer assets](../../../../api/renderer/renderer-assets.md) |

## Syntax

```cpp
class CONJURE_API model_asset
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">model_asset();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">model_asset(
    <span class="k">const</span> <a href="model-asset.html">model_asset</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">model_asset(
    <a href="model-asset.html">model_asset</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~model_asset();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mMaterials</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../../assets/asset-types.html#symbol-asset-id">assets::<wbr>asset_<wbr>id</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mMeshes</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="../../assets/asset-types.html#symbol-asset-id">assets::<wbr>asset_<wbr>id</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mName</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mNodes</code></td><td><code class="signature">std::vector&lt;<a href="model-node.html">model_node</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `add_material` {#function-add-material}

<div class="function-declaration" id="function-add-material-1"><pre><code><a href="../../core/types.html#symbol-u32">u32</a> add_material(
    <span class="k">const</span> <a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a> &amp;id);</code></pre></div>

<p>Appends a material id and returns its index.</p>

#### `add_mesh` {#function-add-mesh}

<div class="function-declaration" id="function-add-mesh-1"><pre><code><a href="../../core/types.html#symbol-u32">u32</a> add_mesh(
    <span class="k">const</span> <a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a> &amp;id);</code></pre></div>

<p>Appends a mesh id and returns its index.</p>

#### `add_node` {#function-add-node}

<div class="function-declaration" id="function-add-node-1"><pre><code><a href="../../core/types.html#symbol-u32">u32</a> add_node(
    <a href="model-node.html">model_node</a> node);</code></pre></div>

<p>Appends a node; the parent must already exist (or be <code>INVALID_INDEX</code>). Returns the index, or <code>INVALID_INDEX</code> (asserting) for a bad parent.</p>

#### `children_of` {#function-children-of}

<div class="function-declaration" id="function-children-of-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt; children_of(
    <a href="../../core/types.html#symbol-u32">u32</a> node) <span class="k">const</span>;</code></pre></div>

<p>Indices of the direct children of a node, in order.</p>

#### `materials` {#function-materials}

<div class="function-declaration" id="function-materials-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a>&gt; materials() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `meshes` {#function-meshes}

<div class="function-declaration" id="function-meshes-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a>&gt; meshes() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `name` {#function-name}

<div class="function-declaration" id="function-name-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view name() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `nodes` {#function-nodes}

<div class="function-declaration" id="function-nodes-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="model-node.html">model_node</a>&gt; nodes() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `parse_blob` {#function-parse-blob}

<div class="function-declaration" id="function-parse-blob-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="model-asset.html">model_asset</a>&gt; parse_blob(
    std::span&lt;<span class="k">const</span> std::byte&gt; bytes);</code></pre></div>

#### `parse_json` {#function-parse-json}

<div class="function-declaration" id="function-parse-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="model-asset.html">model_asset</a>&gt; parse_json(
    std::string_view text);</code></pre></div>

<p>Parses the JSON form. Fails with <code>PARSE_FAILURE</code> for invalid JSON and <code>INVALID_ARGUMENT</code> for a wrong member type, a parent index that is not smaller than the node&#x27;s own index (the list is parent-first), or a mesh / material index out of range.</p>

#### `root_nodes` {#function-root-nodes}

<div class="function-declaration" id="function-root-nodes-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt; root_nodes() <span class="k">const</span>;</code></pre></div>

<p>Indices of the nodes without a parent, in order.</p>

#### `set_name` {#function-set-name}

<div class="function-declaration" id="function-set-name-1"><pre><code><span class="k">void</span> set_name(
    std::string name);</code></pre></div>

#### `to_json` {#function-to-json}

<div class="function-declaration" id="function-to-json-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string to_json() <span class="k">const</span>;</code></pre></div>

<p>The canonical JSON form.</p>

#### `world_transform` {#function-world-transform}

<div class="function-declaration" id="function-world-transform-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/math/transform/transform.html">math::transform</a> world_transform(
    <a href="../../core/types.html#symbol-u32">u32</a> node) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The world transform of a node: the product of its ancestors&#x27; local transforms.</p>

#### `write_blob` {#function-write-blob}

<div class="function-declaration" id="function-write-blob-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;std::byte&gt; write_blob() <span class="k">const</span>;</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="model-asset.html">model_asset</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="model-asset.html">model_asset</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="model-asset.html">model_asset</a> &amp;<span class="k">operator</span>=(
    <a href="model-asset.html">model_asset</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="model-asset.html">model_asset</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
