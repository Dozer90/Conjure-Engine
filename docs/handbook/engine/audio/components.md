---
kind: header
---

[Engine](../../engine.md) → [Audio](../audio.md)

# components.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/audio/components.h` |
| API ↗ | [Audio](../../../api/audio/index.md) |

<p>Component data the engine&#x27;s <code>audio_subsystem</code> registers into the ECS. Plain reflected structs; this module never includes ecs. <code><a href="components/audio-source.html">audio_source</a></code> carries an <code><a href="../assets/asset-ref/asset-ref.html">assets::asset_ref</a>&lt;<a href="clip/audio-clip.html">audio_clip</a>&gt;</code> (an <code>ASSET_REF</code> field: the uuid serialises, the scene bind pass binds it); its field order is the <code>C</code># AudioSourceData mirror and is pinned by the static assertions in <code>components.cpp</code>.</p>

## Structs

| File | Description |
|---|---|
| [`audio_listener`](components/audio-listener.md) | The listener component. The first active one in iteration order drives <code>audio_engine::set_listener</code>. |
| [`audio_source`](components/audio-source.md) | The emitter component (88 bytes). Authored fields come first; the three hidden fields at the end are runtime state: <code>playbackRequest</code> is written by scripts and consumed by <code>engine.audio_sync</code>, <code>isPlaying</code> is written back by the engine, handle links the component to its voice and is <code>TRANSIENT</code> at ECS registration. Hidden fields still take part in the reflection JSON (core serialises every described field); the ECS serializer drops the whole component when it is <code>TRANSIENT</code>, and the engine resets these three on load. |
| [`source_handle`](components/source-handle.md) | Link between a component and its voice. Hidden from the inspector, never serialised. |

## Enumerations

### `audio_playback_request` {#symbol-audio-playback-request}

```cpp
enum class audio_playback_request : u8 {
    NONE = 0,
    PLAY,
    STOP,
    PAUSE,
    RESUME
};
```

Script-side playback command, consumed and reset to <code>NONE</code> by <code>engine.audio_sync</code> (B8).

## Constants

| Name | Type | Value | Description |
|---|---|---|---|
| <a id="variable-ATTENUATION_MODEL_TYPE_NAME"></a><code>ATTENUATION_MODEL_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.audio.attenuation_<wbr>model"</span></code> |  |
| <a id="variable-AUDIO_LISTENER_TYPE_NAME"></a><code>AUDIO_LISTENER_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.audio.audio_<wbr>listener"</span></code> |  |
| <a id="variable-AUDIO_SOURCE_TYPE_NAME"></a><code>AUDIO_SOURCE_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.audio.audio_<wbr>source"</span></code> | Registered reflection names of the component types and enums. |
| <a id="variable-PLAYBACK_REQUEST_TYPE_NAME"></a><code>PLAYBACK_REQUEST_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.audio.audio_<wbr>playback_<wbr>request"</span></code> |  |
| <a id="variable-SOURCE_HANDLE_TYPE_NAME"></a><code>SOURCE_HANDLE_TYPE_NAME</code> | <code class="signature"><span class="k">inline</span> <span class="k">constexpr</span> std::<wbr>string_<wbr>view</code> | <code class="signature"><span class="s">"conjure.audio.source_<wbr>handle"</span></code> |  |

## Macros

### `CONJURE_REFLECT_ENUM` {#symbol-conjure-reflect-enum}

```cpp
CONJURE_REFLECT_ENUM(conjure::audio::attenuation_model, "conjure.audio.attenuation_model", NONE, INVERSE, LINEAR, EXPONENTIAL)
```

### `CONJURE_REFLECT_STRUCT` {#symbol-conjure-reflect-struct}

```cpp
CONJURE_REFLECT_STRUCT(conjure::core::slot_key, "conjure.core.slot_key", CONJURE_FIELD(index), CONJURE_FIELD(generation))
```

## Functions

### Public

#### `play_params_from` {#function-play-params-from}

<div class="function-declaration" id="function-play-params-from-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> <a href="voice/play-params.html">play_params</a> play_params_from(
    <span class="k">const</span> <a href="components/audio-source.html">audio_source</a> &amp;source,
    <a href="mix-groups/group-id.html">group_id</a> group) <span class="k">noexcept</span>;</code></pre></div>

<p>The <code><a href="voice/play-params.html">play_params</a></code> an <code><a href="components/audio-source.html">audio_source</a></code> describes: volume, pitch, loop, spatial, attenuation and distances copied from the fields (clamped by the engine at the API boundary), <code>group</code> as resolved by the caller (<code>audio_engine::find_group(source.group)</code>), retain = <code>true</code> because component voices are released by <code>engine.audio_sync</code>, pan 0 and the default priority.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="attenuation_model_type_name"></a>
<a id="audio_listener"></a>
<a id="audio_listener_type_name"></a>
<a id="audio_listeneractive"></a>
<a id="audio_listenergain"></a>
<a id="audio_listeneroperator"></a>
<a id="audio_playback_request"></a>
<a id="audio_playback_requestnone"></a>
<a id="audio_playback_requestpause"></a>
<a id="audio_playback_requestplay"></a>
<a id="audio_playback_requestresume"></a>
<a id="audio_playback_requeststop"></a>
<a id="audio_source"></a>
<a id="audio_source_type_name"></a>
<a id="audio_sourceattenuation"></a>
<a id="audio_sourceclip"></a>
<a id="audio_sourcegroup"></a>
<a id="audio_sourcehandle"></a>
<a id="audio_sourceisplaying"></a>
<a id="audio_sourceloop"></a>
<a id="audio_sourcemaxdistance"></a>
<a id="audio_sourcemindistance"></a>
<a id="audio_sourceoneshot"></a>
<a id="audio_sourceoperator"></a>
<a id="audio_sourcepitch"></a>
<a id="audio_sourceplaybackrequest"></a>
<a id="audio_sourceplayonstart"></a>
<a id="audio_sourcerolloff"></a>
<a id="audio_sourcespatial"></a>
<a id="audio_sourcevolume"></a>
<a id="classes"></a>
<a id="conjure_reflect_enum"></a>
<a id="conjure_reflect_struct"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="play_params_from"></a>
<a id="playback_request_type_name"></a>
<a id="source_handle"></a>
<a id="source_handle_type_name"></a>
<a id="source_handleclipversion"></a>
<a id="source_handleflag_started_once"></a>
<a id="source_handleflags"></a>
<a id="source_handleoperator"></a>
<a id="source_handlestarted_once"></a>
<a id="source_handlevoice"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
