---
kind: header
---

[Engine](../../engine.md) → [Engine](../engine.md)

# engine_config.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/engine/engine_config.h` |
| API ↗ | [Engine: the application and the frame](../../../api/engine/index.md), [Running the engine](../../../api/engine/running-the-engine.md) |

<p>The engine configuration: plain section structs with camelCase fields merged from <code>defaults()</code>, <code>project.json</code> and the command line by <code>application_desc::from_command_line</code> and validated once. <code>headless_defaults()</code> is what every test and server starts from: <code>null</code> platform / rhi / audio backends, rendering off, a forced frame delta.</p>

## Structs

| File | Description |
|---|---|
| [`assets_settings`](engine-config/assets-settings.md) | The asset pipeline of a development (non-cooked) project. <code>hotReload</code> drives the polling file watcher and the <code><a href="../assets/hot-reload/hot-reload-controller.html">assets::hot_reload_controller</a></code> whose stat passes run on job threads every 500 ms; it is ignored in Distribution (no database there) and off in <code>headless_defaults()</code> so a headless frame stays allocation-free (09 item 15a). <code>A</code> project&#x27;s &quot;assets&quot; section sets it for interactive runs. |
| [`audio_config`](engine-config/audio-config.md) | The &quot;audio&quot; section of the engine configuration. <code>audio_subsystem</code> maps it field by field onto <code><a href="../audio/audio-engine/audio-settings.html">audio::audio_settings</a></code>: backend through <code><a href="engine-config.html#symbol-audio-backend-choice">audio_backend_choice</a></code> (<code>NULL_DEVICE</code> and every headless run pick the <code>null</code> backend), groups by name; <code>enabled</code> is the engine&#x27;s own switch and has no counterpart there. <code>deviceName</code> is the <code>project.json</code> key &quot;device&quot;. |
| [`audio_group_setting`](engine-config/audio-group-setting.md) |  |
| [`engine_config`](engine-config/engine-config.md) |  |
| [`jobs_settings`](engine-config/jobs-settings.md) |  |
| [`memory_settings`](engine-config/memory-settings.md) |  |
| [`modules_settings`](engine-config/modules-settings.md) |  |
| [`pacing_settings`](engine-config/pacing-settings.md) |  |
| [`physics_config`](engine-config/physics-config.md) | The &quot;physics&quot; section of the engine configuration: an engine-side summary only. <code>physics_subsystem</code> builds the full <code><a href="../physics/physics-settings/physics-settings.html">physics::physics_settings</a></code> with <code>physics::physics_settings::from_json</code> over the raw <code>project.json</code> &quot;physics&quot; section and takes gravity and <code>maxBodies</code> from here only when that section does not name them. |
| [`rendering_settings`](engine-config/rendering-settings.md) |  |
| [`scripting_settings`](engine-config/scripting-settings.md) |  |
| [`time_settings`](engine-config/time-settings.md) |  |
| [`window_settings`](engine-config/window-settings.md) |  |

## Enumerations

### `audio_backend_choice` {#symbol-audio-backend-choice}

```cpp
enum class audio_backend_choice : u8 {
    AUTO,
    MINIAUDIO,
    NULL_DEVICE
};
```

Which audio backend the audio engine uses. <code>AUTO</code> picks miniaudio and falls back to the <code>null</code> backend.

### `rhi_backend_choice` {#symbol-rhi-backend-choice}

```cpp
enum class rhi_backend_choice : u8 {
    AUTO,
    D3D12,
    VULKAN,
    NULL_DEVICE
};
```

Which graphics backend the rhi device uses. <code>AUTO</code> tries the platform default, then the other compiled backend once.

### `script_export_mode` {#symbol-script-export-mode}

```cpp
enum class script_export_mode : u8 {
    FRAMEWORK_DEPENDENT,
    SELF_CONTAINED
};
```

How the exported managed runtime is published.

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-AUTO_WORKER_COUNT"></a><code>AUTO_WORKER_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">0</span>xFFFFFFFFu</code> | The <code>workerCount</code> that asks <code>resolve_worker_count()</code> for <code>hardware_concurrency</code> - 1 clamped to [<code>MIN_AUTO_WORKER_COUNT</code>, <code>MAX_AUTO_WORKER_COUNT</code>]. |
| <a id="variable-MAX_AUTO_WORKER_COUNT"></a><code>MAX_AUTO_WORKER_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">16</span></code> |  |
| <a id="variable-MIN_AUTO_WORKER_COUNT"></a><code>MIN_AUTO_WORKER_COUNT</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> <a href="../core/types.html#symbol-u32">u32</a></code> | <code class="signature"><span class="mi">1</span></code> | Smallest and largest worker counts <code>resolve_worker_count()</code> yields for an automatic request. |

