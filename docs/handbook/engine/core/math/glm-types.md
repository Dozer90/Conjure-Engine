---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# glm_types.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/math/glm_types.h` |
| API ↗ | [Math](../../../../api/core/math.md) |

<p>The engine&#x27;s vector, quaternion and matrix types are glm&#x27;s. This is the single public header through which glm reaches the rest of the engine; glm is configured by the build with <code>GLM_FORCE_RADIANS</code>, <code>GLM_FORCE_DEPTH_ZERO_TO_ONE</code>, <code>GLM_ENABLE_EXPERIMENTAL</code> and <code>GLM_FORCE_EXPLICIT_CTOR</code>, so write <code>vec3(0.0f)</code>, never vec3 value = 0.0f.</p>

## Structs

| File | Description |
|---|---|
| [`std::formatter<conjure::math::ivec2>`](glm-types/std-formatter-conjure-math-ivec2.md) |  |
| [`std::formatter<conjure::math::ivec3>`](glm-types/std-formatter-conjure-math-ivec3.md) |  |
| [`std::formatter<conjure::math::ivec4>`](glm-types/std-formatter-conjure-math-ivec4.md) |  |
| [`std::formatter<conjure::math::mat3>`](glm-types/std-formatter-conjure-math-mat3.md) |  |
| [`std::formatter<conjure::math::mat4>`](glm-types/std-formatter-conjure-math-mat4.md) |  |
| [`std::formatter<conjure::math::quat>`](glm-types/std-formatter-conjure-math-quat.md) |  |
| [`std::formatter<conjure::math::uvec2>`](glm-types/std-formatter-conjure-math-uvec2.md) |  |
| [`std::formatter<conjure::math::uvec3>`](glm-types/std-formatter-conjure-math-uvec3.md) |  |
| [`std::formatter<conjure::math::uvec4>`](glm-types/std-formatter-conjure-math-uvec4.md) |  |
| [`std::formatter<conjure::math::vec2>`](glm-types/std-formatter-conjure-math-vec2.md) |  |
| [`std::formatter<conjure::math::vec3>`](glm-types/std-formatter-conjure-math-vec3.md) |  |
| [`std::formatter<conjure::math::vec4>`](glm-types/std-formatter-conjure-math-vec4.md) |  |

## Type Aliases

### `ivec2` {#symbol-ivec2}

```cpp
using ivec2 = glm::ivec2;
```

### `ivec3` {#symbol-ivec3}

```cpp
using ivec3 = glm::ivec3;
```

### `ivec4` {#symbol-ivec4}

```cpp
using ivec4 = glm::ivec4;
```

### `mat3` {#symbol-mat3}

```cpp
using mat3 = glm::mat3;
```

### `mat4` {#symbol-mat4}

```cpp
using mat4 = glm::mat4;
```

### `quat` {#symbol-quat}

```cpp
using quat = glm::quat;
```

### `uvec2` {#symbol-uvec2}

```cpp
using uvec2 = glm::uvec2;
```

### `uvec3` {#symbol-uvec3}

```cpp
using uvec3 = glm::uvec3;
```

### `uvec4` {#symbol-uvec4}

```cpp
using uvec4 = glm::uvec4;
```

### `vec2` {#symbol-vec2}

```cpp
using vec2 = glm::vec2;
```

### `vec3` {#symbol-vec3}

```cpp
using vec3 = glm::vec3;
```

### `vec4` {#symbol-vec4}

```cpp
using vec4 = glm::vec4;
```

## Functions

### Public

#### `detail::format_components` {#function-detail-format-components}

<div class="function-declaration" id="function-detail-format-components-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> ComponentType, <span class="k">typename</span> FormatContext&gt; <span class="k">typename</span> FormatContext::iterator format_components( <span class="k">const</span> <a href="glm-types/std-formatter-conjure-math-vec2.html">std::formatter</a>&lt;ComponentType&gt; &amp;componentFormatter, std::span&lt;<span class="k">const</span> ComponentType&gt; components, FormatContext &amp;context );</code></pre></div>

<p>Writes &quot;(c0, c1, ...)&quot; applying one component formatter (which already parsed the format spec) to every component.</p>

#### `nearly_equal` {#function-nearly-equal}

