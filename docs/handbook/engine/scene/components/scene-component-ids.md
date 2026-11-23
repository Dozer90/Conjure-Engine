---
kind: struct
---

[Engine](../../../engine.md) → [Scene](../../scene.md) → [components.h](../components.md)

# scene_component_ids
<!-- doc-template:type -->

|---|---|
| Type | Struct |
| Namespace | `conjure::scene` |
| Include File Path | `engine/include/conjure/scene/components.h` |
| Source File Path | `engine/src/scene/components.cpp` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../../api/scene/index.md), [Scenes and the hierarchy](../../../../api/scene/scenes-and-hierarchy.md) |

## Syntax

```cpp
struct CONJURE_API scene_component_ids
```







## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">children</code></td><td><code class="signature">ecs::component_id</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">depth</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>ecs::<wbr>component_<wbr>id,<wbr> MAX_<wbr>HIERARCHY_<wbr>DEPTH&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">disabled</code></td><td><code class="signature">ecs::component_id</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">guid</code></td><td><code class="signature">ecs::component_id</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">inactive</code></td><td><code class="signature">ecs::component_id</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">interpolated<wbr>Transform</code></td><td><code class="signature">ecs::component_id</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">layer</code></td><td><code class="signature">ecs::component_id</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">localBounds</code></td><td><code class="signature">ecs::component_id</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">local<wbr>Transform</code></td><td><code class="signature">ecs::component_id</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">name</code></td><td><code class="signature">ecs::component_id</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">parent</code></td><td><code class="signature">ecs::component_id</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">prefab<wbr>Instance</code></td><td><code class="signature">ecs::component_id</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">prefabLink</code></td><td><code class="signature">ecs::component_id</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">render<wbr>World<wbr>Transform</code></td><td><code class="signature">ecs::component_id</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">tag</code></td><td><code class="signature">ecs::component_id</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">transform<wbr>Dirty</code></td><td><code class="signature">ecs::component_id</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">transform<wbr>State</code></td><td><code class="signature">ecs::component_id</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">world<wbr>Transform</code></td><td><code class="signature">ecs::component_id</code></td><td><code class="signature">{}</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `depth_of` {#function-depth-of}

<div class="function-declaration" id="function-depth-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u8">u8</a> depth_of(
    <span class="k">const</span> ecs::component_mask &amp;mask) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The depth encoded in a component mask: the <code>D</code> of the <code><a href="depth-tag.html">depth_tag</a></code> it carries, 0 for none.</p>

#### `has_transform_companions` {#function-has-transform-companions}

<div class="function-declaration" id="function-has-transform-companions-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> has_transform_companions(
    <span class="k">const</span> ecs::component_mask &amp;mask) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the mask holds every transform companion of <code><a href="local-transform.html">local_transform</a></code>.</p>

#### `is_complete` {#function-is-complete}

<div class="function-declaration" id="function-is-complete-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_complete() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when every scene component (the 31 depth tags included) is registered.</p>

#### `resolve` {#function-resolve}

<div class="function-declaration" id="function-resolve-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="scene-component-ids.html">scene_component_ids</a> resolve(
    <span class="k">const</span> <a href="../../ecs/component-registry/component-registry.html">ecs::component_registry</a> &amp;registry) <span class="k">noexcept</span>;</code></pre></div>

<p>Looks every scene component up by <code><a href="name.html">name</a></code>. Ids of unregistered types stay <code>INVALID</code>; see <code><a href="#function-is-complete-1">is_complete</a>()</code>.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<!-- doc-legacy-anchors:end -->
