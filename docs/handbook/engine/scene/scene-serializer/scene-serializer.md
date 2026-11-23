---
kind: class
---

[Engine](../../../engine.md) → [Scene](../../scene.md) → [scene_serializer.h](../scene-serializer.md)

# scene_serializer
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::scene` |
| Include File Path | `engine/include/conjure/scene/scene_serializer.h` |
| Source File Path | `engine/src/scene/scene_serializer.cpp` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../../api/scene/index.md), [Scene files, prefabs and snapshots](../../../../api/scene/scene-files-and-prefabs.md) |

## Syntax

```cpp
class CONJURE_API scene_serializer
```









## Functions

### Public

#### `bind_asset_refs` {#function-bind-asset-refs}

<div class="function-declaration" id="function-bind-asset-refs-1"><pre><code><span class="k">static</span> <span class="k">void</span> bind_asset_refs(
    <a href="../../ecs/world/world.html">ecs::world</a> &amp;target,
    std::span&lt;<span class="k">const</span> <a href="../../ecs/entity/entity.html">ecs::entity</a>&gt; entities,
    <a href="../../assets/asset-manager/asset-manager.html">assets::asset_manager</a> &amp;assets,
    std::vector&lt;<a href="../../assets/asset-types.html#symbol-asset-id">assets::asset_id</a>&gt; *outIds);</code></pre></div>

<p>Binds every non-<code>null</code> <code><a href="../../assets/asset-ref/asset-ref.html">asset_ref</a></code> field of the entities through <code>asset_manager::bind_slot</code> (never loads) and appends the bound ids to <code>outIds</code> when given.</p>

#### `bind_entity` {#function-bind-entity}

<div class="function-declaration" id="function-bind-entity-1"><pre><code><span class="k">static</span> <span class="k">void</span> bind_entity(
    <a href="../../ecs/world/world.html">ecs::world</a> &amp;target,
    <a href="../../ecs/entity/entity.html">ecs::entity</a> entity,
    <a href="../../assets/asset-manager/asset-manager.html">assets::asset_manager</a> &amp;assets);</code></pre></div>

<p><code>bind_asset_refs</code> for one entity (the editor after <code>component_from_json</code>).</p>

#### `capture_subtree` {#function-capture-subtree}

<div class="function-declaration" id="function-capture-subtree-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="../../ecs/entity-archive/entity-archive.html">ecs::entity_archive</a>&gt; capture_subtree(
    <span class="k">const</span> <a href="../scene/scene.html">scene</a> &amp;source,
    <a href="../../ecs/entity/entity.html">ecs::entity</a> root,
    <a href="../../ecs/entity-archive/archive-options.html">ecs::archive_options</a> options = {});</code></pre></div>

<p>Captures a subtree in hierarchy order (root first). Without keepExternalReferences the root&#x27;s parent link is dropped so entry 0 is a <code>true</code> root; with it the link is kept for undo payloads. <code>INVALID_ENTITY</code> for a dead root; <code>BAD_FORMAT</code> naming the entity when one carries a layer index of 32 or more, which instantiation would refuse (so <code>duplicate_subtree</code>, <code>save_subtree</code> and <code>prefab::build_document</code>, diff and apply refuse it too).</p>

#### `collect_hierarchy_order` {#function-collect-hierarchy-order}

<div class="function-declaration" id="function-collect-hierarchy-order-1"><pre><code><span class="k">static</span> <span class="k">void</span> collect_hierarchy_order(
    <span class="k">const</span> <a href="../scene/scene.html">scene</a> &amp;source,
    std::vector&lt;<a href="../../ecs/entity/entity.html">ecs::entity</a>&gt; &amp;out);</code></pre></div>

<p>Every entity of the tree in hierarchy order: roots in root order, each subtree depth first in sibling order.</p>

#### `instantiate_subtree` {#function-instantiate-subtree}

<div class="function-declaration" id="function-instantiate-subtree-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="../../ecs/entity/entity.html">ecs::entity</a>&gt; instantiate_subtree(
    <a href="../scene/scene.html">scene</a> &amp;target,
    <span class="k">const</span> <a href="../../core/json/value/json-value.html">core::json::value</a> &amp;in,
    <a href="../../ecs/entity/entity.html">ecs::entity</a> parent,
    <span class="k">bool</span> keepGuids,
    <a href="../../core/types.html#symbol-u32">u32</a> index = APPEND_CHILD);</code></pre></div>

<p>Parses a prefab-format value and instantiates it under <code>parent</code> at <code>index</code>; <code>keepGuids</code> keeps the file&#x27;s guids (<code>DUPLICATE_GUID</code> on a collision), otherwise every guid is regenerated.</p>

#### `load_scene` {#function-load-scene}

<div class="function-declaration" id="function-load-scene-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="../scene/instantiate-report.html">instantiate_report</a>&gt; load_scene(
    <a href="../scene/scene.html">scene</a> &amp;target,
    <span class="k">const</span> <a href="scene-document.html">scene_document</a> &amp;document,
    <a href="../scene/scene-id.html">scene_id</a> owner,
    <a href="../scene.html#symbol-guid-policy">guid_policy</a> guids);</code></pre></div>

<p>Instantiates a parsed scene into the target (one copy, owner recorded on the new roots) and orders the roots as the header lists them (missing guids skipped with <code>WARN</code>, unlisted roots appended with <code>WARN</code>).</p>

#### `load_scene_file` {#function-load-scene-file}

<div class="function-declaration" id="function-load-scene-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="../scene/instantiate-report.html">instantiate_report</a>&gt; load_scene_file(
    <a href="../scene/scene.html">scene</a> &amp;target,
    <a href="../../core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> &amp;fs,
    std::string_view path,
    <a href="../scene/scene-id.html">scene_id</a> owner,
    <a href="../scene.html#symbol-guid-policy">guid_policy</a> guids);</code></pre></div>

<p>Reads, parses and loads a file from the file system.</p>

#### `parse_document` {#function-parse-document}

<div class="function-declaration" id="function-parse-document-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="scene-document.html">scene_document</a>&gt; parse_document(
    <span class="k">const</span> <a href="../../core/json/value/json-value.html">core::json::value</a> &amp;in,
    <span class="k">const</span> <a href="../../ecs/component-registry/component-registry.html">ecs::component_registry</a> &amp;registry);</code></pre></div>

<p>Validates the header (<code>BAD_FORMAT</code>, <code>UNSUPPORTED_VERSION</code>) and reads the archive against the registry (unknown components preserved, <code>SERIALIZATION_FAILED</code> for malformed ones).</p>

#### `parse_prefab` {#function-parse-prefab}

<div class="function-declaration" id="function-parse-prefab-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<a href="prefab-document.html">prefab_document</a>&gt; parse_prefab(
    <span class="k">const</span> <a href="../../core/json/value/json-value.html">core::json::value</a> &amp;in,
    <span class="k">const</span> <a href="../../ecs/component-registry/component-registry.html">ecs::component_registry</a> &amp;registry);</code></pre></div>

<p>Like <code>parse_document</code> for the prefab format; <code>BAD_FORMAT</code> for an empty archive, <code>PREFAB_MULTIPLE_ROOTS</code> unless entry 0 is the only entry without a parent, <code>DUPLICATE_GUID</code> when two entries share a guid.</p>

#### `save_scene` {#function-save-scene}

<div class="function-declaration" id="function-save-scene-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; save_scene(
    <span class="k">const</span> <a href="../scene/scene.html">scene</a> &amp;source,
    <a href="../../core/json/writer/json-writer.html">core::json::writer</a> &amp;out);</code></pre></div>

<p>Writes the scene (header keys format, version, name, settings, roots, archive) into the writer; every entity of the tree in hierarchy order. <code>BAD_FORMAT</code> naming the entity, and nothing written, when one carries a layer index of 32 or more (loading would refuse it).</p>

#### `save_scene_file` {#function-save-scene-file}

<div class="function-declaration" id="function-save-scene-file-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; save_scene_file(
    <span class="k">const</span> <a href="../scene/scene.html">scene</a> &amp;source,
    <a href="../../core/vfs/file-system/vfs-file-system.html">core::vfs::file_system</a> &amp;fs,
    std::string_view path);</code></pre></div>

<p><code>save_scene</code> into a file written atomically.</p>

#### `save_subtree` {#function-save-subtree}

<div class="function-declaration" id="function-save-subtree-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; save_subtree(
    <span class="k">const</span> <a href="../scene/scene.html">scene</a> &amp;source,
    <a href="../../ecs/entity/entity.html">ecs::entity</a> root,
    <a href="../../core/json/writer/json-writer.html">core::json::writer</a> &amp;out);</code></pre></div>

<p>Writes a subtree in the prefab format (the root keeps its name).</p>

#### `write_document` {#function-write-document}

<div class="function-declaration" id="function-write-document-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; write_document(
    <span class="k">const</span> <a href="scene-document.html">scene_document</a> &amp;document,
    <a href="../../core/json/writer/json-writer.html">core::json::writer</a> &amp;out);</code></pre></div>

<p>Writes the scene format from a document (used by the importer to canonicalise a file).</p>

#### `write_prefab` {#function-write-prefab}

<div class="function-declaration" id="function-write-prefab-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">static</span> <a href="../../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; write_prefab(
    <span class="k">const</span> <a href="prefab-document.html">prefab_document</a> &amp;document,
    <a href="../../core/json/writer/json-writer.html">core::json::writer</a> &amp;out);</code></pre></div>

<p>Writes the prefab format (format, version, name, archive).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="destructors"></a>
<a id="variables"></a>
<!-- doc-legacy-anchors:end -->
