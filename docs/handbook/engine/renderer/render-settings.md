---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# render_settings.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/render_settings.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [Rendering a frame](../../../api/renderer/rendering-a-frame.md) |

<p>What the project and scene layers resolve, one member per <code><a href="render-settings.html#symbol-render-settings-group">render_settings_group</a></code> (80 bytes). Not <code><a href="../engine/engine-config/engine-config.html">engine_config</a>.h</code>&#x27;s <code><a href="../engine/engine-config/rendering-settings.html">rendering_settings</a></code>, which holds the backend and validation choices.</p>

## Structs

| File | Description |
|---|---|
| [`environment_settings`](render-settings/environment-settings.md) | The image-based lighting environment of the scene. |
| [`post_process_settings`](render-settings/post-process-settings.md) | HDR chain settings: exposure, bloom and the tonemap operator. 36 bytes. |
| [`render_settings`](render-settings/render-settings.md) | What the project and scene layers resolve, one member per <code><a href="render-settings.html#symbol-render-settings-group">render_settings_group</a></code> (80 bytes). Not <code><a href="../engine/engine-config/engine-config.html">engine_config</a>.h</code>&#x27;s <code><a href="../engine/engine-config/rendering-settings.html">rendering_settings</a></code>, which holds the backend and validation choices. |
| [`render_settings_problem`](render-settings/render-settings-problem.md) | One problem of a &quot;<code>rendering.defaults</code>&quot; value: the slash path of the member (&quot;rendering/defaults/ssao/radius&quot;) and what is wrong with it. |
| [`render_settings_reading`](render-settings/render-settings-reading.md) | What <code>read_render_settings()</code> found. |
| [`shadow_settings`](render-settings/shadow-settings.md) | Shadow settings shared by every light and every view. 28 bytes. |
| [`ssao_settings`](render-settings/ssao-settings.md) | GTAO settings. 16 bytes. |

## Classes

| File | Description |
|---|---|
| [`render_defaults`](render-settings/render-defaults.md) | The live project defaults (<code>project.json</code> &quot;<code>rendering.defaults</code>&quot;), for the engine to own and publish. Not synchronised: <code>set()</code> and <code>get()</code> belong to the main thread. |

## Enumerations

### `agx_look` {#symbol-agx-look}

```cpp
enum class agx_look : u8 {
    DEFAULT = 0,
    GOLDEN,
    PUNCHY
};
```

The look of the AgX operator (<code>tonemap_operator::AGX</code> only): neutral, golden or punchy.

### `exposure_mode` {#symbol-exposure-mode}

```cpp
enum class exposure_mode : u8 {
    AUTOMATIC = 0,
    MANUAL
};
```

### `render_settings_group` {#symbol-render-settings-group}

```cpp
enum class render_settings_group : u8 {
    POST_PROCESS = 0,
    SHADOWS,
    SSAO,
    COUNT
};
```

The settings groups, in the order <code><a href="render-settings/render-settings.html">render_settings</a></code> holds them. Each group is overridden as a whole by the scene and camera components. <code>COUNT</code> is the number of groups, not a group.

### `tonemap_operator` {#symbol-tonemap-operator}

```cpp
enum class tonemap_operator : u8 {
    ACES_FITTED = 0,
    REINHARD_EXTENDED,
    AGX,
    NONE
};
```

### `view_settings_override_flags` {#symbol-view-settings-override-flags}

```cpp
enum class view_settings_override_flags : u8 {
    NONE = 0,
    POST_PROCESS = 1 << 0,
    SSAO = 1 << 1
};
```

