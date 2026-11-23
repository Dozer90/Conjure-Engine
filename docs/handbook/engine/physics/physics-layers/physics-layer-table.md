---
kind: class
---

[Engine](../../../engine.md) → [Physics](../../physics.md) → [physics_layers.h](../physics-layers.md)

# physics_layer_table
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::physics` |
| Include File Path | `engine/include/conjure/physics/physics_layers.h` |
| Source File Path | `engine/src/physics/physics_layers.cpp` |
| API ↗ | [Bodies, colliders and characters](../../../../api/physics/bodies-and-colliders.md), [Physics](../../../../api/physics/index.md) |

## Syntax

```cpp
class CONJURE_API physics_layer_table
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">physics_layer_table();</code></td><td>One layer &quot;default&quot; that collides with itself.</td></tr>
<tr><td>public</td><td><code class="signature">physics_layer_table(
    <span class="k">const</span> <a href="physics-layer-table.html">physics_layer_table</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">physics_layer_table(
    <a href="physics-layer-table.html">physics_layer_table</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~physics_layer_table();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mCollides</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>u32,<wbr> MAX_<wbr>PHYSICS_<wbr>LAYERS&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mNames</code></td><td><code class="signature"><a href="../../core/containers/small-vector/small-vector.html">core::<wbr>small_<wbr>vector</a>&lt;<wbr>core::<wbr>string_<wbr>id,<wbr> <span class="mi">8</span>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `add_layer` {#function-add-layer}

<div class="function-declaration" id="function-add-layer-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../../core/types.html#symbol-u8">u8</a>&gt; add_layer(
    <a href="../../core/string-id/string-id.html">core::string_id</a> name);</code></pre></div>

<p>Appends a layer that collides with every existing layer and itself; returns its index. <code>LAYER_LIMIT_REACHED</code>, <code>DUPLICATE_LAYER</code> (also for the <code>null</code> id).</p>

#### `collides` {#function-collides}

<div class="function-declaration" id="function-collides-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> collides(
    <a href="../../core/types.html#symbol-u8">u8</a> a,
    <a href="../../core/types.html#symbol-u8">u8</a> b) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the two layers collide; <code>false</code> for indices outside the table.</p>

#### `collision_mask` {#function-collision-mask}

<div class="function-declaration" id="function-collision-mask-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="physics-layer-mask.html">physics_layer_mask</a> collision_mask(
    <a href="../../core/types.html#symbol-u8">u8</a> layer) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The layers <code>layer</code> collides with (the empty mask for an index outside the table).</p>

#### `count` {#function-count}

<div class="function-declaration" id="function-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of layers (at least 1).</p>

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../../core/types.html#symbol-u8">u8</a>&gt; find(
    <a href="../../core/string-id/string-id.html">core::string_id</a> name) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The index of the layer with this name, or nullopt.</p>

#### `from_json` {#function-from-json}

<div class="function-declaration" id="function-from-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="physics-layer-table.html">physics_layer_table</a>&gt; from_json(
    <span class="k">const</span> <a href="../../core/json/value/json-value.html">core::json::value</a> &amp;section);</code></pre></div>

<p>Builds a table from the <code>project.json</code> section: &quot;names&quot; (required, 1..32 unique strings) and &quot;ignore&quot; (optional pairs of names that do not collide). <code>LAYER_CONFIG_INVALID</code>, <code>LAYER_LIMIT_REACHED</code>, <code>DUPLICATE_LAYER</code>, <code>UNKNOWN_LAYER</code>. Everything collides unless ignored.</p>

#### `name` {#function-name}

<div class="function-declaration" id="function-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/string-id/string-id.html">core::string_id</a> name(
    <a href="../../core/types.html#symbol-u8">u8</a> layer) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The name of a layer (the <code>null</code> id for an index outside the table).</p>

#### `set_collides` {#function-set-collides}

<div class="function-declaration" id="function-set-collides-1"><pre><code><span class="k">void</span> set_collides(
    <a href="../../core/types.html#symbol-u8">u8</a> a,
    <a href="../../core/types.html#symbol-u8">u8</a> b,
    <span class="k">bool</span> collides) <span class="k">noexcept</span>;</code></pre></div>

<p>Sets whether two layers collide (symmetric). Indices outside the table are ignored.</p>

#### `to_json` {#function-to-json}

<div class="function-declaration" id="function-to-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; to_json(
    <a href="../../core/json/writer/json-writer.html">core::json::writer</a> &amp;out) <span class="k">const</span>;</code></pre></div>

<p>Writes {&quot;ignore&quot;: [[a, b], ...], &quot;names&quot;: [...]} with sorted keys and the ignored pairs in ascending (a, b) index order, so the text equals the canonical form of its parse. Fails with <code>LAYER_CONFIG_INVALID</code> when a layer name was never interned (it cannot be spelt).</p>

#### `validate` {#function-validate}

<div class="function-declaration" id="function-validate-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; validate() <span class="k">const</span>;</code></pre></div>

<p><code>LAYER_CONFIG_INVALID</code> when the table is empty or the matrix is not symmetric, <code>DUPLICATE_LAYER</code> when two layers share a name.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="physics-layer-table.html">physics_layer_table</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="physics-layer-table.html">physics_layer_table</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="physics-layer-table.html">physics_layer_table</a> &amp;<span class="k">operator</span>=(
    <a href="physics-layer-table.html">physics_layer_table</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="physics-layer-table.html">physics_layer_table</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
