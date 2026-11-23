---
kind: struct
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [archetype.h](../archetype.md)

# chunk_layout
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/archetype.h` |
| Source File Path | `engine/src/ecs/archetype.cpp` |
| API ↗ | [Archives and serialisation](../../../../api/ecs/archives-and-serialization.md) |

## Syntax

```cpp
struct CONJURE_API chunk_layout
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">capacity</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">columnIndex</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>u16,<wbr> MAX_<wbr>COMPONENTS&gt;</code></td><td><code class="signature">make_<wbr>empty_<wbr>column_<wbr>index()</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">column<wbr>Offsets</code></td><td><code class="signature"><a href="../../core/containers/small-vector/small-vector.html">core::<wbr>small_<wbr>vector</a>&lt;<wbr>u32,<wbr> <span class="mi">16</span>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">columnSizes</code></td><td><code class="signature"><a href="../../core/containers/small-vector/small-vector.html">core::<wbr>small_<wbr>vector</a>&lt;<wbr>u32,<wbr> <span class="mi">16</span>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">components</code></td><td><code class="signature"><a href="../../core/containers/small-vector/small-vector.html">core::<wbr>small_<wbr>vector</a>&lt;<wbr>component_<wbr>id,<wbr> <span class="mi">16</span>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">NO_COLUMN</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u16">u16</a></code></td><td><code class="signature"><span class="mi">0</span>xFFFF</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `block_end` {#function-block-end}

<div class="function-declaration" id="function-block-end-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> block_end() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Byte offset one past the last column (the used part of the block).</p>

#### `column_count` {#function-column-count}

<div class="function-declaration" id="function-column-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> column_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of data columns (tags excluded).</p>

#### `column_of` {#function-column-of}

<div class="function-declaration" id="function-column-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u16">u16</a> column_of(
    <a href="../component/component-id.html">component_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The column of a component id, or <code>NO_COLUMN</code> when the <code><a href="archetype.html">archetype</a></code> lacks it or it is a tag.</p>

#### `compute` {#function-compute}

<div class="function-declaration" id="function-compute-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="chunk-layout.html">chunk_layout</a>&gt; compute(
    <span class="k">const</span> <a href="../component/component-mask.html">component_mask</a> &amp;mask,
    <span class="k">const</span> <a href="../component-registry/component-registry.html">component_registry</a> &amp;registry);</code></pre></div>

<p>Computes the maximal layout for a mask: rowBytes = <code>sizeof(entity)</code> + sum of sizes, capacity starts at <code>CHUNK_SIZE</code> / rowBytes and decrements until every aligned column fits the block. <code>ARCHETYPE_TOO_LARGE</code> when not even one row fits, <code>UNKNOWN_COMPONENT</code> for an id the registry does not know. <code>A</code> capacity below <code>SMALL_CAPACITY_WARNING</code> logs one <code>WARN</code>.</p>

#### `make_empty_column_index` {#function-make-empty-column-index}

<div class="function-declaration" id="function-make-empty-column-index-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> std::array&lt;u16, MAX_COMPONENTS&gt; make_empty_column_index() <span class="k">noexcept</span>;</code></pre></div>

<p>An index table with every id mapped to <code>NO_COLUMN</code>.</p>

#### `row_bytes` {#function-row-bytes}

<div class="function-declaration" id="function-row-bytes-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> row_bytes() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Bytes one row occupies across every column, entity included.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
