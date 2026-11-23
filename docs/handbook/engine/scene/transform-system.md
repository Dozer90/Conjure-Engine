---
kind: header
---

[Engine](../../engine.md) → [Scene](../scene.md)

# transform_system.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/scene/transform_system.h` |
| API ↗ | [Scenes, hierarchy and prefabs](../../../api/scene/index.md), [Scenes and the hierarchy](../../../api/scene/scenes-and-hierarchy.md) |

<p>Transform propagation: the body of <code>scene.propagate_transforms</code>. One job dispatch per hierarchy level (roots first), so the parent of every entity in a level has been finished by the previous level&#x27;s join. <code>A</code> chunk is recomputed as a whole when its <code><a href="components/local-transform.html">local_transform</a></code> column changed since the previous run, when it carries <code><a href="components/transform-dirty.html">transform_dirty</a></code> or when it holds interpolated poses; otherwise only its rows marked <code>transform_state::pending</code> are recomputed, the mark a parent&#x27;s pass left when it recomputed its own world matrix (the <code>transform_state::lastUpdateRun</code> stamp), sits in an interpolated subtree or changed flags. Clean rows are never looked up: a level scans the state column of its chunks, collects the chunks with work and splits them into jobs by dirty rows. <code><a href="components/world-transform.html">world_transform</a></code> and <code><a href="components/render-world-transform.html">render_world_transform</a></code> are written in the same visit; dirty tags are removed through the system&#x27;s command buffer after the level join. Allocation-free once the queries have seen every archetype and the level scratch has seen its largest level.</p>

## Structs

| File | Description |
|---|---|
| [`propagation_stats`](transform-system/propagation-stats.md) | Counters of the last run. |
| [`transform_system::chunk_tally`](transform-system/transform-system-chunk-tally.md) | One job range&#x27;s counts, kept on its own stack so the shared counters&#x27; cache line is written once per range instead of four times per chunk. |
| [`transform_system::level_chunk`](transform-system/transform-system-level-chunk.md) | One chunk of the level being processed that has rows to recompute. |
| [`transform_system::run_counters`](transform-system/transform-system-run-counters.md) | Relaxed counters each job range adds its local tally to once (<code>chunksSkipped</code> comes from the serial collect step); copied into <code>mStats</code> after the run. |

## Classes

| File | Description |
|---|---|
| [`transform_system`](transform-system/transform-system.md) |  |

## Functions

### Public

#### `compute_world_transform` {#function-compute-world-transform}

<div class="function-declaration" id="function-compute-world-transform-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/math/transform/transform.html">math::transform</a> compute_world_transform(
    <span class="k">const</span> <a href="scene/scene.html">scene</a> &amp;target,
    <a href="../ecs/entity/entity.html">ecs::entity</a> entity);</code></pre></div>

<p>Exact world transform of an entity from a fresh ancestor walk. An entity or ancestor without <code><a href="components/local-transform.html">local_transform</a></code> contributes the identity and the walk continues upwards, so a transformless entity yields its nearest transformed ancestor&#x27;s world (the identity when there is none).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="compute_world_transform"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="propagation_stats"></a>
<a id="propagation_statschunksskipped"></a>
<a id="propagation_statschunksvisited"></a>
<a id="propagation_statsentitiesvisited"></a>
<a id="propagation_statslevels"></a>
<a id="propagation_statsrenderrecomputed"></a>
<a id="propagation_statsworldrecomputed"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="transform_system"></a>
<a id="transform_systemchunk_tally"></a>
<a id="transform_systemchunk_tallychunks"></a>
<a id="transform_systemchunk_tallyrenderwrites"></a>
<a id="transform_systemchunk_tallyvisited"></a>
<a id="transform_systemchunk_tallyworldwrites"></a>
<a id="transform_systemcollect_chunk"></a>
<a id="transform_systemdescribe"></a>
<a id="transform_systeminterpolation_enabled"></a>
<a id="transform_systemlast_stats"></a>
<a id="transform_systemlevel_chunk"></a>
<a id="transform_systemlevel_chunkrecomputeall"></a>
<a id="transform_systemlevel_chunkremovedirty"></a>
<a id="transform_systemlevel_chunkrows"></a>
<a id="transform_systemlevel_chunkview"></a>
<a id="transform_systemlevel_chunkwhole"></a>
<a id="transform_systemmark_children_pending"></a>
<a id="transform_systemmcounters"></a>
<a id="transform_systemminterpolationenabled"></a>
<a id="transform_systemmjobbounds"></a>
<a id="transform_systemmlevelchunks"></a>
<a id="transform_systemmlevelqueries"></a>
<a id="transform_systemmrootquery"></a>
<a id="transform_systemmruncounter"></a>
<a id="transform_systemmscene"></a>
<a id="transform_systemmstats"></a>
<a id="transform_systemoperator"></a>
<a id="transform_systemprocess_chunk"></a>
<a id="transform_systemrun"></a>
<a id="transform_systemrun_count"></a>
<a id="transform_systemrun_counters"></a>
<a id="transform_systemrun_counterschunksskipped"></a>
<a id="transform_systemrun_counterschunksvisited"></a>
<a id="transform_systemrun_countersentitiesvisited"></a>
<a id="transform_systemrun_countersrenderrecomputed"></a>
<a id="transform_systemrun_countersworldrecomputed"></a>
<a id="transform_systemrun_level"></a>
<a id="transform_systemset_interpolation_enabled"></a>
<a id="transform_systemsystem_name"></a>
<a id="transform_systemtransform_system"></a>
<a id="transform_systemtransform_system_1"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
