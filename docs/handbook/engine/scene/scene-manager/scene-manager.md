---
kind: class
---

[Engine](../../../engine.md) → [Scene](../../scene.md) → [scene_manager.h](../scene-manager.md)

# scene_manager
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::scene` |
| Include File Path | `engine/include/conjure/scene/scene_manager.h` |
| Source File Path | `engine/src/scene/scene_manager.cpp` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../../api/scene/index.md), [Scene files, prefabs and snapshots](../../../../api/scene/scene-files-and-prefabs.md) |

## Syntax

```cpp
class CONJURE_API scene_manager
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">scene_manager(
    <a href="../../ecs/world/world.html">ecs::world</a> &amp;world,
    <a href="../../assets/asset-manager/asset-manager.html">assets::asset_manager</a> &amp;assets);</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Copy constructor and assignment has been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~scene_manager();</code></td><td>Drops the load records (and their asset handles); the scene&#x27;s entities stay in the world.</td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mAssets</code></td><td><code class="signature"><a href="../../assets/asset-manager/asset-manager.html">assets::asset_manager</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLayers</code></td><td><code class="signature"><a href="layer-table.html">layer_table</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mLoadOrder</code></td><td><code class="signature">std::vector&lt;<a href="../../core/types.html#symbol-u32">u32</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Next<wbr>Generation</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">2</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mNextRequest</code></td><td><code class="signature"><a href="../../core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">1</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>On<wbr>Before<wbr>Unload</code></td><td><code class="signature"><a href="../../core/event/event.html">core::<wbr>event</a>&lt;<wbr>scene_<wbr>id,<wbr> std::<wbr>span&lt;<wbr><span class="k">const</span> <a href="../../ecs/entity/entity.html">ecs::<wbr>entity</a>&gt;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mOnLoaded</code></td><td><code class="signature"><a href="../../core/event/event.html">core::event</a>&lt;<a href="../scene/scene-id.html">scene_id</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>On<wbr>Load<wbr>Failed</code></td><td><code class="signature"><a href="../../core/event/event.html">core::<wbr>event</a>&lt;<wbr>load_<wbr>request_<wbr>id,<wbr> <span class="k">const</span> <a href="../../core/error/error.html">core::<wbr>error</a> &amp;&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPending</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="scene-manager-pending-load.html">pending_<wbr>load</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Pending<wbr>Unloads</code></td><td><code class="signature">std::vector&lt;<a href="../scene/scene-id.html">scene_id</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRecords</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="scene-manager-loaded-record.html">loaded_<wbr>record</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mScene</code></td><td><code class="signature"><a href="../scene/scene.html">scene</a></code></td><td>—</td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTransforms</code></td><td><code class="signature"><a href="../transform-system/transform-system.html">transform_system</a></code></td><td>—</td><td></td></tr>
