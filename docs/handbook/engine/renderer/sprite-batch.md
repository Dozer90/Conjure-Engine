---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# sprite_batch.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/sprite_batch.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [Rendering a frame](../../../api/renderer/rendering-a-frame.md) |

<p>The 2D <code><a href="sprite-batch/sprite.html">sprite</a></code> batcher: pure CPU, public so tests build batches by hand. <code>WORLD</code> sprites become world-space quads drawn into the HDR target after blended meshes; <code>SCREEN</code> sprites become pixel-space quads (origin top-left, y down) drawn after tonemapping.</p>

## Structs

| File | Description |
|---|---|
| [`sprite`](sprite-batch/sprite.md) | One <code><a href="sprite-batch/sprite.html">sprite</a></code> as the extraction contract carries it (<code>render_scene::sprites</code>). |
| [`sprite_run`](sprite-batch/sprite-run.md) | <code>A</code> run of consecutive sprites sharing a space and a texture: one drawIndexed each. |
| [`sprite_vertex`](sprite-batch/sprite-vertex.md) | One quad vertex as the GPU consumes it (<code>gpu_sprite_vertex</code>, 32 bytes). |

## Classes

| File | Description |
|---|---|
| [`sprite_batcher`](sprite-batch/sprite-batcher.md) | Sorts sprites by (space, layer, order, texture), emits 4 vertices and 6 indices per <code><a href="sprite-batch/sprite.html">sprite</a></code> and splits the result into runs. The capacity is <code>MAX_SPRITES_PER_FRAME</code> and <code>SCREEN</code> sprites (the HUD) take their slots first: beyond it the <code>WORLD</code> sprites last in sort order (the front-most) are dropped, then, only when <code>SCREEN</code> sprites alone exceed the cap, the last <code>SCREEN</code> sprites. Dropped sprites are counted by every build (<code>dropped_count()</code>). <code>A</code> <code>WARN</code> naming the count per space is logged by the build in which the overflow starts: the builds that follow stay silent while they drop sprites, whatever the count becomes, and a build that drops none (an empty one included) arms the warning again. <code>clear()</code> does not arm it. |

## Enumerations

### `sprite_space` {#symbol-sprite-space}

```cpp
enum class sprite_space : u8 {
    WORLD = 0,
    SCREEN
};
```

## Functions

### Public

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="sprite-batch.html#symbol-sprite-space">sprite_space</a> space) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of a <code><a href="sprite-batch/sprite.html">sprite</a></code> space (&quot;<code>WORLD</code>&quot;).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="sprite"></a>
<a id="sprite_batcher"></a>
<a id="sprite_batcherbuild"></a>
<a id="sprite_batcherclear"></a>
<a id="sprite_batcherdropped_count"></a>
<a id="sprite_batcherindices"></a>
<a id="sprite_batchermdropped"></a>
<a id="sprite_batchermindices"></a>
<a id="sprite_batchermkeys"></a>
<a id="sprite_batchermorder"></a>
<a id="sprite_batchermoverflowwarned"></a>
<a id="sprite_batchermruns"></a>
<a id="sprite_batchermscratchkeys"></a>
<a id="sprite_batchermscratchvalues"></a>
<a id="sprite_batchermvertices"></a>
<a id="sprite_batcheroperator"></a>
<a id="sprite_batcherruns"></a>
<a id="sprite_batchersort_key"></a>
<a id="sprite_batchersprite_batcher"></a>
<a id="sprite_batchersprite_batcher_1"></a>
<a id="sprite_batchersprite_count"></a>
<a id="sprite_batchervertices"></a>
<a id="sprite_run"></a>
<a id="sprite_runfirstindex"></a>
<a id="sprite_runindexcount"></a>
<a id="sprite_runoperator"></a>
<a id="sprite_runspace"></a>
<a id="sprite_runtexture"></a>
<a id="sprite_space"></a>
<a id="sprite_spacescreen"></a>
<a id="sprite_spaceworld"></a>
<a id="sprite_vertex"></a>
<a id="sprite_vertexcolour"></a>
<a id="sprite_vertexoperator"></a>
<a id="sprite_vertexpad0"></a>
<a id="sprite_vertexpad1"></a>
<a id="sprite_vertexposition"></a>
<a id="sprite_vertexuv"></a>
<a id="spritecolour"></a>
<a id="spritelayer"></a>
<a id="spriteoperator"></a>
<a id="spriteorder"></a>
<a id="spritesize"></a>
<a id="spritespace"></a>
<a id="spritetexture"></a>
<a id="spriteuvrect"></a>
<a id="spriteworldfromlocal"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
