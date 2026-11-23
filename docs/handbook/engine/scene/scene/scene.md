---
kind: class
---

[Engine](../../../engine.md) → [Scene](../../scene.md) → [scene.h](../scene.md)

# scene
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::scene` |
| Include File Path | `engine/include/conjure/scene/scene.h` |
| Source File Path | `engine/src/scene/scene.cpp` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../../api/scene/index.md), [Scenes and the hierarchy](../../../../api/scene/scenes-and-hierarchy.md) |

## Syntax

```cpp
class CONJURE_API scene
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">scene(
    <a href="../../ecs/world/world.html">ecs::world</a> &amp;world,
    <a href="../../assets/asset-manager/asset-manager.html">assets::asset_manager</a> *assets);</code></td><td>Binds the <code><a href="scene.html">scene</a></code> to a world whose registry carries the <code><a href="scene.html">scene</a></code> components (<code>register_components</code>). <code>assets</code> may be <code>null</code> in pure ECS tests: the bind pass is then skipped.</td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted. Move constructors have been deleted. Move assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~scene();</code></td><td>Removes the observers. The world and its entities are left as they are.</td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mAssets</code></td><td><code class="signature"><a href="../../assets/asset-manager/asset-manager.html">assets::asset_manager</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Destroy<wbr>Queue</code></td><td><code class="signature">std::vector&lt;<a href="../../ecs/entity/entity.html">ecs::entity</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Expected<wbr>Repairs</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mGuids</code></td><td><code class="signature"><a href="../guid-index/guid-index.html">guid_index</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mIds</code></td><td><code class="signature"><a href="../components/scene-component-ids.html">scene_component_ids</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mInOperation</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Max<wbr>Depth<wbr>In<wbr>Use</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u8">u8</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mName</code></td><td><code class="signature">std::string</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mObservers</code></td><td><code class="signature">std::<wbr>array&lt;<wbr>ecs::<wbr>observer_<wbr>id,<wbr> <span class="mi">6</span>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>On<wbr>Settings<wbr>Changed</code></td><td><code class="signature"><a href="../../core/event/event.html">core::event</a>&lt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>On<wbr>Structure<wbr>Changed</code></td><td><code class="signature"><a href="../../core/event/event.html">core::event</a>&lt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRawEdits</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRepairQueue</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="scene-repair-entry.html">repair_<wbr>entry</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRepairSpare</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="scene-repair-entry.html">repair_<wbr>entry</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRoots</code></td><td><code class="signature">std::vector&lt;<a href="root-entry.html">root_entry</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mScratch</code></td><td><code class="signature"><a href="../../ecs/command-buffer/command-buffer.html">ecs::command_buffer</a></code></td><td>—</td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Scratch<wbr>Entities</code></td><td><code class="signature">std::vector&lt;<a href="../../ecs/entity/entity.html">ecs::entity</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSettings</code></td><td><code class="signature"><a href="../../core/json/document/json-document.html">core::json::document</a></code></td><td><code class="signature">core::<wbr>json::<wbr>document::<wbr>object()</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Structure<wbr>Version</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">1</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTree</code></td><td><code class="signature"><a href="../hierarchy/hierarchy.html">hierarchy</a></code></td><td>—</td><td></td></tr>
