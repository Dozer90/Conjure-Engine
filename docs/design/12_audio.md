# 12 - Audio module design (`conjure::audio`)

Status: **binding** (owner: lead architect). Expands `ARCHITECTURE.md` 4.3 into an implementable design for the
wave-2 module `conjure_audio`. Inputs: `00_brief.md`, `09_cross_area_resolutions.md` R7 (no `ecs` dependency; the
engine's `audio_subsystem` owns components and systems), `10_core.md` (exact core names), `05_assets.md` 2-4 and 8,
`06_engine_loop.md` 3.1 and 8, `08_testing.md` 5.2 and 8.4, `03_scripting.md` 8, `01_ecs.md` 8. Where this document
and `ARCHITECTURE.md` 4.3 differ in detail, this document wins (4.3 is now a pointer paragraph, R65); where it differs
from `09_cross_area_resolutions.md` (R43, R50, R52), that file wins and this one has a bug.

## 1. Overview and decisions

The module plays `audio_clip` assets through **voices**, mixes voices into **groups** (a gain tree rooted at
`MASTER_GROUP`), spatialises 3D voices against one **listener**, and talks to the sound card through a
**backend** (`miniaudio` for real devices, `null` for headless). It knows nothing about entities: the engine's
`audio_subsystem` maps `audio_source` / `audio_listener` components onto voices every frame (R7).

| Id | Decision | Rationale |
|---|---|---|
| A1 | One `audio_engine` per process (owned by `engine::application`, R22), constructed from `audio_settings`; backend chosen by `audio_backend_kind { AUTO, MINIAUDIO, NULL_BACKEND }`. Backend failure **never fails construction**: the engine falls back to `null` and logs WARN (`06_engine_loop.md` 3.4 step 11 "never fails"). | A game must run with no sound card; CI has none. |
| A2 | The mixer is **one backend-agnostic function**, `mixer::render(f32 *out, u32 frameCount)`, invoked by the miniaudio data callback on the device thread and by `null_backend::update(dt)` on the main thread. Output format is fixed: `f32` interleaved, `MIX_CHANNELS = 2`, `audio_settings.sampleRate` (default 48000); miniaudio converts to the device's native format. | One mixing code path is tested headless and shipped as-is. |
| A3 | Main thread -> mixer is a **lock-free SPSC ring of fixed-size commands**; mixer -> main is a second SPSC ring of fixed-size events plus relaxed atomics per voice (state, cursor). The callback never allocates, locks, logs, touches assets or walks the group tree: effective group gain, attenuation, pan and pitch ratio are computed on the main thread and sent as commands. | Glitch-free audio needs a deterministic callback; keeping all policy on the main thread also makes the `null` backend bit-identical to the real one. |
| A4 | Voices are `voice_id` keys of a `core::slot_map` with **`MAX_VOICES = 256`** mixer slots preallocated. A slot is reused only after the callback acknowledged the end of its previous occupant (FINISHED / STOPPED event with generation), so a stale `voice_id` can never address a live voice. | ABA-free voice reuse without locks. |
| A5 | Every voice is **retained or one-shot** (`play_params.retain`, default `false` = one-shot). One-shot voices are released by `audio_engine::end_frame()` once finished; retained voices live until `release(voice)`. `end_frame()` publishes the finished list for the engine's `engine.audio_finish_one_shots` system (FRAME_END). | Fire-and-forget sounds need no bookkeeping by callers; components that keep a handle get an explicit lifetime. |
| A6 | PCM lives in `pcm_buffer` objects shared through `std::shared_ptr<const pcm_buffer>`. The clip payload holds one; every playing voice holds a copy (acquired and released on the main thread only). The callback reads the raw pointer. | A pointer from `asset_ref::get()` is valid only until the end of the next frame (`05_assets.md` D4); a voice plays for minutes. Hot reload swaps the clip, playing voices finish on the old data, no dangling pointer, no `asset_handle` plumbing. |
| A7 | Clip storage is **`PCM`** (decoded `f32` at import, mixed directly) or **`STREAMED`** (compressed bytes kept in memory, decoded incrementally on `core::jobs` into a per-voice ring). Import setting `storage: auto \| pcm \| streamed`; `auto` picks `STREAMED` above `streamThresholdSeconds` (10 s). Decoding never happens inside the callback. | Music does not need 50 MB of PCM; short effects do not need a decoder per voice. |
| A8 | Decoding goes through miniaudio's decoders (`ma_decoder`, built with `MA_NO_ENGINE`, `MA_NO_NODE_GRAPH`, `MA_NO_GENERATION`): WAV, MP3 and FLAC are built in; **Ogg Vorbis requires `stb_vorbis` compiled into the miniaudio translation unit** (`third_party/src/miniaudio_impl.c` gains `#define STB_VORBIS_HEADER_ONLY` + `extras/stb_vorbis.c` before `miniaudio.h` and the implementation after it). Decoding is identical on both backends ("decoding runs for real", `08_testing.md` 8.4). | The brief requires `.wav .ogg .mp3`; without stb_vorbis miniaudio rejects `.ogg`. |
| A9 | Cooked PCM keeps the **source sample rate** (optionally resampled at import by setting `sampleRate`); the per-voice linear resampler that already implements pitch absorbs `clipRate / mixRate`. No conversion at load. | Cooked blobs stay independent of the device; one resampler instead of two. |
| A10 | Spatialisation v1: distance attenuation (`NONE, INVERSE, LINEAR, EXPONENTIAL` with `minDistance`, `maxDistance`, `rolloff`), constant-power stereo pan from listener-space azimuth, optional Doppler (`audio_settings.dopplerFactor`, default 0 = off). Stereo clips played as 3D voices are downmixed to mono before panning. No HRTF, reverb or occlusion. | Covers PacMan through a third-person 3D game; everything else is in section 11. |
| A11 | Groups form a tree (`MAX_GROUPS = 32`, depth <= 8) with `MASTER_GROUP` always present. The main thread computes each group's **effective gain** (product up the chain) whenever a gain changes and sends it per group; a voice's final gain is `volume * effectiveGroupGain * attenuation`. | The callback multiplies one number per voice. |
| A12 | The module publicly depends on **`core` and `assets`** (`conjure_add_module(audio ... PUBLIC_DEPS conjure_core conjure_assets PRIVATE_DEPS conjure::thirdparty::miniaudio)`), like `renderer`. `ARCHITECTURE.md` 2.2 shows `audio -> assets` (R43). Public headers include no miniaudio header. | `register_asset_types(assets::asset_type_registry &, assets::importer_registry &)` and `asset_ref<audio_clip>` in the component need the assets types. |
| A13 | Components `audio_source` and `audio_listener` are **plain reflected structs** (`CONJURE_REFLECT_STRUCT`) declared here; the engine's `audio_subsystem` registers them into the ECS (`CONJURE_COMPONENT` specialisation in the subsystem `.cpp`) and owns `engine.audio_sync` (SYNC, `RUNS_WHILE_PAUSED`) and `engine.audio_finish_one_shots` (FRAME_END). `audio_source` carries two hidden runtime fields, `playbackRequest` (written by scripts through the `audio_*` bindings, consumed and cleared by `engine.audio_sync`) and `isPlaying` (written back by the engine), plus a hidden, transient `source_handle` linking it to its voice (R50). | R7; the audio module compiles and tests without `ecs`; the component is the only entity -> voice map (`15_bindings.md` B8). |
| A14 | Device loss is handled on the main thread in `update(dt)`: the backend's notification sets an atomic `device_state`; the engine retries `start()` with a 1 s backoff up to `maxRecoveryAttempts` (5), then swaps to a `null` backend (WARN) so voices keep finishing and `is_playing` stays truthful. Voices are frozen (not advanced) while the device is down. | Unplugging headphones must not hang gameplay logic that waits for a sound to end. |
| A15 | Clamp rules are applied **at the API boundary on the main thread**, never in the callback: `volume` and group gain in `[0, 2]`, `pitch` in `[0.25, 4]`, `pan` in `[-1, 1]`, `minDistance >= 0.01`, `maxDistance >= minDistance`, `rolloff` in `[0, 10]`, `dopplerFactor` in `[0, 10]`, Doppler pitch ratio in `[0.5, 2]`; NaN becomes the default. Mixed output is hard-clamped to `[-1, 1]`. | Garbage in from scripts never produces NaN or infinite gain in the callback. |
| A16 | Performance targets: 256 simultaneous voices; `mixer::render` allocation-free (asserted by `conjure::testing::allocation_scope`); `audio_engine::update(dt)` plus `end_frame()` with 256 active voices < 0.1 ms main-thread time (`bench_audio`); engine memory fixed after construction (~6 MB: mixer slots, 32 stream rings of 16384 stereo frames, mix block, rings). | `00_brief.md` requirement 2 leaves audio a small slice of the frame. |
| A17 | **Group pause is hierarchical, like group gain** (code sweep F319). Each group keeps its own `paused` flag (what `set_group_paused` stored) and an **effective paused flag**: its own flag or any ancestor's. The main thread recomputes the effective flag with the same subtree walk as the gains in `set_group_paused` and `remove_group` (the whole subtree of each reparented child follows the new parent, as do the moved voices; a pause the removed group held goes with it), seeds it from the parent in `create_group` (a group created under a paused ancestor starts frozen), and sends `SET_GROUP_PAUSED` with the effective flag for every group whose flag changed. **The subtree is not frozen in one step:** the flags reach the mixer as one command per group, and the callback applies whatever has arrived at the top of each `MIX_BLOCK_FRAMES` block, so on a real device two groups of one subtree can freeze or resume apart: normally one block (256 frames), longer when the main thread is preempted between two pushes (nothing bounds that), or an `update()` apart when the command ring is full and the rest of the batch spills. Voices that must stay sample-synchronised belong in the same group, or are held with the whole-mixer `set_paused()`, which is a single command. `group_info` and `null_group_stats` report both (`paused`, `effectivePaused`). `set_group_paused(MASTER_GROUP, true)` therefore freezes every voice; it stays a flag of its own, independent of the whole-mixer `set_paused()` / `is_paused()`, which `effectivePaused` does not report. | The usual bus model: pausing `Music` must pause `Music/Ambience`, as its gain already scales it. Before, `set_group_paused` flipped one bit, so even pausing `MASTER_GROUP` left the voices of child groups playing. The mixer is unchanged: it still tests one bit per voice, which is also why a subtree pause is one command per group and not atomic within a block; a block-atomic subtree pause would need a mixer change and a decision of its own. |

## 2. Data model

Plain aggregate structs use camelCase fields; classes use `mCamelCase` (R31). Public types live in
`engine/include/conjure/audio/`; private types in `engine/src/audio/`.

### 2.1 Formats, ids and constants (`audio_types.h`)

```cpp
namespace conjure::audio {
    inline constexpr u32 MAX_VOICES = 256;
    inline constexpr u32 MAX_GROUPS = 32;
    inline constexpr u32 MAX_GROUP_DEPTH = 8;
    inline constexpr u32 MAX_STREAMING_VOICES = 32;
    inline constexpr u32 MIX_CHANNELS = 2;
    inline constexpr u32 MIX_BLOCK_FRAMES = 256;          // Ramp granularity inside the callback.
    inline constexpr u32 STREAM_RING_FRAMES = 16384;      // Per streaming voice, stereo f32 (128 KiB).
    inline constexpr u32 STREAM_CHUNK_FRAMES = 4096;      // One decode job fills this many frames.
    inline constexpr u32 COMMAND_RING_CAPACITY = 8192;    // Commands per ring (events: 4096); spill beyond.
    inline constexpr f32 DEFAULT_RAMP_SECONDS = 0.005f;   // Gain / stop ramps (click suppression).
    inline constexpr f32 DEFAULT_SPEED_OF_SOUND = 343.0f;

    enum class sample_format : u8 { F32 };                 // The only format the mixer and pcm_buffer use.
    enum class clip_storage : u8 { PCM, STREAMED };
    enum class clip_encoding : u8 { UNKNOWN, WAV, OGG_VORBIS, MP3, FLAC };
    enum class audio_backend_kind : u8 { AUTO, MINIAUDIO, NULL_BACKEND };
    enum class device_state : u8 { STOPPED, STARTING, RUNNING, LOST };
    enum class voice_state : u8 { FREE, PRIMING, PLAYING, PAUSED, STOPPING, FINISHED, STOPPED };
    enum class attenuation_model : u8 { NONE, INVERSE, LINEAR, EXPONENTIAL };

    struct audio_format {
        u32 sampleRate = 48000;
        u16 channels = MIX_CHANNELS;
        sample_format format = sample_format::F32;
        [[nodiscard]] bool operator==(const audio_format &other) const noexcept = default;
    };
    [[nodiscard]] std::string_view to_string(voice_state state) noexcept;   // Also for every other enum above.
}
```

`voice_id` (`voice.h`) wraps a `core::slot_key`: `struct voice_id { core::slot_key key = {}; is_valid(); to_u64();
static from_u64(u64); <=>; }`, 8 bytes, trivially copyable, mirrored by the C# `readonly struct AudioSource`
(`03_scripting.md` 4.1). `group_id` (`mix_groups.h`) is `{ u8 index = 0; u8 generation = 0; }` with
`MASTER_GROUP = { 0, 1 }`; `remove_group` retires the id at once, and `create_group` gives a reused slot a new
generation, so stale ids are rejected.

### 2.2 Clip (`clip.h`)

```cpp
namespace conjure::audio {
    /// <summary>Interleaved f32 frames. Immutable after construction; shared by clip and voices (A6).</summary>
    class pcm_buffer {
        private:
            audio_format mFormat = {};
            u64 mFrameCount = 0;
            std::vector<f32> mSamples = {};               // frameCount * channels
        public:
            pcm_buffer();
            pcm_buffer(audio_format format, std::vector<f32> samples);
            ~pcm_buffer();
        public:
            [[nodiscard]] const audio_format &format() const noexcept;
            [[nodiscard]] u64 frame_count() const noexcept;
            [[nodiscard]] std::span<const f32> samples() const noexcept;
            [[nodiscard]] f64 duration_seconds() const noexcept;
    };

    struct clip_info { audio_format format; u64 frameCount = 0; clip_encoding encoding; clip_storage storage; };

    /// <summary>The conjure.audio.clip payload: info plus one of two shared buffers.</summary>
    struct audio_clip {
        clip_info info = {};
        std::shared_ptr<const pcm_buffer> pcm = nullptr;                 // PCM storage.
        std::shared_ptr<const std::vector<byte>> encoded = nullptr;       // STREAMED storage (original file).
        [[nodiscard]] bool is_streamed() const noexcept { return info.storage == clip_storage::STREAMED; }
        [[nodiscard]] f64 duration_seconds() const noexcept;
    };
    CONJURE_DECLARE_ASSET_TYPE(conjure::audio::audio_clip, "conjure.audio.clip");
}
```

Cooked blob payload (`clip_asset.h`, little-endian, 32-byte header): `clip_blob_header { char magic[4] = "CJAC";
u32 version = 1; u8 storage; u8 channels; u8 encoding; u8 reserved; u32 sampleRate; u64 frameCount; u64 dataBytes; }`
followed by `dataBytes` of interleaved `f32` samples (PCM) or the original encoded file (STREAMED). Importer id
`conjure.audio.clip`, version 1, extensions `.wav .ogg .mp3 .flac`. Import settings (JSON, canonicalised and hashed by
the assets module), mirrored by `clip_import_settings` with `from_json` / `to_json`:

```json
{ "storage": "auto", "streamThresholdSeconds": 10.0, "forceMono": false, "sampleRate": 0, "normalize": false }
```

`forceMono` downmixes at import (recommended for 3D effects); `sampleRate` 0 keeps the source rate, otherwise the
importer resamples with miniaudio's linear resampler (deterministic for a given build); `normalize` scales the peak
to 1.0.

### 2.3 Voices (`voice.h`)

```cpp
namespace conjure::audio {
    struct play_params {
        f32 volume = 1.0f;                 // [0, 2]
        f32 pitch = 1.0f;                  // [0.25, 4]
        f32 pan = 0.0f;                    // [-1, 1]; used only when spatial == false
        bool loop = false;
        bool spatial = false;              // 3D: attenuation + pan from position (set_position)
        bool retain = false;               // false = one-shot, released automatically when finished (A5)
        bool startPaused = false;
        u8 priority = 128;                 // Voice stealing: lower is stolen first
        group_id group = MASTER_GROUP;
        f64 startSeconds = 0.0;            // Seek before start
        attenuation_model attenuation = attenuation_model::INVERSE;
        f32 minDistance = 1.0f;
        f32 maxDistance = 100.0f;
        f32 rolloff = 1.0f;
    };
}
```

`voice_info` (same header, main-thread mirror refreshed by `update()` / `end_frame()`): `{ voice_state state; u64
cursorFrames (relaxed atomic from the callback); u64 clipFrames; f32 volume; f32 pitch; f32 effectiveGain (volume *
groupGain * attenuation, last value sent); group_id group; bool loop; bool spatial; bool retain; u32 clipVersion
(`asset_ref::version()` at play time, for reload detection); }`.

Private mixer-side slot (`engine/src/audio/mixer.h`), exactly `MAX_VOICES` in a `std::array`: `mixer_voice {
std::atomic<u8> state; u32 generation; const pcm_buffer *pcm; stream_ring *stream; f64 cursor; f64 step;
f32 gainCurrent; f32 gainTarget; f32 panCurrent; f32 panTarget; bool loop; bool downmix; u8 groupIndex;
u16 channels; }` with `step = pitch * clipRate / mixRate`. Main-thread slot (`voice_slot`, the `slot_map` element):
`voice_info`, shared `pcm` / `encoded` pointers, `stream_slot *`, `position`, `velocity`, `play_params`, `awaitingAck`.

### 2.4 Groups (`mix_groups.h`)

`struct group_desc { core::string_id name; group_id parent = MASTER_GROUP; f32 gain = 1.0f; }` and
`struct group_info { group_id id; core::string_id name; group_id parent; f32 gain; f32 effectiveGain; bool paused;
bool effectivePaused; }` (`paused` is the group's own flag, `effectivePaused` its own flag or any ancestor's, A17).
The main thread owns `std::array<group_slot, MAX_GROUPS>`; the mixer owns `std::array<f32, MAX_GROUPS>` of effective
gains plus an effective paused bit per group, written only by commands.

### 2.5 Listener and spatial maths (`spatialisation.h`)

```cpp
namespace conjure::audio {
    // Defaults: position / velocity zero, forward -Z, up +Y, gain 1 (one declaration per line in code).
    struct listener_state { math::vec3 position; math::vec3 forward; math::vec3 up; math::vec3 velocity; f32 gain; };
    struct attenuation_params { attenuation_model model; f32 minDistance; f32 maxDistance; f32 rolloff; };
    struct spatial_result { f32 gain = 1.0f; f32 pan = 0.0f; f32 pitchRatio = 1.0f; };

    [[nodiscard]] f32 compute_attenuation(const attenuation_params &params, f32 distance) noexcept;
    [[nodiscard]] f32 compute_pan(const listener_state &listener, math::vec3 sourcePosition) noexcept;
    [[nodiscard]] f32 compute_doppler_ratio(/* listener, position, velocity, dopplerFactor, speedOfSound */) noexcept;
    [[nodiscard]] spatial_result compute_spatial(
      const listener_state &listener,
      math::vec3 sourcePosition,
      math::vec3 sourceVelocity,
      const attenuation_params &params,
      f32 dopplerFactor,
      f32 speedOfSound
    ) noexcept;
}
```

Formulas with `d = clamp(distance, minDistance, maxDistance)`: INVERSE `min / (min + rolloff * (d - min))`; LINEAR
`1 - rolloff * (d - min) / (max - min)` clamped to `[0, 1]`; EXPONENTIAL `(d / min) ^ -rolloff`; NONE `1`. Pan is the
listener-right component of the normalised direction; the mixer applies constant power
(`left = cos((pan + 1) * PI / 4)`, `right = sin(...)`). Doppler: `(c - v_l . dir) / (c - v_s . dir)` raised to
`dopplerFactor`, clamped to `[0.5, 2]`.

### 2.6 Components (`components.h`)

```cpp
namespace conjure::audio {
    /// <summary>Link between a component and its voice. Hidden from the inspector, never serialised.</summary>
    struct source_handle {
        voice_id voice = {};
        u32 clipVersion = 0;               // asset_ref::version() when the voice was started
        u8 flags = 0;                      // bit 0: started once (play requests live in audio_source::playbackRequest, R50)
    };

    /// <summary>Script-side playback command, consumed and reset to NONE by engine.audio_sync (15_bindings.md B8, R50).</summary>
    enum class audio_playback_request : u8 { NONE = 0, PLAY, STOP, PAUSE, RESUME };

    struct audio_source {                  // 88 bytes; field order is the C# AudioSourceData mirror (15_bindings.md 2.6)
        assets::asset_ref<audio_clip> clip = {};
        f32 volume = 1.0f;                 // 0..2
        f32 pitch = 1.0f;                  // 0.25..4
        bool loop = false;
        bool spatial = true;               // The "3D flag"
        bool playOnStart = true;
        bool oneShot = false;              // Destroy the entity when the voice finishes
        core::string_id group = {};        // Null = MASTER
        attenuation_model attenuation = attenuation_model::INVERSE;
        f32 minDistance = 1.0f;
        f32 maxDistance = 100.0f;
        f32 rolloff = 1.0f;
        audio_playback_request playbackRequest = audio_playback_request::NONE;   // CONJURE_FIELD_HIDDEN; never serialised (R50)
        bool isPlaying = false;            // CONJURE_FIELD_HIDDEN; written by the engine, one frame old for scripts (R50)
        source_handle handle = {};         // CONJURE_FIELD_HIDDEN; TRANSIENT at ECS registration
    };

    struct audio_listener {
        f32 gain = 1.0f;
        bool active = true;
    };
}
CONJURE_REFLECT_STRUCT(conjure::audio::audio_source, "conjure.audio.audio_source",
    CONJURE_FIELD(clip), CONJURE_FIELD(volume, .min = 0.0, .max = 2.0), CONJURE_FIELD(pitch, .min = 0.25, .max = 4.0),
    CONJURE_FIELD(loop), CONJURE_FIELD(spatial), CONJURE_FIELD(playOnStart), CONJURE_FIELD(oneShot),
    CONJURE_FIELD(group), CONJURE_FIELD(attenuation), CONJURE_FIELD(minDistance, .min = 0.01),
    CONJURE_FIELD(maxDistance, .min = 0.01), CONJURE_FIELD(rolloff, .min = 0.0, .max = 10.0),
    CONJURE_FIELD_HIDDEN(playbackRequest), CONJURE_FIELD_HIDDEN(isPlaying), CONJURE_FIELD_HIDDEN(handle))
CONJURE_REFLECT_STRUCT(conjure::audio::audio_listener, "conjure.audio.audio_listener",
    CONJURE_FIELD(gain, .min = 0.0, .max = 2.0), CONJURE_FIELD(active))
CONJURE_REFLECT_ENUM(conjure::audio::attenuation_model, "conjure.audio.attenuation_model",
    NONE, INVERSE, LINEAR, EXPONENTIAL)
```

Both structs are trivially copyable (`static_assert`), so ECS chunk relocation is a memcpy. `clip` serialises as a
uuid string (`field_kind::ASSET_REF`, bound by the scene bind pass, R25); the C# `[NativeComponent]` mirror is
generated from the same field list (layout hash checked, `03_scripting.md` 3.6).

### 2.7 Commands and events (private, `engine/src/audio/commands.h`)

`voice_command` (48 bytes, trivially copyable): `command_kind : u8 { PLAY, STOP, PAUSE, RESUME, SET_GAIN, SET_PAN,
SET_STEP, SET_LOOP, SEEK, SET_GROUP, SET_GROUP_GAIN, SET_GROUP_PAUSED, SET_MASTER_PAUSED }`, `u32 voiceIndex`,
`u32 generation`, `u8 groupIndex`, payload union (`play { pcm *, stream_ring *, gain, pan, step, startFrame, loop,
channels, downmix }`, `f32 scalar`, `f64 step`, `u64 frame`). `voice_event` (24 bytes): `event_kind : u8 { STARTED,
FINISHED, STOPPED, LOOPED, UNDERRUN, PAUSED, RESUMED }`, `voiceIndex`, `generation`, `u64 frame`. `spsc_ring<T,
Capacity>` is power-of-two with acquire / release head and tail on separate cache lines, `try_push` / `try_pop` only.

## 3. Algorithms and flow

### 3.1 Play

```
play(ref, params)                                                       [main]
    clip = ref.get(); null -> error CLIP_NOT_LOADED; params = clamp(params) (A15)
    slot = voices.emplace(); mixer slots full -> steal() (3.6) or error VOICE_LIMIT_REACHED
    slot.pcm = clip->pcm | slot.encoded = clip->encoded                 shared_ptr copies (A6)
    STREAMED: acquire_stream_slot() or error STREAM_SLOT_LIMIT; state PRIMING; schedule the first decode job
    PCM:      push PLAY { pcm, step = pitch * clipRate / mixRate, gain, pan, loop, startFrame }; state PLAYING
    spatial:  position defaults to the listener position until set_position(); gain is sent by update()
```

A PRIMING streamed voice sends its PLAY from `update()` once its ring holds one chunk; `is_playing()` is already true
during PRIMING so callers never observe a gap. `startPaused` starts the voice in PAUSED.

### 3.2 `update(deltaSeconds)` (main thread, once per frame from `engine.audio_sync`)

```
1. backend.update(dt)   miniaudio: poll device_state (3.8); null: advance the clock, render dt * rate frames (3.3)
2. drain event ring     FINISHED / STOPPED: mirror state, awaitingAck = false; LOOPED / UNDERRUN: counters
3. spatial pass         per PLAYING / PAUSED spatial voice: compute_spatial(...); push SET_GAIN when |delta| > 1e-4,
                        SET_PAN when > 1e-3, SET_STEP when Doppler is on and the ratio moved > 1e-3
4. streaming pass       per stream slot: ring free >= STREAM_CHUNK_FRAMES and no job in flight -> schedule
                        decode_chunk (3.7); PRIMING slot with one chunk -> push PLAY
5. flush spill          move spilled commands into the ring in order while space allows
6. stats                activeVoices, peak, callback counters (relaxed loads); null: write the stats snapshot
```

Step 3 costs ~60 flops and one 48-byte push per voice; 256 voices stay far below 0.1 ms (A16).

### 3.3 Mixer callback (device thread, or main thread on `null`)

```
render(out, frameCount):                                 no allocation, locks, logging, decoding
    for block in split(frameCount, MIX_BLOCK_FRAMES):
        drain the command ring (bounded by its capacity); drop commands whose generation mismatches
        zero mix[block]
        for v in mixer_voices with state PLAYING or STOPPING and group not paused:
            ramp gainCurrent -> gainTarget and panCurrent -> panTarget linearly over the block
            for frame in block:
                s = PCM ? lerp(pcm, cursor) : stream.pop()   (empty ring -> 0, one UNDERRUN event per block)
                s = downmix ? 0.5 * (s.l + s.r) : s;  g = gainCurrent * groupGain[v.group]
                mix[frame] += (g * cos((pan + 1) * PI / 4) * s, g * sin((pan + 1) * PI / 4) * s);  cursor += step
                cursor >= frames: loop ? (cursor -= frames; event LOOPED) : (state FINISHED; event FINISHED)
            STOPPING and gainCurrent <= 1e-4: state STOPPED; event STOPPED
            store cursor (relaxed) for the main-thread mirror
        clamp mix to [-1, 1]; copy to out
```

Paused groups and the master pause skip voices without advancing cursors. The group bit the mixer tests is the
effective flag (the group's own or any ancestor's, A17), so the callback never walks the tree. `groupGain[]` is a
plain array written only by commands applied at the top of the same block.

### 3.4 Stop, pause, release, end of frame

`stop(voice, fadeSeconds = DEFAULT_RAMP_SECONDS)` pushes STOP; the callback ramps to zero, then emits STOPPED.
`release(voice)` on a retained voice stops it if needed and marks it for erasure at the acknowledgement. `pause` /
`resume` keep the cursor. `stop_all()` and `set_paused(bool)` are single commands. `end_frame()` erases every
acknowledged one-shot (FINISHED or STOPPED), releases its shared pointers and stream slot, and publishes the frame's
finished ids through `finished_voices()` (a span valid until the next `end_frame()`).

### 3.5 Group gains and group pause

`set_group_gain(group, gain)`: clamp, store, recompute `effectiveGain` for the group and its descendants (depth-first
over at most 32 entries), push one `SET_GROUP_GAIN { index, effectiveGain }` per changed group. `create_group` fails
with `GROUP_LIMIT_REACHED`, `DUPLICATE_GROUP`, `GROUP_CYCLE` or `INVALID_GROUP` (unknown parent); `remove_group`
reparents children and moves voices to the parent (`SET_GROUP` per voice) before retiring the id.

`set_group_paused(group, paused)` (A17): store the group's own flag, recompute `effectivePaused = paused ||
parent.effectivePaused` over the same subtree walk, push one `SET_GROUP_PAUSED { index, effectivePaused }` per group
whose effective flag changed (none when the flag is unchanged, or when an ancestor already holds the group paused).
`create_group` seeds the new group's effective flag from its parent and sends it; `remove_group` recomputes the
subtree of each reparented child against its new parent, sends the groups that changed, and clears the retired
slot's bit for its next occupant. Releasing a child under a paused parent clears only its own flag; it plays once
every ancestor is released too.

The commands of one call are pushed one after the other and the callback drains the ring at the top of every block
(3.3), so a subtree pause is not atomic on a real device: a block can render with the parent frozen and a descendant
group still playing (or the reverse on release), which normally leaves their voices `MIX_BLOCK_FRAMES` frames apart.
That is no upper bound: a main thread preempted between two pushes leaves them apart for as many blocks as the
callback renders meanwhile, and commands that spill from a full ring arrive an `update()` later. The null backend
renders on the calling thread (inside `update()` or `render_frames()`), never between two pushes of one call, so
tests never see it. Voices that must stay sample-synchronised share one group or are held with `set_paused()`, a
single command (A17).

### 3.6 Voice stealing

When all mixer slots are occupied, `play()` picks the victim with the lowest `priority <= params.priority`, ties broken
by lowest `effectiveGain`, excluding looping retained voices; the victim gets STOP with a 2 ms fade and the new voice
waits in PRIMING for the acknowledgement. No candidate: `VOICE_LIMIT_REACHED`, `stats.voicesDropped++`, TRACE log.

### 3.7 Streaming decode job

```
decode_chunk(slot):                                      [worker; one job per slot at a time]
    frames = decoder.read(scratch, STREAM_CHUNK_FRAMES)  ma_decoder over the in-memory encoded bytes
    frames < chunk and loop: decoder.seek(0); read the remainder
    frames == 0 and not loop: slot.endOfStream = true
    ring.push(scratch, frames)                           single producer (this job), single consumer (callback)
    slot.jobInFlight.store(false, release)
```

An empty ring with `endOfStream` ends the voice; an empty ring without it is an UNDERRUN (silence, counted).
`seek()` on a streamed voice resets ring and decoder in the next job.

### 3.8 Device loss and recovery (miniaudio backend)

miniaudio's notification callback (`ma_device_notification_type_stopped`, `rerouted`, `interruption_began/ended`)
stores `device_state::LOST` or `RUNNING` into an atomic; no engine code runs inside it. `update()` observing LOST stops
the device, waits `recoveryBackoffSeconds` (1.0) of frame time, re-enumerates and re-opens the configured or default
device; mixer state is untouched (voices neither advance nor finish while down). After `maxRecoveryAttempts` failures
the engine replaces the backend with `null_backend` (WARN, `stats.deviceRestarts`, `stats.fellBackToNull = true`).
Default-device changes on WASAPI / CoreAudio reroute inside miniaudio without an engine restart.

### 3.9 Device enumeration and selection

`enumerate_devices()` runs `ma_context_get_devices` (playback only) and returns `audio_device_info { std::string name;
bool isDefault; std::vector<u32> sampleRates; u32 minChannels; u32 maxChannels; }`. An empty `deviceName` selects
the default; an unknown name logs WARN and uses the default. `null` reports one device, `"Null Output"` (48000, 2).

### 3.10 Decoding (`decoder.h`)

`probe(bytes)` sniffs the magic (`RIFF/WAVE`, `OggS`, `ID3` or MP3 sync, `fLaC`) and opens a `ma_decoder` to read
format and length. `decode_to_pcm(bytes, options)` decodes to interleaved `f32` at native channels / rate, applying
`forceMono`, `sampleRate` (via `ma_data_converter`) and `normalize`; failures return `DECODE_FAILED` naming
miniaudio's result. `streaming_decoder` wraps a `ma_decoder` over memory. All of this is backend independent.

## 4. Public API (`engine/include/conjure/audio/`)

Every header includes only `conjure/core/...`, `conjure/assets/...` and the standard library.

| Header | Main types |
|---|---|
| `audio.h` | umbrella include |
| `log_category.h` | `CONJURE_DECLARE_LOG_CATEGORY(LOG_AUDIO)` (defined with `"audio"` in `audio_engine.cpp`) |
| `audio_error.h` | `audio_error`, `audio_error_name`, `CONJURE_DECLARE_ERROR_CATEGORY(audio_error, "audio", audio_error_name)` |
| `audio_types.h` | constants, `sample_format`, `clip_storage`, `clip_encoding`, `audio_backend_kind`, `device_state`, `voice_state`, `attenuation_model`, `audio_format`, `to_string` |
| `clip.h` | `pcm_buffer`, `clip_info`, `audio_clip`, `CONJURE_DECLARE_ASSET_TYPE` |
| `decoder.h` | `decode_options`, `probe`, `decode_to_pcm`, `streaming_decoder` |
| `clip_asset.h` | `clip_blob_header`, `clip_import_settings`, `audio_clip_importer`, `audio_clip_loader`, `register_asset_types` |
| `voice.h` | `voice_id`, `play_params`, `voice_info` |
| `mix_groups.h` | `group_id`, `MASTER_GROUP`, `group_desc`, `group_info` |
| `spatialisation.h` | `listener_state`, `attenuation_params`, `spatial_result`, `compute_*` |
| `components.h` | `source_handle`, `audio_source`, `audio_listener` + reflection |
| `audio_device.h` | `audio_device_info`, `backend_desc`, `mix_callback` |
| `audio_backend.h` | `audio_backend` interface, `create_backend` |
| `null_backend.h` | `null_voice_stats`, `null_group_stats`, `null_backend_stats`, `null_backend` |
| `audio_engine.h` | `audio_settings`, `audio_engine_stats`, `audio_engine` |

Private (`engine/src/audio/`): `mixer.{h,cpp}`, `commands.h`, `spsc_ring.h` (candidate for `core/containers`),
`stream_ring.{h,cpp}`, `voice_table.{h,cpp}`, `group_table.{h,cpp}`, `miniaudio_backend.{h,cpp}` and
`decoder_impl.cpp` (the only files including `miniaudio.h`), one `.cpp` per public header, `README.md`.

### 4.1 `audio_error.h`

```cpp
namespace conjure::audio {
    enum class audio_error : u16 {
        BACKEND_INIT_FAILED = 1, DEVICE_NOT_FOUND, DEVICE_START_FAILED, DEVICE_LOST, UNSUPPORTED_FORMAT,
        DECODE_FAILED, INVALID_CLIP, CLIP_NOT_LOADED, VOICE_LIMIT_REACHED, INVALID_VOICE, STREAM_SLOT_LIMIT,
        INVALID_GROUP, GROUP_LIMIT_REACHED, GROUP_CYCLE, DUPLICATE_GROUP, COMMAND_QUEUE_FULL, INVALID_SETTINGS,
        NOT_INITIALISED
    };
    [[nodiscard]] std::string_view audio_error_name(audio_error code) noexcept;
}
CONJURE_DECLARE_ERROR_CATEGORY(conjure::audio::audio_error, "audio", conjure::audio::audio_error_name);
```

### 4.2 `decoder.h` and `clip_asset.h`

```cpp
namespace conjure::audio {
    struct decode_options { bool forceMono = false; u32 sampleRate = 0; bool normalize = false; };   // 0 = keep rate

    [[nodiscard]] core::result<clip_info> probe(std::span<const byte> encoded);
    [[nodiscard]] core::result<pcm_buffer> decode_to_pcm(std::span<const byte> encoded, const decode_options &options);

    class streaming_decoder {
        public:
            streaming_decoder();
            ~streaming_decoder();
        public:
            /// <summary>Opens a decoder over caller-owned bytes, which must outlive the decoder.</summary>
            [[nodiscard]] core::result<void> open(std::span<const byte> encoded);
            [[nodiscard]] const clip_info &info() const noexcept;
            /// <summary>Decodes up to out.size() / channels frames; returns frames written (0 at the end).</summary>
            [[nodiscard]] usize read(std::span<f32> out) noexcept;
            [[nodiscard]] core::result<void> seek(u64 frame) noexcept;
            void close() noexcept;
    };

    class audio_clip_importer final : public assets::asset_importer {
        public:
            audio_clip_importer();
            ~audio_clip_importer() override;
        public:
            [[nodiscard]] std::string_view id() const noexcept override;        // "conjure.audio.clip"
            [[nodiscard]] u32 version() const noexcept override;                // 1
            [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override;
            [[nodiscard]] core::json::value default_settings() const override;
            [[nodiscard]] core::result<assets::import_output> import(assets::import_context &context) const override;
    };

    class audio_clip_loader final : public assets::typed_asset_loader<audio_clip, audio_clip> {
        public:
            audio_clip_loader();
            ~audio_clip_loader() override;
        public:
            /// <summary>A 0.1 s silent mono 48 kHz clip (the "silent clip" fallback of 05_assets.md D14).</summary>
            [[nodiscard]] core::result<assets::asset_payload> create_fallback() const override;
        protected:
            [[nodiscard]] core::result<audio_clip> load_cpu_typed(
              const assets::load_context &context,
              std::span<const byte> payloadBytes
            ) const override;
            [[nodiscard]] core::result<audio_clip> finalise_typed(
              const assets::finalise_context &context,
              audio_clip &&intermediate
            ) const override;                                                   // Returns the clip unchanged.
    };

    /// <summary>Registers conjure.audio.clip and its importer. Main thread, in audio_subsystem::startup.</summary>
    [[nodiscard]] core::result<void> register_asset_types(
      assets::asset_type_registry &types,
      assets::importer_registry &importers
    );
}
```

The loader needs no `audio_engine &`: the clip is a self-contained CPU payload (A6); `ARCHITECTURE.md` 5 and
`05_assets.md` 8 say the same (R43). `load_cpu_typed` validates the header, copies PCM
into a fresh `pcm_buffer` (or wraps the encoded bytes) and `probe`s streamed clips so `info` is complete at publish.

### 4.3 `audio_backend.h`, `audio_device.h`, `null_backend.h`

```cpp
namespace conjure::audio {
    using mix_callback = void (*)(void *user, f32 *output, u32 frameCount) noexcept;

    class audio_backend {
        public:
            audio_backend();
            virtual ~audio_backend();
        public:
            [[nodiscard]] virtual audio_backend_kind kind() const noexcept = 0;
            [[nodiscard]] virtual core::result<void> start(const backend_desc &desc) = 0;
            virtual void stop() noexcept = 0;
            [[nodiscard]] virtual device_state state() const noexcept = 0;             // Atomic; any thread
            [[nodiscard]] virtual audio_format output_format() const noexcept = 0;    // Actual format after start
            [[nodiscard]] virtual core::result<std::vector<audio_device_info>> enumerate_devices() const = 0;
            /// <summary>Main thread, once per frame. null: advance the clock and render; miniaudio: poll.</summary>
            virtual void update(f32 deltaSeconds) = 0;
    };

    /// <summary>AUTO = MINIAUDIO when a device backend is compiled in, else NULL_BACKEND.</summary>
    [[nodiscard]] core::result<std::unique_ptr<audio_backend>> create_backend(audio_backend_kind kind);

    class null_backend final : public audio_backend {
        public:
            null_backend();
            ~null_backend() override;
        public:
            // audio_backend overrides as above; start() fails only when desc.callback == nullptr.
            [[nodiscard]] const null_backend_stats &stats() const noexcept;
            /// <summary>Tests: keep the last `frames` rendered stereo f32 frames for inspection.</summary>
            void set_capture_frames(u32 frames);
            [[nodiscard]] std::span<const f32> captured() const noexcept;
            [[nodiscard]] f64 virtual_time() const noexcept;
    };
}
```

`backend_desc` (`audio_device.h`): `{ audio_format format; u32 periodFrames = 480; std::string deviceName;
mix_callback callback; void *user; }`, the requested mix format (channels fixed at `MIX_CHANNELS`), period (480 =
10 ms at 48 kHz; miniaudio may round), device name (empty = default) and the callback the backend drives. Stats
structs (`null_backend.h`, all fields initialised): `null_voice_stats { voice_state state; u64 cursorFrames; f32 gain;
f32 pan; f64 step; bool loop; u8 groupIndex; }`, `null_group_stats { group_id id; core::string_id name; f32 gain;
f32 effectiveGain; bool paused; bool effectivePaused; }`, `null_backend_stats { f64 virtualTimeSeconds;
u64 framesRendered; u32 renderCalls; u32 activeVoices; u32 underruns; u32 commandsApplied; listener_state listener;
std::array<null_voice_stats, MAX_VOICES> voices; std::vector<null_group_stats> groups; }`.
`null_backend::update(dt)` renders exactly `round(dt * sampleRate)` frames (carrying the fractional remainder)
through the registered callback in `MIX_BLOCK_FRAMES` blocks; the engine then writes the voice / group / listener
snapshot into `stats()` through a private friend hook, so `08_testing.md` 4.4's `audio_stats()` reads one struct.

### 4.4 `audio_engine.h`

```cpp
namespace conjure::audio {
    struct audio_settings {
        audio_backend_kind backend = audio_backend_kind::AUTO;
        u32 sampleRate = 48000;                       // Mix rate; 8000..192000
        u32 periodFrames = 480;
        std::string deviceName = {};
        f32 masterGain = 1.0f;
        f32 dopplerFactor = 0.0f;                     // 0 = off
        f32 speedOfSound = DEFAULT_SPEED_OF_SOUND;
        u32 maxRecoveryAttempts = 5;
        f32 recoveryBackoffSeconds = 1.0f;
        std::vector<group_desc> groups = {};          // Created at construction; parents resolved by name
        core::jobs::job_system *jobs = nullptr;       // nullptr: streaming decodes run inline in update()
        [[nodiscard]] core::result<void> validate() const;
    };

    class audio_engine {
        public:
            explicit audio_engine(audio_settings settings);      // Never fails; see last_backend_error()
            ~audio_engine();                                       // stop_all, backend stop, drain acks
            audio_engine(const audio_engine &) = delete;
            audio_engine &operator=(const audio_engine &) = delete;
        public:
            // Backend and device
            [[nodiscard]] audio_backend_kind backend_kind() const noexcept;
            [[nodiscard]] const core::error *last_backend_error() const noexcept;   // Why AUTO fell back
            [[nodiscard]] null_backend *null_backend_instance() noexcept;           // nullptr on miniaudio
            [[nodiscard]] audio_format output_format() const noexcept;              // Mixer callback format (A2)
            [[nodiscard]] core::result<std::vector<audio_device_info>> enumerate_devices() const;
            [[nodiscard]] core::result<void> select_device(std::string_view deviceName);   // Restarts backend
            // Frame
            void update(f32 deltaSeconds);                                          // 3.2
            void end_frame();                                                       // 3.4
            [[nodiscard]] std::span<const voice_id> finished_voices() const noexcept;
            // Voices
            [[nodiscard]] core::result<voice_id> play(
              const assets::asset_ref<audio_clip> &clip,
              const play_params &params = {}
            );
            [[nodiscard]] core::result<voice_id> play(
              std::shared_ptr<const pcm_buffer> pcm,
              const play_params &params = {}
            );                                                                      // Tests, tools: no asset manager
            void stop(voice_id voice, f32 fadeSeconds = DEFAULT_RAMP_SECONDS);
            void stop_all(f32 fadeSeconds = DEFAULT_RAMP_SECONDS);
            void pause(voice_id voice);
            void resume(voice_id voice);
            void release(voice_id voice);                                           // Retained voices only
            void set_volume(voice_id voice, f32 volume);
            void set_pitch(voice_id voice, f32 pitch);
            void set_pan(voice_id voice, f32 pan);
            void set_loop(voice_id voice, bool loop);
            void set_group(voice_id voice, group_id group);
            void set_position(voice_id voice, math::vec3 position, math::vec3 velocity = math::vec3(0.0f));
            void set_positions(std::span<const voice_transform> transforms);       // One call per frame
            void set_attenuation(voice_id voice, const attenuation_params &params);
            void apply_source(voice_id voice, const audio_source &source, group_id group);   // All fields at once
            void seek(voice_id voice, f64 seconds);
            [[nodiscard]] bool is_valid(voice_id voice) const noexcept;
            [[nodiscard]] bool is_playing(voice_id voice) const noexcept;          // PRIMING, PLAYING or PAUSED
            [[nodiscard]] std::optional<voice_info> voice(voice_id voice) const noexcept;
            [[nodiscard]] u32 active_voice_count() const noexcept;
            // Listener
            void set_listener(const listener_state &listener);
            [[nodiscard]] const listener_state &listener() const noexcept;
            // Groups
            [[nodiscard]] core::result<group_id> create_group(const group_desc &desc);
            [[nodiscard]] core::result<void> remove_group(group_id group);
            [[nodiscard]] group_id find_group(core::string_id name) const noexcept;   // Null name = MASTER
            void set_group_gain(group_id group, f32 gain);
            void set_group_paused(group_id group, bool paused);                    // Group and descendants (A17)
            [[nodiscard]] std::optional<group_info> group(group_id group) const noexcept;
            [[nodiscard]] std::span<const group_info> groups() const noexcept;
            void set_master_gain(f32 gain);
            void set_paused(bool paused);                                           // Whole mixer
            [[nodiscard]] audio_engine_stats stats() const noexcept;
    };
}
```

Same header: `voice_transform { voice_id voice; math::vec3 position; math::vec3 velocity; }` and `audio_engine_stats
{ u32 activeVoices, peakVoices, streamingVoices, voicesDropped; u64 commandsSent, commandsSpilled, eventsReceived,
renderCalls, framesRendered; u32 underruns, deviceRestarts; bool fellBackToNull; audio_format outputFormat;
device_state deviceState; }` (one declaration per line in code). Setters on an invalid or stale `voice_id` are no-ops
(TRACE log); `result`-returning functions report `INVALID_VOICE` or `INVALID_GROUP`. All members are main-thread only
except `is_valid`, `is_playing`, `voice()`, `stats()` and `output_format()`, which are lock-free reads safe from any
thread (mirrors are refreshed by `update()`).

## 5. Threading and ownership

| Thread | May do |
|---|---|
| Main | every `audio_engine` mutation (`play`, `stop`, setters, groups, listener, `update`, `end_frame`, `select_device`), slot allocation and release, `shared_ptr` acquire / release of PCM and encoded bytes, command ring **producer**, event ring **consumer**, all logging, backend start / stop / recovery, the `null_backend_stats` snapshot |
| Device thread (miniaudio data callback) | `mixer::render` only: command ring consumer, event ring producer, stream ring consumer, relaxed atomic stores of `mixer_voice.state` and cursor; **no allocation, locks, logging, asset access, group tree walk or decoding** |
| miniaudio notification callback | one `std::atomic<device_state>` store |
| Job workers | `decode_chunk` (stream ring producer, one job per slot at a time), importer `import()`, loader `load_cpu` (`decode_to_pcm`) |
| Any | `is_valid`, `is_playing`, `voice()`, `stats()`, `output_format()`, `pcm_buffer` reads, `compute_*` functions |

Memory ordering: ring `try_push` publishes with a `release` store of the tail, `try_pop` acquires it; the generation in
every command and event protects a reused slot from stale traffic (both sides drop a mismatch). Spilled commands keep
order: once the spill vector is non-empty every new command appends to it until `update()` drains it. `stream_ring` is
SPSC between one decode job and the callback; the `jobInFlight` acquire / release flag makes "single producer" true.
The backend is stopped (`~audio_engine`, `select_device`, recovery) only when the callback is not running
(`ma_device_stop` joins the device thread); the rings are then drained on the main thread.

Ownership: `engine::application` owns the `audio_engine` through `engine_services` (R22), constructs it at startup
step 11 and destroys it in reverse order, after scene unload released every component voice and before the
`asset_manager` (`06_engine_loop.md` 3.5). The engine owns its backend, mixer and preallocated rings; `pcm_buffer` is
shared between clips and voices (A6); clips are owned by `asset_slot`s; `voice_id` and `group_id` are non-owning. No
`std::thread` exists in this module: the device thread belongs to miniaudio (`ARCHITECTURE.md` 3) and decoding runs on
`core::jobs`.

## 6. Failure handling

| Situation | Behaviour |
|---|---|
| `audio_settings::validate()` fails (rate outside 8000..192000, period 0, unknown or cyclic group parent) | engine constructs on the `null` backend, `last_backend_error()` = `INVALID_SETTINGS`, ERROR log; the engine's `project.json` validation catches it earlier |
| Backend init or device start fails (`MINIAUDIO` or `AUTO`) | `null` backend substituted, WARN with miniaudio's result name, `fellBackToNull = true`; construction never fails (A1) |
| Requested `deviceName` not found | WARN, default device used |
| Device lost or stopped by the OS | `device_state::LOST`; voices frozen; recovery per 3.8; after `maxRecoveryAttempts` -> `null` backend, WARN |
| Clip not loaded (`get()` null) | `play` returns `CLIP_NOT_LOADED`; the engine's sync retries next frame or plays `get_or_fallback` (silent clip) |
| STREAMED clip and no stream slot free | `STREAM_SLOT_LIMIT`; `voicesDropped++` |
| 256 voices busy, no stealable candidate | `VOICE_LIMIT_REACHED`; `voicesDropped++`; TRACE |
| Command ring full | command spilled to the main-thread vector and flushed by `update()`; `commandsSpilled++`; never dropped |
| Event ring full | the callback drops the event and sets `eventsDropped`; `update()` reconciles from the per-voice atomics (FINISHED / STOPPED are readable there), so no voice leaks |
| Stale or invalid `voice_id` | setters no-op with TRACE; `result` functions return `INVALID_VOICE` |
| Group errors | `create_group`: `GROUP_LIMIT_REACHED`, `DUPLICATE_GROUP`, `GROUP_CYCLE`, `INVALID_GROUP`; `remove_group(MASTER_GROUP)`: `INVALID_GROUP` |
| Decode failure (import or load) | `DECODE_FAILED` naming the miniaudio result; importer failure keeps the previous blob (`05_assets.md` D14); loader failure -> FAILED slot, fallback silent clip |
| Unsupported encoding (`.ogg` without stb_vorbis, exotic WAV codec) | `UNSUPPORTED_FORMAT`, message names the codec |
| Blob header mismatch (magic, version, `dataBytes` vs payload size, channels not 1 or 2) | `INVALID_CLIP` |
| Stream underrun (decode job late) | silence for the block, UNDERRUN event, `underruns++`; WARN once per voice per second in Development |
| NaN or infinity in a setter | sanitised to the clamp range before leaving the API (A15); `CONJURE_ASSERT` in Debug |
| `update()` / `end_frame()` from a worker | `CONJURE_ASSERT(core::jobs::is_main_thread())` |
| Engine destroyed with live retained voices | voices stopped and released; INFO count in Development |

The callback itself cannot fail: every failure path above runs on the main thread or on a job.

## 7. Headless / null behaviour

`null_backend` is the default under `engine_config::headless_defaults()` (`audio.backend = NULL_DEVICE`, mapped by the
`audio_subsystem` to `audio_backend_kind::NULL_BACKEND`). It has **no device, no thread and no OS calls**; its
`update(dt)` advances a virtual clock and calls the **real mixer** for `dt * sampleRate` frames on the main thread, so
cursors, loops, FINISHED events, ramps, group gains and clamping behave exactly as on hardware and deterministically.
Decoding is identical on both backends. `null_backend_stats` (4.3) exposes per-voice state / cursor / gain / pan /
step / loop / group, per-group gain and effective gain, the listener, virtual time, frames rendered, underruns and
commands applied; `set_capture_frames(n)` keeps the last `n` rendered stereo frames so tests assert on sample values
(a 0.5-gain sine renders with amplitude 0.5; two voices sum; master pause renders zeros). With `jobs == nullptr`
streaming decodes run inline inside `update()`, so streamed clips need no job system. The engine test fixture
`headless_engine::audio_stats()` reads `audio_engine::null_backend_instance()->stats()` (`08_testing.md` 4.4).

## 8. Integration points

**Needs from `core` (wave 1).** `result<T>` / `error` / `CONJURE_DECLARE_ERROR_CATEGORY` / `make_error`
(`10_core.md` C3); `CONJURE_DECLARE_LOG_CATEGORY` / `CONJURE_LOG_*`; `CONJURE_ASSERT`; `slot_map` + `slot_key`;
`small_vector`; `string_id`; `math::vec3`, `nearly_equal`; `ticks_now`; `jobs::job_system::schedule` +
`is_main_thread`; `json::value` / `writer`; `reflect` macros `CONJURE_REFLECT_STRUCT`, `CONJURE_FIELD`,
`CONJURE_FIELD_HIDDEN`, `CONJURE_REFLECT_ENUM` and the `ASSET_REF` field kind (R25); `memory::allocation_counters()`.

**Needs from `assets` (wave 2, built before audio).** `asset_type_registry`, `importer_registry`, `asset_importer`,
`typed_asset_loader<T, Intermediate>`, `import_context` (`source_bytes`, `settings`, `source_path`),
`imported_asset` / `import_output`, `asset_payload` / `make_payload`, `asset_ref<T>` (`get()`, `version()`,
`state()`), `CONJURE_DECLARE_ASSET_TYPE`, `create_fallback` (`05_assets.md` 4.2-4.4, 8). The clip type has no flags
and no dependencies.

**Needs from the third-party build.** `conjure::thirdparty::miniaudio` compiled from `third_party/src/miniaudio_impl.c`
with `MA_NO_ENGINE`, `MA_NO_NODE_GRAPH`, `MA_NO_GENERATION` **plus `stb_vorbis`** (A8); the TU is `SYSTEM`-included so
warnings stay suppressed. Only `miniaudio_backend.cpp` and `decoder_impl.cpp` include `miniaudio.h`.

**Offers to `engine` (wave 4, `audio_subsystem`, R7).**
- Startup step 11: map `engine::audio_settings` (`enabled`, `backend`, `sampleRate`, `device`, `masterGain`,
  `dopplerFactor`, `speedOfSound`, `groups[] { name, parent, gain }`; `project.json` `"audio"` keys in snake_case,
  `06_engine_loop.md` 2.3/2.10, R52) to `audio::audio_settings` with `jobs = &context.jobs`; construct `audio_engine`;
  call `audio::register_asset_types(assets.types(), importers)`; register `audio_source` (field `handle` `TRANSIENT`)
  and `audio_listener` into the `component_registry` from their reflection descriptions; set `engine_context::audio`.
- `engine.audio_sync` (SYNC, `RUNS_WHILE_PAUSED`; reads `world_transform`, `audio_listener`; writes `audio_source`):
  1. listener: the first entity with `audio_listener { active }` in iteration order -> `set_listener({ position,
     forward, up, velocity from the previous frame's position, gain })`; none -> unchanged; several -> WARN once.
  2. per `audio_source` chunk: `handle.voice` invalid and (`playOnStart` on first sight or `playbackRequest == PLAY`)
     and `clip.get() != nullptr` -> `play(clip, params from fields, retain = true)`, store `voice` and `clipVersion`;
     `playbackRequest` `PLAY` on a playing voice -> `seek(0)` (restart), `STOP` -> `stop`, `PAUSE` / `RESUME` ->
     `pause` / `resume`; then `playbackRequest = NONE` and `isPlaying = is_playing(voice)` (R50);
     `is_valid(voice)` false (finished or stolen) -> clear `handle`, `isPlaying = false`; `clip.version() != handle.clipVersion`
     (hot reload) -> stop and replay; `changed<audio_source>` chunks -> `apply_source(voice, source, find_group(group))`.
  3. one `set_positions(span)` with every spatial voice's world position and velocity, then `update(deltaTime)`.
  Budget: < 0.1 ms at 256 sources, dominated by `update()` (A16).
- `engine.audio_finish_one_shots` (FRAME_END): `end_frame()`; for each id in `finished_voices()` whose component has
  `oneShot` -> `command_buffer.destroy_entity`, otherwise clear `handle.voice` and set `isPlaying = false`. Component
  voices are retained, so `world::observe(audio_source, REMOVED)` -> `release(voice)` (`01_ecs.md` 8).
- Voice ownership (code sweep F144; none of the releases above existed, so every component voice stayed in the
  table and a destroyed emitter played on). The subsystem records which voice each entity owns (`entity -> voice_id`)
  and releases it when it ends (`audio_finish_one_shots`), when a `PLAY` request replaces it (implemented as release
  and a new voice, not `seek(0)`) and in the `REMOVED` observer, which also covers a destroyed entity, a scene unload
  and `world::clear`. The record is the truth, not `handle`: the handle is plain component bytes that a duplicated
  entity or a restored snapshot carries along, so a component whose `handle.voice` is not the voice its entity owns
  is reset to a fresh component (its own voice, `playOnStart` applies) and never releases or steers the original's.
- Pause: the editor's PAUSED simulation mode calls `set_paused(true / false)` (R13); SYNC keeps running so inspector
  edits take effect when play resumes.

**Offers to `bindings` (wave 4, R26, R50).** The `audio_*` native entries are entity-keyed and listed in
`15_bindings.md` 2.3 (that document owns the table): `audio_play/stop/pause/resume(entity)` write
`audio_source::playbackRequest`, `audio_is_playing(entity)` reads `audio_source::isPlaying`,
`audio_play_one_shot(clip, positionOrNull, volume, pitch, groupHash)` calls `audio_engine::play(clipRef, play_params
{ retain = false, spatial = position != null, group = find_group(groupHash) })` (the engine owns that voice and
releases it in `end_frame()`), `audio_set_group_gain / audio_get_group_gain(groupHash)` use `find_group(string_id)`
+ `set_group_gain` / `group(id)->gain`, `audio_set_master_gain / audio_get_master_gain` use `set_master_gain` /
`group(MASTER_GROUP)->gain`, `audio_clip_duration` reads `audio_clip::duration_seconds()` from the resident payload.
`voice_id` never crosses the ABI; volume, pitch and loop are edited through the `AudioSourceData` mirror
(`15_bindings.md` 2.6), whose field list is 2.6 above in order.

**Offers to `editor` and `samples`.** Inspector via reflection (ranges above); `audio_engine_stats` and
`null_backend_stats` for the profiler; `enumerate_devices()` / `select_device()` for a settings dialog. `feature_test`
(`08_testing.md` 7.2): `MainCamera` carries `audio_listener`, `MusicLoop` is a looping spatial source in group `Music`,
`OneShotEmitter` a one-shot source, `PlayerController` plays `beep.wav` through `audio_play`; checks
`audio.music_playing`, `audio.one_shot_finished`, `audio.group_gain_applied`.

## 9. Implementation plan

1. `audio_types.h`, `audio_error.h`, `log_category.h`, `voice.h` (`voice_id`), `mix_groups.h` (ids),
   `spatialisation.h`. Tests: `test_audio_types`, `test_audio_error`, `test_spatialisation`.
2. `clip.h`, `decoder.h` + `decoder_impl.cpp` (miniaudio decoders; stb_vorbis added to the third-party TU). Tests:
   `test_clip`, `test_decoder` (WAV generated in-test).
3. `clip_asset.h`: blob header, `clip_import_settings`, importer, loader, fallback, `register_asset_types`. Tests:
   `test_clip_asset` with `assets::memory_asset_source` and `load_blocking`.
4. Private `spsc_ring.h`, `commands.h`, `stream_ring`, `mixer.{h,cpp}` (render, ramps, loops, pan, clamp, group
   table). Tests: `test_mixer` (private header test through `tests/audio/mixer_access.h`, allowed for the hot path).
5. `audio_backend.h`, `audio_device.h`, `null_backend.h`. Tests: `test_audio_backend`, `test_audio_device`,
   `test_null_backend`.
6. `audio_engine.h`: construction and fallback, voices, one-shot release, groups, listener, spatial pass, stealing,
   stats, `end_frame`. Tests: `test_audio_engine`, `test_source_listener`, `test_mix_groups`, `test_voice`.
7. Streaming: stream slots, decode jobs, priming, underrun accounting; cases in `test_audio_engine` with
   `jobs = nullptr` and with a 2-worker `job_system`.
8. `miniaudio_backend.{h,cpp}`: context, enumeration, device open at the requested format, notification callback,
   recovery, `select_device`. Tests: `test_miniaudio_backend` `[audio_device]` (skips without a device; never in CI).
9. `components.h` + reflection, `audio.h`, `engine/src/audio/README.md`, CMake (`conjure_add_module`,
   `conjure_add_test_module(audio)`, `conjure_header_check(audio)`), `tests/benchmarks/bench_audio.cpp`. Tests:
   `test_components`.

Steps 1-3 are independent of 4-6 and can proceed in parallel; 7 needs 4 and 6; 8 needs 5; 9 is last.

## 10. Tests (`tests/audio/`, target `conjure_audio_tests`, all headless)

One file per public header (R16) plus the topic files mandated by `08_testing.md` 5.2. Clip data for the mandatory
tests is **generated by the tests**: `tests/audio/wav_builder.h` writes a canonical 16-bit or 32-bit float PCM WAV
(RIFF + `fmt ` + `data`) from a sine, square or ramp, so no binary asset is required. `.ogg` / `.mp3` / `.flac` decode
tests use `tests/.downloaded/audio/tone.{ogg,mp3,flac}` behind `precondition::DOWNLOADED_ASSETS` (fetched by
`tools/download_test_assets.py`); this refines the `tests/assets` wording of `08_testing.md` 5.2 and
`ARCHITECTURE.md` 11.

| File | Cases |
|---|---|
| `test_audio_types.cpp` | constants; `audio_format` equality; `to_string` of every enum value; `voice_id` null / valid / `to_u64` round trip; `MASTER_GROUP` valid |
| `test_audio_error.cpp` | every `audio_error` has a name; `make_error(audio_error::X)` has category `"audio"`; `is()` works |
| `test_spatialisation.cpp` | `compute_attenuation`: every model is 1 at `minDistance`; LINEAR reaches 0 at `maxDistance`; INVERSE and EXPONENTIAL monotonic; all clamp beyond `maxDistance`; NONE = 1; rolloff 0 = 1. `compute_pan`: right = +1, left = -1, ahead = 0, behind = 0, listener rotated 90 deg. Doppler: approaching > 1, receding < 1, factor 0 = 1, clamp to [0.5, 2]. `compute_spatial` at distance 0 and at max distance (`08_testing.md` 5.2) |
| `test_clip.cpp` | `pcm_buffer` duration and frame count; `audio_clip` copy shares PCM (`use_count`); `is_streamed`; asset type id equals `make_asset_type_id("conjure.audio.clip")`; fallback clip is 0.1 s of mono 48 kHz zeros |
| `test_decoder.cpp` | WAV generated in-test (mono / stereo, 16-bit and f32, 22050 / 44100 / 48000 Hz, 1 and 48000 frames): `probe` reports format and length; `decode_to_pcm` reproduces the sine within 1e-4 (16-bit) / 1e-6 (f32); `forceMono` averages channels; `sampleRate` conversion scales the frame count; `normalize` peak = 1; truncated header -> `DECODE_FAILED`; empty span -> `INVALID_CLIP`; random bytes -> error, never an assert; `streaming_decoder` read in 1000-frame chunks equals the full decode, `seek` to the middle, read past the end returns 0; `[downloaded]` ogg / mp3 / flac decode to the expected length within 1 %; WAV decoding is allocation-free after `open()` |
| `test_clip_asset.cpp` | importer: `.wav` -> PCM blob with a correct header; `storage: streamed` keeps the original bytes byte-identical; `auto` switches at the threshold; `forceMono` / `sampleRate` / `normalize`; corrupt input -> `IMPORT_FAILED` with a message; unknown extension not claimed; `default_settings` round trips through JSON; two imports byte-identical. Loader: blob -> `audio_clip` via `memory_asset_source` + `load_blocking`; bad magic / version / size -> `INVALID_CLIP`; fallback served for a FAILED slot; reload publishes a new `pcm` pointer while an old `shared_ptr` stays valid |
| `test_voice.cpp` | `play_params` defaults; clamping of every field (volume 5 -> 2, pitch 0 -> 0.25, NaN -> default, `maxDistance < minDistance` fixed up); `voice_info` defaults |
| `test_mix_groups.cpp` | MASTER exists; `create_group` by name with parent; `find_group`; effective gain is the product up the chain; a parent gain change updates children in `null_backend_stats::groups`; a voice at volume 0.5 in `Music` (0.5) under master (0.5) renders at 0.125 (capture); `GROUP_LIMIT_REACHED` at 33; `DUPLICATE_GROUP`; `GROUP_CYCLE`; `remove_group` moves voices and children; `remove_group(MASTER)` refused; `set_group_paused` freezes cursors; hierarchical pause (A17, `regression - code sweep F319`): a paused group freezes its descendants' voices, `MASTER_GROUP` freezes every group without setting `is_paused()`, a group created under a paused group starts frozen, `remove_group` lifts an inherited pause from the whole reparented subtree and keeps a remaining ancestor's or the child's own; own against effective flag, through `group()` and `groups()`; one `SET_GROUP_PAUSED` per changed group and none for an unchanged flag or a stale id (also once its slot is reused); a removed paused group's slot is reused unpaused; the deepest chain; a voice moved between groups; `stop` under a paused ancestor; both flags in `null_backend_stats::groups` |
| `test_components.cpp` | `audio_source` / `audio_listener` trivially copyable; `sizeof(audio_source) == 88`; reflection lists the fields in order with offsets equal to `offsetof`; `playbackRequest`, `isPlaying`, `handle` hidden and absent from JSON; `clip` kind `ASSET_REF`; ranges present; JSON round trip of a source with a uuid clip; `attenuation_model` and `audio_playback_request` reflected |
| `test_audio_device.cpp` | `backend_desc` defaults; `audio_device_info` of the null backend (`"Null Output"`, default) |
| `test_audio_backend.cpp` | `create_backend(NULL_BACKEND)` succeeds; `create_backend(AUTO)` never fails; `start` with a null callback -> `INVALID_SETTINGS`; `state()` STOPPED -> RUNNING -> STOPPED |
| `test_null_backend.cpp` | `update(1/60)` renders 800 frames at 48 kHz with the fractional remainder carried (60 updates = exactly 48000 frames); callback invoked in `MIX_BLOCK_FRAMES` blocks; `renderCalls`; capture of a 1 kHz sine at gain 0.5 peaks at 0.5 +/- 1e-3; `virtual_time()` accumulates; `stats()` voice entry reflects the cursor advance; rendering 256 voices is allocation-free (`allocation_scope`) |
| `test_audio_engine.cpp` | construction with `NULL_BACKEND`; `AUTO` without a device falls back to null with `fellBackToNull` and no error; invalid settings -> null + `last_backend_error`; `play` records clip, gain and state in `null_backend_stats` (`08_testing.md` 5.2); `is_playing` true immediately; a 0.5 s clip finishes after updates totalling 0.5 s plus one block and appears in `finished_voices()` exactly once; the one-shot slot is FREE after `end_frame` and its `voice_id` invalid; a retained voice stays FINISHED until `release`; `loop` wraps (LOOPED counted, cursor < clipFrames); `stop` ramps over 5 ms (no capture sample jump > 0.1); `pause` / `resume` freeze and continue the cursor; `seek`; `set_volume`; `set_pitch` (pitch 2 finishes in half the time); `set_pan` (left-only capture); a 3D voice at `maxDistance` renders at the attenuation floor and at distance 0 at full gain; `set_positions` equals individual `set_position`; listener gain scales everything; `set_paused(true)` renders zeros and holds cursors; 256 simultaneous voices all PLAYING; the 257th steals the lowest priority (victim STOPPED event) or fails `VOICE_LIMIT_REACHED` when all are high-priority loops; stale `voice_id` setters are no-ops; 10,000 `set_volume` calls in one frame are all applied (final value correct, `commandsSpilled > 0`); a streamed clip with `jobs == nullptr` plays to completion and matches the PCM render sample-for-sample; a streamed clip with a 2-worker `job_system` completes; `STREAM_SLOT_LIMIT` at 33 streamed voices; hot reload: a voice keeps playing the old `pcm_buffer` after the clip is republished; destruction with 100 live voices releases everything (`memory_guard` clean) |
| `test_source_listener.cpp` | drives the engine the way `engine.audio_sync` will: builds `audio_source` structs plus positions, calls `play` / `apply_source` / `set_positions` / `set_listener` / `update` / `end_frame` for 120 frames; asserts voice count, finished one-shots, the listener position in stats and attenuation changing as the listener moves |
| `test_mixer.cpp` | private: ramp step bounded; linear interpolation at fractional cursors matches a reference; stereo downmix for spatial voices; output clamp; a generation mismatch drops a stale command; event ring overflow sets `eventsDropped` without losing the FINISHED state |
| `test_miniaudio_backend.cpp` `[audio_device]` | local only: enumerate >= 1 device; start / stop; `output_format().sampleRate == 48000`; play a 100 ms tone (assertion: no error and `renderCalls > 0`) |

Benchmarks (`tests/benchmarks/bench_audio.cpp`, Development, outside ctest): `audio.update_256_voices` (< 0.1 ms
main thread including `end_frame`), `audio.render_256_voices_480_frames` (< 0.5 ms, allocation-free),
`audio.decode_wav_10s` (< 20 ms). Integration (wave 4, `tests/integration/test_audio_scene.cpp`): `MusicLoop`,
`OneShotEmitter` and a listener through `headless_engine` for 120 frames; asserts `audio_stats()` shows one looping
voice and the finished one-shot entity destroyed. Definition of done (`08_testing.md` 7.4 plus): every file above
green on the three CI platforms under ASan / UBSan; `conjure_header_check(audio)` passes; `bench_audio` thresholds met
on the reference desktop; the `miniaudio` backend plays the feature-test tone locally on Windows, Linux and macOS.

## 11. Out of scope / future

- HRTF / binaural output, surround (5.1 / 7.1) mixing, reverb zones, low-pass occlusion, per-voice DSP effects and
  group filters (a build without `MA_NO_NODE_GRAPH` would be the starting point); sinc resampling (linear only in v1).
- Streaming from disk (today STREAMED keeps the compressed file in memory); audio packs with external payloads.
- Capture / microphone input; loopback recording to file; audio-reactive analysis (FFT); exclusive mode and latency
  tuning beyond `periodFrames`; a device hot-swap UI.
- Editor clip preview (`audio.preview` command), waveform thumbnails in the asset browser, a mixer panel with meters.
- Multiple listeners (split-screen) and a listener-relative "UI bus" that bypasses the master pause.
- Cross-platform decode hashes for `.mp3` (decoder output may differ by a few LSBs between builds); only WAV decode is
  hash-stable and used for golden comparisons.
