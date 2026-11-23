---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# debug_draw.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/debug_draw.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [Rendering a frame](../../../api/renderer/rendering-a-frame.md) |

<p>The engine-owned facade: immediate shapes (drawn for one frame) and timed shapes (kept for <code>durationSeconds</code>). Thread-safe; <code>append_to()</code> and <code>tick()</code> run on the main thread each frame. Immediate shapes are written straight into the frame list under the mutex; only timed shapes build a buffer of their own (outside the lock).</p>

## Structs

| File | Description |
|---|---|
| [`debug_draw::timed_entry`](debug-draw/debug-draw-timed-entry.md) |  |
| [`debug_vertex`](debug-draw/debug-vertex.md) | One line vertex as the GPU consumes it (<code>gpu_debug_vertex</code>): position and an RGBA8 colour. |

## Classes

| File | Description |
|---|---|
| [`debug_draw`](debug-draw/debug-draw.md) | The engine-owned facade: immediate shapes (drawn for one frame) and timed shapes (kept for <code>durationSeconds</code>). Thread-safe; <code>append_to()</code> and <code>tick()</code> run on the main thread each frame. Immediate shapes are written straight into the frame list under the mutex; only timed shapes build a buffer of their own (outside the lock). |
| [`debug_draw_buffer`](debug-draw/debug-draw-buffer.md) | Line primitives collected for one frame. Each shape call appends pairs of vertices to the depth-tested or the overlay list; the lists are capped at <code>MAX_DEBUG_VERTICES</code> each (excess is dropped and counted). |

## Enumerations

### `debug_depth` {#symbol-debug-depth}

```cpp
enum class debug_depth : u8 {
    TESTED,
    OVERLAY
};
```

How a debug line meets the scene: hidden behind geometry (<code>TESTED</code>) or drawn on top (<code>OVERLAY</code>). <code>A</code> typed argument, so it can never be confused with the facade&#x27;s duration in the same position.

## Functions

### Public

#### `pack_debug_colour` {#function-pack-debug-colour}

<div class="function-declaration" id="function-pack-debug-colour-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-u32">u32</a> pack_debug_colour(
    <span class="k">const</span> <a href="../core/math/glm-types.html#symbol-vec4">math::vec4</a> &amp;colour) <span class="k">noexcept</span>;</code></pre></div>

<p>Packs a linear colour with components in [0, 1] into RGBA8 (red in the low <code><a href="../core/types.html#symbol-byte">byte</a></code>).</p>

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
<a id="debug_depth"></a>
<a id="debug_depthoverlay"></a>
<a id="debug_depthtested"></a>
<a id="debug_draw"></a>
<a id="debug_draw_buffer"></a>
<a id="debug_draw_bufferappend"></a>
<a id="debug_draw_bufferaxes"></a>
<a id="debug_draw_bufferbox"></a>
<a id="debug_draw_bufferclear"></a>
<a id="debug_draw_bufferdebug_draw_buffer"></a>
<a id="debug_draw_bufferdebug_draw_buffer_1"></a>
<a id="debug_draw_bufferdepth_tested"></a>
<a id="debug_draw_bufferdropped_count"></a>
<a id="debug_draw_bufferempty"></a>
<a id="debug_draw_bufferfrustum"></a>
<a id="debug_draw_buffergrid"></a>
<a id="debug_draw_bufferline"></a>
<a id="debug_draw_buffermdepthtested"></a>
<a id="debug_draw_buffermdropped"></a>
<a id="debug_draw_buffermoverlay"></a>
<a id="debug_draw_bufferoperator"></a>
<a id="debug_draw_bufferoverlay"></a>
<a id="debug_draw_bufferpush_line"></a>
<a id="debug_draw_buffersphere"></a>
<a id="debug_drawadd_timed"></a>
<a id="debug_drawappend_to"></a>
<a id="debug_drawaxes"></a>
<a id="debug_drawbox"></a>
<a id="debug_drawclear"></a>
<a id="debug_drawdebug_draw"></a>
<a id="debug_drawdebug_draw_1"></a>
<a id="debug_drawline"></a>
<a id="debug_drawmimmediate"></a>
<a id="debug_drawmmutex"></a>
<a id="debug_drawmtimed"></a>
<a id="debug_drawoperator"></a>
<a id="debug_drawsphere"></a>
<a id="debug_drawsubmit"></a>
<a id="debug_drawtick"></a>
<a id="debug_drawtimed_entry"></a>
<a id="debug_drawtimed_entry_count"></a>
<a id="debug_drawtimed_entryremainingseconds"></a>
<a id="debug_drawtimed_entryshape"></a>
<a id="debug_vertex"></a>
<a id="debug_vertexcolour"></a>
<a id="debug_vertexoperator"></a>
<a id="debug_vertexposition"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="pack_debug_colour"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
