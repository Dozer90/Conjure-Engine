---
kind: header
---

[Engine](../../engine.md) → [Audio](../audio.md)

# audio_engine.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/audio/audio_engine.h` |
| API ↗ | [Clips and decoding](../../../api/audio/clips-and-decoding.md), [Audio](../../../api/audio/index.md), [Playing sounds](../../../api/audio/playing-sounds.md) |

<p>This header brings together construction settings, batched voice transforms, diagnostic counters, and the audio engine facade. Include it when constructing an audio engine directly, driving audio in a tool or test, or extending the application&#x27;s audio integration. The engine owns its backend, mixer, voice table, groups, and listener; the small structures describe its inputs and observations.</p>

## Structs

| File | Description |
|---|---|
| [`audio_engine_stats`](audio-engine/audio-engine-stats.md) | <code>A</code> snapshot of voice, command, event, rendering, and device-recovery counters for diagnostics and profiling. |
| [`audio_settings`](audio-engine/audio-settings.md) | Construction settings for the audio engine, including backend selection, mix format, recovery policy, initial groups, and optional decoding jobs. |
| [`voice_transform`](audio-engine/voice-transform.md) | <code>A</code> voice identifier with its world-space position and velocity, used to submit several spatial updates together. |

## Classes

| File | Description |
|---|---|
| [`audio_engine`](audio-engine/audio-engine.md) | The audio facade that owns the backend, mixer, voices, group hierarchy, and listener, and coordinates them during each frame. |

## Forward Declarations

### `audio_engine::state` {#symbol-audio-engine-state}

```cpp
struct state;
```

### `detail::audio_engine_helpers` {#symbol-detail-audio-engine-helpers}

```cpp
struct audio_engine_helpers;
```

<!-- doc-content:notes:start -->

<!-- doc-content:notes:end -->

<!-- doc-content:threading:start -->

<!-- doc-content:threading:end -->

<!-- doc-content:extensions:start -->

<!-- doc-content:extensions:end -->

<!-- doc-content:tests:start -->

<!-- doc-content:tests:end -->

<!-- doc-legacy-anchors:start -->
<a id="audio_engine"></a>
<a id="audio_engine_helpers"></a>
<a id="audio_engine_stats"></a>
<a id="audio_engine_statsactivevoices"></a>
<a id="audio_engine_statscommandssent"></a>
<a id="audio_engine_statscommandsspilled"></a>
<a id="audio_engine_statsdevicerestarts"></a>
<a id="audio_engine_statsdevicestate"></a>
<a id="audio_engine_statseventsdropped"></a>
<a id="audio_engine_statseventsreceived"></a>
<a id="audio_engine_statsfellbacktonull"></a>
<a id="audio_engine_statsframesrendered"></a>
<a id="audio_engine_statsloops"></a>
<a id="audio_engine_statsoutputformat"></a>
<a id="audio_engine_statspeakvoices"></a>
<a id="audio_engine_statsrendercalls"></a>
<a id="audio_engine_statsstreamingvoices"></a>
<a id="audio_engine_statsunderruns"></a>
<a id="audio_engine_statsvoicesdropped"></a>
<a id="audio_engineactive_voice_count"></a>
<a id="audio_engineapply_params"></a>
<a id="audio_engineapply_source"></a>
<a id="audio_engineaudio_engine"></a>
<a id="audio_engineaudio_engine_1"></a>
<a id="audio_engineaudio_engine_helpers"></a>
<a id="audio_enginebackend_kind"></a>
<a id="audio_enginecreate_group"></a>
<a id="audio_engineend_frame"></a>
<a id="audio_engineenumerate_devices"></a>
<a id="audio_enginefind_group"></a>
<a id="audio_enginefinished_voices"></a>
<a id="audio_enginegroup"></a>
<a id="audio_enginegroups"></a>
<a id="audio_engineis_paused"></a>
<a id="audio_engineis_playing"></a>
<a id="audio_engineis_valid"></a>
<a id="audio_enginelast_backend_error"></a>
<a id="audio_enginelistener"></a>
<a id="audio_enginemstate"></a>
<a id="audio_enginenull_backend_instance"></a>
<a id="audio_engineoperator"></a>
<a id="audio_engineoutput_format"></a>
<a id="audio_enginepause"></a>
<a id="audio_engineplay"></a>
<a id="audio_enginerelease"></a>
<a id="audio_engineremove_group"></a>
<a id="audio_engineresume"></a>
<a id="audio_engineseek"></a>
<a id="audio_engineselect_device"></a>
<a id="audio_engineset_attenuation"></a>
<a id="audio_engineset_group"></a>
<a id="audio_engineset_group_gain"></a>
<a id="audio_engineset_group_paused"></a>
<a id="audio_engineset_listener"></a>
<a id="audio_engineset_loop"></a>
<a id="audio_engineset_master_gain"></a>
<a id="audio_engineset_pan"></a>
<a id="audio_engineset_paused"></a>
<a id="audio_engineset_pitch"></a>
<a id="audio_engineset_position"></a>
<a id="audio_engineset_positions"></a>
<a id="audio_engineset_priority"></a>
<a id="audio_engineset_spatial"></a>
<a id="audio_engineset_volume"></a>
<a id="audio_enginesettings"></a>
<a id="audio_enginestate"></a>
<a id="audio_enginestats"></a>
<a id="audio_enginestop"></a>
<a id="audio_enginestop_all"></a>
<a id="audio_engineupdate"></a>
<a id="audio_enginevoice"></a>
<a id="audio_settings"></a>
<a id="audio_settingsbackend"></a>
<a id="audio_settingsdevicename"></a>
<a id="audio_settingsdopplerfactor"></a>
<a id="audio_settingsgroups"></a>
<a id="audio_settingsjobs"></a>
<a id="audio_settingsmastergain"></a>
<a id="audio_settingsmaxrecoveryattempts"></a>
<a id="audio_settingsperiodframes"></a>
<a id="audio_settingsrecoverybackoffseconds"></a>
<a id="audio_settingssamplerate"></a>
<a id="audio_settingsspeedofsound"></a>
<a id="audio_settingsvalidate"></a>
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="detailaudio_engine_helpers"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<a id="voice_transform"></a>
<a id="voice_transformposition"></a>
<a id="voice_transformvelocity"></a>
<a id="voice_transformvoice"></a>
<!-- doc-legacy-anchors:end -->
