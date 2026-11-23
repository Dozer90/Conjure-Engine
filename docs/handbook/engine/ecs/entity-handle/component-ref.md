---
kind: class
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [entity_handle.h](../entity-handle.md)

# component_ref
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/entity_handle.h` |
| Source File Path | N/A |
| API ↗ | [Entities and components](../../../../api/ecs/entities-and-components.md) |

## Syntax

```cpp
template <typename ComponentType> class component_ref
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">component_ref() <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td><code>A</code> reference to nothing.</td></tr>
<tr><td>public</td><td><code class="signature">component_ref(
    <a href="../world/world.html">world</a> &amp;target,
    <a href="../entity/entity.html">entity</a> owner) <span class="k">noexcept</span>;</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">component_ref(
    <span class="k">const</span> <a href="component-ref.html">component_ref</a> &amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~component_ref() = <span class="k">default</span>;</code></td><td></td></tr>
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

#### `is_valid` {#function-is-valid}

<div class="function-declaration" id="function-is-valid-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_valid() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while the entity is alive and holds the component.</p>

#### `owner` {#function-owner}

<div class="function-declaration" id="function-owner-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../entity/entity.html">entity</a> owner() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The entity this reference points at.</p>

#### `read` {#function-read}

<div class="function-declaration" id="function-read-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> ComponentType *read() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The component for reading; <code>nullptr</code> when dead or missing. Never stamps.</p>

#### `set` {#function-set}

<div class="function-declaration" id="function-set-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set(
    ComponentType value);</code></pre></div>

<p>Replaces the value. <code>ENTITY_NOT_ALIVE</code> / <code>COMPONENT_NOT_PRESENT</code> when the reference is invalid.</p>

#### `valid` {#function-valid}

<div class="function-declaration" id="function-valid-1"><pre><code>[[<span class="na">deprecated</span>(<span class="s">"use is_valid()"</span>)]] [[<span class="na">nodiscard</span>]] <span class="k">bool</span> valid() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The former name of <code><a href="#function-is-valid-1">is_valid</a>()</code>, kept for one wave; returns <code><a href="#function-is-valid-1">is_valid</a>()</code>.</p>

#### `write` {#function-write}

<div class="function-declaration" id="function-write-1"><pre><code>[[<span class="na">nodiscard</span>]] ComponentType *write() <span class="k">noexcept</span>;</code></pre></div>

<p>The component for writing; stamps its chunk column. <code>nullptr</code> when dead or missing.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="component-ref.html">component_ref</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="component-ref.html">component_ref</a> &amp;) <span class="k">noexcept</span> = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
