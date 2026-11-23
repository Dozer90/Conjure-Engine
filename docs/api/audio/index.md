# Audio
<!-- doc-template:wiki-index -->
[API](../index.md)

<!-- doc-content:purpose:start -->
The audio module plays **clips** through **voices**, mixes voices into **groups**, places 3D voices around one
**listener** and talks to the sound card (or to nothing at all, in tests and on servers). Everything lives in
`namespace conjure::audio`. Include `conjure/audio/audio.h` to get all of it, or the single header named on each
page below.

| Page | Covers | Headers |
|---|---|---|
| this page | the pieces and how they fit, the `audio_source` / `audio_listener` components and how the engine syncs them | `conjure/audio/components.h` |
| [Playing sounds](playing-sounds.md) | `audio_engine`, `play_params`, one-shots and retained voices, mix groups, 3D, devices | `audio_engine.h`, `voice.h`, `mix_groups.h`, `spatialisation.h`, `audio_types.h`, `audio_device.h` |
| [Clips and decoding](clips-and-decoding.md) | `audio_clip`, the `conjure.audio.clip` asset type, import settings, WAV / OGG / MP3 / FLAC decoding, backends and the null backend for tests | `clip.h`, `clip_asset.h`, `decoder.h`, `audio_backend.h`, `null_backend.h` |
<!-- doc-content:purpose:end -->

<!-- doc-content:body:start -->
## At a glance

| You want to... | Use |
|---|---|
| Put a sound on an entity | an `audio_source` component with a `clip` (and `audio_listener` on the camera) |
| Fire a sound effect and forget it | `audio_engine::play(clip, params)` with the default `retain = false` |
| Keep control of a sound (music, an engine hum) | `play_params::retain = true`, then `stop()`, `pause()`, `release()` |
| Make it 3D | `play_params::spatial = true` and `set_position()` every frame |
| A volume slider for music, effects, voice | mix groups: `create_group()`, `set_group_gain()` |
| Run without a sound card (tests, CI, servers) | `audio_settings::backend = audio_backend_kind::NULL_BACKEND` |
| Decode a file yourself | `decode_to_pcm()` or `streaming_decoder` |

## Quick example

```cpp
#include "conjure/audio/audio_engine.h"

using namespace conjure;
using namespace conjure::audio;

// A one-shot effect at a world position; the engine frees the voice when the clip ends.
void play_explosion(audio_engine &engine, const assets::asset_ref<audio_clip> &clip, math::vec3 where) {
    play_params params = {};
    params.volume = 0.8f;
    params.spatial = true;                                  // attenuate and pan from the position
    params.group = engine.find_group(core::string_id("Effects"));
    const core::result<voice_id> voice = engine.play(clip, params);
    if (!voice.has_value()) {
        return;                                             // CLIP_NOT_LOADED while the asset is still loading
    }
    engine.set_position(voice.value(), where);
}

// Once per frame, in this order.
void audio_frame(audio_engine &engine, const listener_state &listener, f32 deltaSeconds) {
    engine.set_listener(listener);
    engine.update(deltaSeconds);                            // spatial pass, streaming, device recovery
    engine.end_frame();                                     // frees finished one-shots
    for (const voice_id finished : engine.finished_voices()) {
        (void)finished;                                     // forget any handle you kept for it
    }
}
```

## The pieces

