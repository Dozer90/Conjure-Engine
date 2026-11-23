---
kind: class
---

[Editor](../../../editor.md) → [User Interface](../../user-interface.md) → [editor_camera.h](../editor-camera.md)

# editor_camera
<!-- doc-template:type -->

|---|---|
| Type | Class |
| Namespace | `conjure::editor` |
| Include File Path | `engine/include/conjure/editor/editor_camera.h` |
| Source File Path | `engine/src/editor/core/editor_camera.cpp` |
| API ↗ | [Headless mode and the command model](../../../../api/editor/headless-and-commands.md), [The editor](../../../../api/editor/index.md) |

## Syntax

```cpp
class CONJURE_API editor_camera
```



## Constructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">editor_camera();</code></td><td></td></tr>
<tr><td>public</td><td><code class="signature">editor_camera(
    <span class="k">const</span> <a href="editor-camera.html">editor_camera</a> &amp;) = <span class="k">default</span>;</code></td><td></td></tr>
</tbody>
</table>

## Destructors

<table class="member-table">
<thead><tr><th>Access</th><th>Signature</th><th>Description</th></tr></thead>
<tbody>
<tr><td>public</td><td><code class="signature">~editor_camera();</code></td><td></td></tr>
</tbody>
</table>

## Variables

<table class="variable-table">
<thead><tr><th>Access</th><th>Name</th><th>Type</th><th>Default</th><th>Description</th></tr></thead>
<tbody>
<tr><td>private</td><td><code class="signature">mFarPlane</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1000.0</span>f</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mNearPlane</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">0.1</span>f</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Orthographic</code></td><td><code class="signature"><span class="k">bool</span></code></td><td><code class="signature">false</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Orthographic<wbr>Height</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">10.0</span>f</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mPosition</code></td><td><code class="signature"><a href="../../../engine/core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../../engine/core/math/glm-types.html#symbol-vec3">math::<wbr>vec3</a>(<span class="mi">10.0</span>f,<wbr> <span class="mi">10.0</span>f,<wbr> <span class="mi">10.0</span>f)</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mTarget</code></td><td><code class="signature"><a href="../../../engine/core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../../engine/core/math/glm-types.html#symbol-vec3">math::vec3</a>(<span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">mUp</code></td><td><code class="signature"><a href="../../../engine/core/math/glm-types.html#symbol-vec3">math::vec3</a></code></td><td><code class="signature"><a href="../../../engine/core/math/glm-types.html#symbol-vec3">math::<wbr>vec3</a>(<span class="mi">0.0</span>f,<wbr> <span class="mi">1.0</span>f,<wbr> <span class="mi">0.0</span>f)</code></td><td></td></tr>
<tr><td>private</td><td><code class="signature">m<wbr>Vertical<wbr>Fov<wbr>Radians</code></td><td><code class="signature"><a href="../../../engine/core/types.html#symbol-f32">f32</a></code></td><td><code class="signature"><span class="mi">1.0472</span>f</code></td><td></td></tr>
</tbody>
</table>

## Functions

### Public

#### `apply_json` {#function-apply-json}

<div class="function-declaration" id="function-apply-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/result/result.html">core::result</a>&lt;<span class="k">void</span>&gt; apply_json(
    <span class="k">const</span> <a href="../../../engine/core/json/value/json-value.html">core::json::value</a> &amp;value);</code></pre></div>

<p>Applies the members present in the object (<code>view.set_camera</code>); fov in degrees, 0 &lt; fov &lt; 180. <code>INVALID_ARGS</code> for a malformed member.</p>

#### `far_plane` {#function-far-plane}

<div class="function-declaration" id="function-far-plane-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-f32">f32</a> far_plane() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `focus` {#function-focus}

<div class="function-declaration" id="function-focus-1"><pre><code><span class="k">void</span> focus(
    <span class="k">const</span> <a href="../../../engine/core/math/aabb/aabb.html">math::aabb</a> &amp;bounds);</code></pre></div>

<p>Frames a box: the target becomes its centre and the distance fits the box in the vertical field of view (orthographic: the height covers the box). An invalid box is ignored.</p>

#### `forward` {#function-forward}

<div class="function-declaration" id="function-forward-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/math/glm-types.html#symbol-vec3">math::vec3</a> forward() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `is_orthographic` {#function-is-orthographic}

<div class="function-declaration" id="function-is-orthographic-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">bool</span> is_orthographic() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `near_plane` {#function-near-plane}

<div class="function-declaration" id="function-near-plane-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-f32">f32</a> near_plane() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `orbit` {#function-orbit}

<div class="function-declaration" id="function-orbit-1"><pre><code><span class="k">void</span> orbit(
    <a href="../../../engine/core/types.html#symbol-f32">f32</a> yawRadians,
    <a href="../../../engine/core/types.html#symbol-f32">f32</a> pitchRadians);</code></pre></div>

<p>Rotates around the target by yaw and pitch (radians); pitch is clamped short of the poles.</p>

#### `orthographic_height` {#function-orthographic-height}

<div class="function-declaration" id="function-orthographic-height-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-f32">f32</a> orthographic_height() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `pan` {#function-pan}

<div class="function-declaration" id="function-pan-1"><pre><code><span class="k">void</span> pan(
    <span class="k">const</span> <a href="../../../engine/core/math/glm-types.html#symbol-vec2">math::vec2</a> &amp;viewOffset);</code></pre></div>

<p>Moves position and target by an offset in view space (right, up).</p>

#### `position` {#function-position}

<div class="function-declaration" id="function-position-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../../../engine/core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;position() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `projection_matrix` {#function-projection-matrix}

<div class="function-declaration" id="function-projection-matrix-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/math/glm-types.html#symbol-mat4">math::mat4</a> projection_matrix(
    <a href="../../../engine/core/types.html#symbol-f32">f32</a> aspectRatio) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>Reverse-<code>Z</code> projection for a viewport aspect ratio (width / height); a non-positive aspect uses 1.</p>

#### `screen_ray` {#function-screen-ray}

<div class="function-declaration" id="function-screen-ray-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/math/ray/ray.html">math::ray</a> screen_ray(
    <span class="k">const</span> <a href="../../../engine/core/math/glm-types.html#symbol-vec2">math::vec2</a> &amp;viewportUv,
    <a href="../../../engine/core/types.html#symbol-f32">f32</a> aspectRatio) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>The world-space ray through a normalised viewport position (origin top-left, y down).</p>

#### `set_look_at` {#function-set-look-at}

<div class="function-declaration" id="function-set-look-at-1"><pre><code><span class="k">void</span> set_look_at(
    <span class="k">const</span> <a href="../../../engine/core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;position,
    <span class="k">const</span> <a href="../../../engine/core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;target);</code></pre></div>

<p>Places the camera at <code>position</code> looking at <code>target</code> (a target equal to the position keeps the previous view direction).</p>

#### `set_orthographic` {#function-set-orthographic}

<div class="function-declaration" id="function-set-orthographic-1"><pre><code><span class="k">void</span> set_orthographic(
    <span class="k">bool</span> orthographic) <span class="k">noexcept</span>;</code></pre></div>

#### `set_orthographic_height` {#function-set-orthographic-height}

<div class="function-declaration" id="function-set-orthographic-height-1"><pre><code><span class="k">void</span> set_orthographic_height(
    <a href="../../../engine/core/types.html#symbol-f32">f32</a> height) <span class="k">noexcept</span>;</code></pre></div>

#### `set_planes` {#function-set-planes}

<div class="function-declaration" id="function-set-planes-1"><pre><code><span class="k">void</span> set_planes(
    <a href="../../../engine/core/types.html#symbol-f32">f32</a> nearPlane,
    <a href="../../../engine/core/types.html#symbol-f32">f32</a> farPlane) <span class="k">noexcept</span>;</code></pre></div>

#### `set_vertical_fov` {#function-set-vertical-fov}

<div class="function-declaration" id="function-set-vertical-fov-1"><pre><code><span class="k">void</span> set_vertical_fov(
    <a href="../../../engine/core/types.html#symbol-f32">f32</a> radians) <span class="k">noexcept</span>;</code></pre></div>

#### `target` {#function-target}

<div class="function-declaration" id="function-target-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">const</span> <a href="../../../engine/core/math/glm-types.html#symbol-vec3">math::vec3</a> &amp;target() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `to_json` {#function-to-json}

<div class="function-declaration" id="function-to-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/json/document/json-document.html">core::json::document</a> to_json() <span class="k">const</span>;</code></pre></div>

<p>{position, target, fov, orthographic, orthographicHeight, near, far} as <code>view.get_camera</code> returns; fov is the vertical field of view in degrees (the wire unit; the camera stores radians).</p>

#### `vertical_fov` {#function-vertical-fov}

<div class="function-declaration" id="function-vertical-fov-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-f32">f32</a> vertical_fov() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

#### `view_matrix` {#function-view-matrix}

<div class="function-declaration" id="function-view-matrix-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/math/glm-types.html#symbol-mat4">math::mat4</a> view_matrix() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>View matrix (world to view).</p>

#### `world_from_view` {#function-world-from-view}

<div class="function-declaration" id="function-world-from-view-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/math/glm-types.html#symbol-mat4">math::mat4</a> world_from_view() <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>World-from-view matrix (the inverse of <code><a href="#function-view-matrix-1">view_matrix</a>()</code>).</p>

#### `world_units_per_pixel` {#function-world-units-per-pixel}

<div class="function-declaration" id="function-world-units-per-pixel-1"><pre><code>[[<span class="na">nodiscard</span>]] <a href="../../../engine/core/types.html#symbol-f32">f32</a> world_units_per_pixel(
    <a href="../../../engine/core/types.html#symbol-f32">f32</a> viewportHeightPixels) <span class="k">const</span> <span class="k">noexcept</span>;</code></pre></div>

<p>World units one viewport pixel spans at the target: the visible height (the orthographic height, or 2 * distance * <code>tan(fov / 2)</code> in perspective) over the viewport height in pixels (at least 1), so a pan moves the scene with the cursor whatever the zoom.</p>

#### `zoom` {#function-zoom}

<div class="function-declaration" id="function-zoom-1"><pre><code><span class="k">void</span> zoom(
    <a href="../../../engine/core/types.html#symbol-f32">f32</a> factor);</code></pre></div>

<p>Scales the distance to the target (factor &gt; 1 moves away); orthographic scales the height.</p>

## Operator Overloads

### Public

#### `operator=` {#operator-3d}

<div class="function-declaration" id="operator-3d-1"><pre><code><a href="editor-camera.html">editor_camera</a> &amp;<span class="k">operator</span>=(
    <span class="k">const</span> <a href="editor-camera.html">editor_camera</a> &amp;) = <span class="k">default</span>;</code></pre></div>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->
