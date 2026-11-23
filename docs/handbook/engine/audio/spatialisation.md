---
kind: header
---

[Engine](../../engine.md) → [Audio](../audio.md)

# spatialisation.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/audio/spatialisation.h` |
| API ↗ | [Playing sounds](../../../api/audio/playing-sounds.md) |

<p>Listener and spatial maths. Pure functions, callable from any thread; the engine runs them on the main thread once per spatial voice per frame and sends the results to the mixer as commands.</p>

## Structs

| File | Description |
|---|---|
| [`attenuation_params`](spatialisation/attenuation-params.md) | Distance attenuation of one voice. |
| [`listener_state`](spatialisation/listener-state.md) | The one listener. Defaults: origin, forward -<code>Z</code>, up +<code>Y</code>, no velocity, gain 1. |
| [`spatial_result`](spatialisation/spatial-result.md) | What the mixer needs for a spatial voice. |

## Functions

### Public

#### `clamp_attenuation` {#function-clamp-attenuation}

<div class="function-declaration" id="function-clamp-attenuation-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="spatialisation/attenuation-params.html">attenuation_params</a> clamp_attenuation(
    <span class="k">const</span> <a href="spatialisation/attenuation-params.html">attenuation_params</a> &amp;params) <span class="k">noexcept</span>;</code></pre></div>

<p>Clamp rules of: <code>minDistance</code> &gt;= 0.01, <code>maxDistance</code> &gt;= <code>minDistance</code>, rolloff [0, 10]; NaN becomes the default.</p>

#### `clamp_listener` {#function-clamp-listener}

<div class="function-declaration" id="function-clamp-listener-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="spatialisation/listener-state.html">listener_state</a> clamp_listener(
    <span class="k">const</span> <a href="spatialisation/listener-state.html">listener_state</a> &amp;listener) <span class="k">noexcept</span>;</code></pre></div>

<p>Sanitises a listener: non-finite components become the defaults, forward and up are normalised (and up made perpendicular to forward), gain clamped to [0, 2].</p>

#### `compute_attenuation` {#function-compute-attenuation}

<div class="function-declaration" id="function-compute-attenuation-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-f32">f32</a> compute_attenuation(
    <span class="k">const</span> <a href="spatialisation/attenuation-params.html">attenuation_params</a> &amp;params,
    <a href="../core/types.html#symbol-f32">f32</a> distance) <span class="k">noexcept</span>;</code></pre></div>

<p>Gain in [0, 1] for a distance, with d = <code>clamp(distance, minDistance, maxDistance)</code>: <code>INVERSE</code> min / (min + rolloff * (d - min)), <code>LINEAR</code> 1 - rolloff * (d - min) / (max - min) clamped to [0, 1], <code>EXPONENTIAL</code> (d / min) ^ -rolloff, <code>NONE</code> 1.</p>

#### `compute_doppler_ratio` {#function-compute-doppler-ratio}

<div class="function-declaration" id="function-compute-doppler-ratio-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-f32">f32</a> compute_doppler_ratio(
    <span class="k">const</span> <a href="spatialisation/listener-state.html">listener_state</a> &amp;listener,
    <a href="../core/math/glm-types.html#symbol-vec3">math::vec3</a> sourcePosition,
    <a href="../core/math/glm-types.html#symbol-vec3">math::vec3</a> sourceVelocity,
    <a href="../core/types.html#symbol-f32">f32</a> dopplerFactor,
    <a href="../core/types.html#symbol-f32">f32</a> speedOfSound) <span class="k">noexcept</span>;</code></pre></div>

<p>Doppler pitch ratio ((c - <code>v_listener</code>. dir) / (c - <code>v_source</code>. dir)) ^ <code>dopplerFactor</code>, clamped to [0.5, 2]; 1 when <code>dopplerFactor</code> is 0 or the source is at the listener.</p>

#### `compute_pan` {#function-compute-pan}

<div class="function-declaration" id="function-compute-pan-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-f32">f32</a> compute_pan(
    <span class="k">const</span> <a href="spatialisation/listener-state.html">listener_state</a> &amp;listener,
    <a href="../core/math/glm-types.html#symbol-vec3">math::vec3</a> sourcePosition) <span class="k">noexcept</span>;</code></pre></div>

<p>Stereo pan in [-1, 1]: the listener-right component of the normalised direction to the source (+1 right, -1 left, 0 ahead, behind or coincident).</p>

#### `compute_spatial` {#function-compute-spatial}

<div class="function-declaration" id="function-compute-spatial-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="spatialisation/spatial-result.html">spatial_result</a> compute_spatial(
    <span class="k">const</span> <a href="spatialisation/listener-state.html">listener_state</a> &amp;listener,
    <a href="../core/math/glm-types.html#symbol-vec3">math::vec3</a> sourcePosition,
    <a href="../core/math/glm-types.html#symbol-vec3">math::vec3</a> sourceVelocity,
    <span class="k">const</span> <a href="spatialisation/attenuation-params.html">attenuation_params</a> &amp;params,
    <a href="../core/types.html#symbol-f32">f32</a> dopplerFactor,
    <a href="../core/types.html#symbol-f32">f32</a> speedOfSound) <span class="k">noexcept</span>;</code></pre></div>

<p>Attenuation, pan and Doppler ratio of a source relative to the listener in one call.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="attenuation_params"></a>
<a id="attenuation_paramsmaxdistance"></a>
<a id="attenuation_paramsmindistance"></a>
<a id="attenuation_paramsmodel"></a>
<a id="attenuation_paramsoperator"></a>
<a id="attenuation_paramsrolloff"></a>
<a id="clamp_attenuation"></a>
<a id="clamp_listener"></a>
<a id="classes"></a>
<a id="compute_attenuation"></a>
<a id="compute_doppler_ratio"></a>
<a id="compute_pan"></a>
<a id="compute_spatial"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="listener_state"></a>
<a id="listener_stateforward"></a>
<a id="listener_stategain"></a>
<a id="listener_stateoperator"></a>
<a id="listener_stateposition"></a>
<a id="listener_stateup"></a>
<a id="listener_statevelocity"></a>
<a id="spatial_result"></a>
<a id="spatial_resultgain"></a>
<a id="spatial_resultoperator"></a>
<a id="spatial_resultpan"></a>
<a id="spatial_resultpitchratio"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
