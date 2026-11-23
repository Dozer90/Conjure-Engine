---
kind: header
---

[Engine](../../engine.md) → [Audio](../audio.md)

# audio_error.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/audio/audio_error.h` |
| API ↗ | [Audio](../../../api/audio/index.md) |

<p>Error codes of the audio module. Built with the generic <code>core::make_error(audio_error::X, detail)</code> and bound to the category &quot;audio&quot; below.</p>

## Enumerations

### `audio_error` {#symbol-audio-error}

```cpp
enum class audio_error : u16 {
    BACKEND_INIT_FAILED = 1,
    DEVICE_NOT_FOUND,
    DEVICE_START_FAILED,
    DEVICE_LOST,
    UNSUPPORTED_FORMAT,
    DECODE_FAILED,
    INVALID_CLIP,
    CLIP_NOT_LOADED,
    VOICE_LIMIT_REACHED,
    INVALID_VOICE,
    STREAM_SLOT_LIMIT,
    INVALID_GROUP,
    GROUP_LIMIT_REACHED,
    GROUP_CYCLE,
    DUPLICATE_GROUP,
    COMMAND_QUEUE_FULL,
    INVALID_SETTINGS,
    NOT_INITIALISED
};
```

## Macros

### `CONJURE_DECLARE_ERROR_CATEGORY` {#symbol-conjure-declare-error-category}

```cpp
CONJURE_DECLARE_ERROR_CATEGORY(conjure::audio::audio_error, "audio", conjure::audio::audio_error_name)
```

## Functions

### Public

#### `audio_error_name` {#function-audio-error-name}

<div class="function-declaration" id="function-audio-error-name-1"><pre><code>[[<span class="na">nodiscard</span>]] <span class="na">CONJURE_API</span> std::string_view audio_error_name(
    <a href="audio-error.html#symbol-audio-error">audio_error</a> code) <span class="k">noexcept</span>;</code></pre></div>

<p>The enumerator name of a code (&quot;<code>DECODE_FAILED</code>&quot;); &quot;&quot; for unknown values.</p>

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="audio_error"></a>
<a id="audio_error_name"></a>
<a id="audio_errorbackend_init_failed"></a>
<a id="audio_errorclip_not_loaded"></a>
<a id="audio_errorcommand_queue_full"></a>
<a id="audio_errordecode_failed"></a>
<a id="audio_errordevice_lost"></a>
<a id="audio_errordevice_not_found"></a>
<a id="audio_errordevice_start_failed"></a>
<a id="audio_errorduplicate_group"></a>
<a id="audio_errorgroup_cycle"></a>
<a id="audio_errorgroup_limit_reached"></a>
<a id="audio_errorinvalid_clip"></a>
<a id="audio_errorinvalid_group"></a>
<a id="audio_errorinvalid_settings"></a>
<a id="audio_errorinvalid_voice"></a>
<a id="audio_errornot_initialised"></a>
<a id="audio_errorstream_slot_limit"></a>
<a id="audio_errorunsupported_format"></a>
<a id="audio_errorvoice_limit_reached"></a>
<a id="classes"></a>
<a id="conjure_declare_error_category"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="internals-and-algorithms"></a>
<a id="other-declarations"></a>
<a id="structs"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
