---
kind: class
---

[Editor](../../../../editor.md) → [User Interface](../../../user-interface.md) → [gizmo_controller.h](../gizmo-controller.md)

# ui::gizmo_controller
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor::ui` |
| Include File Path | `engine/include/conjure/editor/ui/gizmo_controller.h` |
| Source File Path | `engine/src/editor/ui/gizmo_controller.cpp` |
| API ↗ | [The workspace UI](../../../../../api/editor/workspace-ui.md) |

## Syntax

```cpp
class CONJURE_API gizmo_controller
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">gizmo_controller();</code></td><td></td></tr>
</tbody>
</table>

<div class="admonition important" role="note"><p class="admonition-title">Important</p><p>Additional constructor overloads have been deleted.</p></div>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~gizmo_controller();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mDragged</code></td><td><code class="signature">std::<wbr>vector&lt;<wbr><a href="ui-gizmo-controller-dragged-entity.html">dragged_<wbr>entity</a>&gt;</code></td><td><code class="signature">{}</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDragging</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mDragSession</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-u64">u64</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Manipulations</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-u32">u32</a></code></td><td><code class="signature"><span class="mi">0</span></code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mOperation</code></td><td><code class="signature"><a href="../gizmo-controller.html#symbol-ui-gizmo-operation">gizmo_operation</a></code></td><td><code class="signature">gizmo_<wbr>operation::<wbr>TRANSLATE</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mOverHandle</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPivot</code></td><td><code class="signature"><a href="../../../../engine/core/math/glm-types.html#symbol-mat4">math::mat4</a></code></td><td><code class="signature"><a href="../../../../engine/core/math/glm-types.html#symbol-mat4">math::mat4</a>(<span class="mi">1.0</span>f)</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Snap<wbr>Rotate<wbr>Degrees</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">15.0</span>f</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSnapScale</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.1</span>f</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Snap<wbr>Translate</code></td><td><code class="signature"><a href="../../../../engine/core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.5</span>f</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mSpace</code></td><td><code class="signature"><a href="../gizmo-controller.html#symbol-ui-gizmo-space">gizmo_space</a></code></td><td><code class="signature">gizmo_<wbr>space::<wbr>WORLD</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mUseSnap</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `apply_delta` {#function-apply-delta}

<div class="function-declaration" id="function-apply-delta-1"><pre><code><span class="k">void</span> apply_delta(
    <a href="../../editor-context/editor-context.html">editor_context</a> &amp;context,
    <span class="k">const</span> <a href="../../../../engine/core/math/glm-types.html#symbol-mat4">math::mat4</a> &amp;deltaWorld);</code></pre></div>

<p>Applies a delta matrix to the selection as if the gizmo produced it (tests, keyboard nudges): begins the drag when none is open.</p>

#### `draw` {#function-draw}

<div class="function-declaration" id="function-draw-1"><pre><code><span class="k">bool</span> draw(
    <a href="../../editor-context/editor-context.html">editor_context</a> &amp;context,
    <span class="k">const</span> <a href="../../../../engine/core/math/glm-types.html#symbol-vec2">math::vec2</a> &amp;rectMin,
    <span class="k">const</span> <a href="../../../../engine/core/math/glm-types.html#symbol-vec2">math::vec2</a> &amp;rectSize);</code></pre></div>

<p>Draws the gizmo for the current selection inside the viewport rectangle and applies the user&#x27;s manipulation. Call once per frame between <code>ImGui::Begin</code> and End of the viewport window. Returns <code>true</code> while a drag is in progress.</p>

#### `end_drag` {#function-end-drag}

<div class="function-declaration" id="function-end-drag-1"><pre><code><span class="k">void</span> end_drag(
    <a href="../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

<p>Ends an open drag (release).</p>

#### `is_dragging` {#function-is-dragging}

<div class="function-declaration" id="function-is-dragging-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_dragging() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_over` {#function-is-over}

<div class="function-declaration" id="function-is-over-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_over() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>True when the last <code><a href="#function-draw-1">draw</a>()</code> showed the gizmo and the mouse was over one of its handles (the previous frame&#x27;s hotspot, as ImGuizmo reports it) or a drag is in progress. False after a <code><a href="#function-draw-1">draw</a>()</code> that showed no gizmo (no frame, no scene or an empty selection), so the viewport can pick again.</p>

#### `manipulation_count` {#function-manipulation-count}

<div class="function-declaration" id="function-manipulation-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-u32">u32</a> manipulation_count() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `operation` {#function-operation}

<div class="function-declaration" id="function-operation-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../gizmo-controller.html#symbol-ui-gizmo-operation">gizmo_operation</a> operation() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `set_operation` {#function-set-operation}

<div class="function-declaration" id="function-set-operation-1"><pre><code><span class="k">void</span> set_operation(
    <a href="../gizmo-controller.html#symbol-ui-gizmo-operation">gizmo_operation</a> operation) <span class="k">noexcept</span>;</code></pre></div>

#### `set_snap` {#function-set-snap}

<div class="function-declaration" id="function-set-snap-1"><pre><code><span class="k">void</span> set_snap(
    <span class="k">bool</span> enabled,
    <a href="../../../../engine/core/types.html#symbol-f32">f32</a> translate,
    <a href="../../../../engine/core/types.html#symbol-f32">f32</a> rotateDegrees,
    <a href="../../../../engine/core/types.html#symbol-f32">f32</a> scale) <span class="k">noexcept</span>;</code></pre></div>

<p>Snapping and its steps: world units for translate (and universal), degrees for rotate, a factor for scale. The defaults are 0.5, 15 and 0.1 with snapping off; the viewport toolbar&#x27;s Snap toggle and step field drive it.</p>

#### `set_space` {#function-set-space}

<div class="function-declaration" id="function-set-space-1"><pre><code><span class="k">void</span> set_space(
    <a href="../gizmo-controller.html#symbol-ui-gizmo-space">gizmo_space</a> space) <span class="k">noexcept</span>;</code></pre></div>

#### `snap_enabled` {#function-snap-enabled}

<div class="function-declaration" id="function-snap-enabled-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> snap_enabled() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `snap_rotate_degrees` {#function-snap-rotate-degrees}

<div class="function-declaration" id="function-snap-rotate-degrees-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-f32">f32</a> snap_rotate_degrees() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `snap_scale` {#function-snap-scale}

<div class="function-declaration" id="function-snap-scale-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-f32">f32</a> snap_scale() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `snap_translate` {#function-snap-translate}

<div class="function-declaration" id="function-snap-translate-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../../engine/core/types.html#symbol-f32">f32</a> snap_translate() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `space` {#function-space}

<div class="function-declaration" id="function-space-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../gizmo-controller.html#symbol-ui-gizmo-space">gizmo_space</a> space() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

### Private

#### `begin_drag` {#function-begin-drag}

<div class="function-declaration" id="function-begin-drag-1"><pre><code><span class="k">void</span> begin_drag(
    <a href="../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

#### `gather_selection` {#function-gather-selection}

<div class="function-declaration" id="function-gather-selection-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> gather_selection(
    <a href="../../editor-context/editor-context.html">editor_context</a> &amp;context);</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