<tr><td>private</td><td><code class="signature">mWorld</code></td><td><code class="signature"><a href="../../ecs/world/world.html">ecs::world</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `add_transform` {#function-add-transform}

<div class="function-declaration" id="function-add-transform-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; add_transform(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <span class="k">const</span> <a href="../components/local-transform.html">local_transform</a> &amp;value = {});</code></pre></div>

<p>Adds <code><a href="../components/local-transform.html">local_transform</a></code> and its companions (world, render, state) plus <code><a href="../components/transform-dirty.html">transform_dirty</a></code> in one move. <code>A</code> present <code><a href="../components/local-transform.html">local_transform</a></code> is overwritten. <code>INVALID_ENTITY</code> for a dead entity.</p>

#### `apply_hierarchy_changes` {#function-apply-hierarchy-changes}

<div class="function-declaration" id="function-apply-hierarchy-changes-1"><pre><code><span class="k">void</span> apply_hierarchy_changes();</code></pre></div>

<p>Body of <code><a href="scene.html">scene</a>.apply_hierarchy_changes</code>: repairs raw and deferred parent edits (3.7), one flush per repaired entity so every retag reads live depth tags. Costs nothing when no repair is queued.</p>

#### `assets` {#function-assets}

<div class="function-declaration" id="function-assets-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../assets/asset-manager/asset-manager.html">assets::asset_manager</a> *assets() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The asset manager used by the bind pass, or <code>nullptr</code>.</p>

#### `clear` {#function-clear}

<div class="function-declaration" id="function-clear-1"><pre><code><span class="k">void</span> clear();</code></pre></div>

<p>Destroys every entity of the world, empties the roots, the guid index and the queues. Settings and name are kept.</p>

#### `contains` {#function-contains}

<div class="function-declaration" id="function-contains-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> contains(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the entity is alive in this <code><a href="scene.html">scene</a></code>&#x27;s world.</p>

#### `create_entities` {#function-create-entities}

<div class="function-declaration" id="function-create-entities-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="instantiate-report.html">instantiate_report</a>&gt; create_entities(
    <a href="../../core/types.html#symbol-u32">u32</a> count,
    <a href="../../ecs/entity/entity.html">ecs::entity</a> templateEntity,
    <a href="../../ecs/entity/entity.html">ecs::entity</a> parent,
    std::span&lt;<a href="../../ecs/entity/entity.html">ecs::entity</a>&gt; out);</code></pre></div>

<p>Creates <code>count</code> copies of the template&#x27;s subtree under <code>parent</code> (fresh guids). <code>A</code> <code>null</code> template creates plain entities exactly like <code>create_entity</code>, as one structural operation. <code>out</code> receives the roots and must hold at least <code>count</code> entries. Fails atomically: on an error no entity created by the call remains.</p>

#### `create_entity` {#function-create-entity}

<div class="function-declaration" id="function-create-entity-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../../ecs/entity/entity.html">ecs::entity</a>&gt; create_entity(
    <a href="../../core/string-id/string-id.html">core::string_id</a> name,
    <a href="../../ecs/entity/entity.html">ecs::entity</a> parent = ecs::NULL_ENTITY,
    <a href="../../core/types.html#symbol-u32">u32</a> index = APPEND_CHILD);</code></pre></div>

<p>Creates an entity with guid, name, <code><a href="../components/local-transform.html">local_transform</a></code> and its companions in its final archetype, as a root or as the child of <code>parent</code> at <code>index</code>. <code>INVALID_ENTITY</code> for a dead parent, <code>HIERARCHY_TOO_DEEP</code> below depth 31.</p>

#### `deferred_set_parent` {#function-deferred-set-parent}

<div class="function-declaration" id="function-deferred-set-parent-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; deferred_set_parent(
    <a href="../../ecs/command-buffer/command-buffer.html">ecs::command_buffer</a> &amp;commands,
    <a href="../../ecs/entity/entity.html">ecs::entity</a> child,
    <a href="../../ecs/entity/entity.html">ecs::entity</a> newParent,
    <a href="../scene.html#symbol-reparent-mode">reparent_mode</a> mode = reparent_mode::KEEP_WORLD,
    <a href="../../core/types.html#symbol-u32">u32</a> index = APPEND_CHILD);</code></pre></div>

<p>The same contract recorded into a command buffer while the world is locked: the parent change is applied at the flush, children and depth tags are repaired by the first <code><a href="scene.html">scene</a>.apply_hierarchy_changes</code> that runs after that flush, without a <code>WARN</code>. The repair entry waits (<code>pending_repair_count</code>) while the buffer is still unflushed. <code>A</code> <code>KEEP_WORLD</code> local transform is computed from the live poses at the time of this call.</p>

#### `destroy` {#function-destroy}

<div class="function-declaration" id="function-destroy-1"><pre><code><span class="k">void</span> destroy(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target);</code></pre></div>

<p>Queues the entity (and therefore its subtree) for <code>destroy_pending</code>. Queuing is <code>O(1)</code>: an entity queued twice is listed twice and dies once (the second request finds it dead).</p>

#### `destroy_now` {#function-destroy-now}

<div class="function-declaration" id="function-destroy-now-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; destroy_now(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target);</code></pre></div>

