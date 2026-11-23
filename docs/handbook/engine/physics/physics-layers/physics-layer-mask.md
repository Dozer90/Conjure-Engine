---
kind: struct
---

[Engine](../../../engine.md) → [Physics](../../physics.md) → [physics_layers.h](../physics-layers.md)

# physics_layer_mask
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::physics` |
| Include File Path | `engine/include/conjure/physics/physics_layers.h` |
| Source File Path | `engine/src/physics/physics_layers.cpp` |
| API ↗ | [Bodies, colliders and characters](../../../../api/physics/bodies-and-colliders.md), [Physics](../../../../api/physics/index.md) |

## Syntax

```cpp
struct physics_layer_mask
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">bits</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span>xFFFFFFFFu</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `all` {#function-all}

<div class="function-declaration" id="function-all-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> <a href="physics-layer-mask.html">physics_layer_mask</a> all() <span class="k">noexcept</span>;</code></pre></div>

<p>Every layer.</p>

#### `includes` {#function-includes}

<div class="function-declaration" id="function-includes-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> includes(
    <a href="../../core/types.html#symbol-u8">u8</a> layer) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the layer&#x27;s bit is set (<code>false</code> for indices of 32 or more).</p>

#### `none` {#function-none}

<div class="function-declaration" id="function-none-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> <a href="physics-layer-mask.html">physics_layer_mask</a> none() <span class="k">noexcept</span>;</code></pre></div>

<p>No layer.</p>

#### `only` {#function-only}

<div class="function-declaration" id="function-only-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> <a href="physics-layer-mask.html">physics_layer_mask</a> only(
    <a href="../../core/types.html#symbol-u8">u8</a> layer) <span class="k">noexcept</span>;</code></pre></div>

<p>Exactly one layer (indices of 32 or more yield the empty mask).</p>

#### `with` {#function-with}

<div class="function-declaration" id="function-with-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="physics-layer-mask.html">physics_layer_mask</a> with(
    <a href="../../core/types.html#symbol-u8">u8</a> layer) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>This mask with the layer added.</p>

#### `without` {#function-without}

<div class="function-declaration" id="function-without-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="physics-layer-mask.html">physics_layer_mask</a> without(
    <a href="../../core/types.html#symbol-u8">u8</a> layer) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>This mask with the layer removed.</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="physics-layer-mask.html">physics_layer_mask</a> &amp;) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
