---
kind: class
---

[Engine](../../../engine.md) → [ECS](../../ecs.md) → [entity_archive.h](../entity-archive.md)

# entity_archive
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::ecs` |
| Include File Path | `engine/include/conjure/ecs/entity_archive.h` |
| Source File Path | `engine/src/ecs/entity_archive.cpp` |
| API ↗ | [Archives and serialisation](../../../../api/ecs/archives-and-serialization.md) |

## Syntax

```cpp
class CONJURE_API entity_archive
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> entity_archive(
    <span class="k">const</span> <a href="../component-registry/component-registry.html">component_registry</a> &amp;registry) <span class="k">noexcept</span>;</code></td><td>An empty archive bound to a registry.</td></tr>
<tr><td>public</td><td><code class="signature">entity_archive(
    <a href="entity-archive.html">entity_archive</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~entity_archive();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mEntries</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="archive-entry.html">archive_<wbr>entry</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRegistry</code></td><td><code class="signature"><span class="k">const</span> <a href="../component-registry/component-registry.html">component_<wbr>registry</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `capture` {#function-capture}

<div class="function-declaration" id="function-capture-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="entity-archive.html">entity_archive</a>&gt; capture(
    <span class="k">const</span> <a href="../world/world.html">world</a> &amp;source,
    std::span&lt;<span class="k">const</span> <a href="../entity/entity.html">entity</a>&gt; entities,
    <a href="archive-options.html">archive_options</a> options = {});</code></pre></div>

<p>Copies the given entities (every non-<code>TRANSIENT</code> component through <code>ops.copy</code>) and rewrites their entity fields to local indices. <code>ENTITY_NOT_ALIVE</code> when an entity is dead; the world must be unlocked (<code>STRUCTURE_LOCKED</code> otherwise).</p>

#### `component_bytes` {#function-component-bytes}

<div class="function-declaration" id="function-component-bytes-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> std::byte&gt; component_bytes(
    <a href="../../core/types.html#symbol-u32">u32</a> entryIndex,
    <a href="../component/component-id.html">component_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The component payload of one entry, or an empty span when absent (or a tag).</p>

#### `entity_count` {#function-entity-count}

<div class="function-declaration" id="function-entity-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> entity_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of archived entities.</p>

#### `entries` {#function-entries}

<div class="function-declaration" id="function-entries-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="archive-entry.html">archive_entry</a>&gt; entries() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The entries in capture order (entry 0 is the root).</p>

#### `push_entry` {#function-push-entry}

<div class="function-declaration" id="function-push-entry-1"><pre><code><span class="k">void</span> push_entry(
    <a href="archive-entry.html">archive_entry</a> entry);</code></pre></div>

<p>Appends an entry whose payloads are moved in (used by <code>read_archive_json</code> and tests). Payload bytes must be object images owned by the caller until this call.</p>

#### `registry` {#function-registry}

<div class="function-declaration" id="function-registry-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../component-registry/component-registry.html">component_registry</a> &amp;registry() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The registry the component ids belong to.</p>

#### `validate` {#function-validate}

<div class="function-declaration" id="function-validate-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; validate() <span class="k">const</span>;</code></pre></div>

<p>Checks that every component id is registered and non-tag payloads have the registered size, every mask bit has its component (and vice versa), and every archive-local entity reference points inside the archive. <code>ARCHIVE_CORRUPT</code> otherwise.</p>

### Private

#### `destroy_payloads` {#function-destroy-payloads}

<div class="function-declaration" id="function-destroy-payloads-1"><pre><code><span class="k">void</span> destroy_payloads() <span class="k">noexcept</span>;</code></pre></div>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="entity-archive.html">entity_archive</a> &amp;<span class="k">operator</span>=(
    <a href="entity-archive.html">entity_archive</a> &amp;&amp;other) <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
