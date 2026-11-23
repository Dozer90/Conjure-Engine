---
kind: header
---

[Engine](../../engine.md) → [Core](../core.md)

# random.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/random.h` |
| API ↗ | [Time, events and random](../../../api/core/time-and-events.md) |

<p>Pseudo-random numbers.</p>



## Classes

| File | Description |
|---|---|
| [`random::random_engine`](random/random-random-engine.md) | An owned, independently seeded engine with the helpers above as members. Satisfies <code>std::uniform_random_bit_generator</code> so it can also feed the standard algorithms. |

## Type Aliases

### `random::engine` {#symbol-random-engine}

```cpp
using engine = std::mt19937_64;
```

The engine type used throughout Conjure. Its output sequence is fixed by the <code>C</code>++ standard, so seeded sequences are portable.

### `random::random_engine::result_type` {#symbol-random-random-engine-result-type}

```cpp
using result_type = engine::result_type;
```

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-random::DEFAULT_SEED"></a><code>random::DEFAULT_SEED</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="types.html#symbol-u64">u64</a></code> | <code class="signature"><span class="mi">0</span>x5EED</code> | Seed used by every thread&#x27;s default engine until <code>seed()</code> changes it. |

## Functions

### Public

#### `random::chance` {#function-random-chance}

<div class="function-declaration" id="function-random-chance-1"><pre><code><span class="k">template</span> &lt;std::uniform_random_bit_generator Generator&gt; [[<span class="na">nodiscard</span>]] <span class="k">bool</span> chance(Generator &amp;generator, <a href="types.html#symbol-f64">f64</a> probability);</code></pre></div>

<p>True with the given probability (clamped to [0, 1]). 0 is never <code>true</code>, 1 is always <code>true</code>.</p>

#### `random::current_seed` {#function-random-current-seed}

<div class="function-declaration" id="function-random-current-seed-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="types.html#symbol-u64">u64</a> current_seed() <span class="k">noexcept</span>;</code></pre></div>

<p>The seed new default engines are created with.</p>

#### `random::default_engine` {#function-random-default-engine}

<div class="function-declaration" id="function-random-default-engine-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="random.html#symbol-random-engine">engine</a> &amp;default_engine() <span class="k">noexcept</span>;</code></pre></div>

<p>This thread&#x27;s engine. Created on first use, seeded with the process seed (see <code>seed()</code>). Every thread starts from the same seed, so two threads doing the same work produce the same numbers.</p>

#### `random::detail::bounded_u64` {#function-random-detail-bounded-u64}

<div class="function-declaration" id="function-random-detail-bounded-u64-1"><pre><code><span class="k">template</span> &lt;std::uniform_random_bit_generator Generator&gt; [[<span class="na">nodiscard</span>]] <a href="types.html#symbol-u64">u64</a> bounded_u64(Generator &amp;generator, <a href="types.html#symbol-u64">u64</a> span);</code></pre></div>

<p>Uniform integer in [0, span] (inclusive). The span of all 64 bits is a plain draw.</p>

#### `random::detail::next_u64` {#function-random-detail-next-u64}

<div class="function-declaration" id="function-random-detail-next-u64-1"><pre><code><span class="k">template</span> &lt;std::uniform_random_bit_generator Generator&gt; [[<span class="na">nodiscard</span>]] <a href="types.html#symbol-u64">u64</a> next_u64(Generator &amp;generator);</code></pre></div>

<p>Draws exactly 64 uniformly distributed bits from any generator, combining several outputs when the generator yields fewer bits per call.</p>

#### `random::detail::unit_f32` {#function-random-detail-unit-f32}

<div class="function-declaration" id="function-random-detail-unit-f32-1"><pre><code><span class="k">template</span> &lt;std::uniform_random_bit_generator Generator&gt; [[<span class="na">nodiscard</span>]] <a href="types.html#symbol-f32">f32</a> unit_f32(Generator &amp;generator);</code></pre></div>

<p>Uniform float in [0, 1) with 24 random bits.</p>

#### `random::detail::unit_f64` {#function-random-detail-unit-f64}

<div class="function-declaration" id="function-random-detail-unit-f64-1"><pre><code><span class="k">template</span> &lt;std::uniform_random_bit_generator Generator&gt; [[<span class="na">nodiscard</span>]] <a href="types.html#symbol-f64">f64</a> unit_f64(Generator &amp;generator);</code></pre></div>

<p>Uniform double in [0, 1) with 53 random bits.</p>

#### `random::seed` {#function-random-seed}

