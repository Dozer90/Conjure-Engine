# Playing sounds

📘 **Public API wiki** · 🛠 Handbook: [Audio module](../../handbook/modules/audio.md)

`audio_engine` (`conjure/audio/audio_engine.h`) is the one object that plays sounds: it owns the voices, the mix
groups, the listener and the backend. This page covers it together with the small value types it takes:
`voice_id` and `play_params` (`conjure/audio/voice.h`), `group_id` and `group_desc` (`conjure/audio/mix_groups.h`),
`listener_state` and the attenuation maths (`conjure/audio/spatialisation.h`), the shared enumerations and
constants (`conjure/audio/audio_types.h`) and device descriptions (`conjure/audio/audio_device.h`). Everything is
in `namespace conjure::audio`.

## At a glance

| You want to... | Use |
|---|---|
| Play a sound once and forget it | `engine.play(clip)`; the voice frees itself when the clip ends |
| Play music you control later | `play_params p; p.retain = true; p.loop = true;` then `stop()` / `release()` |
| Change volume, pitch, pan, loop | `set_volume()`, `set_pitch()`, `set_pan()`, `set_loop()` |
| Pause and continue | `pause()`, `resume()`; `set_paused(true)` for the whole mixer |
| Jump to a position | `seek(voice, seconds)`, or `play_params::startSeconds` |
| Know whether it is still playing | `is_playing(voice)`; `voice(voice)` for the full `voice_info` |
| Volume sliders for Music / Effects / Voice | `create_group()`, `set_group_gain()`, `find_group()` |
| 3D sound | `play_params::spatial = true`, `set_position()` each frame, `set_listener()` |
| Apply everything from a component at once | `apply_source(voice, source, group)` or `apply_params(voice, params)` |
| Pick an output device | `enumerate_devices()`, `select_device(name)`, `audio_settings::deviceName` |

## Quick example

```cpp
#include "conjure/audio/audio_engine.h"

using namespace conjure;
using namespace conjure::audio;

class music_player {
    private:
        audio_engine &mEngine;
        voice_id mVoice = {};

    public:
        explicit music_player(audio_engine &engine) : mEngine(engine) {}

        void start(const assets::asset_ref<audio_clip> &track) {
            play_params params = {};
            params.loop = true;
            params.retain = true;                            // we keep the handle, so we release it
            params.volume = 0.6f;
            params.group = mEngine.find_group(core::string_id("Music"));
            const core::result<voice_id> voice = mEngine.play(track, params);
            mVoice = voice.has_value() ? voice.value() : voice_id{};
        }

        void fade_out() {
            mEngine.stop(mVoice, 1.5f);                      // 1.5 s fade, then the voice reports STOPPED
            mEngine.release(mVoice);                         // erased once the mixer acknowledged
            mVoice = {};
        }
};
```

## Setting up the engine

Construct one `audio_engine` from an `audio_settings`; the engine's `application` does this for you at startup,
so a game only needs its own when it runs the module standalone or in a test.

| `audio_settings` field | Default | Meaning |
|---|---|---|
| `backend` | `AUTO` | `MINIAUDIO` when compiled in, falling back to `NULL_BACKEND` when no device opens |
| `sampleRate` | `48000` | mix rate, `MIN_SAMPLE_RATE` (8000) to `MAX_SAMPLE_RATE` (192000) |
| `periodFrames` | `480` | callback period (10 ms at 48 kHz); the device may round it |
| `deviceName` | empty | empty = default device; an unknown name warns and uses the default |
| `masterGain` | `1` | initial `MASTER_GROUP` gain |
| `dopplerFactor`, `speedOfSound` | `0` (off), `343` | Doppler strength `[0, 10]` and speed in units per second |
| `maxRecoveryAttempts`, `recoveryBackoffSeconds` | `5`, `1` | what to do when the OS takes the device away |
| `groups` | none | `group_desc` list created at construction; parents given by `parentName` |
| `jobs` | `nullptr` | a `core::jobs::job_system` for streaming decodes; `nullptr` decodes inline in `update()` |

`validate()` tells you in advance what construction would reject (`INVALID_SETTINGS`).

> [!IMPORTANT]
> Construction **never fails**. If the settings are invalid or no device can be opened the engine runs on the
> null backend and `last_backend_error()` says why; `stats().fellBackToNull` is true. A game therefore always
> runs, silently at worst. Call `update(deltaSeconds)` once per frame and `end_frame()` at the end of it, from the
> main thread, or nothing moves.

