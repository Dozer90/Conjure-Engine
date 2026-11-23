---
kind: header
---

[Engine](../../engine.md) → [Scene](../scene.md)

# scene_snapshot.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scene/scene_snapshot.h` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../api/scene/index.md), [Scene files, prefabs and snapshots](../../../api/scene/scene-files-and-prefabs.md) |

<p>Everything needed to put a scene back the way it was. Move-only; owns its bytes.</p>

## Structs

| File | Description |
|---|---|
| [`scene_snapshot`](scene-snapshot/scene-snapshot.md) | Everything needed to put a scene back the way it was. Move-only; owns its bytes. |

## Functions

### Public

#### `capture` {#function-capture}

<div class="function-declaration" id="function-capture-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="scene-snapshot/scene-snapshot.html">scene_snapshot</a>&gt; capture(
    <span class="k">const</span> <a href="scene/scene.html">scene</a> &amp;source);</code></pre></div>

<p>Captures every entity of the tree, the root order and owners, the settings and the name. <code>BAD_FORMAT</code> naming the entity when one carries a layer index of 32 or more, which restore would refuse only after clearing the scene.</p>

#### `restore` {#function-restore}

<div class="function-declaration" id="function-restore-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; restore(
    <a href="scene/scene.html">scene</a> &amp;target,
    <span class="k">const</span> <a href="scene-snapshot/scene-snapshot.html">scene_snapshot</a> &amp;snapshot);</code></pre></div>

<p>Clears the scene and instantiates the snapshot with kept guids, restores settings, name, root order and owners and binds asset references. On failure the scene is left empty.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="capture"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="restore"></a>
<a id="scene_snapshot"></a>
<a id="scene_snapshotarchive"></a>
<a id="scene_snapshotentity_count"></a>
<a id="scene_snapshotname"></a>
<a id="scene_snapshotoperator"></a>
<a id="scene_snapshotrootentries"></a>
<a id="scene_snapshotrootowners"></a>
<a id="scene_snapshotscene_snapshot"></a>
<a id="scene_snapshotscene_snapshot_1"></a>
<a id="scene_snapshotsettings"></a>
<a id="scene_snapshotstructureversion"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
