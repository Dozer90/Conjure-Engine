---
kind: class
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [entity_handle.h](../entity-handle.md)

# entity_handle
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/entity_handle.h` |
| Source File Path | N/A |
| API ↗ | [Entities and components](../../../../api/ecs/entities-and-components.md) |

## Syntax

```cpp
class entity_handle
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">entity_handle() <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td><code>A</code> handle to nothing.</td></tr>
<tr><td>public</td><td><code class="signature">entity_handle(
    <a href="../world/world.html">world</a> &amp;target,
    <a href="../entity/entity.html">entity</a> id) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">entity_handle(
    <span class="k">const</span> <a href="entity-handle.html">entity_handle</a> &amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~entity_handle() = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mEntity</code></td><td><code class="signature"><a href="../entity/entity.html">entity</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mWorld</code></td><td><code class="signature"><a href="../world/world.html">world</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `add` {#function-add}

<div class="function-declaration" id="function-add-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; [[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="component-ref.html">component_ref</a>&lt;ComponentType&gt;&gt; add(
    ComponentType value = {});</code></pre></div>

<p>Adds a component (<code>world::add</code>) and returns a reference to it.</p>

#### `component` {#function-component}

<div class="function-declaration" id="function-component-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; [[<span class="na">nodiscard</span>]] <a href="component-ref.html">component_ref</a>&lt;ComponentType&gt; component() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p><code>A</code> reference to a component; <code>is_valid()</code> tells whether it exists.</p>

#### `destroy` {#function-destroy}

<div class="function-declaration" id="function-destroy-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; destroy();</code></pre></div>

<p>Destroys the entity (<code>world::destroy_entity</code>).</p>

#### `has` {#function-has}

<div class="function-declaration" id="function-has-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; [[<span class="na">nodiscard</span>]] <span class="k">bool</span> has() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the entity is alive and holds the component.</p>

#### `id` {#function-id}

<div class="function-declaration" id="function-id-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../entity/entity.html">entity</a> id() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The underlying entity id.</p>

#### `is_alive` {#function-is-alive}

<div class="function-declaration" id="function-is-alive-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_alive() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while the entity is alive.</p>

#### `owner` {#function-owner}

<div class="function-declaration" id="function-owner-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../world/world.html">world</a> *owner() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The world the handle belongs to, or <code>nullptr</code>.</p>

#### `remove` {#function-remove}

<div class="function-declaration" id="function-remove-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType&gt; [[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; remove();</code></pre></div>

<p>Removes a component (<code>world::remove</code>).</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="entity-handle.html">entity_handle</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="entity-handle.html">entity_handle</a> &amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

#### `operator==` {#operator-3d--3d}

<div class="function-declaration" id="operator-3d--3d-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> <span class="k">operator</span>==(
    <span class="k">const</span> <a href="entity-handle.html">entity_handle</a> &amp;other) <span class="k">const</span> <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
