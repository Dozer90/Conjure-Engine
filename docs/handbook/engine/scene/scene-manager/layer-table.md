---
kind: class
---

[Engine](../../../engine.md) → [Scene](../../scene.md) → [scene_manager.h](../scene-manager.md)

# layer_table
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::scene` |
| Include File Path | `engine/include/conjure/scene/scene_manager.h` |
| Source File Path | `engine/src/scene/scene_manager.cpp` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../../api/scene/index.md), [Scene files, prefabs and snapshots](../../../../api/scene/scene-files-and-prefabs.md) |

## Syntax

```cpp
class CONJURE_API layer_table
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">layer_table();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~layer_table();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">LAYER_COUNT</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">32</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mNames</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>core::<wbr>string_<wbr>id,<wbr> LAYER_<wbr>COUNT&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `find` {#function-find}

<div class="function-declaration" id="function-find-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../../core/types.html#symbol-u8">u8</a>&gt; find(
    <a href="../../core/string-id/string-id.html">core::string_id</a> value) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The index of the layer with this name, or nullopt.</p>

#### `name` {#function-name}

<div class="function-declaration" id="function-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/string-id/string-id.html">core::string_id</a> name(
    <a href="../../core/types.html#symbol-u8">u8</a> index) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The name of a layer (the <code>null</code> id when unnamed or out of range).</p>

#### `names` {#function-names}

<div class="function-declaration" id="function-names-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../../core/string-id/string-id.html">core::string_id</a>&gt; names() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `set_name` {#function-set-name}

<div class="function-declaration" id="function-set-name-1"><pre><code><span class="k">void</span> set_name(
    <a href="../../core/types.html#symbol-u8">u8</a> index,
    <a href="../../core/string-id/string-id.html">core::string_id</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>Names a layer; indices above 31 are ignored (asserted).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