## Playing a clip

`play()` has three overloads and returns a `result<voice_id>`:

| Overload | Use when |
|---|---|
| `play(const assets::asset_ref<audio_clip> &, const play_params & = {})` | you hold an asset reference (the normal case); `CLIP_NOT_LOADED` while `get()` is null |
| `play(const audio_clip &, const play_params & = {}, u32 clipVersion = 0)` | you have the payload itself |
| `play(std::shared_ptr<const pcm_buffer>, const play_params & = {})` | tests and tools without an asset manager |

Other errors: `INVALID_CLIP` (empty or inconsistent clip), `INVALID_GROUP` (stale `params.group`),
`STREAM_SLOT_LIMIT` (33rd streamed voice), `VOICE_LIMIT_REACHED` (256 voices busy, nothing stealable),
`DECODE_FAILED` (a streamed clip that does not open).

### `play_params`

| Field | Default | Range | Meaning |
|---|---|---|---|
| `volume` | `1` | `[0, 2]` | linear gain |
| `pitch` | `1` | `[0.25, 4]` | playback speed; also shifts pitch |
| `pan` | `0` | `[-1, 1]` | left to right; ignored when `spatial` |
| `loop` | `false` | | restart at the end |
| `spatial` | `false` | | 3D: gain and pan come from `set_position()`; an `audio_source` component defaults to `true` |
| `retain` | `false` | | `false` = one-shot (auto-released), `true` = you call `release()` |
| `startPaused` | `false` | | begin in `PAUSED`; `resume()` starts it |
| `priority` | `128` | `0..255` | voice stealing: lower is stolen first |
| `group` | `MASTER_GROUP` | | the mix group |
| `startSeconds` | `0` | `>= 0` | seek before the first sample |
| `attenuation`, `minDistance`, `maxDistance`, `rolloff` | `INVERSE`, `1`, `100`, `1` | see below | the distance curve of a spatial voice |

Values outside the ranges are clamped at the call and NaN becomes the default; `clamp_play_params()`,
`clamp_volume()`, `clamp_pitch()` and `clamp_pan()` expose the same rules. `volume` (per voice) and gain (groups,
master, listener) are the same linear multiplier in `[0, 2]`; the final level is volume x group gain x
attenuation x listener gain.

## One-shots and retained voices

A **one-shot** (`retain = false`) is fire-and-forget: when the clip ends (or you `stop()` it) the engine erases
the voice in the next `end_frame()` and the `voice_id` goes stale. A **retained** voice (`retain = true`) stays
yours after it finishes, in state `FINISHED` or `STOPPED`, until you call `release()`. Either way the id appears
exactly once in `finished_voices()` during the frame it ended.

```cpp
#include "conjure/audio/audio_engine.h"

using namespace conjure;
using namespace conjure::audio;

// Fire-and-forget: no handle to keep.
void play_click(audio_engine &engine, const assets::asset_ref<audio_clip> &click) {
    (void)engine.play(click);                                // one-shot by default
}

// Retained: poll it, then give it back.
bool poll_intro(audio_engine &engine, voice_id intro) {
    if (engine.is_playing(intro)) {
        return false;
    }
    engine.release(intro);                                   // FINISHED voices are not freed for you
    return true;
}
```

> [!TIP]
> Default to one-shots. Retain a voice only when you will call something on it later (loop it, fade it, follow
> it with `set_position()`). Every retained voice you forget to `release()` holds a mixer slot for ever.

> [!WARNING]
> Keep the `voice_id`, never a pointer. Every engine call rejects a stale id: setters do nothing (TRACE log),
> `is_valid()` and `is_playing()` return false, `voice()` returns `nullopt`. A finished one-shot's id is stale
> after the `end_frame()` that reported it, so handle `finished_voices()` before the next frame.

## Controlling a voice

| Call | Effect |
|---|---|
| `stop(voice, fadeSeconds = 0.005)` | fade to silence, then `STOPPED`; `stop_all(fade)` for every voice. A paused voice, or one whose group is frozen (paused itself or through an ancestor group) or while the master is paused, ends at once without a fade |
| `pause(voice)` / `resume(voice)` | hold and continue the cursor; a paused voice still counts as `is_playing()`. `resume()` on a voice still `PRIMING` cancels `startPaused`: it starts playing |
| `seek(voice, seconds)` | jump (clamped to the clip length); a streamed voice resumes a few frames later, and a seek its decoder cannot reach continues from where the decoder stands (`voice()` reports that frame, WARN) |
| `set_volume`, `set_pitch`, `set_pan`, `set_loop`, `set_priority`, `set_group` | one field each; volume ramps over 5 ms so it never clicks |
| `set_spatial(voice, bool)` | switch between flat and 3D; a stereo clip is downmixed to mono while 3D |
| `apply_params(voice, params)` | volume, pitch, pan, loop, spatial, priority, group and attenuation at once |
| `apply_source(voice, source, group)` | the same from an `audio_source` component |
| `is_valid`, `is_playing`, `voice`, `active_voice_count` | queries; the first three are safe from any thread |

