---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# math.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/math/math.h` |
| API ↗ | [Math](../../../../api/core/math.md) |

<p>Umbrella header of the math area: glm aliases and formatters, scalar utilities, transform, aabb, plane, frustum, ray, rect and the projection helpers below. Include this (or conjure/core/<code>math.h</code>) rather than the individual headers unless compile time matters.</p>

## Functions

### Public

#### `infinite_perspective` {#function-infinite-perspective}

<div class="function-declaration" id="function-infinite-perspective-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="glm-types.html#symbol-mat4">mat4</a> infinite_perspective(
    <a href="../types.html#symbol-f32">f32</a> fovYRadians,
    <a href="../types.html#symbol-f32">f32</a> aspect,
    <a href="../types.html#symbol-f32">f32</a> nearPlane) <span class="k">noexcept</span>;</code></pre></div>

<p>Perspective projection without a far plane: depth tends to 1 at infinity. Asserts on a non-positive field of view, aspect or near plane.</p>

#### `look_at` {#function-look-at}

<div class="function-declaration" id="function-look-at-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="glm-types.html#symbol-mat4">mat4</a> look_at(
    <a href="glm-types.html#symbol-vec3">vec3</a> eye,
    <a href="glm-types.html#symbol-vec3">vec3</a> target,
    <a href="glm-types.html#symbol-vec3">vec3</a> up) <span class="k">noexcept</span>;</code></pre></div>

<p><code>A</code> view matrix (world to camera) for a camera at eye looking at target: the inverse of <code>transform::look_at(eye, target, up)</code>.<code>to_matrix()</code>, with the same handling of degenerate inputs.</p>

#### `orthographic` {#function-orthographic}

<div class="function-declaration" id="function-orthographic-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="glm-types.html#symbol-mat4">mat4</a> orthographic(
    <a href="../types.html#symbol-f32">f32</a> left,
    <a href="../types.html#symbol-f32">f32</a> right,
    <a href="../types.html#symbol-f32">f32</a> bottom,
    <a href="../types.html#symbol-f32">f32</a> top,
    <a href="../types.html#symbol-f32">f32</a> nearPlane,
    <a href="../types.html#symbol-f32">f32</a> farPlane) <span class="k">noexcept</span>;</code></pre></div>

<p>Orthographic projection mapping x in [left, right], y in [bottom, top] and -z in [<code>nearPlane</code>, <code>farPlane</code>] to clip space with depth 0..1. Asserts when any range is empty.</p>

#### `perspective` {#function-perspective}

<div class="function-declaration" id="function-perspective-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="glm-types.html#symbol-mat4">mat4</a> perspective(
    <a href="../types.html#symbol-f32">f32</a> fovYRadians,
    <a href="../types.html#symbol-f32">f32</a> aspect,
    <a href="../types.html#symbol-f32">f32</a> nearPlane,
    <a href="../types.html#symbol-f32">f32</a> farPlane) <span class="k">noexcept</span>;</code></pre></div>

<p>Perspective projection with a vertical field of view in radians, mapping -<code>nearPlane</code> to depth 0 and -<code>farPlane</code> to depth 1 (equals <code>glm::perspective</code> under the engine&#x27;s glm configuration). Asserts on a non-positive field of view, aspect or near plane and on <code>farPlane</code> &lt;= <code>nearPlane</code>.</p>

#### `project` {#function-project}

<div class="function-declaration" id="function-project-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="glm-types.html#symbol-vec3">vec3</a>&gt; project(
    <a href="glm-types.html#symbol-vec3">vec3</a> point,
    <span class="k">const</span> <a href="glm-types.html#symbol-mat4">mat4</a> &amp;viewProjection) <span class="k">noexcept</span>;</code></pre></div>

<p>Projects a world point to normalised device coordinates (x, y in [-1, 1] on screen, depth in [0, 1] between the near and far planes). Returns nullopt for points on the camera plane (clip w of zero).</p>

#### `unproject` {#function-unproject}

<div class="function-declaration" id="function-unproject-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::optional&lt;<a href="glm-types.html#symbol-vec3">vec3</a>&gt; unproject(
    <a href="glm-types.html#symbol-vec3">vec3</a> normalizedDeviceCoordinates,
    <span class="k">const</span> <a href="glm-types.html#symbol-mat4">mat4</a> &amp;inverseViewProjection) <span class="k">noexcept</span>;</code></pre></div>

<p>Converts normalised device coordinates (x, y in [-1, 1], depth in [0, 1]) back to a point using an inverse view-projection matrix. Returns nullopt when the result is at infinity (clip w of zero, e.g. depth 1 of an infinite projection).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="infinite_perspective"></a>
<a id="internals-and-algorithms"></a>
<a id="look_at"></a>
<a id="orthographic"></a>
<a id="perspective"></a>
<a id="project"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="unproject"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
