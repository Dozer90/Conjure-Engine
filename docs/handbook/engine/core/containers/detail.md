---
kind: header
---

[Engine](../../../engine.md) → [Core](../../core.md)

# detail.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/core/containers/detail.h` |
| API ↗ | [Containers](../../../../api/core/containers.md) |

<p>Helpers shared by the container headers. Nothing in here is part of the public API; engine code includes the container it needs (<code><a href="small-vector/small-vector.html">small_vector</a>.h</code>, <code><a href="flat-hash-map/flat-hash-map.html">flat_hash_map</a>.h</code>, ...) and never this file.</p>

## Type Aliases

### `detail::synth_three_way_result` {#symbol-detail-synth-three-way-result}

```cpp
template <synth_three_way_comparable T> using synth_three_way_result = decltype(synth_three_way(std::declval<const T &>(), std::declval<const T &>()));
```

## Concepts

### `detail::synth_three_way_comparable` {#symbol-detail-synth-three-way-comparable}

```cpp
template <typename T> concept synth_three_way_comparable = std::three_way_comparable<T> || requires(const T &left, const T &right) { { left < right } -> std::convertible_to<bool>; };
```

Element types a container can order lexicographically: either three-way comparable or at least less-than comparable.

### `detail::transparent_compare` {#symbol-detail-transparent-compare}

```cpp
template <typename Compare> concept transparent_compare = requires { typename Compare::is_transparent; };
```

Comparators that opt into heterogeneous lookup, e.g. <code>std::less&lt;&gt;</code>.

## Functions

### Public

#### `detail::fatal_container_access` {#function-detail-fatal-container-access}

<div class="function-declaration" id="function-detail-fatal-container-access-1"><pre><code>[[<span class="na">noreturn</span>]] <span class="na">CONJURE_API</span> <span class="k">void</span> fatal_container_access(<span class="k">const</span> <span class="k">char</span> *message);</code></pre></div>

<p>Checked access that found nothing to return (<code>at()</code> on a missing key or index) is a programmer error. It asserts, then terminates even when the assertion handler continues. Defined in <code>containers.cpp</code>.</p>

#### `detail::mix_hash` {#function-detail-mix-hash}

<div class="function-declaration" id="function-detail-mix-hash-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <a href="../types.html#symbol-u64">u64</a> mix_hash(<a href="../types.html#symbol-u64">u64</a> hash) <span class="k">noexcept</span>;</code></pre></div>

<p>Finaliser that spreads entropy into every bit so identity-style <code><a href="slot-map/std-hash-conjure-core-slot-key.html">std::hash</a></code> specialisations (integers on libstdc++) still spread across power-of-two buckets.</p>

#### `detail::synth_three_way` {#function-detail-synth-three-way}

<div class="function-declaration" id="function-detail-synth-three-way-1"><pre><code><span class="k">template</span> &lt;<a href="detail.html#symbol-detail-synth-three-way-comparable">synth_three_way_comparable</a> T&gt; [[<span class="na">nodiscard</span>]] <span class="k">constexpr</span> <span class="k">auto</span> synth_three_way(<span class="k">const</span> T &amp;left, <span class="k">const</span> T &amp;right);</code></pre></div>

<p>The standard library&#x27;s exposition-only synth-three-way: uses <code>operator&lt;=&gt;</code> when the type has one and falls back to operator&lt; (yielding a weak ordering) otherwise, exactly like <code>std::vector</code>&#x27;s <code>operator&lt;=&gt;</code>.</p>

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
<a id="detailfatal_container_access"></a>
<a id="detailmix_hash"></a>
<a id="detailsynth_three_way"></a>
<a id="detailsynth_three_way_comparable"></a>
<a id="detailsynth_three_way_result"></a>
<a id="detailtransparent_compare"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="fatal_container_access"></a>
<a id="internals-and-algorithms"></a>
<a id="mix_hash"></a>
<a id="other-declarations"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="synth_three_way"></a>
<a id="synth_three_way_comparable"></a>
<a id="synth_three_way_result"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="transparent_compare"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
