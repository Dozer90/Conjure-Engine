---
kind: header
---

[Engine](../../engine.md) → [Audio](../audio.md)

# null_backend.h
<!-- doc-template:header -->

|---|---|
| File Path | `engine/include/conjure/audio/null_backend.h` |
| API ↗ | [Clips and decoding](../../../api/audio/clips-and-decoding.md) |

<p>The headless backend: no device, no thread, no OS calls. Its <code>update(dt)</code> advances a virtual clock and calls the real mixer for <code>round(dt * sampleRate)</code> frames on the main thread (carrying the fractional remainder), so cursors, loops, ramps and group gains behave exactly as on hardware. The engine writes a snapshot of its voices, groups and listener into <code>stats()</code> every update, which is what headless tests assert on.</p>

## Structs

| File | Description |
|---|---|
| [`null_backend_stats`](null-backend/null-backend-stats.md) | Everything the <code>null</code> backend and the engine publish for tests and the profiler. |
| [`null_group_stats`](null-backend/null-group-stats.md) | Snapshot of one group (<code>effectivePaused</code> is the group&#x27;s own flag or any ancestor&#x27;s). |
| [`null_voice_stats`](null-backend/null-voice-stats.md) | Snapshot of one mixer slot. |

## Classes

| File | Description |
|---|---|
| [`null_backend`](null-backend/null-backend.md) |  |

## Forward Declarations

### `audio_engine` {#symbol-audio-engine}

```cpp
class audio_engine;
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
<a id="constructors"></a>
<a id="constructors-and-destructors"></a>
<a id="declaration-summaries"></a>
<a id="dependencies"></a>
<a id="detailaudio_engine_helpers"></a>
<a id="direct-header-dependencies"></a>
<a id="extension-points"></a>
<a id="functions"></a>
<a id="internals-and-algorithms"></a>
<a id="null_backend"></a>
<a id="null_backend_stats"></a>
<a id="null_backend_statsactivevoices"></a>
<a id="null_backend_statscommandsapplied"></a>
<a id="null_backend_statsframesrendered"></a>
<a id="null_backend_statsgroups"></a>
<a id="null_backend_statslistener"></a>
<a id="null_backend_statsrendercalls"></a>
<a id="null_backend_statsunderruns"></a>
<a id="null_backend_statsvirtualtimeseconds"></a>
<a id="null_backend_statsvoices"></a>
<a id="null_backendaudio_engine"></a>
<a id="null_backendaudio_engine_helpers"></a>
<a id="null_backendcaptured"></a>
<a id="null_backendenumerate_devices"></a>
<a id="null_backendfail_next_starts"></a>
<a id="null_backendkind"></a>
<a id="null_backendmcaptured"></a>
<a id="null_backendmcaptureframes"></a>
<a id="null_backendmdesc"></a>
<a id="null_backendmfailingstarts"></a>
<a id="null_backendmfractionalframes"></a>
<a id="null_backendmrenderbuffer"></a>
<a id="null_backendmstate"></a>
<a id="null_backendmstats"></a>
<a id="null_backendmutable_stats"></a>
<a id="null_backendnull_backend"></a>
<a id="null_backendnull_backend_1"></a>
<a id="null_backendoutput_format"></a>
<a id="null_backendrender_frames"></a>
<a id="null_backendset_capture_frames"></a>
<a id="null_backendsimulate_device_loss"></a>
<a id="null_backendstart"></a>
<a id="null_backendstate"></a>
<a id="null_backendstats"></a>
<a id="null_backendstop"></a>
<a id="null_backendupdate"></a>
<a id="null_backendvirtual_time"></a>
<a id="null_group_stats"></a>
<a id="null_group_statseffectivegain"></a>
<a id="null_group_statseffectivepaused"></a>
<a id="null_group_statsgain"></a>
<a id="null_group_statsid"></a>
<a id="null_group_statsname"></a>
<a id="null_group_statspaused"></a>
<a id="null_voice_stats"></a>
<a id="null_voice_statscursorframes"></a>
<a id="null_voice_statsgain"></a>
<a id="null_voice_statsgroupindex"></a>
<a id="null_voice_statsloop"></a>
<a id="null_voice_statspan"></a>
<a id="null_voice_statsstate"></a>
<a id="null_voice_statsstep"></a>
<a id="structs-and-types"></a>
<a id="tests"></a>
<a id="threading-model"></a>
<a id="variables-and-properties"></a>
<!-- doc-legacy-anchors:end -->