## Functions

### Public

#### `parse_audio_backend_choice` {#function-parse-audio-backend-choice}

<div class="function-declaration" id="function-parse-audio-backend-choice-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="engine-config.html#symbol-audio-backend-choice">audio_backend_choice</a>&gt; parse_audio_backend_choice(
    std::string_view text);</code></pre></div>

#### `parse_rhi_backend_choice` {#function-parse-rhi-backend-choice}

<div class="function-declaration" id="function-parse-rhi-backend-choice-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="engine-config.html#symbol-rhi-backend-choice">rhi_backend_choice</a>&gt; parse_rhi_backend_choice(
    std::string_view text);</code></pre></div>

<p>Parses the names above case-insensitively. Fails with <code>BAD_ARGUMENTS</code> for anything else.</p>

#### `parse_script_export_mode` {#function-parse-script-export-mode}

<div class="function-declaration" id="function-parse-script-export-mode-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/result/result.html">core::result</a>&lt;<a href="engine-config.html#symbol-script-export-mode">script_export_mode</a>&gt; parse_script_export_mode(
    std::string_view text);</code></pre></div>

#### `resolve_worker_count` {#function-resolve-worker-count}

<div class="function-declaration" id="function-resolve-worker-count-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-u32">u32</a> resolve_worker_count(
    <span class="k">const</span> <a href="engine-config/jobs-settings.html">jobs_settings</a> &amp;settings,
    <a href="../core/types.html#symbol-u32">u32</a> hardwareConcurrency) <span class="k">noexcept</span>;</code></pre></div>

<p>The worker count a <code><a href="engine-config/jobs-settings.html">jobs_settings</a></code> resolves to: the request itself (0 = every job inline on the calling thread) unless it is <code>AUTO_WORKER_COUNT</code>, which becomes <code>hardwareConcurrency</code> - 1 clamped to [<code>MIN_AUTO_WORKER_COUNT</code>, <code>MAX_AUTO_WORKER_COUNT</code>].</p>

<div class="function-declaration" id="function-resolve-worker-count-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="../core/types.html#symbol-u32">u32</a> resolve_worker_count(
    <span class="k">const</span> <a href="engine-config/jobs-settings.html">jobs_settings</a> &amp;settings) <span class="k">noexcept</span>;</code></pre></div>

<p><code>resolve_worker_count</code> with the machine&#x27;s <code>std::thread::hardware_concurrency()</code>.</p>

#### `to_string` {#function-to-string}

<div class="function-declaration" id="function-to-string-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="engine-config.html#symbol-rhi-backend-choice">rhi_backend_choice</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>The lower-case names used by <code>project.json</code> and the command line (&quot;auto&quot;, &quot;d3d12&quot;, &quot;vulkan&quot;, &quot;<code>null</code>&quot;).</p>

<div class="function-declaration" id="function-to-string-2"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="engine-config.html#symbol-audio-backend-choice">audio_backend_choice</a> value) <span class="k">noexcept</span>;</code></pre></div>

<div class="function-declaration" id="function-to-string-3"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view to_string(
    <a href="engine-config.html#symbol-script-export-mode">script_export_mode</a> value) <span class="k">noexcept</span>;</code></pre></div>