<tr><td>private</td><td><code class="signature">mWorld</code></td><td><code class="signature"><a href="../../ecs/world/world.html">ecs::world</a> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `active` {#function-active}

<div class="function-declaration" id="function-active-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../scene/scene.html">scene</a> &amp;active() <span class="k">noexcept</span>;</code></pre></div>

<p>The one scene over the world.</p>

<div class="function-declaration" id="function-active-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../scene/scene.html">scene</a> &amp;active() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `apply_pending_loads` {#function-apply-pending-loads}

<div class="function-declaration" id="function-apply-pending-loads-1"><pre><code><span class="k">void</span> apply_pending_loads();</code></pre></div>

<p>Body of <code>scene.apply_pending_loads</code>: pending unloads, then pending loads in request order (a <code>FAILED</code> asset drops its request with ERROR and <code>on_load_failed</code>; a load still in flight stops the pass).</p>

#### `assets` {#function-assets}

<div class="function-declaration" id="function-assets-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../assets/asset-manager/asset-manager.html">assets::asset_manager</a> &amp;assets() <span class="k">noexcept</span>;</code></pre></div>

<p>The asset manager the scene binds through and loads scene assets from.</p>

#### `find_loaded` {#function-find-loaded}

<div class="function-declaration" id="function-find-loaded-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="../scene/scene-id.html">scene_id</a>&gt; find_loaded(
    <span class="k">const</span> <a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a> &amp;asset) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The loaded scene created from an asset, or nullopt.</p>

#### `info` {#function-info}

<div class="function-declaration" id="function-info-1"><pre><code>[[<span class="na">nodiscard</span>]] std::optional&lt;<a href="loaded-scene-info.html">loaded_scene_info</a>&gt; info(
    <a href="../scene/scene-id.html">scene_id</a> id) <span class="k">const</span>;</code></pre></div>

<p>The record of a loaded scene with its current roots (gathered on each call), or nullopt.</p>

#### `is_load_pending` {#function-is-load-pending}

<div class="function-declaration" id="function-is-load-pending-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_load_pending(
    <span class="k">const</span> <a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a> &amp;id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while a request for the asset waits.</p>

#### `is_loaded` {#function-is-loaded}

<div class="function-declaration" id="function-is-loaded-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_loaded(
    <a href="../scene/scene-id.html">scene_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True while the id names a loaded scene (unload and <code>REPLACE</code> loads retire ids for good).</p>

#### `layers` {#function-layers}

<div class="function-declaration" id="function-layers-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="layer-table.html">layer_table</a> &amp;layers() <span class="k">noexcept</span>;</code></pre></div>

<p>The layer names, filled by the engine from <code>project.json</code>.</p>

<div class="function-declaration" id="function-layers-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="layer-table.html">layer_table</a> &amp;layers() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `load` {#function-load}

<div class="function-declaration" id="function-load-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../scene/scene-id.html">scene_id</a>&gt; load(
    <span class="k">const</span> <a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a> &amp;id,
    <a href="../scene-manager.html#symbol-load-mode">load_mode</a> mode = load_mode::REPLACE);</code></pre></div>

<p>Loads a scene asset synchronously (<code>load_blocking</code>, dependencies included) and applies it. <code>LOAD_FAILED</code> when the asset is not a scene or failed to load; <code>SCENE_NOT_LOADED</code> when an <code>on_structure_changed</code> handler unloads the scene before its load finishes.</p>

#### `load_file` {#function-load-file}

<div class="function-declaration" id="function-load-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../scene/scene-id.html">scene_id</a>&gt; load_file(
    <a href="../../core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> &amp;fs,
    std::string_view path,
    <a href="../scene-manager.html#symbol-load-mode">load_mode</a> mode = load_mode::REPLACE);</code></pre></div>

<p>Parses a scene file from the file system, applies it and loads the referenced assets as a batch (the record keeps the handles). <code>SCENE_NOT_LOADED</code> as for load.</p>

#### `loaded_scenes` {#function-loaded-scenes}

<div class="function-declaration" id="function-loaded-scenes-1"><pre><code>[[<span class="na">nodiscard</span>]] std::vector&lt;<a href="loaded-scene-info.html">loaded_scene_info</a>&gt; loaded_scenes() <span class="k">const</span>;</code></pre></div>

<p>Every loaded scene in load order, with its current roots.</p>

#### `on_before_unload` {#function-on-before-unload}

<div class="function-declaration" id="function-on-before-unload-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/event/event.html">core::event</a>&lt;scene_id, std::span&lt;<span class="k">const</span> <a href="../../ecs/entity/entity.html">ecs::entity</a>&gt;&gt; &amp;on_before_unload() <span class="k">noexcept</span>;</code></pre></div>

<p>Fires before a scene&#x27;s entities die with the roots about to be destroyed (scripting runs <code>destroy_all</code> here); <code>unload_all</code> fires it once more with <code>SCENE_ID_AUTHORED</code> and the remaining roots when any entity is left. Handlers may load scenes; one loaded during <code>unload_all</code> is cleared and retired with the rest.</p>

#### `on_load_failed` {#function-on-load-failed}

<div class="function-declaration" id="function-on-load-failed-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/event/event.html">core::event</a>&lt;load_request_id, <span class="k">const</span> <a href="../../core/error/error.html">core::error</a> &amp;&gt; &amp;on_load_failed() <span class="k">noexcept</span>;</code></pre></div>

<p>Fires when a deferred request is dropped: the asset <code>FAILED</code>, is not a scene, or could not be applied.</p>

#### `on_loaded` {#function-on-loaded}

<div class="function-declaration" id="function-on-loaded-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/event/event.html">core::event</a>&lt;<a href="../scene/scene-id.html">scene_id</a>&gt; &amp;on_loaded() <span class="k">noexcept</span>;</code></pre></div>

<p>Fires once a load (synchronous or deferred) has instantiated and ordered its roots.</p>

#### `pending_count` {#function-pending-count}

<div class="function-declaration" id="function-pending-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> pending_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Requests not yet applied.</p>

#### `request_load` {#function-request-load}

<div class="function-declaration" id="function-request-load-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="load-request-id.html">load_request_id</a> request_load(
    <span class="k">const</span> <a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a> &amp;id,
    <a href="../scene-manager.html#symbol-load-mode">load_mode</a> mode = load_mode::ADDITIVE);</code></pre></div>