`voice(id)` returns a `voice_info`: `state`, `cursorFrames`, `clipFrames`, `volume`, `pitch`, `effectiveGain`
(volume x group gain x attenuation x listener gain as last sent), `group`, `loop`, `spatial`, `retain`,
`streamed`, `clipVersion`. States (`voice_state`): `PRIMING` (waiting for its first streamed chunk or for a
stolen slot), `PLAYING`, `PAUSED`, `STOPPING`, `FINISHED`, `STOPPED`; `is_active_state()` names the first three.

> [!WARNING]
> Setters take effect at the **next `update()`** for spatial values and at the next 256-frame mixer block for
> the rest, so reading `voice()` straight after a setter shows the old mixer-side state. Everything except the
> thread-safe queries must be called from the main thread; every call that changes something asserts it in
> Debug and Development builds.

## Mix groups

Groups form a tree under `MASTER_GROUP` (at most 32 groups, 8 deep). A group's **effective gain** is its own gain
multiplied up the chain, and every voice is multiplied by the effective gain of its group, so one
`set_group_gain()` moves a whole category. Groups are named with a `string_id`; `find_group(name)` looks one up
(the null name is `MASTER_GROUP`, an unknown name gives an id whose `is_valid()` is false).

Pause follows the tree the same way: `set_group_paused(id, true)` freezes the voices of that group and of every
group below it, and a group plays again only once neither it nor any ancestor is paused. `group_info::paused` is
the group's own flag, `group_info::effectivePaused` says whether its voices are frozen right now.

```cpp
#include "conjure/audio/audio_engine.h"

using namespace conjure;
using namespace conjure::audio;

core::result<void> create_mix(audio_engine &engine) {
    group_desc music = {};
    music.name = core::string_id("Music");
    music.gain = 0.8f;                                       // parent defaults to MASTER_GROUP
    CONJURE_TRY(engine.create_group(music));

    group_desc effects = {};
    effects.name = core::string_id("Effects");
    CONJURE_TRY(engine.create_group(effects));

    group_desc footsteps = {};
    footsteps.name = core::string_id("Footsteps");
    footsteps.parentName = core::string_id("Effects");      // parent by name, as settings do
    footsteps.gain = 0.5f;
    CONJURE_TRY(engine.create_group(footsteps));
    return {};
}

void apply_options(audio_engine &engine, f32 musicSlider, f32 effectsSlider, bool muteAll) {
    engine.set_group_gain(engine.find_group(core::string_id("Music")), musicSlider);
    engine.set_group_gain(engine.find_group(core::string_id("Effects")), effectsSlider);
    engine.set_paused(muteAll);                              // holds every cursor; resumes where it stopped
}
```

| Call | Effect |
|---|---|
| `create_group(desc)` | `GROUP_LIMIT_REACHED`, `DUPLICATE_GROUP` (taken or null name), `INVALID_GROUP` (unknown parent), `GROUP_CYCLE` (too deep); a group created under a paused group starts frozen |
| `remove_group(id)` | children and voices move to the parent and follow its pause from then on (a pause the removed group held goes with it); `INVALID_GROUP` for `MASTER_GROUP` |
| `set_group_gain(id, gain)` / `set_master_gain(gain)` | gain in `[0, 2]`; descendants update too |
| `set_group_paused(id, bool)` | freeze a group and every group below it; cursors hold, output is silence |
| `set_paused(bool)` / `is_paused()` | freeze the whole mixer; a flag of its own, not reported by `effectivePaused` |
| `group(id)` / `groups()` | `group_info { id, name, parent, gain, effectiveGain, paused, effectivePaused }` |

> [!NOTE]
> `set_group_paused(MASTER_GROUP, true)` freezes every voice, like `set_paused(true)`, but the two flags are
> independent: each must be released by the call that set it, and `is_paused()` reports only `set_paused()`.