<p>&quot;framework-dependent&quot; or &quot;self-contained&quot;.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="assets_settings"></a>
<a id="assets_settingshotreload"></a>
<a id="assets_settingsoperator"></a>
<a id="audio_backend_choice"></a>
<a id="audio_backend_choiceauto"></a>
<a id="audio_backend_choiceminiaudio"></a>
<a id="audio_backend_choicenull_device"></a>
<a id="audio_config"></a>
<a id="audio_configbackend"></a>
<a id="audio_configdevicename"></a>
<a id="audio_configdopplerfactor"></a>
<a id="audio_configenabled"></a>
<a id="audio_configgroups"></a>
<a id="audio_configmastergain"></a>
<a id="audio_configoperator"></a>
<a id="audio_configsamplerate"></a>
<a id="audio_configspeedofsound"></a>
<a id="audio_group_setting"></a>
<a id="audio_group_settinggain"></a>
<a id="audio_group_settingname"></a>
<a id="audio_group_settingoperator"></a>
<a id="audio_group_settingparent"></a>
<a id="auto_worker_count"></a>
<a id="classes"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="engine_config"></a>
<a id="engine_configassets"></a>
<a id="engine_configaudio"></a>
<a id="engine_configdefaults"></a>
<a id="engine_configeditor"></a>
<a id="engine_configheadless"></a>
<a id="engine_configheadless_defaults"></a>
<a id="engine_configjobs"></a>
<a id="engine_configlogfile"></a>
<a id="engine_configloglevel"></a>
<a id="engine_configmaxframes"></a>
<a id="engine_configmemory"></a>
<a id="engine_configmodules"></a>
<a id="engine_configoperator"></a>
<a id="engine_configpacing"></a>
<a id="engine_configphysics"></a>
<a id="engine_configprojectpath"></a>
<a id="engine_configrendering"></a>
<a id="engine_configscripting"></a>
<a id="engine_configstartupsceneoverride"></a>
<a id="engine_configtime"></a>
<a id="engine_configvalidate"></a>
<a id="engine_configwindow"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="jobs_settings"></a>
<a id="jobs_settingsoperator"></a>
<a id="jobs_settingsworkercount"></a>
<a id="max_auto_worker_count"></a>
<a id="memory_settings"></a>
<a id="memory_settingsframearenabytes"></a>
<a id="memory_settingsframethreadblockbytes"></a>
<a id="memory_settingsoperator"></a>
<a id="min_auto_worker_count"></a>
<a id="modules_settings"></a>
<a id="modules_settingsengineplugins"></a>
<a id="modules_settingsoperator"></a>
<a id="modules_settingsrequired"></a>
<a id="modules_settingssearchpaths"></a>
<a id="other-declarations"></a>
<a id="pacing_settings"></a>
<a id="pacing_settingsoperator"></a>
<a id="pacing_settingssleepwhenunfocused"></a>
<a id="pacing_settingstargethz"></a>
<a id="parse_audio_backend_choice"></a>
<a id="parse_rhi_backend_choice"></a>
<a id="parse_script_export_mode"></a>
<a id="physics_config"></a>
<a id="physics_configenabled"></a>
<a id="physics_configgravity"></a>
<a id="physics_configmaxbodies"></a>
<a id="physics_configoperator"></a>
<a id="rendering_settings"></a>
<a id="rendering_settingsbackend"></a>
<a id="rendering_settingsenabled"></a>
<a id="rendering_settingsgpuvalidation"></a>
<a id="rendering_settingsoperator"></a>
<a id="rendering_settingsshaderdirectory"></a>
<a id="resolve_worker_count"></a>
<a id="rhi_backend_choice"></a>
<a id="rhi_backend_choiceauto"></a>
<a id="rhi_backend_choiced3d12"></a>
<a id="rhi_backend_choicenull_device"></a>
<a id="rhi_backend_choicevulkan"></a>
<a id="script_export_mode"></a>
<a id="script_export_modeframework_dependent"></a>
<a id="script_export_modeself_contained"></a>
<a id="scripting_settings"></a>
<a id="scripting_settingsenabled"></a>
<a id="scripting_settingsexportdebugsymbols"></a>
<a id="scripting_settingsexportmode"></a>
<a id="scripting_settingsgameassembly"></a>
<a id="scripting_settingshotreload"></a>
<a id="scripting_settingsmanageddirectory"></a>
<a id="scripting_settingsoperator"></a>
<a id="scripting_settingsrequired"></a>
<a id="scripting_settingsusenullhost"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="time_settings"></a>
<a id="time_settingsfixeddeltaseconds"></a>
<a id="time_settingsforcedframedeltaseconds"></a>
<a id="time_settingsinterpolatephysics"></a>
<a id="time_settingsmaxfixedstepsperframe"></a>
<a id="time_settingsmaxframedeltaseconds"></a>
<a id="time_settingsoperator"></a>
<a id="time_settingstimescale"></a>
<a id="to_string"></a>
<a id="variables-and-properties"></a>
<a id="window_settings"></a>
<a id="window_settingsfullscreen"></a>
<a id="window_settingsheight"></a>
<a id="window_settingsoperator"></a>
<a id="window_settingsresizable"></a>
<a id="window_settingstitle"></a>
<a id="window_settingsvsync"></a>
<a id="window_settingswidth"></a>
<!-- doc-legacy-anchors:end -->
