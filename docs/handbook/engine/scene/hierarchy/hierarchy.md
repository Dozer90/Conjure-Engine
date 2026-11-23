---
kind: class
---

[Engine](../../../engine.md) → [Scene](../../scene.md) → [hierarchy.h](../hierarchy.md)

# hierarchy
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::scene` |
| Include File Path | `engine/include/conjure/scene/hierarchy.h` |
| Source File Path | `engine/src/scene/hierarchy.cpp` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../../api/scene/index.md), [Scenes and the hierarchy](../../../../api/scene/scenes-and-hierarchy.md) |

## Syntax

```cpp
class CONJURE_API hierarchy
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">hierarchy(
    <a href="../../ecs/world/world.html">ecs::world</a> &amp;world,
    <span class="k">const</span> <a href="../components/scene-component-ids.html">scene_component_ids</a> &amp;ids) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">hierarchy(
    <span class="k">const</span> <a href="hierarchy.html">hierarchy</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~hierarchy();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mIds</code></td><td><code class="signature"><span class="k">const</span> <a href="../components/scene-component-ids.html">scene_<wbr>component_<wbr>ids</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mWorld</code></td><td><code class="signature"><a href="../../ecs/world/world.html">ecs::world</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `children_of` {#function-children-of}

<div class="function-declaration" id="function-children-of-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="../../ecs/entity/entity.html">ecs::entity</a>&gt; children_of(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The children in sibling order; empty for leaves and dead entities. The span is valid until the next structural change.</p>

#### `collect_subtree` {#function-collect-subtree}

<div class="function-declaration" id="function-collect-subtree-1"><pre><code><span class="k">void</span> collect_subtree(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    std::vector&lt;<a href="../../ecs/entity/entity.html">ecs::entity</a>&gt; &amp;out) <span class="k">const</span>;</code></pre></div>

<p>Appends the entity and every descendant to <code>out</code>, depth first in sibling order.</p>

#### `depth_of` {#function-depth-of}

<div class="function-declaration" id="function-depth-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u8">u8</a> depth_of(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Hierarchy depth from the depth tag: 0 for roots and dead entities.</p>

#### `for_each_in_subtree` {#function-for-each-in-subtree}

<div class="function-declaration" id="function-for-each-in-subtree-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Callable&gt; <span class="k">void</span> for_each_in_subtree(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    Callable &amp;&amp;callable) <span class="k">const</span>;</code></pre></div>

<p>Invokes <code>callable(entity)</code> for the entity and every descendant, depth first in sibling order, with an explicit stack (never recursive). Nothing happens for a dead entity.</p>

#### `ids` {#function-ids}

<div class="function-declaration" id="function-ids-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../components/scene-component-ids.html">scene_component_ids</a> &amp;ids() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The component ids the tree reads.</p>

#### `is_alive` {#function-is-alive}

<div class="function-declaration" id="function-is-alive-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_alive(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the entity is alive in the world.</p>

#### `is_ancestor_of` {#function-is-ancestor-of}

<div class="function-declaration" id="function-is-ancestor-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_ancestor_of(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> ancestor,
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when <code>ancestor</code> is a strict ancestor of <code>target</code>.</p>

#### `parent_of` {#function-parent-of}

<div class="function-declaration" id="function-parent-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../ecs/entity/entity.html">ecs::entity</a> parent_of(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The parent entity; <code>NULL_ENTITY</code> for roots and dead entities.</p>

#### `root_of` {#function-root-of}

<div class="function-declaration" id="function-root-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../ecs/entity/entity.html">ecs::entity</a> root_of(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The topmost ancestor (the entity itself for a root); <code>NULL_ENTITY</code> for a dead entity.</p>

#### `sibling_index` {#function-sibling-index}

<div class="function-declaration" id="function-sibling-index-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> sibling_index(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Position among the parent&#x27;s children; 0 for roots (their order is <code>scene::roots()</code>) and for dead entities.</p>

#### `subtree_height` {#function-subtree-height}

<div class="function-declaration" id="function-subtree-height-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u8">u8</a> subtree_height(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of levels below the entity: 0 for a leaf, 1 when it has children without grandchildren, and so on. 0 for a dead entity.</p>

#### `subtree_size` {#function-subtree-size}

<div class="function-declaration" id="function-subtree-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> subtree_size(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of entities in the subtree, the entity included; 0 for a dead entity.</p>

#### `world` {#function-world}

<div class="function-declaration" id="function-world-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../ecs/world/world.html">ecs::world</a> &amp;world() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The world the tree lives in.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="hierarchy.html">hierarchy</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="hierarchy.html">hierarchy</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