> [!WARNING]
> A group pause reaches the mixer group by group, so the groups of one subtree may freeze or resume at different
> times: usually at most one 256-frame mixer block apart, and with no fixed upper limit (the call can be
> interrupted between two groups, and a frame that sends more commands than the mixer takes at once delivers the
> rest an update later).
> Voices that must stay sample-synchronised, such as the stems of one piece of music, belong in the **same group**,
> or are held with `set_paused()`, which freezes the whole mixer in one step.

> [!TIP]
> Declare the groups in `audio_settings::groups` (the project file does this) rather than creating them at run
> time: they exist before the first `play()`, parents are named, and `find_group()` is all the game code needs.

## 3D sound and the listener

A spatial voice (`play_params::spatial = true`) gets its gain and pan from its position relative to the
listener. Set the listener once per frame with `set_listener(listener_state)` (`position`, `forward`, `up`,
`velocity`, `gain`; defaults are the origin looking down -Z) and each spatial voice's position with
`set_position(voice, position, velocity)`, or all of them in one `set_positions(span<const voice_transform>)`.
Until its first `set_position()` a spatial voice sits at the listener and plays at full gain.

Distance attenuation uses `d = clamp(distance, minDistance, maxDistance)`:

| `attenuation_model` | Gain |
|---|---|
| `INVERSE` (default) | `minDistance / (minDistance + rolloff * (d - minDistance))`; natural falloff, never reaches zero |
| `LINEAR` | `1 - rolloff * (d - minDistance) / (maxDistance - minDistance)`, clamped to `[0, 1]`; reaches zero at `maxDistance` |
| `EXPONENTIAL` | `(d / minDistance) ^ -rolloff` |
| `NONE` | `1`; pan only |

Pan is the listener-right component of the direction to the source (ahead or behind is centre), rendered with
constant power. With `dopplerFactor > 0` the pitch ratio `((c - v_listener . dir) / (c - v_source . dir)) ^
dopplerFactor` is applied, clamped to `[0.5, 2]`. The pure functions `compute_attenuation()`, `compute_pan()`,
`compute_doppler_ratio()` and `compute_spatial()` are public, so you can preview a curve in a tool.

```cpp
#include "conjure/audio/audio_engine.h"

#include <vector>

using namespace conjure;
using namespace conjure::audio;

struct emitter {
    voice_id voice = {};
    math::vec3 position = math::vec3(0.0f);
    math::vec3 velocity = math::vec3(0.0f);
};

void sync_positions(audio_engine &engine, const std::vector<emitter> &emitters, math::vec3 cameraPosition) {
    listener_state listener = {};
    listener.position = cameraPosition;                     // forward -Z and up +Y by default
    engine.set_listener(listener);

    std::vector<voice_transform> transforms = {};
    for (const emitter &item : emitters) {
        if (engine.is_valid(item.voice)) {
            voice_transform transform = {};
            transform.voice = item.voice;
            transform.position = item.position;
            transform.velocity = item.velocity;
            transforms.push_back(transform);
        }
    }
    engine.set_positions(transforms);                       // one call per frame, before update()
}
```