<div class="function-declaration" id="function-nearly-equal-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <span class="k">bool</span> nearly_equal(
    <span class="k">const</span> <a href="glm-types.html#symbol-vec2">vec2</a> &amp;a,
    <span class="k">const</span> <a href="glm-types.html#symbol-vec2">vec2</a> &amp;b,
    <a href="../types.html#symbol-f32">f32</a> epsilon = EPSILON) <span class="k">noexcept</span>;</code></pre></div>

<p>True when every component of a is within epsilon of the matching component of b.</p>

<div class="function-declaration" id="function-nearly-equal-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <span class="k">bool</span> nearly_equal(
    <span class="k">const</span> <a href="glm-types.html#symbol-vec3">vec3</a> &amp;a,
    <span class="k">const</span> <a href="glm-types.html#symbol-vec3">vec3</a> &amp;b,
    <a href="../types.html#symbol-f32">f32</a> epsilon = EPSILON) <span class="k">noexcept</span>;</code></pre></div>

<p>True when every component of a is within epsilon of the matching component of b.</p>

<div class="function-declaration" id="function-nearly-equal-3"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <span class="k">bool</span> nearly_equal(
    <span class="k">const</span> <a href="glm-types.html#symbol-vec4">vec4</a> &amp;a,
    <span class="k">const</span> <a href="glm-types.html#symbol-vec4">vec4</a> &amp;b,
    <a href="../types.html#symbol-f32">f32</a> epsilon = EPSILON) <span class="k">noexcept</span>;</code></pre></div>

<p>True when every component of a is within epsilon of the matching component of b.</p>

<div class="function-declaration" id="function-nearly-equal-4"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <span class="k">bool</span> nearly_equal(
    <span class="k">const</span> <a href="glm-types.html#symbol-quat">quat</a> &amp;a,
    <span class="k">const</span> <a href="glm-types.html#symbol-quat">quat</a> &amp;b,
    <a href="../types.html#symbol-f32">f32</a> epsilon = EPSILON) <span class="k">noexcept</span>;</code></pre></div>

<p>True when every component of a is within epsilon of the matching component of b. Note that q and -q are different quaternions here although they encode the same rotation; use <code><a href="#function-same-rotation-1">same_rotation</a>()</code> to compare rotations.</p>

<div class="function-declaration" id="function-nearly-equal-5"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <span class="k">bool</span> nearly_equal(
    <span class="k">const</span> <a href="glm-types.html#symbol-mat3">mat3</a> &amp;a,
    <span class="k">const</span> <a href="glm-types.html#symbol-mat3">mat3</a> &amp;b,
    <a href="../types.html#symbol-f32">f32</a> epsilon = EPSILON) <span class="k">noexcept</span>;</code></pre></div>

<p>True when every element of a is within epsilon of the matching element of b.</p>

<div class="function-declaration" id="function-nearly-equal-6"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <span class="k">bool</span> nearly_equal(
    <span class="k">const</span> <a href="glm-types.html#symbol-mat4">mat4</a> &amp;a,
    <span class="k">const</span> <a href="glm-types.html#symbol-mat4">mat4</a> &amp;b,
    <a href="../types.html#symbol-f32">f32</a> epsilon = EPSILON) <span class="k">noexcept</span>;</code></pre></div>

<p>True when every element of a is within epsilon of the matching element of b.</p>

#### `nearly_zero` {#function-nearly-zero}

<div class="function-declaration" id="function-nearly-zero-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <span class="k">bool</span> nearly_zero(
    <span class="k">const</span> <a href="glm-types.html#symbol-vec3">vec3</a> &amp;value,
    <a href="../types.html#symbol-f32">f32</a> epsilon = EPSILON) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the vector&#x27;s length is at most epsilon.</p>

<div class="function-declaration" id="function-nearly-zero-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <span class="k">bool</span> nearly_zero(
    <span class="k">const</span> <a href="glm-types.html#symbol-vec2">vec2</a> &amp;value,
    <a href="../types.html#symbol-f32">f32</a> epsilon = EPSILON) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the vector&#x27;s length is at most epsilon.</p>

#### `safe_normalize` {#function-safe-normalize}

