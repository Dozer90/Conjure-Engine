---
kind: class
---

[Engine](../../../engine.md) → [Scene](../../scene.md) → [prefab.h](../prefab.md)

# prefab_builder
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::scene` |
| Include File Path | `engine/include/conjure/scene/prefab.h` |
| Source File Path | `engine/src/scene/prefab.cpp` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../../api/scene/index.md), [Scene files, prefabs and snapshots](../../../../api/scene/scene-files-and-prefabs.md) |

## Syntax

```cpp
class CONJURE_API prefab_builder
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature"><span class="k">explicit</span> prefab_builder(
    <span class="k">const</span> <a href="../../ecs/component-registry/component-registry.html">ecs::component_registry</a> &amp;registry);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~prefab_builder();</code></td><td>Destroys the component images.</td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mIds</code></td><td><code class="signature"><a href="../components/scene-component-ids.html">scene_component_ids</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mNodes</code></td><td><code class="signature">std::vector&lt;<a href="prefab-builder-node.html">node</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRegistry</code></td><td><code class="signature"><span class="k">const</span> <a href="../../ecs/component-registry/component-registry.html">ecs::<wbr>component_<wbr>registry</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">NO_PARENT</code></td><td><code class="signature"><span class="k">static</span> <span class="k">constexpr</span> <a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span>xFFFFFFFFu</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `add_component_json` {#function-add-component-json}

<div class="function-declaration" id="function-add-component-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; add_component_json(
    <a href="../../core/types.html#symbol-u32">u32</a> node,
    <a href="../../core/string-id/string-id.html">core::string_id</a> component,
    <span class="k">const</span> <a href="../../core/json/value/json-value.html">core::json::value</a> &amp;value);</code></pre></div>

<p>Adds a component by registered name from JSON. <code>UNKNOWN_COMPONENT</code> for an unknown name, <code>SERIALIZATION_FAILED</code> for malformed JSON, <code>BUILDER_INVALID</code> for an unknown node or a component the builder writes itself (parent, children, guid, <code><a href="../components/prefab-link.html">prefab_link</a></code>, name, <code><a href="../components/local-transform.html">local_transform</a></code>, depth tags, companions, inactive): name and transform come from <code>add_node</code>.</p>

#### `add_component_raw` {#function-add-component-raw}

<div class="function-declaration" id="function-add-component-raw-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; add_component_raw(
    <a href="../../core/types.html#symbol-u32">u32</a> node,
    ecs::component_id component,
    std::span&lt;<span class="k">const</span> std::byte&gt; bytes);</code></pre></div>

<p>Adds a component from an object image of exactly the registered size.</p>

#### `add_node` {#function-add-node}

<div class="function-declaration" id="function-add-node-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> add_node(
    <a href="../../core/types.html#symbol-u32">u32</a> parentIndex,
    std::string_view name,
    <span class="k">const</span> <a href="../components/local-transform.html">local_transform</a> &amp;transform = {});</code></pre></div>

<p>Adds a node and returns its index. <code>parentIndex</code> is <code>NO_PARENT</code> for the root; an unknown parent makes <code><a href="#function-build-1">build</a>()</code> fail with <code>BUILDER_INVALID</code>.</p>

#### `add_tag` {#function-add-tag}

<div class="function-declaration" id="function-add-tag-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; add_tag(
    <a href="../../core/types.html#symbol-u32">u32</a> node,
    ecs::component_id component);</code></pre></div>

<p>Adds a tag component. <code>BUILDER_INVALID</code> when the id is not a tag.</p>

#### `build` {#function-build}

<div class="function-declaration" id="function-build-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../scene-serializer/prefab-document.html">prefab_document</a>&gt; build(
    std::string_view name) <span class="k">const</span>;</code></pre></div>

<p>Produces the document: guids generated, parent and siblingIndex from insertion order, <code><a href="../components/prefab-link.html">prefab_link</a></code> on every entry, entries in hierarchy order. <code>BUILDER_INVALID</code> unless exactly one root exists and every parent index names an earlier-or-later node without a cycle.</p>

#### `node_count` {#function-node-count}

<div class="function-declaration" id="function-node-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> node_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of nodes added.</p>

### Private

#### `destroy_images` {#function-destroy-images}

<div class="function-declaration" id="function-destroy-images-1"><pre><code><span class="k">void</span> destroy_images() <span class="k">noexcept</span>;</code></pre></div>

#### `is_reserved` {#function-is-reserved}

<div class="function-declaration" id="function-is-reserved-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_reserved(
    ecs::component_id id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