> [!TIP]
> Import 3D effects with `forceMono` (see [Clips and decoding](clips-and-decoding.md#import-settings)). A
> stereo clip played spatially is downmixed to mono every frame anyway, and mono halves the memory.

> [!WARNING]
> Positions and the listener are consumed by `update()`: set them **before** it in the frame, and call
> `set_positions()` once with every spatial voice rather than `set_position()` hundreds of times. Doppler is off
> by default (`dopplerFactor = 0`); with it on, a velocity you forget to update keeps shifting the pitch.

## Formats, states and constants

`conjure/audio/audio_types.h` holds what every other header shares: `audio_format { sampleRate, channels,
format }`, the enumerations `sample_format` (`F32` only), `clip_storage` (`PCM`, `STREAMED`), `clip_encoding`
(`WAV`, `OGG_VORBIS`, `MP3`, `FLAC`), `audio_backend_kind`, `device_state` (`STOPPED`, `STARTING`, `RUNNING`,
`LOST`), `voice_state`, `attenuation_model`, each with `to_string()` and a `std::formatter`, plus the limits:
`MAX_VOICES` 256, `MAX_GROUPS` 32, `MAX_GROUP_DEPTH` 8, `MAX_STREAMING_VOICES` 32, `MIX_CHANNELS` 2,
`MIX_BLOCK_FRAMES` 256, `DEFAULT_RAMP_SECONDS` 0.005, `DEFAULT_SPEED_OF_SOUND` 343.

## Devices

`enumerate_devices()` lists playback devices as `audio_device_info { name, isDefault, sampleRates, minChannels,
maxChannels }` (`conjure/audio/audio_device.h`); `select_device(name)` restarts the backend on that device,
freezing voices for the restart and continuing them afterwards (the previous device is kept on failure). An
unknown non-empty name falls back to the default device with a WARN and counts as success. If neither the new
nor the previous device starts, the engine continues on the null backend with `last_backend_error()` set and
`stats().fellBackToNull` true, and `select_device()` returns the new device's error. The null backend reports
one device, `NULL_DEVICE_NAME` (`"Null Output"`). `output_format()` is the mixer's format;
`backend_kind()` the backend in use; `stats()` an `audio_engine_stats` with `activeVoices`, `peakVoices`,
`streamingVoices`, `voicesDropped`, `underruns`, `loops`, `deviceRestarts`, `fellBackToNull`, `deviceState` and
ring counters, readable from any thread.

> [!NOTE]
> When the OS takes the device away (headphones unplugged), `stats().deviceState` becomes `LOST`, voices
> freeze, and the engine retries every `recoveryBackoffSeconds`. After `maxRecoveryAttempts` failures it swaps
> in the null backend so voices still finish and `is_playing()` stays truthful. Gameplay never hangs on audio.

## API summary

| Signature (short) | Purpose |
|---|---|
| `explicit audio_engine(audio_settings)` | never fails; `last_backend_error()` says if it fell back |
| `void update(f32 dt)` / `void end_frame()` / `span<const voice_id> finished_voices()` | the per-frame calls |
| `result<voice_id> play(asset_ref<audio_clip>, play_params = {})` and the `audio_clip` / `pcm_buffer` overloads | start a voice |
| `void stop(voice_id, f32 fade = 0.005)` / `stop_all(f32)` / `pause(voice_id)` / `resume(voice_id)` / `release(voice_id)` | lifecycle |
| `void set_volume / set_pitch / set_pan (voice_id, f32)`, `set_loop / set_spatial (voice_id, bool)`, `set_priority(voice_id, u8)`, `set_group(voice_id, group_id)`, `seek(voice_id, f64)` | per-voice setters |
| `void set_position(voice_id, vec3, vec3 velocity = 0)` / `set_positions(span<const voice_transform>)` / `set_attenuation(voice_id, attenuation_params)` | 3D |
| `void apply_params(voice_id, play_params)` / `apply_source(voice_id, audio_source, group_id)` | everything at once |
| `bool is_valid(voice_id)` / `is_playing(voice_id)`, `optional<voice_info> voice(voice_id)`, `u32 active_voice_count()` | queries |
| `void set_listener(listener_state)` / `const listener_state &listener()` | the ear |
| `result<group_id> create_group(group_desc)` / `result<void> remove_group(group_id)` / `group_id find_group(string_id)` | groups |
| `void set_group_gain(group_id, f32)` / `set_group_paused(group_id, bool)` / `set_master_gain(f32)` / `set_paused(bool)` / `bool is_paused()` | group and master control |
| `optional<group_info> group(group_id)` / `span<const group_info> groups()` | group queries |
| `result<vector<audio_device_info>> enumerate_devices()` / `result<void> select_device(string_view)` / `audio_format output_format()` / `audio_engine_stats stats()` | backend and device |
| `struct voice_id { slot_key key; }` with `is_valid()`, `to_u64()`, `from_u64()`, `null()` | 8-byte voice handle |
| `struct group_id { u8 index, generation; }`, `MASTER_GROUP`, `struct group_desc { name, parent, parentName, gain }` | group handle and description |
| `f32 compute_attenuation(attenuation_params, f32 d)` / `compute_pan(listener_state, vec3)` / `compute_doppler_ratio(...)` / `spatial_result compute_spatial(...)` | the spatial maths, callable anywhere |

## See also

- 📘 API: [Audio](index.md) for the components and how the engine syncs them
- 📘 API: [Clips and decoding](clips-and-decoding.md) for `audio_clip`, import settings and the null backend
- 📘 API: [Strings and identifiers](../core/strings-and-identifiers.md) for `string_id` group names
- 📘 API: [Math](../core/math.md) for `math::vec3`
- 📘 API: [Jobs and threading](../core/jobs.md) for the `job_system` streaming decodes run on
- 🛠 Handbook: [Audio module](../../handbook/modules/audio.md)