The groups a view replaces with its own values instead of the scene&#x27;s (<code>render_view::settingsOverride</code>). Bit 2 is kept free for a later group.

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-MAX_ADAPTATION_SPEED"></a><code>MAX_ADAPTATION_SPEED</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">100.0</span>f</code> |  |
| <a id="variable-MAX_BLOOM_INTENSITY"></a><code>MAX_BLOOM_INTENSITY</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">1.0</span>f</code> |  |
| <a id="variable-MAX_CASTER_PADDING"></a><code>MAX_CASTER_PADDING</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">1000.0</span>f</code> |  |
| <a id="variable-MAX_EV100"></a><code>MAX_EV100</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">24.0</span>f</code> |  |
| <a id="variable-MAX_EXPOSURE_COMPENSATION"></a><code>MAX_EXPOSURE_COMPENSATION</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">10.0</span>f</code> |  |
| <a id="variable-MAX_REINHARD_WHITE_POINT"></a><code>MAX_REINHARD_WHITE_POINT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">100.0</span>f</code> |  |
| <a id="variable-MAX_SHADOW_DISTANCE"></a><code>MAX_SHADOW_DISTANCE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">10000.0</span>f</code> |  |
| <a id="variable-MAX_SHADOW_FILTER_TEXELS"></a><code>MAX_SHADOW_FILTER_TEXELS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">8.0</span>f</code> |  |
| <a id="variable-MAX_SSAO_INTENSITY"></a><code>MAX_SSAO_INTENSITY</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">8.0</span>f</code> |  |
| <a id="variable-MAX_SSAO_RADIUS"></a><code>MAX_SSAO_RADIUS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">10.0</span>f</code> |  |
| <a id="variable-MIN_EV100"></a><code>MIN_EV100</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature">-<span class="mi">16.0</span>f</code> |  |
| <a id="variable-MIN_REINHARD_WHITE_POINT"></a><code>MIN_REINHARD_WHITE_POINT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">1.0</span>f</code> |  |
| <a id="variable-MIN_SHADOW_DISTANCE"></a><code>MIN_SHADOW_DISTANCE</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">1.0</span>f</code> |  |
| <a id="variable-MIN_SSAO_RADIUS"></a><code>MIN_SSAO_RADIUS</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-f32">f32</a></code> | <code class="signature"><span class="mi">0.05</span>f</code> |  |

## Macros

### `CONJURE_ENUM_FLAGS` {#symbol-conjure-enum-flags}

```cpp
CONJURE_ENUM_FLAGS(view_settings_override_flags)
```

### `CONJURE_REFLECT_ENUM` {#symbol-conjure-reflect-enum}

```cpp
CONJURE_REFLECT_ENUM(conjure::renderer::shadow_filter, "conjure.renderer.shadow_filter", DEFAULT, HARD, PCF, PCSS)
```

### `CONJURE_REFLECT_STRUCT` {#symbol-conjure-reflect-struct}

```cpp
CONJURE_REFLECT_STRUCT(conjure::renderer::post_process_settings, "conjure.renderer.post_process_settings", CONJURE_FIELD(manualEv100, .min = conjure::renderer::MIN_EV100, .max = conjure::renderer::MAX_EV100, .tooltip = "EV100 (exposure value at ISO 100) of the scene; manual exposure only"), CONJURE_FIELD(exposureCompensation, .min = -conjure::renderer::MAX_EXPOSURE_COMPENSATION, .max = conjure::renderer::MAX_EXPOSURE_COMPENSATION, .tooltip = "EV added to the exposure in both modes"), CONJURE_FIELD(adaptationSpeedUp, .min = 0.0, .max = conjure::renderer::MAX_ADAPTATION_SPEED, .tooltip = "Per second: how fast automatic exposure adapts to a brighter scene"), CONJURE_FIELD(adaptationSpeedDown, .min = 0.0, .max = conjure::renderer::MAX_ADAPTATION_SPEED, .tooltip = "Per second: how fast automatic exposure adapts to a darker scene"), CONJURE_FIELD(minEv100, .min = conjure::renderer::MIN_EV100, .max = conjure::renderer::MAX_EV100, .tooltip = "EV100 (exposure value at ISO 100): the lowest automatic exposure"), CONJURE_FIELD(maxEv100, .min = conjure::renderer::MIN_EV100, .max = conjure::renderer::MAX_EV100, .tooltip = "EV100 (exposure value at ISO 100): the highest automatic exposure, at least the lowest"), CONJURE_FIELD(bloomIntensity, .min = 0.0, .max = conjure::renderer::MAX_BLOOM_INTENSITY, .tooltip = "Unitless share of the bloom in the image"), CONJURE_FIELD(reinhardWhitePoint, .min = conjure::renderer::MIN_REINHARD_WHITE_POINT, .max = conjure::renderer::MAX_REINHARD_WHITE_POINT, .tooltip = "Linear luminance that maps to white (extended Reinhard only)"), CONJURE_FIELD(exposure, .tooltip = "Automatic (metered) or manual exposure"), CONJURE_FIELD(tonemap, .tooltip = "The operator that maps HDR to the display"), CONJURE_FIELD(agxLook, .tooltip = "The look of the AgX operator"), CONJURE_FIELD(bloomEnabled, .tooltip = "Adds the glow around bright pixels"))
```

