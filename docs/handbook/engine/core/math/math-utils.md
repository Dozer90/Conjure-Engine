---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# math_utils.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/math/math_utils.h` |
| API ↗ | [Math](../../../../api/core/math.md) |

<p>Scalar helpers that need no glm: constants, angle conversion, approximate comparison, interpolation and integer alignment / power-of-two utilities. Everything here is pure, constexpr where the standard library allows it, and safe to call from any thread.</p>

## Concepts

### `unsigned_integer` {#symbol-unsigned-integer}

```cpp
template <typename IntegerType> concept unsigned_integer = std::unsigned_integral<IntegerType> && !std::same_as<IntegerType, bool>;
```

An unsigned integer type other than bool: what the alignment and power-of-two helpers accept.

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-DEGREES_TO_RADIANS"></a><code>DEGREES_TO_RADIANS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../types.html#symbol-f32">f32</a></code> | <code class="signature">PI / <span class="mi">180.0</span>f</code> |  |
| <a id="variable-EPSILON"></a><code>EPSILON</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">1.0</span>e-<span class="mi">6</span>f</code> |  |
| <a id="variable-EPSILON_F64"></a><code>EPSILON_F64</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../types.html#symbol-f64">f64</a></code> | <code class="signature"><span class="mi">1.0</span>e-<span class="mi">12</span></code> |  |
| <a id="variable-F32_INFINITY"></a><code>F32_INFINITY</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../types.html#symbol-f32">f32</a></code> | <code class="signature">std::<wbr>numeric_<wbr>limits&lt;<wbr><a href="../types.html#symbol-f32">f32</a>&gt;::<wbr>infinity()</code> |  |
| <a id="variable-F32_MAX"></a><code>F32_MAX</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../types.html#symbol-f32">f32</a></code> | <code class="signature">std::<wbr>numeric_<wbr>limits&lt;<wbr><a href="../types.html#symbol-f32">f32</a>&gt;::<wbr>max()</code> |  |
| <a id="variable-HALF_PI"></a><code>HALF_PI</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">0.5</span>f * PI</code> |  |
| <a id="variable-INVERSE_PI"></a><code>INVERSE_PI</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">1.0</span>f / PI</code> |  |
| <a id="variable-PI"></a><code>PI</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">3.14159265358979323846</span>f</code> |  |
| <a id="variable-QUARTER_PI"></a><code>QUARTER_PI</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">0.25</span>f * PI</code> |  |
| <a id="variable-RADIANS_TO_DEGREES"></a><code>RADIANS_TO_DEGREES</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">180.0</span>f / PI</code> |  |
| <a id="variable-TWO_PI"></a><code>TWO_PI</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">2.0</span>f * PI</code> |  |

## Functions

### Public

#### `absolute` {#function-absolute}

