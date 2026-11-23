---
kind: header
---

[Engine](../../engine.md) → [Renderer](../renderer.md)

# render_settings.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/renderer/render_settings.h` |
| API ↗ | [Rendering](../../../api/renderer/index.md), [Rendering a frame](../../../api/renderer/rendering-a-frame.md) |

<p>Per-scene rendering settings carried by the <code><a href="render-scene/render-scene.html">render_scene</a></code>. Default values for tunable settings are defined here.</p>

## Structs

| File | Description |
|---|---|
| [`environment_settings`](render-settings/environment-settings.md) | The image-based lighting environment of the scene. |
| [`post_process_settings`](render-settings/post-process-settings.md) | HDR chain settings: exposure, bloom and the tonemap operator. |
| [`shadow_settings`](render-settings/shadow-settings.md) | Shadow settings shared by every light. |
| [`ssao_settings`](render-settings/ssao-settings.md) | GTAO settings. |

## Enumerations

### `exposure_mode` {#symbol-exposure-mode}

```cpp
enum class exposure_mode : u8 {
    AUTOMATIC = 0,
    MANUAL
};
```

### `tonemap_operator` {#symbol-tonemap-operator}

```cpp
enum class tonemap_operator : u8 {
    ACES_FITTED = 0,
    REINHARD_EXTENDED,
    AGX,
    NONE
};
```

## Functions

### Public

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="render-settings.html#symbol-tonemap-operator">tonemap_operator</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of an operator (&quot;<code>ACES_FITTED</code>&quot;).</p>

<div class="function-declaration" id="function-to-string-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="render-settings.html#symbol-exposure-mode">exposure_mode</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of an exposure mode (&quot;<code>AUTOMATIC</code>&quot;).</p>

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