<p>Destroys the entity and its whole subtree now, through the ECS deferred buffer (no recursion). <code>INVALID_ENTITY</code> for a dead entity.</p>

#### `destroy_pending` {#function-destroy-pending}

<div class="function-declaration" id="function-destroy-pending-1"><pre><code><span class="k">void</span> destroy_pending();</code></pre></div>

<p>Body of <code><a href="scene.html">scene</a>.destroy_pending</code>: <code>destroy_now</code> for every queued entity.</p>

#### `duplicate_subtree` {#function-duplicate-subtree}

<div class="function-declaration" id="function-duplicate-subtree-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../../ecs/entity/entity.html">ecs::entity</a>&gt; duplicate_subtree(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> root,
    std::optional&lt;<a href="../../ecs/entity/entity.html">ecs::entity</a>&gt; parent = std::nullopt,
    <a href="../../core/types.html#symbol-u32">u32</a> index = APPEND_CHILD);</code></pre></div>

<p>Copies a subtree (fresh guids, scripts deep-copied by their own ops) and returns the new root. Without <code>parent</code> the copy lands under the source root&#x27;s own parent (a root copy for a root source); <code>NULL_ENTITY</code> makes it a <code><a href="scene.html">scene</a></code> root; any other entity becomes its parent. <code>index</code> is the sibling position (the root position for a root copy). <code>A</code> root copy belongs to the loaded <code><a href="scene.html">scene</a></code> asset that owns the source&#x27;s tree. <code>INVALID_ENTITY</code> for a dead source or a dead parent, <code>HIERARCHY_TOO_DEEP</code> when the copy would pass depth 31, <code>BAD_FORMAT</code> for a layer index of 32 or more in the subtree; on an error nothing is created.</p>

#### `ensure_guid` {#function-ensure-guid}

<div class="function-declaration" id="function-ensure-guid-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../../core/uuid/uuid.html">core::uuid</a>&gt; ensure_guid(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target);</code></pre></div>

<p>The entity&#x27;s guid, generated and indexed when it has none. <code>INVALID_ENTITY</code> for a dead entity.</p>

#### `find_by_layer` {#function-find-by-layer}

<div class="function-declaration" id="function-find-by-layer-1"><pre><code><span class="k">void</span> find_by_layer(
    <a href="../../core/types.html#symbol-u8">u8</a> value,
    std::vector&lt;<a href="../../ecs/entity/entity.html">ecs::entity</a>&gt; &amp;out) <span class="k">const</span>;</code></pre></div>

<p>Appends every entity carrying a layer component whose index equals <code>value</code> (world order, a scan of the layer column). Entities that never received a layer are not listed, although <code><a href="#function-layer-of-1">layer_of</a>()</code> reports 0 for them.</p>

#### `find_by_name` {#function-find-by-name}

<div class="function-declaration" id="function-find-by-name-1"><pre><code><span class="k">void</span> find_by_name(
    <a href="../../core/string-id/string-id.html">core::string_id</a> value,
    std::vector&lt;<a href="../../ecs/entity/entity.html">ecs::entity</a>&gt; &amp;out) <span class="k">const</span>;</code></pre></div>

<p>Appends every entity whose name equals <code>value</code> (world order, a full component scan).</p>

#### `find_by_tag` {#function-find-by-tag}

<div class="function-declaration" id="function-find-by-tag-1"><pre><code><span class="k">void</span> find_by_tag(
    <a href="../../core/string-id/string-id.html">core::string_id</a> value,
    std::vector&lt;<a href="../../ecs/entity/entity.html">ecs::entity</a>&gt; &amp;out) <span class="k">const</span>;</code></pre></div>

<p>Appends every entity whose tag equals <code>value</code>.</p>

#### `find_guid` {#function-find-guid}