<div class="function-declaration" id="function-absolute-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-f32">f32</a> absolute(
    <a href="../types.html#symbol-f32">f32</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>Absolute value usable in constant expressions.</p>

#### `align_down` {#function-align-down}

<div class="function-declaration" id="function-align-down-1"><pre><code><span class="k">template</span> &lt;<a href="math-utils.html#symbol-unsigned-integer">unsigned_integer</a> IntegerType&gt; [[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> IntegerType align_down(
    IntegerType value,
    std::type_identity_t&lt;IntegerType&gt; alignment) <span class="k">noexcept</span>;</code></pre></div>

<p>Rounds value down to the previous multiple of alignment, which must be a power of two (asserted; a bad alignment returns value unchanged).</p>

#### `align_up` {#function-align-up}

<div class="function-declaration" id="function-align-up-1"><pre><code><span class="k">template</span> &lt;<a href="math-utils.html#symbol-unsigned-integer">unsigned_integer</a> IntegerType&gt; [[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> IntegerType align_up(
    IntegerType value,
    std::type_identity_t&lt;IntegerType&gt; alignment) <span class="k">noexcept</span>;</code></pre></div>

<p>Rounds value up to the next multiple of alignment, which must be a power of two (asserted; a bad alignment returns value unchanged). When the rounded value does not fit the type (value within alignment - 1 of its maximum) the function asserts and returns value unchanged rather than wrapping to 0.</p>

#### `clamp` {#function-clamp}

<div class="function-declaration" id="function-clamp-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-f32">f32</a> clamp(
    <a href="../types.html#symbol-f32">f32</a> value,
    <a href="../types.html#symbol-f32">f32</a> low,
    <a href="../types.html#symbol-f32">f32</a> high) <span class="k">noexcept</span>;</code></pre></div>

<p>Clamps value into [low, high]. Asserts when low &gt; high.</p>

#### `degrees` {#function-degrees}

<div class="function-declaration" id="function-degrees-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-f32">f32</a> degrees(
    <a href="../types.html#symbol-f32">f32</a> angleRadians) <span class="k">noexcept</span>;</code></pre></div>

<p>Converts radians to degrees.</p>

#### `divide_round_up` {#function-divide-round-up}

<div class="function-declaration" id="function-divide-round-up-1"><pre><code><span class="k">template</span> &lt;<a href="math-utils.html#symbol-unsigned-integer">unsigned_integer</a> IntegerType&gt; [[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> IntegerType divide_round_up(
    IntegerType numerator,
    std::type_identity_t&lt;IntegerType&gt; denominator) <span class="k">noexcept</span>;</code></pre></div>

<p><code>ceil(numerator / denominator)</code> for integers; asserts and returns 0 when denominator is 0.</p>

#### `inverse_lerp` {#function-inverse-lerp}

<div class="function-declaration" id="function-inverse-lerp-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-f32">f32</a> inverse_lerp(
    <a href="../types.html#symbol-f32">f32</a> a,
    <a href="../types.html#symbol-f32">f32</a> b,
    <a href="../types.html#symbol-f32">f32</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>The t for which <code><a href="#function-lerp-1">lerp</a>(a, b, t)</code> == value. Returns 0 when a == b exactly; any non-zero range, however narrow, is divided through.</p>

#### `is_aligned` {#function-is-aligned}

<div class="function-declaration" id="function-is-aligned-1"><pre><code><span class="k">template</span> &lt;<a href="math-utils.html#symbol-unsigned-integer">unsigned_integer</a> IntegerType&gt; [[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_aligned(
    IntegerType value,
    std::type_identity_t&lt;IntegerType&gt; alignment) <span class="k">noexcept</span>;</code></pre></div>

<p>True when value is a multiple of alignment (a power of two; asserted, <code>false</code> otherwise).</p>

<div class="function-declaration" id="function-is-aligned-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <span class="k">bool</span> is_aligned(
    <span class="k">const</span> <span class="k">void</span> *pointer,
    <a href="../types.html#symbol-usize">usize</a> alignment) <span class="k">noexcept</span>;</code></pre></div>

<p>True when the pointer address is a multiple of alignment.</p>

#### `is_power_of_two` {#function-is-power-of-two}

<div class="function-declaration" id="function-is-power-of-two-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> is_power_of_two(
    <a href="../types.html#symbol-u64">u64</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>True when value is a power of two. Zero is not a power of two.</p>

#### `lerp` {#function-lerp}

<div class="function-declaration" id="function-lerp-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-f32">f32</a> lerp(
    <a href="../types.html#symbol-f32">f32</a> a,
    <a href="../types.html#symbol-f32">f32</a> b,
    <a href="../types.html#symbol-f32">f32</a> t) <span class="k">noexcept</span>;</code></pre></div>

<p>Linear interpolation from a (t = 0) to b (t = 1); t is not clamped.</p>

#### `log2_floor` {#function-log2-floor}

<div class="function-declaration" id="function-log2-floor-1"><pre><code><span class="k">template</span> &lt;<a href="math-utils.html#symbol-unsigned-integer">unsigned_integer</a> IntegerType&gt; [[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-u32">u32</a> log2_floor(
    IntegerType value) <span class="k">noexcept</span>;</code></pre></div>

<p><code>floor(log2(value))</code> for value &gt; 0; asserts and returns 0 for zero.</p>

#### `nearly_equal` {#function-nearly-equal}

<div class="function-declaration" id="function-nearly-equal-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> nearly_equal(
    <a href="../types.html#symbol-f32">f32</a> a,
    <a href="../types.html#symbol-f32">f32</a> b,
    <a href="../types.html#symbol-f32">f32</a> epsilon = EPSILON) <span class="k">noexcept</span>;</code></pre></div>

<p>True when |a - b| is within epsilon (absolute tolerance). Exactly equal values compare equal even when infinite (inf - inf is NaN); NaN never compares equal.</p>

<div class="function-declaration" id="function-nearly-equal-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> nearly_equal(
    <a href="../types.html#symbol-f64">f64</a> a,
    <a href="../types.html#symbol-f64">f64</a> b,
    <a href="../types.html#symbol-f64">f64</a> epsilon = EPSILON_F64) <span class="k">noexcept</span>;</code></pre></div>

<p>True when |a - b| is within epsilon (absolute tolerance). Exactly equal values compare equal even when infinite (inf - inf is NaN); NaN never compares equal.</p>

#### `nearly_zero` {#function-nearly-zero}

<div class="function-declaration" id="function-nearly-zero-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">bool</span> nearly_zero(
    <a href="../types.html#symbol-f32">f32</a> value,
    <a href="../types.html#symbol-f32">f32</a> epsilon = EPSILON) <span class="k">noexcept</span>;</code></pre></div>

<p>True when |value| is within epsilon of zero.</p>

#### `next_power_of_two` {#function-next-power-of-two}

<div class="function-declaration" id="function-next-power-of-two-1"><pre><code><span class="k">template</span> &lt;<a href="math-utils.html#symbol-unsigned-integer">unsigned_integer</a> IntegerType&gt; [[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> IntegerType next_power_of_two(
    IntegerType value) <span class="k">noexcept</span>;</code></pre></div>

<p>The smallest power of two that is &gt;= value; 1 for zero. Returns 0 when the result does not fit the type (value above the largest representable power of two).</p>

#### `previous_power_of_two` {#function-previous-power-of-two}

<div class="function-declaration" id="function-previous-power-of-two-1"><pre><code><span class="k">template</span> &lt;<a href="math-utils.html#symbol-unsigned-integer">unsigned_integer</a> IntegerType&gt; [[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> IntegerType previous_power_of_two(
    IntegerType value) <span class="k">noexcept</span>;</code></pre></div>

<p>The largest power of two that is &lt;= value; 0 for zero.</p>

#### `radians` {#function-radians}

<div class="function-declaration" id="function-radians-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-f32">f32</a> radians(
    <a href="../types.html#symbol-f32">f32</a> angleDegrees) <span class="k">noexcept</span>;</code></pre></div>

<p>Converts degrees to radians.</p>

#### `remap` {#function-remap}

<div class="function-declaration" id="function-remap-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-f32">f32</a> remap(
    <a href="../types.html#symbol-f32">f32</a> value,
    <a href="../types.html#symbol-f32">f32</a> inLow,
    <a href="../types.html#symbol-f32">f32</a> inHigh,
    <a href="../types.html#symbol-f32">f32</a> outLow,
    <a href="../types.html#symbol-f32">f32</a> outHigh) <span class="k">noexcept</span>;</code></pre></div>

<p>Maps value from [<code>inLow</code>, <code>inHigh</code>] onto [<code>outLow</code>, <code>outHigh</code>] without clamping.</p>

#### `saturate` {#function-saturate}

<div class="function-declaration" id="function-saturate-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-f32">f32</a> saturate(
    <a href="../types.html#symbol-f32">f32</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>Clamps value into [0, 1].</p>

#### `sign` {#function-sign}

<div class="function-declaration" id="function-sign-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-f32">f32</a> sign(
    <a href="../types.html#symbol-f32">f32</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>-1, 0 or 1 depending on the sign of value (0 for zero and NaN).</p>

#### `smoothstep` {#function-smoothstep}

<div class="function-declaration" id="function-smoothstep-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-f32">f32</a> smoothstep(
    <a href="../types.html#symbol-f32">f32</a> edge0,
    <a href="../types.html#symbol-f32">f32</a> edge1,
    <a href="../types.html#symbol-f32">f32</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>Hermite smooth step: 0 at or below edge0, 1 at or above edge1, smooth in between.</p>

#### `square` {#function-square}

<div class="function-declaration" id="function-square-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-f32">f32</a> square(
    <a href="../types.html#symbol-f32">f32</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>value * value.</p>

#### `wrap_angle` {#function-wrap-angle}

<div class="function-declaration" id="function-wrap-angle-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">inline</span> <a href="../types.html#symbol-f32">f32</a> wrap_angle(
    <a href="../types.html#symbol-f32">f32</a> angleRadians) <span class="k">noexcept</span>;</code></pre></div>

<p>Wraps an angle in radians into the half-open range [-<code>PI</code>, <code>PI</code>).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="absolute"></a>
<a id="align_down"></a>
<a id="align_up"></a>
<a id="clamp"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="degrees"></a>
<a id="degrees_to_radians"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="divide_round_up"></a>
<a id="epsilon"></a>
<a id="epsilon_f64"></a>
<a id="extension-points"></a>
<a id="f32_infinity"></a>
<a id="f32_max"></a>
<a id="half_pi"></a>
<a id="internals-and-algorithms"></a>
<a id="inverse_lerp"></a>
<a id="inverse_pi"></a>
<a id="is_aligned"></a>
<a id="is_power_of_two"></a>
<a id="lerp"></a>
<a id="log2_floor"></a>
<a id="nearly_equal"></a>
<a id="nearly_zero"></a>
<a id="next_power_of_two"></a>
<a id="other-declarations"></a>
<a id="pi"></a>
<a id="previous_power_of_two"></a>
<a id="quarter_pi"></a>
<a id="radians"></a>
<a id="radians_to_degrees"></a>
<a id="remap"></a>
<a id="saturate"></a>
<a id="sign"></a>
<a id="smoothstep"></a>
<a id="square"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="two_pi"></a>
<a id="unsigned_integer"></a>
<a id="variables-and-properties"></a>
<a id="wrap_angle"></a>
<!-- doc-legacy-anchors:end -->