<p>Queues an asynchronous load applied by <code>apply_pending_loads</code> once the asset is resident.</p>

#### `request_unload` {#function-request-unload}

<div class="function-declaration" id="function-request-unload-1"><pre><code><span class="k">void</span> request_unload(
    <a href="../scene/scene-id.html">scene_id</a> id);</code></pre></div>

<p>Queues an unload applied at the next <code>apply_pending_loads</code>; unknown ids are ignored.</p>

#### `transforms` {#function-transforms}

<div class="function-declaration" id="function-transforms-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../transform-system/transform-system.html">transform_system</a> &amp;transforms() <span class="k">noexcept</span>;</code></pre></div>

<p>The propagation system <code>register_systems</code> adds to the scheduler (the engine toggles its interpolation from <code><a href="../../engine/engine-time/engine-time.html">engine_time</a>.interpolatePhysics</code>).</p>

#### `unload` {#function-unload}

<div class="function-declaration" id="function-unload-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; unload(
    <a href="../scene/scene-id.html">scene_id</a> id);</code></pre></div>

<p>Emits <code>on_before_unload</code>, destroys the scene&#x27;s roots (reverse order), drops the asset handles and runs <code>asset_manager::collect</code>. <code>SCENE_NOT_LOADED</code> for an unknown id.</p>

#### `unload_all` {#function-unload-all}

<div class="function-declaration" id="function-unload-all-1"><pre><code><span class="k">void</span> unload_all();</code></pre></div>

<p>Unloads every scene in reverse load order, then clears the remaining authored entities; <code>unload_all</code> fires <code>on_before_unload</code> once more with <code>SCENE_ID_AUTHORED</code> and the remaining roots when any entity is left. <code>A</code> scene an <code>on_before_unload</code> handler loads meanwhile is cleared and retired with the rest, so no scene is loaded afterwards.</p>

### Private

#### `allocate_record` {#function-allocate-record}

<div class="function-declaration" id="function-allocate-record-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/types.html#symbol-u32">u32</a> allocate_record();</code></pre></div>

#### `apply_document` {#function-apply-document}

<div class="function-declaration" id="function-apply-document-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../core/result/result.html">core::result</a>&lt;<a href="../scene/scene-id.html">scene_id</a>&gt; apply_document(
    <span class="k">const</span> <a href="../scene-serializer/scene-document.html">scene_document</a> &amp;document,
    <a href="../scene-manager.html#symbol-load-mode">load_mode</a> mode,
    <a href="../../assets/asset-manager/asset-load-batch.html">assets::asset_load_batch</a> batch,
    <span class="k">const</span> <a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a> &amp;asset,
    std::string path);</code></pre></div>

#### `collect_owned_roots` {#function-collect-owned-roots}

<div class="function-declaration" id="function-collect-owned-roots-1"><pre><code><span class="k">void</span> collect_owned_roots(
    <a href="../scene/scene-id.html">scene_id</a> id,
    std::vector&lt;<a href="../../ecs/entity/entity.html">ecs::entity</a>&gt; &amp;out) <span class="k">const</span>;</code></pre></div>

#### `find_record` {#function-find-record}

<div class="function-declaration" id="function-find-record-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="scene-manager-loaded-record.html">loaded_record</a> *find_record(
    <a href="../scene/scene-id.html">scene_id</a> id) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-find-record-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="scene-manager-loaded-record.html">loaded_record</a> *find_record(
    <a href="../scene/scene-id.html">scene_id</a> id) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
