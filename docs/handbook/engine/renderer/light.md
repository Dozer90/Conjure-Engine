---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# light.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/light.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [Rendering a frame](../../../api/renderer/rendering-a-frame.md) |

<p>Lights as the extraction contract carries them. Units follow glTF <code>KHR_lights_punctual</code>: lux for directional lights, candela for point and spot lights.</p>

## Structs

| File | Description |
|---|---|
| [`light`](light/light.md) |  |

## Enumerations

### `light_type` {#symbol-light-type}

```cpp
enum class light_type : u8 {
    DIRECTIONAL = 0,
    POINT,
    SPOT
};
```

### `shadow_filter` {#symbol-shadow-filter}

```cpp
enum class shadow_filter : u8 {
    DEFAULT = 0,
    HARD,
    PCF,
    PCSS
};
```

Per-<code><a href="light/light.html">light</a></code> shadow filter. <code>DEFAULT</code> resolves to <code>shadow_settings::defaultFilter</code> at render time so the editor can flip every <code><a href="light/light.html">light</a></code> at once.

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-MAX_SHADOW_TILE_SIZE"></a><code>MAX_SHADOW_TILE_SIZE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u16">u16</a></code> | <code class="signature"><span class="mi">1024</span></code> |  |
| <a id="variable-MIN_SHADOW_TILE_SIZE"></a><code>MIN_SHADOW_TILE_SIZE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u16">u16</a></code> | <code class="signature"><span class="mi">256</span></code> | The smallest and largest atlas tile a local <code><a href="light/light.html">light</a></code> may request. |

## Functions

### Public

#### `clamp_shadow_resolution` {#function-clamp-shadow-resolution}

<div class="function-declaration" id="function-clamp-shadow-resolution-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-u16">u16</a> clamp_shadow_resolution(
    <a href="../core/types.html#symbol-u16">u16</a> requested) <span class="k">noexcept</span>;</code></pre></div>

<p>Rounds a requested tile size to the nearest allowed size (256, 512 or 1024).</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="light.html#symbol-light-type">light_type</a> type) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of a <code><a href="light/light.html">light</a></code> type (&quot;<code>POINT</code>&quot;).</p>

<div class="function-declaration" id="function-to-string-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="light.html#symbol-shadow-filter">shadow_filter</a> filter) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of a shadow filter (&quot;<code>PCF</code>&quot;).</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="clamp_shadow_resolution"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="light"></a>
<a id="light_type"></a>
<a id="light_typedirectional"></a>
<a id="light_typepoint"></a>
<a id="light_typespot"></a>
<a id="lightcastsshadows"></a>
<a id="lightcolour"></a>
<a id="lightdirection"></a>
<a id="lightentityid"></a>
<a id="lightfilter"></a>
<a id="lightinnerconeradians"></a>
<a id="lightintensity"></a>
<a id="lightoperator"></a>
<a id="lightouterconeradians"></a>
<a id="lightposition"></a>
<a id="lightrange"></a>
<a id="lightshadowresolution"></a>
<a id="lightsourceradius"></a>
<a id="lighttype"></a>
<a id="max_shadow_tile_size"></a>
<a id="min_shadow_tile_size"></a>
<a id="other-declarations"></a>
<a id="shadow_filter"></a>
<a id="shadow_filterdefault"></a>
<a id="shadow_filterhard"></a>
<a id="shadow_filterpcf"></a>
<a id="shadow_filterpcss"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