## Functions

### Public

#### `read_render_settings` {#function-read-render-settings}

<div class="function-declaration" id="function-read-render-settings-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="render-settings/render-settings-reading.html">render_settings_reading</a> read_render_settings(
    <span class="k">const</span> <a href="../core/json/value/json-value.html">core::json::value</a> &amp;defaults);</code></pre></div>

<p>Reads a <code>project.json</code> &quot;<code>rendering.defaults</code>&quot; value: an object with the optional members &quot;<code>post_process</code>&quot;, &quot;shadows&quot; and &quot;ssao&quot;, each an object of <code>snake_case</code> keys (<code>render_settings_to_json</code> lists them all). Numbers must be finite and inside the field&#x27;s range, whole for <code>cascade_count</code>; booleans are <code>true</code> or <code>false</code>; enumerators are strings matched without regard to ASCII case. Never fails: a bad leaf keeps its engine default and is listed in errors, a group that is not an object keeps all its defaults, and <code>max_ev100</code> below <code>min_ev100</code> (after the leaves are read) puts both back to their defaults with one error at <code>max_ev100</code> (at <code>min_ev100</code> when only that key is present). Unknown keys are listed, never errors. <code>A</code> <code>null</code> or absent (invalid) value means engine defaults; any other non-object is an error at &quot;rendering/defaults&quot;. The result always passes <code><a href="#function-sanitise-1">sanitise</a>()</code> unchanged.</p>

#### `render_setting_leaf_to_json` {#function-render-setting-leaf-to-json}

<div class="function-declaration" id="function-render-setting-leaf-to-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/json/document/json-document.html">core::json::document</a> render_setting_leaf_to_json(
    <a href="render-settings.html#symbol-render-settings-group">render_settings_group</a> group,
    std::string_view camelField,
    <span class="k">const</span> <a href="../core/json/value/json-value.html">core::json::value</a> &amp;reflectedValue);</code></pre></div>

<p>One leaf in <code>project.json</code> form, the edit payload of the project settings panel: the group&#x27;s key holding one member, the field&#x27;s <code>snake_case</code> key, with the value as reflection JSON gives it (&quot;<code>bloomIntensity</code>&quot;, 0.06 gives {&quot;<code>post_process</code>&quot;: {&quot;<code>bloom_intensity</code>&quot;: 0.06}}). An enumerator string is written lower case; every other value, <code>null</code> included (a merge patch that removes the key), is copied as it is, and an invalid value is written as <code>null</code>. <code>A</code> group outside the enum or a field the group does not have asserts and gives an empty object.</p>

#### `render_settings_from_json` {#function-render-settings-from-json}

<div class="function-declaration" id="function-render-settings-from-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="render-settings/render-settings.html">render_settings</a>&gt; render_settings_from_json(
    <span class="k">const</span> <a href="../core/json/value/json-value.html">core::json::value</a> &amp;defaults);</code></pre></div>

<p>The strict form of <code><a href="#function-read-render-settings-1">read_render_settings</a>()</code>: the settings, or the first error as <code>renderer_error::INVALID_SETTINGS</code> with the message &quot;&lt;path&gt;: &lt;reason&gt;&quot;. Unknown keys are not errors.</p>

#### `render_settings_to_json` {#function-render-settings-to-json}

<div class="function-declaration" id="function-render-settings-to-json-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/json/document/json-document.html">core::json::document</a> render_settings_to_json(
    <span class="k">const</span> <a href="render-settings/render-settings.html">render_settings</a> &amp;settings);</code></pre></div>

<p>Every field of every group in <code>project.json</code> form (<code>snake_case</code> keys, enumerators lower case), which <code><a href="#function-read-render-settings-1">read_render_settings</a>()</code> reads back to the same settings. An enumerator value outside its enum is written as its number, which the reader rejects.</p>

#### `sanitise` {#function-sanitise}

