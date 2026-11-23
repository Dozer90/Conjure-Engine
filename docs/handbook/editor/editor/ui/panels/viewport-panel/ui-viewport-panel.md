---
kind: class
---

[Editor](../../../../../editor.md) → [User Interface](../../../../user-interface.md) → [viewport_panel.h](../viewport-panel.md)

# ui::viewport_panel
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/ui/panels/viewport_panel.h` |
| Source File Path | `engine/src/editor/ui/panels/viewport_panel.cpp` |
| API ↗ | [The workspace UI](../../../../../../api/editor/workspace-ui.md) |

## Syntax

```cpp
class CONJURE_API viewport_panel final : public editor_panel
```

## Inheritance Hierarchy


[`ui::editor_panel`](../../editor-panel/ui-editor-panel.md) → `ui::viewport_panel`

## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">viewport_panel();</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~viewport_panel() <span class="k">override</span>;</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mGizmo</code></td><td><code class="signature"><a href="../../gizmo-controller/ui-gizmo-controller.html">gizmo_controller</a></code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPicks</code></td><td><code class="signature"><a href="../../../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Pictures<wbr>Drawn</code></td><td><code class="signature"><a href="../../../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRectMin</code></td><td><code class="signature"><a href="../../../../../engine/core/math/glm-types.html#symbol-vec2">math::vec2</a></code></td><td><code class="signature"><a href="../../../../../engine/core/math/glm-types.html#symbol-vec2">math::vec2</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mRectSize</code></td><td><code class="signature"><a href="../../../../../engine/core/math/glm-types.html#symbol-vec2">math::vec2</a></code></td><td><code class="signature"><a href="../../../../../engine/core/math/glm-types.html#symbol-vec2">math::vec2</a>(<span class="mi">1.0</span>f)</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Scene<wbr>Texture</code></td><td><code class="signature"><a href="../../../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSceneView</code></td><td><code class="signature">std::<wbr>unique_<wbr>ptr&lt;<wbr><a href="../viewport-panel.html">scene_<wbr>view</a>&gt;</code></td><td>—</td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Target<wbr>Failure<wbr>Logged</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Texture<wbr>Owner</code></td><td><code class="signature"><span class="k">const</span> <span class="k">void</span> *</code></td><td><code class="signature">nullptr</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `draw_contents` {#function-draw-contents}

<div class="function-declaration" id="function-draw-contents-1"><pre><code><span class="k">void</span> draw_contents(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context) <span class="k">override</span>;</code></pre></div>

#### `gizmo` {#function-gizmo}

<div class="function-declaration" id="function-gizmo-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../gizmo-controller/ui-gizmo-controller.html">gizmo_controller</a> &amp;gizmo() <span class="k">noexcept</span>;</code></pre></div>

#### `pick_at` {#function-pick-at}

<div class="function-declaration" id="function-pick-at-1"><pre><code><span class="k">void</span> pick_at(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context,
    <span class="k">const</span> <a href="../../../../../engine/core/math/glm-types.html#symbol-vec2">math::vec2</a> &amp;viewportUv,
    <span class="k">bool</span> additive);</code></pre></div>

<p>Picks at a viewport position (0..1) and updates the selection; public so tests drive it.</p>

#### `pick_count` {#function-pick-count}

<div class="function-declaration" id="function-pick-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../../engine/core/types.html#symbol-u32">u32</a> pick_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `pictures_drawn` {#function-pictures-drawn}

<div class="function-declaration" id="function-pictures-drawn-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../../engine/core/types.html#symbol-u64">u64</a> pictures_drawn() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Frames in which the panel showed the scene through the editor camera; stays 0 where no ImGui renderer exists to show it (headless, the <code>null</code> device).</p>

#### `rect_min` {#function-rect-min}

<div class="function-declaration" id="function-rect-min-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../../../../../engine/core/math/glm-types.html#symbol-vec2">math::vec2</a> &amp;rect_min() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The viewport rectangle of the last frame (screen pixels).</p>

#### `rect_size` {#function-rect-size}

<div class="function-declaration" id="function-rect-size-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../../../../../engine/core/math/glm-types.html#symbol-vec2">math::vec2</a> &amp;rect_size() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

### Private

#### `draw_scene_picture` {#function-draw-scene-picture}

<div class="function-declaration" id="function-draw-scene-picture-1"><pre><code><span class="k">bool</span> draw_scene_picture(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context,
    <span class="k">const</span> <a href="../../../../../engine/core/math/glm-types.html#symbol-vec2">math::vec2</a> &amp;origin,
    <span class="k">const</span> <a href="../../../../../engine/core/math/glm-types.html#symbol-vec2">math::vec2</a> &amp;size);</code></pre></div>

<p>Keeps the off-screen target at the rectangle&#x27;s size in framebuffer pixels, submits the editor camera&#x27;s view for this frame and draws the picture at the rectangle. False when nothing was drawn (no ImGui renderer, or the target could not be made).</p>

#### `draw_snap_controls` {#function-draw-snap-controls}

<div class="function-declaration" id="function-draw-snap-controls-1"><pre><code><span class="k">void</span> draw_snap_controls();</code></pre></div>

#### `draw_toolbar` {#function-draw-toolbar}

<div class="function-declaration" id="function-draw-toolbar-1"><pre><code><span class="k">void</span> draw_toolbar(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

#### `handle_camera_input` {#function-handle-camera-input}

<div class="function-declaration" id="function-handle-camera-input-1"><pre><code><span class="k">void</span> handle_camera_input(
    <a href="../../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
