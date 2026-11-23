---
kind: class
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [entity.h](../entity.md)

# entity_remap
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/entity.h` |
| Source File Path | N/A |
| API ↗ | [Entities and components](../../../../api/ecs/entities-and-components.md) |

## Syntax

```cpp
class entity_remap
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">entity_remap() = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> entity_remap(
    std::vector&lt;<a href="entity.html">entity</a>&gt; targets);</code></td><td>Takes ownership of a local -&gt; real table.</td></tr>
<tr><td>public</td><td><code class="signature">entity_remap(
    <span class="k">const</span> <a href="entity-remap.html">entity_remap</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">entity_remap(
    <a href="entity-remap.html">entity_remap</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~entity_remap() = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mIndex</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr>std::<wbr>pair&lt;<wbr>u64,<wbr> <a href="../../core/types.html#symbol-u32">u32</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTargets</code></td><td><code class="signature">std::vector&lt;<a href="entity.html">entity</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear() <span class="k">noexcept</span>;</code></pre></div>

<p>Empties the table and its index.</p>

#### `index_targets` {#function-index-targets}

<div class="function-declaration" id="function-index-targets-1"><pre><code><span class="k">void</span> index_targets();</code></pre></div>

<p>Optional: builds a sorted index of the targets so <code><a href="#function-remap-1">remap</a>()</code> turns a real <code><a href="entity.html">entity</a></code> into its local index by binary search instead of a linear scan. Worth calling once the table is complete and many real -&gt; local lookups follow (<code>entity_archive::capture</code> does). <code>A</code> target listed twice maps to its first local index, as the linear scan does. <code><a href="#function-set-1">set</a>()</code>, <code><a href="#function-resize-1">resize</a>()</code> and <code><a href="#function-clear-1">clear</a>()</code> drop the index, after which <code><a href="#function-remap-1">remap</a>()</code> scans again until the next call.</p>

#### `remap` {#function-remap}

<div class="function-declaration" id="function-remap-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="entity.html">entity</a> remap(
    <a href="entity.html">entity</a> value) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Resolves one reference in whichever direction the value needs: an archive-local value becomes its real <code><a href="entity.html">entity</a></code> (as resolve), a real <code><a href="entity.html">entity</a></code> that is one of the targets becomes the archive-local index of that target, and every other value is returned unchanged. The same table therefore serves <code>entity_archive::capture</code> (real -&gt; local, targets = the captured entities) and <code>world::instantiate</code> (local -&gt; real): a live world never holds archive-local ids and an archive never holds a real id that is also one of its own targets. <code>A</code> real value is found by binary search after <code><a href="#function-index-targets-1">index_targets</a>()</code>, by a linear scan otherwise.</p>

#### `resize` {#function-resize}

<div class="function-declaration" id="function-resize-1"><pre><code><span class="k">void</span> resize(
    <a href="../../core/types.html#symbol-u32">u32</a> count);</code></pre></div>

<p>Resizes the table; new entries are <code>NULL_ENTITY</code>. Drops the <code><a href="#function-index-targets-1">index_targets</a>()</code> index.</p>

#### `resolve` {#function-resolve}

<div class="function-declaration" id="function-resolve-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="entity.html">entity</a> resolve(
    <a href="entity.html">entity</a> value) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Resolves one reference: an archive-local value becomes its real <code><a href="entity.html">entity</a></code> (<code>NULL_ENTITY</code> when the local index is out of range); every other value, real or <code>null</code>, is returned unchanged.</p>

#### `set` {#function-set}

<div class="function-declaration" id="function-set-1"><pre><code><span class="k">void</span> set(
    <a href="../../core/types.html#symbol-u32">u32</a> localIndex,
    <a href="entity.html">entity</a> target);</code></pre></div>

<p>Sets the real <code><a href="entity.html">entity</a></code> of a local index, growing the table when needed. Drops the <code><a href="#function-index-targets-1">index_targets</a>()</code> index.</p>

#### `size` {#function-size}

<div class="function-declaration" id="function-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of local indices the table covers.</p>

#### `target` {#function-target}

<div class="function-declaration" id="function-target-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="entity.html">entity</a> target(
    <a href="../../core/types.html#symbol-u32">u32</a> localIndex) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The real <code><a href="entity.html">entity</a></code> of a local index, or <code>NULL_ENTITY</code> when out of range.</p>

#### `targets` {#function-targets}

<div class="function-declaration" id="function-targets-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="entity.html">entity</a>&gt; targets() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The whole table.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="entity-remap.html">entity_remap</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="entity-remap.html">entity_remap</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<div class="function-declaration" id="operator-3d-2"><pre><code><a href="entity-remap.html">entity_remap</a> &amp;<span class="k">operator</span>=(
    <a href="entity-remap.html">entity_remap</a> &amp;&amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