<div class="function-declaration" id="function-sanitise-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view sanitise(
    <a href="render-settings/post-process-settings.html">post_process_settings</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

<p>Clamps every field of the group into its range: a NaN takes the field&#x27;s default, an infinity the bound it passes, <code>maxEv100</code> below <code>minEv100</code> becomes <code>minEv100</code> and an enumerator outside its enum takes the default one. Returns the <code>project.json</code> key of the first field it changed (&quot;<code>bloom_intensity</code>&quot;, in declaration order), or &quot;&quot; when the value was already valid.</p>

<div class="function-declaration" id="function-sanitise-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view sanitise(
    <a href="render-settings/shadow-settings.html">shadow_settings</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

<p>As the post-process form; <code>defaultFilter</code> <code>DEFAULT</code> (a per-light choice, 02) also becomes <code>PCF</code>, so it never survives.</p>

<div class="function-declaration" id="function-sanitise-3"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view sanitise(
    <a href="render-settings/ssao-settings.html">ssao_settings</a> &amp;value) <span class="k">noexcept</span>;</code></pre></div>

<p>As the post-process form, for the <code>SSAO</code> group.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="render-settings.html#symbol-tonemap-operator">tonemap_operator</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of an operator (&quot;<code>ACES_FITTED</code>&quot;).</p>

<div class="function-declaration" id="function-to-string-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="render-settings.html#symbol-exposure-mode">exposure_mode</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of an exposure mode (&quot;<code>AUTOMATIC</code>&quot;).</p>

<div class="function-declaration" id="function-to-string-3"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="render-settings.html#symbol-agx-look">agx_look</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of an AgX look (&quot;<code>GOLDEN</code>&quot;); &quot;<code>DEFAULT</code>&quot; for values outside the enum.</p>

<div class="function-declaration" id="function-to-string-4"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="render-settings.html#symbol-render-settings-group">render_settings_group</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>The <code>project.json</code> key of a group (&quot;<code>post_process</code>&quot;, &quot;shadows&quot;, &quot;ssao&quot;); &quot;&quot; for <code>COUNT</code> and values outside the enum.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="environment_settings"></a>
<a id="environment_settingsenvironment"></a>
<a id="environment_settingsintensity"></a>
<a id="environment_settingsoperator"></a>
<a id="environment_settingsrotationradians"></a>
<a id="exposure_mode"></a>
<a id="exposure_modeautomatic"></a>
<a id="exposure_modemanual"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="post_process_settings"></a>
<a id="post_process_settingsadaptationspeeddown"></a>
<a id="post_process_settingsadaptationspeedup"></a>
<a id="post_process_settingsagxlook"></a>
<a id="post_process_settingsbloomenabled"></a>
<a id="post_process_settingsbloomintensity"></a>
<a id="post_process_settingsexposure"></a>
<a id="post_process_settingsexposurecompensation"></a>
<a id="post_process_settingsmanualev100"></a>
<a id="post_process_settingsmaxev100"></a>
<a id="post_process_settingsminev100"></a>
<a id="post_process_settingsoperator"></a>
<a id="post_process_settingsreinhardwhitepoint"></a>
<a id="post_process_settingstonemap"></a>
<a id="shadow_settings"></a>
<a id="shadow_settingscascadecount"></a>
<a id="shadow_settingscasterpadding"></a>
<a id="shadow_settingsdefaultfilter"></a>
<a id="shadow_settingsdistance"></a>
<a id="shadow_settingsenabled"></a>
<a id="shadow_settingsnormaloffsettexels"></a>
<a id="shadow_settingsoperator"></a>
<a id="shadow_settingspcfradiustexels"></a>
<a id="shadow_settingssplitlambda"></a>
<a id="ssao_settings"></a>
<a id="ssao_settingsenabled"></a>
<a id="ssao_settingsintensity"></a>
<a id="ssao_settingsoperator"></a>
<a id="ssao_settingsradius"></a>
<a id="ssao_settingsthickness"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="to_string"></a>
<a id="tonemap_operator"></a>
<a id="tonemap_operatoraces_fitted"></a>
<a id="tonemap_operatoragx"></a>
<a id="tonemap_operatornone"></a>
<a id="tonemap_operatorreinhard_extended"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
