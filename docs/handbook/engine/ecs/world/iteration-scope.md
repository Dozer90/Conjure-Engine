---
kind: class
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [world.h](../world.md)

# iteration_scope
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/world.h` |
| Source File Path | `engine/src/ecs/world.cpp` |
| API ↗ | [Archives and serialisation](../../../../api/ecs/archives-and-serialization.md), [Command buffers and observers](../../../../api/ecs/command-buffers-and-observers.md), [Entities and components](../../../../api/ecs/entities-and-components.md), [Entities, components and systems](../../../../api/ecs/index.md), [Queries, views and systems](../../../../api/ecs/queries-views-and-systems.md) |

## Syntax

```cpp
class CONJURE_API iteration_scope
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">iteration_scope() <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td><code>A</code> scope that locks nothing.</td></tr>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> iteration_scope(
    <a href="world.html">world</a> &amp;target) <span class="k">noexcept</span>;</code></td><td>Locks <code>target</code> until destruction. Used by <code>world::lock_iteration()</code>.</td></tr>
<tr><td>public</td><td><code class="signature">iteration_scope(
    <a href="iteration-scope.html">iteration_scope</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~iteration_scope();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mWorld</code></td><td><code class="signature"><a href="world.html">world</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `is_active` {#function-is-active}

<div class="function-declaration" id="function-is-active-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_active() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while this scope holds a lock.</p>

#### `release` {#function-release}

<div class="function-declaration" id="function-release-1"><pre><code><span class="k">void</span> release() <span class="k">noexcept</span>;</code></pre></div>

<p>Unlocks now; later destruction does nothing.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="iteration-scope.html">iteration_scope</a> &amp;<span class="k">operator</span>=(
    <a href="iteration-scope.html">iteration_scope</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
