---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# camera.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/camera.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [Rendering a frame](../../../api/renderer/rendering-a-frame.md) |

<p>Cameras and projection helpers. Every projection is reverse-<code>Z</code>: the near plane maps to depth 1 and the far plane to depth 0, for perspective and orthographic cameras alike, so a single GreaterOrEqual depth test serves every pass.</p>

## Structs

| File | Description |
|---|---|
| [`camera`](camera/camera.md) |  |

## Enumerations

### `projection_type` {#symbol-projection-type}

```cpp
enum class projection_type : u8 {
    PERSPECTIVE = 0,
    ORTHOGRAPHIC
};
```

## Functions

### Public

#### `clip_from_view` {#function-clip-from-view}

<div class="function-declaration" id="function-clip-from-view-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/math/glm-types.html#symbol-mat4">math::mat4</a> clip_from_view(
    <span class="k">const</span> <a href="camera/camera.html">camera</a> &amp;camera,
    <a href="../core/types.html#symbol-f32">f32</a> aspectRatio) <span class="k">noexcept</span>;</code></pre></div>

<p>Reverse-<code>Z</code> projection: near maps to depth 1, far to depth 0, for both projection types. <code>A</code> non-positive aspect ratio, near plane or field of view, or a far plane at or before the near plane, asserts and yields the identity.</p>

#### `frustum_from_clip` {#function-frustum-from-clip}

<div class="function-declaration" id="function-frustum-from-clip-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/math/frustum/frustum.html">math::frustum</a> frustum_from_clip(
    <span class="k">const</span> <a href="../core/math/glm-types.html#symbol-mat4">math::mat4</a> &amp;clipFromWorld) <span class="k">noexcept</span>;</code></pre></div>

<p>The inward-facing frustum planes of a clip-from-world matrix (reverse-<code>Z</code> aware: the near plane is the one at depth 1).</p>

#### `ray_from_viewport` {#function-ray-from-viewport}

<div class="function-declaration" id="function-ray-from-viewport-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/math/ray/ray.html">math::ray</a> ray_from_viewport(
    <span class="k">const</span> <a href="camera/camera.html">camera</a> &amp;camera,
    <span class="k">const</span> <a href="../core/math/glm-types.html#symbol-mat4">math::mat4</a> &amp;worldFromView,
    <a href="../core/math/glm-types.html#symbol-vec2">math::vec2</a> viewportUv,
    <a href="../core/types.html#symbol-f32">f32</a> aspectRatio) <span class="k">noexcept</span>;</code></pre></div>

<p>The world-space ray through a viewport position given in [0, 1] x [0, 1] (origin top-left, y down). The ray starts on the near plane; for a perspective <code><a href="camera/camera.html">camera</a></code> it passes through the eye. The aspect ratio is the viewport&#x27;s width over height (a non-square viewport requires the aspect ratio for unprojection).</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="camera.html#symbol-projection-type">projection_type</a> type) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of a projection type (&quot;<code>PERSPECTIVE</code>&quot;).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="camera"></a>
<a id="camerafarplane"></a>
<a id="cameranearplane"></a>
<a id="cameraoperator"></a>
<a id="cameraorthographicheight"></a>
<a id="cameraprojection"></a>
<a id="cameraverticalfovradians"></a>
<a id="classes"></a>
<a id="clip_from_view"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="frustum_from_clip"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="projection_type"></a>
<a id="projection_typeorthographic"></a>
<a id="projection_typeperspective"></a>
<a id="ray_from_viewport"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