<div class="function-declaration" id="function-random-seed-1"><pre><code><span class="na">CONJURE_API</span> <span class="k">void</span> seed(<a href="types.html#symbol-u64">u64</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>Sets the seed that every thread&#x27;s default engine receives on first use and reseeds the calling thread&#x27;s engine immediately, so the sequence restarts on this thread. Engines already created on other threads keep running until they call <code>seed()</code> themselves. Thread-safe.</p>

#### `random::uniform` {#function-random-uniform}

<div class="function-declaration" id="function-random-uniform-1"><pre><code><span class="k">template</span> &lt;std::uniform_random_bit_generator Generator, std::integral T&gt; <span class="k">requires</span> (!std::same_as&lt;T, <span class="k">bool</span>&gt;) [[<span class="na">nodiscard</span>]] T uniform(Generator &amp;generator, T min, T max);</code></pre></div>

<p>Uniform integer in the inclusive range [min, max]. Asserts when min &gt; max and then returns min. Works for every integral type including 8-bit ones (bool excluded).</p>

<div class="function-declaration" id="function-random-uniform-2"><pre><code><span class="k">template</span> &lt;std::uniform_random_bit_generator Generator, std::floating_point T&gt; [[<span class="na">nodiscard</span>]] T uniform(Generator &amp;generator, T min, T max);</code></pre></div>

<p>Uniform floating point value in the half-open range [min, max). Asserts when min &gt; max and then returns min. Never returns max.</p>

#### `random::unit_vector3` {#function-random-unit-vector3}

<div class="function-declaration" id="function-random-unit-vector3-1"><pre><code><span class="k">template</span> &lt;<span class="k">typename</span> Vector3 = std::array&lt;f32, <span class="mi">3</span>&gt;, std::uniform_random_bit_generator Generator&gt; [[<span class="na">nodiscard</span>]] Vector3 unit_vector3(Generator &amp;generator);</code></pre></div>

<p><code>A</code> uniformly distributed point on the unit sphere, built as Vector3{x, y, z}. The default is a plain array; pass <code><a href="math/glm-types.html#symbol-vec3">math::vec3</a></code> (or any type constructible from three floats) to get it. Consumes two 64-bit draws (two calls of <code><a href="random.html#symbol-random-engine">random::engine</a></code>). x and y come from <code>std::cos</code> / <code>std::sin</code> and are not bit-identical across platforms; do not hash or compare them bit for bit across machines.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="bounded_u64"></a>
<a id="chance"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="current_seed"></a>
<a id="declaration-summaries"></a>
<a id="default_engine"></a>
<a id="default_seed"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="engine"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="next_u64"></a>
<a id="other-declarations"></a>
<a id="random_engine"></a>
<a id="random_enginechance"></a>
<a id="random_enginecurrent_seed"></a>
<a id="random_enginediscard"></a>
<a id="random_enginemax"></a>
<a id="random_enginemengine"></a>
<a id="random_enginemin"></a>
<a id="random_enginemseed"></a>
<a id="random_engineoperator"></a>
<a id="random_engineoperator_1"></a>
<a id="random_engineoperator_2"></a>
<a id="random_enginerandom_engine"></a>
<a id="random_enginerandom_engine_1"></a>
<a id="random_engineraw"></a>
<a id="random_engineresult_type"></a>
<a id="random_engineseed"></a>
<a id="random_engineuniform"></a>
<a id="random_engineunit_vector3"></a>
<a id="randomchance"></a>
<a id="randomcurrent_seed"></a>
<a id="randomdefault_engine"></a>
<a id="randomdefault_seed"></a>
<a id="randomdetailbounded_u64"></a>
<a id="randomdetailnext_u64"></a>
<a id="randomdetailunit_f32"></a>
<a id="randomdetailunit_f64"></a>
<a id="randomengine"></a>
<a id="randomrandom_engine"></a>
<a id="randomrandom_enginechance"></a>
<a id="randomrandom_enginecurrent_seed"></a>
<a id="randomrandom_enginediscard"></a>
<a id="randomrandom_enginemax"></a>
<a id="randomrandom_enginemengine"></a>
<a id="randomrandom_enginemin"></a>
<a id="randomrandom_enginemseed"></a>
<a id="randomrandom_engineoperator"></a>
<a id="randomrandom_engineoperator_1"></a>
<a id="randomrandom_engineoperator_2"></a>
<a id="randomrandom_enginerandom_engine"></a>
<a id="randomrandom_enginerandom_engine_1"></a>
<a id="randomrandom_engineraw"></a>
<a id="randomrandom_engineresult_type"></a>
<a id="randomrandom_engineseed"></a>
<a id="randomrandom_engineuniform"></a>
<a id="randomrandom_engineunit_vector3"></a>
<a id="randomseed"></a>
<a id="randomuniform"></a>
<a id="randomunit_vector3"></a>
<a id="seed"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="uniform"></a>
<a id="unit_f32"></a>
<a id="unit_f64"></a>
<a id="unit_vector3"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