| Piece | What it is | Page |
|---|---|---|
| Clip | the samples of one sound, loaded as the `conjure.audio.clip` asset; `PCM` (decoded at import) or `STREAMED` (decoded while playing) | [Clips and decoding](clips-and-decoding.md) |
| Voice | one playing instance of a clip, addressed by a `voice_id` handle; up to 256 at once | [Playing sounds](playing-sounds.md) |
| Group | a node of the gain tree under `MASTER_GROUP`; a voice belongs to exactly one | [Playing sounds](playing-sounds.md#mix-groups) |
| Listener | the ear: position, orientation, velocity and gain; there is one | [Playing sounds](playing-sounds.md#3d-sound-and-the-listener) |
| Backend | the device driver: `miniaudio` for real hardware, `null` for headless runs | [Clips and decoding](clips-and-decoding.md#backends) |
| Engine | `audio_engine`, the one object that owns all of the above | [Playing sounds](playing-sounds.md) |

> [!NOTE]
> The mixer renders interleaved `f32` stereo at `audio_settings::sampleRate` (48 kHz by default) and the backend
> converts to whatever the device wants. Clips keep their own sample rate; the engine resamples while playing, so a
> 22 kHz clip costs no conversion at load.

## Components: `audio_source` and `audio_listener`

`conjure/audio/components.h` declares the two components as plain reflected structs. They are data only: the
audio module never touches entities. The engine's `audio_subsystem` registers them into the ECS and
runs two systems that map them onto voices every frame. Until that lands you drive `audio_engine` directly; the
mapping below is what the systems will do, and `play_params_from()` already turns a source into `play_params`.

### `audio_source`

| Field | Default | Meaning |
|---|---|---|
| `clip` | unbound | `assets::asset_ref<audio_clip>`; serialised as the asset uuid |
| `volume` | `1` | `[0, 2]` |
| `pitch` | `1` | `[0.25, 4]`; `2` plays twice as fast and an octave higher |
| `loop` | `false` | restart at the end |
| `spatial` | `true` | 3D: attenuate and pan from the entity's world position; `play_params::spatial` defaults to `false` instead |
| `playOnStart` | `true` | start the first time the sync sees the entity |
| `oneShot` | `false` | destroy the entity when the voice finishes |
| `group` | null (`MASTER_GROUP`) | `string_id` of a mix group, resolved with `find_group()` |
| `attenuation`, `minDistance`, `maxDistance`, `rolloff` | `INVERSE`, `1`, `100`, `1` | the distance curve, see [3D sound](playing-sounds.md#3d-sound-and-the-listener) |

Three more fields are hidden from the inspector and are runtime state: `playbackRequest` (`PLAY`, `STOP`,
`PAUSE`, `RESUME`; written by scripts through the `audio_*` bindings and cleared by the sync), `isPlaying`
(written back by the engine, one frame old when a script reads it) and `handle` (a `source_handle` linking the
component to its `voice_id` and the clip version it started with).

### `audio_listener`

`{ f32 gain = 1; bool active = true; }`. Put it on the camera entity. The first active listener in iteration
order drives `audio_engine::set_listener()` with the entity's world position and orientation; its `gain`
scales every voice.

### How the engine syncs them

The `engine.audio_sync` system runs once per frame, also while the game is paused, and does this in order:

1. **Listener.** Copies the first active `audio_listener` into a `listener_state` (velocity is the position
   delta since last frame) and calls `set_listener()`.
2. **Sources.** For every `audio_source`: a source without a voice that has `playOnStart` (first sight) or
   `playbackRequest == PLAY` and a loaded clip is started with `play(clip, play_params_from(source,
   find_group(source.group)))`; `PLAY` on a playing voice replaces it (the old voice is released with a short
   fade and a new one starts from the beginning); `STOP`, `PAUSE` and `RESUME`
   call `stop()`, `pause()` and `resume()`. Then `playbackRequest` is reset and `isPlaying = is_playing(voice)`.
   An edited component (`changed<audio_source>`) is pushed to its voice with `apply_source()`. A voice that
   ended or was stolen clears `handle`; a reloaded clip (`clip.version()` changed) is stopped and replayed.
3. **Positions.** One `set_positions()` call with every spatial voice's world position and velocity, then
   `update(deltaTime)`.

At frame end, `engine.audio_finish_one_shots` calls `end_frame()` and walks `finished_voices()`: a source with
`oneShot` has its entity destroyed, any other loses its `handle` and gets `isPlaying = false`; either way the
voice is released. Component voices are always retained and the engine gives each one back: when it ends, when a
`PLAY` replaces it, and when the component or its entity goes (a scene unload included), so a destroyed emitter
stops sounding. A copied component (a duplicated entity, a prefab made from a playing source) does not share the
original's voice: it starts its own.

```cpp
#include "conjure/audio/audio_engine.h"
#include "conjure/audio/components.h"

using namespace conjure;
using namespace conjure::audio;

// What the sync does for one source the first time it sees it.
void start_source(audio_engine &engine, audio_source &source) {
    if (source.handle.voice.is_valid() || !source.playOnStart || source.clip.get() == nullptr) {
        return;
    }
    const group_id group = engine.find_group(source.group);          // null name resolves to MASTER_GROUP
    const core::result<voice_id> voice = engine.play(source.clip, play_params_from(source, group));
    if (!voice.has_value()) {
        return;                                                      // try again next frame
    }
    source.handle.voice = voice.value();
    source.handle.clipVersion = source.clip.version();
    source.handle.flags |= source_handle::FLAG_STARTED_ONCE;
    source.isPlaying = true;
}
```

> [!TIP]
> Use the component for anything attached to an entity and `audio_engine::play()` directly only for sounds
> that belong to no entity (UI clicks, a global one-shot from a script). The component gives you serialisation,
> the inspector, hot reload and automatic release for free.

> [!WARNING]
> `isPlaying` on the component is written by the sync, so a script sees the value from the previous frame.
> When a script needs to know the instant a sound ended, use a `oneShot` source and react to the entity being
> destroyed, or keep a retained voice yourself and poll `is_playing()`.

> [!NOTE]
> Both components are trivially copyable (`audio_source` is 88 bytes) so the ECS can move them with a memcpy,
> and their field order is mirrored by the C# `AudioSourceData` / `AudioListenerData` structs. Reflection names:
> `conjure.audio.audio_source`, `conjure.audio.audio_listener`, `conjure.audio.attenuation_model`,
> `conjure.audio.audio_playback_request`.

## Errors

Every fallible call returns a `result<T>` carrying an error of category `"audio"`: `CLIP_NOT_LOADED` (the asset
is not resident yet), `INVALID_CLIP`, `VOICE_LIMIT_REACHED`, `STREAM_SLOT_LIMIT`, `INVALID_VOICE`,
`INVALID_GROUP`, `GROUP_LIMIT_REACHED`, `DUPLICATE_GROUP`, `GROUP_CYCLE`, `DECODE_FAILED`, `UNSUPPORTED_FORMAT`,
`INVALID_SETTINGS`, `BACKEND_INIT_FAILED`, `DEVICE_START_FAILED`, `DEVICE_NOT_FOUND`, `DEVICE_LOST`. Setters
(`set_volume`, `set_position`,...) return nothing: on a stale `voice_id` they do nothing and log at TRACE. See
📘 API: [Errors and results](../core/errors-and-results.md) for `result<T>`.

## API summary

| Signature (short) | Purpose |
|---|---|
| `struct audio_source { asset_ref<audio_clip> clip; f32 volume, pitch; bool loop, spatial, playOnStart, oneShot; string_id group; attenuation_model attenuation; f32 minDistance, maxDistance, rolloff; ... }` | the emitter component |
| `struct audio_listener { f32 gain; bool active; }` | the ear component |
| `struct source_handle { voice_id voice; u32 clipVersion; u8 flags; }` with `started_once()`, `FLAG_STARTED_ONCE` | hidden link from a source to its voice |
| `enum class audio_playback_request { NONE, PLAY, STOP, PAUSE, RESUME }` | script command on a source |
| `play_params play_params_from(const audio_source &, group_id)` | the `play_params` a source describes (`retain = true`) |
| `AUDIO_SOURCE_TYPE_NAME`, `AUDIO_LISTENER_TYPE_NAME`,... | reflection type names |

## See also

- 📘 API: [Playing sounds](playing-sounds.md)
- 📘 API: [Clips and decoding](clips-and-decoding.md)
- 📘 API: [Reflection](../core/reflection.md) for how the component fields reach the inspector
- 📘 API: [Glossary](../glossary.md)
- 🛠 Handbook: [Audio module](../../handbook/engine/audio.md)

## Supporting headers

| Include path | Purpose |
|---|---|
| `conjure/audio/audio_error.h` | Declares `audio_error`, the module failure codes carried by `core::error` and `core::result<T>`. |
| `conjure/audio/log_category.h` | Declares the module log category used to filter its diagnostics through core logging. |
<!-- doc-content:body:end -->