<div class="function-declaration" id="function-safe-normalize-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <a href="glm-types.html#symbol-vec3">vec3</a> safe_normalize(
    <span class="k">const</span> <a href="glm-types.html#symbol-vec3">vec3</a> &amp;value,
    <span class="k">const</span> <a href="glm-types.html#symbol-vec3">vec3</a> &amp;fallback = <a href="glm-types.html#symbol-vec3">vec3</a>(<span class="mi">0.0</span>f, <span class="mi">0.0</span>f, -<span class="mi">1.0</span>f)) <span class="k">noexcept</span>;</code></pre></div>

<p>Normalises the vector, or returns fallback when its length is (nearly) zero. <code>glm::normalize</code> would produce NaN there.</p>

<div class="function-declaration" id="function-safe-normalize-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <a href="glm-types.html#symbol-vec2">vec2</a> safe_normalize(
    <span class="k">const</span> <a href="glm-types.html#symbol-vec2">vec2</a> &amp;value,
    <span class="k">const</span> <a href="glm-types.html#symbol-vec2">vec2</a> &amp;fallback = <a href="glm-types.html#symbol-vec2">vec2</a>(<span class="mi">1.0</span>f, <span class="mi">0.0</span>f)) <span class="k">noexcept</span>;</code></pre></div>

<p>Normalises the vector, or returns fallback when its length is (nearly) zero.</p>

#### `same_rotation` {#function-same-rotation}

<div class="function-declaration" id="function-same-rotation-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <span class="k">bool</span> same_rotation(
    <span class="k">const</span> <a href="glm-types.html#symbol-quat">quat</a> &amp;a,
    <span class="k">const</span> <a href="glm-types.html#symbol-quat">quat</a> &amp;b,
    <a href="../types.html#symbol-f32">f32</a> epsilon = EPSILON) <span class="k">noexcept</span>;</code></pre></div>

<p>True when two unit quaternions encode the same rotation within epsilon. q and -q are one rotation, so b is negated first when its dot product with a is negative; then every component of a must be within epsilon of the matching component of b, the tolerance <code><a href="#function-nearly-equal-1">nearly_equal</a>()</code> uses. In angles, for a small epsilon: two rotations up to 2 * epsilon radians apart compare equal, two more than about 4 * epsilon radians apart do not, and in between it depends on the axis. Float rounding of the inputs comes on top, so an epsilon near 1e-7 only accepts quaternions equal to the last bit or two.</p>

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
<a id="detailformat_components"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="format_components"></a>
<a id="internals-and-algorithms"></a>
<a id="ivec2"></a>
<a id="ivec3"></a>
<a id="ivec4"></a>
<a id="mat3"></a>
<a id="mat4"></a>
<a id="nearly_equal"></a>
<a id="nearly_zero"></a>
<a id="other-declarations"></a>
<a id="quat"></a>
<a id="safe_normalize"></a>
<a id="same_rotation"></a>
<a id="std"></a>
<a id="stdformat"></a>
<a id="stdformatterconjuremathivec2"></a>
<a id="stdformatterconjuremathivec2format"></a>
<a id="stdformatterconjuremathivec3"></a>
<a id="stdformatterconjuremathivec3format"></a>
<a id="stdformatterconjuremathivec4"></a>
<a id="stdformatterconjuremathivec4format"></a>
<a id="stdformatterconjuremathmat3"></a>
<a id="stdformatterconjuremathmat3format"></a>
<a id="stdformatterconjuremathmat4"></a>
<a id="stdformatterconjuremathmat4format"></a>
<a id="stdformatterconjuremathquat"></a>
<a id="stdformatterconjuremathquatformat"></a>
<a id="stdformatterconjuremathuvec2"></a>
<a id="stdformatterconjuremathuvec2format"></a>
<a id="stdformatterconjuremathuvec3"></a>
<a id="stdformatterconjuremathuvec3format"></a>
<a id="stdformatterconjuremathuvec4"></a>
<a id="stdformatterconjuremathuvec4format"></a>
<a id="stdformatterconjuremathvec2"></a>
<a id="stdformatterconjuremathvec2format"></a>
<a id="stdformatterconjuremathvec3"></a>
<a id="stdformatterconjuremathvec3format"></a>
<a id="stdformatterconjuremathvec4"></a>
<a id="stdformatterconjuremathvec4format"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="uvec2"></a>
<a id="uvec3"></a>
<a id="uvec4"></a>
<a id="variables-and-properties"></a>
<a id="vec2"></a>
<a id="vec3"></a>
<a id="vec4"></a>
<!-- doc-legacy-anchors:end -->