<div class="function-declaration" id="function-find-guid-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../ecs/entity/entity.html">ecs::entity</a> find_guid(
    <span class="k">const</span> <a href="../../core/uuid/uuid.html">core::uuid</a> &amp;id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The entity carrying the guid, or <code>NULL_ENTITY</code>.</p>

#### `find_path` {#function-find-path}

<div class="function-declaration" id="function-find-path-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../ecs/entity/entity.html">ecs::entity</a> find_path(
    std::string_view path) <span class="k">const</span>;</code></pre></div>

<p>Resolves &quot;Root/Child/Leaf&quot; by names, first among the roots then among children. <code>NULL_ENTITY</code> when a segment does not match.</p>

#### `guid_of` {#function-guid-of}

<div class="function-declaration" id="function-guid-of-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../../core/uuid/uuid.html">core::uuid</a>&gt; guid_of(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The entity&#x27;s guid; nullopt when it has none (or a <code>null</code> one).</p>

#### `guids` {#function-guids}

<div class="function-declaration" id="function-guids-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../guid-index/guid-index.html">guid_index</a> &amp;guids() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `ids` {#function-ids}

<div class="function-declaration" id="function-ids-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../components/scene-component-ids.html">scene_component_ids</a> &amp;ids() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `instantiate_archive` {#function-instantiate-archive}

<div class="function-declaration" id="function-instantiate-archive-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="instantiate-report.html">instantiate_report</a>&gt; instantiate_archive(
    <span class="k">const</span> std::shared_ptr&lt;<span class="k">const</span> <a href="../../ecs/entity-archive/entity-archive.html">ecs::entity_archive</a>&gt; &amp;archive,
    <a href="../../core/types.html#symbol-u32">u32</a> count,
    <span class="k">const</span> <a href="instantiate-options.html">instantiate_options</a> &amp;options,
    std::span&lt;<a href="../../ecs/entity/entity.html">ecs::entity</a>&gt; outRoots);</code></pre></div>

<p>Instantiates <code>count</code> copies of an archive (3.9): validates guids, creates every entity in its final archetype, links children, depth tags, companions, dirty and inactive tags in one flush, applies the root overrides and binds asset references. <code>outRoots</code> receives the root (entry 0) of every copy and must hold at least <code>count</code> entries. Nothing is created on failure (<code>DUPLICATE_GUID</code>, <code>HIERARCHY_TOO_DEEP</code>, <code>HIERARCHY_CYCLE</code>, <code>BAD_FORMAT</code>, <code>ARCHIVE_CORRUPT</code>).</p>

#### `is_active` {#function-is-active}

<div class="function-declaration" id="function-is-active-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_active(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the entity and every ancestor are enabled.</p>

#### `is_enabled` {#function-is-enabled}

<div class="function-declaration" id="function-is-enabled-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_enabled(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the entity is alive and not disabled itself.</p>

#### `layer_of` {#function-layer-of}

<div class="function-declaration" id="function-layer-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u8">u8</a> layer_of(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The layer index, 0 when the entity has no layer component.</p>

#### `local_transform_of` {#function-local-transform-of}

<div class="function-declaration" id="function-local-transform-of-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../../core/math/transform/transform.html">math::transform</a>&gt; local_transform_of(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The local transform; nullopt when absent.</p>

#### `max_depth_in_use` {#function-max-depth-in-use}

<div class="function-declaration" id="function-max-depth-in-use-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u8">u8</a> max_depth_in_use() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The deepest level any entity has carried since the last <code><a href="#function-clear-1">clear</a>()</code> (0 = roots only).</p>

#### `name` {#function-name}

<div class="function-declaration" id="function-name-1"><pre><code>[[<span class="na">nodiscard</span>]] std::string_view name() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `name_of` {#function-name-of}

<div class="function-declaration" id="function-name-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/string-id/string-id.html">core::string_id</a> name_of(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The name, or the <code>null</code> id when the entity has none.</p>

#### `on_settings_changed` {#function-on-settings-changed}

<div class="function-declaration" id="function-on-settings-changed-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/event/event.html">core::event</a>&lt;&gt; &amp;on_settings_changed() <span class="k">noexcept</span>;</code></pre></div>

<p>Emitted by <code>set_settings</code> so engine subsystems re-read the keys they own (main thread).</p>

#### `on_structure_changed` {#function-on-structure-changed}

<div class="function-declaration" id="function-on-structure-changed-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/event/event.html">core::event</a>&lt;&gt; &amp;on_structure_changed() <span class="k">noexcept</span>;</code></pre></div>

<p>Emitted after every structural change (main thread).</p>

#### `pending_destroy_count` {#function-pending-destroy-count}

<div class="function-declaration" id="function-pending-destroy-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> pending_destroy_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Destroy requests waiting for <code>destroy_pending</code> (one per <code><a href="#function-destroy-1">destroy</a>()</code> call on a live entity).</p>

#### `pending_repair_count` {#function-pending-repair-count}

<div class="function-declaration" id="function-pending-repair-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> pending_repair_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Number of hierarchy repairs waiting for <code>apply_hierarchy_changes</code>.</p>

#### `resolve_guid` {#function-resolve-guid}

<div class="function-declaration" id="function-resolve-guid-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../../ecs/entity/entity.html">ecs::entity</a>&gt; resolve_guid(
    <span class="k">const</span> <a href="../../core/uuid/uuid.html">core::uuid</a> &amp;id) <span class="k">const</span>;</code></pre></div>

<p>The live entity carrying the guid; <code>GUID_NOT_FOUND</code> when none does (the editor&#x27;s entity arguments report this instead of a silent <code>null</code>).</p>

#### `root_owner` {#function-root-owner}

<div class="function-declaration" id="function-root-owner-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="scene-id.html">scene_id</a>&gt; root_owner(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> root) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The owner of a root; nullopt for an entity that is not a root.</p>

#### `roots` {#function-roots}

<div class="function-declaration" id="function-roots-1"><pre><code>[[<span class="na">nodiscard</span>]] std::span&lt;<span class="k">const</span> <a href="root-entry.html">root_entry</a>&gt; roots() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The roots in order with their owning <code><a href="scene.html">scene</a></code> ids.</p>

#### `set_enabled` {#function-set-enabled}

<div class="function-declaration" id="function-set-enabled-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_enabled(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <span class="k">bool</span> enabled);</code></pre></div>

<p>Toggles the entity&#x27;s own <code>disabled</code> tag and the <code>inactive</code> tag of its descendants.</p>

#### `set_guid` {#function-set-guid}

<div class="function-declaration" id="function-set-guid-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_guid(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <span class="k">const</span> <a href="../../core/uuid/uuid.html">core::uuid</a> &amp;value);</code></pre></div>

<p>Replaces the entity&#x27;s guid (adding the component when needed) and rebinds the index. <code>DUPLICATE_GUID</code> when another entity carries it.</p>

#### `set_layer` {#function-set-layer}

<div class="function-declaration" id="function-set-layer-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_layer(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <a href="../../core/types.html#symbol-u8">u8</a> value);</code></pre></div>

<p>Sets the layer (0..31; larger values are <code>OUT_OF_RANGE</code>).</p>

#### `set_local_transform` {#function-set-local-transform}

<div class="function-declaration" id="function-set-local-transform-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_local_transform(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <span class="k">const</span> <a href="../components/local-transform.html">local_transform</a> &amp;value);</code></pre></div>

<p>Writes the local transform (stamping the chunk). <code>NO_TRANSFORM</code> when the entity has none.</p>

<div class="function-declaration" id="function-set-local-transform-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_local_transform(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <span class="k">const</span> <a href="../../core/math/transform/transform.html">math::transform</a> &amp;value);</code></pre></div>

<p>The same write from a <code><a href="../../core/math/transform/transform.html">math::transform</a></code> (through <code>local_transform::from_transform</code>), so the result of <code>local_transform_of</code> feeds it directly. <code>A</code> bare {} argument is ambiguous between the two overloads: name the type.</p>

#### `set_name` {#function-set-name}

<div class="function-declaration" id="function-set-name-1"><pre><code><span class="k">void</span> set_name(
    std::string_view value);</code></pre></div>

<p>Renames the <code><a href="scene.html">scene</a></code> (the file header&#x27;s &quot;name&quot;); not a structural change.</p>

<div class="function-declaration" id="function-set-name-2"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_name(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <a href="../../core/string-id/string-id.html">core::string_id</a> value);</code></pre></div>

<p>Sets the name (adding the component when needed). <code>INVALID_ENTITY</code> for a dead entity.</p>

#### `set_parent` {#function-set-parent}

<div class="function-declaration" id="function-set-parent-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_parent(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> child,
    <a href="../../ecs/entity/entity.html">ecs::entity</a> newParent,
    <a href="../scene.html#symbol-reparent-mode">reparent_mode</a> mode = reparent_mode::KEEP_WORLD,
    <a href="../../core/types.html#symbol-u32">u32</a> index = APPEND_CHILD);</code></pre></div>

<p>Reparents <code>child</code> under <code>newParent</code> (<code>null</code> = root) at <code>index</code>. <code>KEEP_WORLD</code> recomputes the local transform so the world transform stays put, <code>KEEP_LOCAL</code> keeps the local transform (the mode is ignored for a transformless child). <code>SELF_PARENT</code>, <code>HIERARCHY_CYCLE</code>, <code>HIERARCHY_TOO_DEEP</code> and <code>INVALID_ENTITY</code> leave everything unchanged.</p>

#### `set_root_order` {#function-set-root-order}

<div class="function-declaration" id="function-set-root-order-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_root_order(
    std::span&lt;<span class="k">const</span> <a href="../../ecs/entity/entity.html">ecs::entity</a>&gt; ordered);</code></pre></div>

<p>Puts the listed roots, in the given order, into the positions they collectively occupy; roots that are not listed keep their relative order. <code>INVALID_ENTITY</code> when an entry is not a root or appears twice.</p>

#### `set_root_owner` {#function-set-root-owner}

<div class="function-declaration" id="function-set-root-owner-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_root_owner(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> root,
    <a href="scene-id.html">scene_id</a> owner);</code></pre></div>

<p>Reassigns the owner of a root. <code>INVALID_ENTITY</code> when the entity is not a root.</p>

#### `set_settings` {#function-set-settings}

<div class="function-declaration" id="function-set-settings-1"><pre><code><span class="k">void</span> set_settings(
    <a href="../../core/json/document/json-document.html">core::json::document</a> value);</code></pre></div>

<p>Replaces the settings (a non-object root becomes an empty object) and emits <code>on_settings_changed</code>.</p>

#### `set_sibling_index` {#function-set-sibling-index}

<div class="function-declaration" id="function-set-sibling-index-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_sibling_index(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <a href="../../core/types.html#symbol-u32">u32</a> index);</code></pre></div>

<p>Moves the entity among its siblings (or among the roots); an index past the end is clamped to the last position. <code>INVALID_ENTITY</code> for a dead entity, <code>INVALID_SIBLING_INDEX</code> when the parent is dead.</p>

#### `set_tag` {#function-set-tag}

<div class="function-declaration" id="function-set-tag-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_tag(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <a href="../../core/string-id/string-id.html">core::string_id</a> value);</code></pre></div>

<p>Sets the tag (adding the component when needed). <code>INVALID_ENTITY</code> for a dead entity.</p>

#### `set_world_transform` {#function-set-world-transform}

<div class="function-declaration" id="function-set-world-transform-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; set_world_transform(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <span class="k">const</span> <a href="../../core/math/transform/transform.html">math::transform</a> &amp;value);</code></pre></div>

<p>Sets the local transform so that the world transform equals <code>value</code> (fresh ancestor walk).</p>

#### `settings` {#function-settings}

<div class="function-declaration" id="function-settings-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/json/document/json-document.html">core::json::document</a> &amp;settings() <span class="k">noexcept</span>;</code></pre></div>

<p>The opaque <code><a href="scene.html">scene</a></code> settings document (ambient, gravity, ... are owned by engine subsystems).</p>

<div class="function-declaration" id="function-settings-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../../core/json/document/json-document.html">core::json::document</a> &amp;settings() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `structure_version` {#function-structure-version}

<div class="function-declaration" id="function-structure-version-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u64">u64</a> structure_version() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Incremented once per structural operation (create, destroy, reparent, reorder, load, ...).</p>

#### `subtree_bounds` {#function-subtree-bounds}

<div class="function-declaration" id="function-subtree-bounds-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../../core/math/aabb/aabb.html">math::aabb</a>&gt; subtree_bounds(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span>;</code></pre></div>

<p>Union of the world bounds of the subtree; nullopt when nothing in it has bounds.</p>

#### `tag_of` {#function-tag-of}

<div class="function-declaration" id="function-tag-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/string-id/string-id.html">core::string_id</a> tag_of(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The tag, or the <code>null</code> id when the entity has none.</p>

#### `tree` {#function-tree}

<div class="function-declaration" id="function-tree-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../hierarchy/hierarchy.html">hierarchy</a> &amp;tree() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `validate` {#function-validate}

<div class="function-declaration" id="function-validate-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; validate() <span class="k">const</span>;</code></pre></div>

<p>Checks every invariant of section 6: parent/children agreement, root uniqueness, depth tags, inactive tags and the guid index bijection. <code>A</code> parentless entity must be a listed root only when it carries at least one <code><a href="scene.html">scene</a></code> component; raw ECS entities that never entered the <code><a href="scene.html">scene</a></code> are ignored. The message names the first violation.</p>

#### `world` {#function-world}

<div class="function-declaration" id="function-world-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../ecs/world/world.html">ecs::world</a> &amp;world() <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-world-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../../ecs/world/world.html">ecs::world</a> &amp;world() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `world_bounds` {#function-world-bounds}

<div class="function-declaration" id="function-world-bounds-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../../core/math/aabb/aabb.html">math::aabb</a>&gt; world_bounds(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span>;</code></pre></div>

<p><code><a href="../components/local-bounds.html">local_bounds</a></code> transformed by the world matrix; nullopt without bounds or transform.</p>

#### `world_transform_of` {#function-world-transform-of}

<div class="function-declaration" id="function-world-transform-of-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../../core/math/transform/transform.html">math::transform</a>&gt; world_transform_of(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span>;</code></pre></div>

<p>The exact world transform from a fresh ancestor walk; nullopt when the entity has no <code><a href="../components/local-transform.html">local_transform</a></code>.</p>

### Private

#### `add_companions` {#function-add-companions}

<div class="function-declaration" id="function-add-companions-1"><pre><code><span class="k">void</span> add_companions(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <span class="k">const</span> ecs::component_mask &amp;mask);</code></pre></div>

#### `add_dirty` {#function-add-dirty}

<div class="function-declaration" id="function-add-dirty-1"><pre><code><span class="k">void</span> add_dirty(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target);</code></pre></div>

#### `bump_structure` {#function-bump-structure}

<div class="function-declaration" id="function-bump-structure-1"><pre><code><span class="k">void</span> bump_structure();</code></pre></div>

#### `check_reparent` {#function-check-reparent}

<div class="function-declaration" id="function-check-reparent-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; check_reparent(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> child,
    <a href="../../ecs/entity/entity.html">ecs::entity</a> newParent) <span class="k">const</span>;</code></pre></div>

#### `create_entity_unannounced` {#function-create-entity-unannounced}

<div class="function-declaration" id="function-create-entity-unannounced-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../../ecs/entity/entity.html">ecs::entity</a>&gt; create_entity_unannounced(
    <a href="../../core/string-id/string-id.html">core::string_id</a> entityName,
    <a href="../../ecs/entity/entity.html">ecs::entity</a> parentEntity,
    <a href="../../core/types.html#symbol-u32">u32</a> index);</code></pre></div>

#### `destroy_unannounced` {#function-destroy-unannounced}

<div class="function-declaration" id="function-destroy-unannounced-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; destroy_unannounced(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target);</code></pre></div>

#### `erase_root` {#function-erase-root}

<div class="function-declaration" id="function-erase-root-1"><pre><code><span class="k">void</span> erase_root(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target);</code></pre></div>

#### `flush_scratch` {#function-flush-scratch}

<div class="function-declaration" id="function-flush-scratch-1"><pre><code><span class="k">void</span> flush_scratch();</code></pre></div>

#### `insert_root` {#function-insert-root}

<div class="function-declaration" id="function-insert-root-1"><pre><code><span class="k">void</span> insert_root(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <a href="../../core/types.html#symbol-u32">u32</a> index,
    <a href="scene-id.html">scene_id</a> owner);</code></pre></div>

#### `link_child` {#function-link-child}

<div class="function-declaration" id="function-link-child-1"><pre><code><span class="k">void</span> link_child(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> child,
    <a href="../../ecs/entity/entity.html">ecs::entity</a> parentEntity,
    <a href="../../core/types.html#symbol-u32">u32</a> index);</code></pre></div>

#### `link_instantiated` {#function-link-instantiated}

<div class="function-declaration" id="function-link-instantiated-1"><pre><code><span class="k">void</span> link_instantiated(
    <span class="k">const</span> <a href="../../ecs/entity-archive/entity-archive.html">ecs::entity_archive</a> &amp;archive,
    <span class="k">const</span> <a href="../scene.html">detail::archive_shape</a> &amp;shape,
    std::span&lt;<span class="k">const</span> <a href="../../ecs/entity/entity.html">ecs::entity</a>&gt; entities,
    <a href="../../core/types.html#symbol-u32">u32</a> copies,
    <span class="k">const</span> <a href="instantiate-options.html">instantiate_options</a> &amp;options);</code></pre></div>

#### `on_entity_destroyed` {#function-on-entity-destroyed}

<div class="function-declaration" id="function-on-entity-destroyed-1"><pre><code><span class="k">void</span> on_entity_destroyed(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <a href="../../ecs/command-buffer/command-buffer.html">ecs::command_buffer</a> &amp;deferred);</code></pre></div>

#### `on_guid_added` {#function-on-guid-added}

<div class="function-declaration" id="function-on-guid-added-1"><pre><code><span class="k">void</span> on_guid_added(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target);</code></pre></div>

#### `on_guid_removed` {#function-on-guid-removed}

<div class="function-declaration" id="function-on-guid-removed-1"><pre><code><span class="k">void</span> on_guid_removed(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target);</code></pre></div>

#### `on_local_transform_added` {#function-on-local-transform-added}

<div class="function-declaration" id="function-on-local-transform-added-1"><pre><code><span class="k">void</span> on_local_transform_added(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <a href="../../ecs/command-buffer/command-buffer.html">ecs::command_buffer</a> &amp;deferred);</code></pre></div>

#### `on_parent_added` {#function-on-parent-added}

<div class="function-declaration" id="function-on-parent-added-1"><pre><code><span class="k">void</span> on_parent_added(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target);</code></pre></div>

#### `on_parent_removed` {#function-on-parent-removed}

<div class="function-declaration" id="function-on-parent-removed-1"><pre><code><span class="k">void</span> on_parent_removed(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target);</code></pre></div>

#### `owner_of_tree` {#function-owner-of-tree}

<div class="function-declaration" id="function-owner-of-tree-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="scene-id.html">scene_id</a> owner_of_tree(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `parent_chain_inactive` {#function-parent-chain-inactive}

<div class="function-declaration" id="function-parent-chain-inactive-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> parent_chain_inactive(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> parentEntity) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `queue_repair` {#function-queue-repair}

<div class="function-declaration" id="function-queue-repair-1"><pre><code><span class="k">void</span> queue_repair(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target,
    <a href="scene-id.html">scene_id</a> owner);</code></pre></div>

#### `refresh_inactive_subtree` {#function-refresh-inactive-subtree}

<div class="function-declaration" id="function-refresh-inactive-subtree-1"><pre><code><span class="k">void</span> refresh_inactive_subtree(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> root,
    <span class="k">bool</span> ancestorInactive);</code></pre></div>

#### `register_observers` {#function-register-observers}

<div class="function-declaration" id="function-register-observers-1"><pre><code><span class="k">void</span> register_observers();</code></pre></div>

#### `renumber_children` {#function-renumber-children}

<div class="function-declaration" id="function-renumber-children-1"><pre><code><span class="k">void</span> renumber_children(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> parentEntity,
    <a href="../../core/types.html#symbol-usize">usize</a> from);</code></pre></div>

#### `repair_entity` {#function-repair-entity}

<div class="function-declaration" id="function-repair-entity-1"><pre><code><span class="k">void</span> repair_entity(
    <span class="k">const</span> <a href="scene-repair-entry.html">repair_entry</a> &amp;entry);</code></pre></div>

#### `retag_subtree` {#function-retag-subtree}

<div class="function-declaration" id="function-retag-subtree-1"><pre><code><span class="k">void</span> retag_subtree(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> root,
    <a href="../../core/types.html#symbol-u8">u8</a> newDepth);</code></pre></div>

#### `root_index_of` {#function-root-index-of}

<div class="function-declaration" id="function-root-index-of-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-usize">usize</a> root_index_of(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> target) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `unlink_child` {#function-unlink-child}

<div class="function-declaration" id="function-unlink-child-1"><pre><code><span class="k">void</span> unlink_child(
    <a href="../../ecs/entity/entity.html">ecs::entity</a> child,
    <a href="../../ecs/entity/entity.html">ecs::entity</a> parentEntity);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
