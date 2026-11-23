---
kind: struct
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [entity.h](../entity.md)

# entity
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/entity.h` |
| Source File Path | N/A |
| API ↗ | [Entities and components](../../../../api/ecs/entities-and-components.md) |

## Syntax

```cpp
struct entity
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">generation</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">index</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `archive_local` {#function-archive-local}

<div class="function-declaration" id="function-archive-local-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> <a href="entity.html">entity</a> archive_local(
    <a href="../../core/types.html#symbol-u32">u32</a> localIndex) <span class="k">noexcept</span>;</code></pre></div>

<p>An archive-local reference to entry <code>localIndex</code>.</p>

#### `from_u64` {#function-from-u64}

<div class="function-declaration" id="function-from-u64-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <span class="k">constexpr</span> <a href="entity.html">entity</a> from_u64(
    <a href="../../core/types.html#symbol-u64">u64</a> packed) <span class="k">noexcept</span>;</code></pre></div>

<p>Inverse of <code><a href="#function-to-u64-1">to_u64</a>()</code>.</p>

#### `is_archive_local` {#function-is-archive-local}

<div class="function-declaration" id="function-is-archive-local-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_archive_local() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when this handle is a local index inside an <code><a href="../entity-archive/entity-archive.html">entity_archive</a></code>.</p>

#### `is_null` {#function-is-null}

<div class="function-declaration" id="function-is-null-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_null() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True for <code>NULL_ENTITY</code> (index 0, generation 0).</p>

#### `to_u64` {#function-to-u64}

<div class="function-declaration" id="function-to-u64-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u64">u64</a> to_u64() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>(generation &lt;&lt; 32) | index, the packed form handed to <code>C</code>#.</p>

## Operator Overloads

### Public

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="entity.html">entity</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
