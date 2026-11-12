# Audio module

🛠 **Engineering handbook** · 📘 API: [Audio](../../api/audio/index.md), [Playing sounds](../../api/audio/playing-sounds.md), [Clips and decoding](../../api/audio/clips-and-decoding.md)

`conjure::audio` (target `conjure_audio`, alias `conjure::audio`) is the wave-2 module that mixes voices into
groups, spatialises them against one listener, decodes and cooks the `conjure.audio.clip` asset and drives a
device through a backend. This chapter is for people changing the module: it explains the one-mixer / two-driver
architecture, the lock-free rings between the main thread and the callback, how voice slots are reused without
ABA, the group gain tree, the spatial maths, the null backend's virtual clock, device-loss recovery and the
tests. The binding design is [`12_audio.md`](../../design/12_audio.md); the README at
`engine/src/audio/README.md` carries the finer behavioural notes.

## Contents

- [1. Shape of the module](#1-shape-of-the-module)
- [2. Mixer architecture and callback rules](#2-mixer-architecture-and-callback-rules)
- [3. Command and event rings](#3-command-and-event-rings)
- [4. Voices: table, slots and lifecycle](#4-voices-table-slots-and-lifecycle)
- [5. Group gain tree](#5-group-gain-tree)
- [6. Spatialisation](#6-spatialisation)
- [7. Streaming](#7-streaming)
- [8. Null backend and the virtual clock](#8-null-backend-and-the-virtual-clock)
- [9. Device loss and recovery](#9-device-loss-and-recovery)
- [10. Failure handling](#10-failure-handling)
- [11. Threading model](#11-threading-model)
- [12. Extension points](#12-extension-points)
- [13. Tests](#13-tests)
- [14. Open items](#14-open-items)
- [Related](#related)

## 1. Shape of the module

`engine/src/audio/CMakeLists.txt` declares the module with `conjure_add_module(audio ... PUBLIC_DEPS
conjure::core conjure::assets PRIVATE_DEPS conjure::thirdparty::miniaudio)`, guarded by the `CONJURE_WITH_AUDIO`
option (ON by default; OFF skips the module) and by the presence of `conjure::assets`. The module never includes
`ecs`: the engine's `audio_subsystem` (wave 4) registers the components and owns the `engine.audio_sync` and
`engine.audio_finish_one_shots` systems (`09_cross_area_resolutions.md` R7). Only `decoder_impl.cpp` and
`miniaudio_backend.cpp` include `miniaudio.h`; miniaudio is built in `third_party/src/miniaudio_impl.c` with
`MA_NO_ENGINE`, `MA_NO_NODE_GRAPH`, `MA_NO_GENERATION` plus `stb_vorbis` for Ogg (A8).

| Public header (`conjure/audio/`) | Contents | Wiki page |
|---|---|---|
| `audio_types.h` | constants, `sample_format`, `clip_storage`, `clip_encoding`, `audio_backend_kind`, `device_state`, `voice_state`, `attenuation_model`, `audio_format`, `to_string`, `is_active_state`, `is_terminal_state` | Playing sounds |
| `audio_error.h`, `log_category.h` | `audio_error` (18 codes, category `"audio"`), `LOG_AUDIO` | - |
| `clip.h` | `pcm_buffer`, `clip_info`, `audio_clip`, `make_pcm_clip`, `make_fallback_clip`, asset type declaration | Clips and decoding |
| `decoder.h` | `decode_options`, `sniff_encoding`, `probe`, `decode_to_pcm`, `streaming_decoder` | Clips and decoding |
| `clip_asset.h` | `clip_blob_header`, `clip_import_settings`, `cook_clip`, `read_clip_blob_header`, `load_clip_blob`, importer, loader, `register_asset_types` | Clips and decoding |
| `voice.h` | `voice_id`, `play_params`, clamp helpers, `voice_info` | Playing sounds |
| `mix_groups.h` | `group_id`, `MASTER_GROUP`, `group_desc`, `group_info`, `clamp_group_gain` | Playing sounds |
| `spatialisation.h` | `listener_state`, `attenuation_params`, `spatial_result`, `compute_*`, `clamp_attenuation`, `clamp_listener` | Playing sounds |
| `components.h` | `audio_source`, `audio_listener`, `source_handle`, `audio_playback_request`, `play_params_from`, reflection | Audio |
| `audio_device.h` | `mix_callback`, `audio_device_info`, `backend_desc`, `NULL_DEVICE_NAME` | Playing sounds |
| `audio_backend.h` | `audio_backend` interface, `is_backend_compiled`, `create_backend` | Clips and decoding |
| `null_backend.h` | `null_*_stats`, `null_backend` | Clips and decoding |
| `audio_engine.h` | `audio_settings`, `voice_transform`, `audio_engine_stats`, `audio_engine` | Playing sounds |

Private (`engine/src/audio/`): `spsc_ring.h`, `commands.h`, `stream_ring.{h,cpp}`, `mixer.{h,cpp}`,
`group_table.{h,cpp}`, `miniaudio_backend.{h,cpp}`, `decoder_impl.cpp`, and one `.cpp` per public header.
`audio_engine.cpp` keeps the engine's state in a private `struct audio_engine::state` and its algorithms in
`detail::audio_engine_helpers`, which is also the friend that writes the null backend's stats snapshot.

Memory is fixed after construction: 256 mixer slots, a 512-entry voice table, 32 stream rings of 16384 stereo
frames (128 KiB each), the two rings and one mix block, about 6 MB in total (A16). `mixer::render` is
allocation-free, asserted by `conjure::testing::allocation_scope`.

## 2. Mixer architecture and callback rules

There is exactly one mixing code path, `mixer::render(f32 *out, u32 frameCount)` (`mixer.h`), and two drivers:
the miniaudio data callback on the device thread and `null_backend::update(dt)` on the main thread. Output is
interleaved `f32`, `MIX_CHANNELS = 2`, at `audio_settings::sampleRate`; miniaudio converts to the device's
native format. Because every policy decision (group tree walk, attenuation, pan, Doppler, clamping, stealing) is
made on the main thread and sent as a command, the null backend is bit-identical to the real one and tests
assert on captured samples.

### 2.1 Render loop

```
render(out, frameCount):
    for block in split(frameCount, MIX_BLOCK_FRAMES):           256 frames: the ramp granularity
        apply_commands()                                         drain the command ring; drop generation mismatches
        zero mMixBlock
        STOPPING slot while its group or the master is paused: STOPPED at once (no fade)
        for slot in PLAYING | PAUSED | STOPPING, group and master not paused:
            advance gain ramp (per frame) and pan ramp (per block)
            PCM:    s = lerp(samples, cursor); STREAMED: s = stream.pop_frame() (empty -> 0, one UNDERRUN per block)
            downmix ? mono = 0.5 * (l + r); apply pan law; g = gainCurrent * mGroupGain[groupIndex]
            cursor += step; cursor >= clipFrames: loop ? wrap + LOOPED : FINISHED
            STOPPING and ramp done and gain <= 1e-4: STOPPED
            publishedCursor.store(relaxed)
        clamp mMixBlock to [-1, 1]; copy to out
```

Pan laws (`mixer.h` header comment): a mono or downmixed spatial voice uses constant power
(`left = cos((pan + 1) * PI / 4)`, `right = sin(...)`, centre is -3 dB per channel); a flat stereo voice uses a
balance law (`left = min(1, 1 - pan)`, `right = min(1, 1 + pan)`) so a stereo clip at gain `g` renders at
amplitude `g`. `SET_DOWNMIX` switches the law when `set_spatial()` flips a stereo voice. Pitch is the per-voice
`step = pitch * clipRate / mixRate * dopplerRatio` through a linear interpolator; cooked PCM keeps its source
rate (A9). A `STOP` on a `PAUSED` voice, or on a voice whose group or the master is paused, ends it at once: a
frozen voice cannot run its fade, and its slot (wanted by a voice waiting to steal it) must not wait for an unpause.

### 2.2 Rules inside `render`

| Rule | Why |
|---|---|
| no allocation, lock, log, assert handler, asset access, decoding or group tree walk | the callback has a hard deadline; the null backend must behave identically |
| every state change of a slot is published through its atomics (`state`, `publishedCursor`, `generation`) | `is_valid` / `is_playing` / `voice()` read them from any thread |
| `PLAY` writes every plain field, cursor and state before storing `generation` (release) | a reader that loads the generation (acquire) and finds the occupant it expects also sees that occupant's state, never the previous occupant's terminal state |
| commands whose `generation` does not match the slot are dropped and counted | stale traffic for a reused slot is harmless |
| events that do not fit the ring are dropped and `eventsDropped` incremented | the main thread reconciles from the atomics (section 3) |

## 3. Command and event rings

`spsc_ring<T, Capacity>` (`spsc_ring.h`) is a power-of-two single-producer single-consumer ring of trivially
copyable items with head and tail on separate cache lines (explicit padding instead of `alignas`, which MSVC
reports as C4324); `try_push` publishes with a release store of the tail, `try_pop` acquires it. Nothing blocks
or allocates.

| Ring | Direction | Item | Capacity | Overflow |
|---|---|---|---|---|
| commands | main -> mixer | `voice_command` (<= 80 bytes: `command_kind`, `voiceIndex`, `generation`, `fadeFrames`, payload union) | `COMMAND_RING_CAPACITY` 8192 | spilled into a main-thread vector, flushed in order by `update()`; `commandsSpilled++`; never dropped |
| events | mixer -> main | `voice_event` (24 bytes: `event_kind`, `voiceIndex`, `generation`, `frame`) | `EVENT_RING_CAPACITY` 4096 | dropped by the callback, `eventsDropped++`; `update()` reconciles every voice from the slot atomics |

Command kinds: `PLAY`, `STOP`, `PAUSE`, `RESUME`, `SET_GAIN`, `SET_PAN`, `SET_STEP`, `SET_LOOP`, `SEEK`,
`SET_GROUP`, `SET_DOWNMIX`, `SET_GROUP_GAIN`, `SET_GROUP_PAUSED`, `SET_MASTER_PAUSED`. Event kinds: `STARTED`,
`FINISHED`, `STOPPED`, `LOOPED`, `UNDERRUN`, `PAUSED`, `RESUMED`. Once the spill vector is non-empty every new
command appends to it, so ordering is preserved across the overflow. Because `FINISHED` / `STOPPED` are also
visible as slot states, the reconcile pass in `update()` (compare generation first, then read state and cursor)
means a terminal state can never be lost even when every event was dropped.

## 4. Voices: table, slots and lifecycle

### 4.1 Two tables

| Table | Owner | Size | Purpose |
|---|---|---|---|
| voice table (`voice_entry`) | main thread | `VOICE_TABLE_CAPACITY` = 512 | the `voice_id` space: atomic generation and state (read by `is_valid` / `is_playing` from any thread), a `voice_mirror` behind a per-entry seqlock (read by `voice()`), shared `pcm` / `encoded` pointers, stream slot, position, velocity, `play_params`, `awaitingAck` |
| mixer slots (`mixer_voice`) | mixer | `MAX_VOICES` = 256 | the audible state: atomics `state`, `publishedCursor`, `generation`; plain fields for samples, cursor, step, gain and pan ramps, loop, downmix, group |

The voice table is twice the slot count so a voice waiting for a stolen slot owns an id of its own. `voice_id`
wraps a `core::slot_key` (8 bytes); `voice_info` is republished into the seqlocked mirror after `play()`, after
every setter that changes it and after every `update()`; `read_mirror()` is the classic seqlock loop.

### 4.2 `play()`

1. Clamp `play_params` (A15); reject an invalid clip (`INVALID_CLIP`) or a stale group (`INVALID_GROUP`).
2. Take a voice entry; copy the clip's `shared_ptr` (A6: the voice keeps the samples alive through a hot reload).
3. STREAMED: take one of 32 stream slots (`STREAM_SLOT_LIMIT`), attach it to the voice *before* opening the
   `streaming_decoder` so a clip that fails to decode releases the slot again (`DECODE_FAILED`); the voice starts in
   `PRIMING`.
4. Take a free mixer slot, or steal: the victim is the voice with the lowest `priority <= params.priority`, ties
   broken by lowest `effectiveGain`, excluding looping retained voices; it gets `STOP` with a
   `STEAL_FADE_SECONDS` (2 ms) fade and the new voice waits in `PRIMING` until the `STOPPED` acknowledgement frees
   the slot. No candidate: `VOICE_LIMIT_REACHED`, `voicesDropped++`, TRACE.
5. PCM with a slot: push `PLAY { samples, step, gain, pan, loop, startFrame, downmix, channels }`; the voice is
   `PLAYING` (or `PAUSED` with `startPaused`). `is_playing()` is true from here, including during `PRIMING`.
   `PLAY` starts paused from `userPaused` alone (seeded from `startPaused`), so `resume()` in `PRIMING` cancels it.

### 4.3 End of life

A slot is reused only after the callback acknowledged its previous occupant (`FINISHED` / `STOPPED` with the
matching generation), which is what makes reuse ABA-free. `end_frame()` erases every acknowledged **one-shot**
(`retain == false`), releases its `shared_ptr`s and stream slot and publishes the frame's ended ids through
`finished_voices()` (valid until the next `end_frame()`, each id exactly once). A **retained** voice stays in its
terminal state until `release()`, which stops it first if needed and erases it at the acknowledgement. The
destructor stops every voice, stops the backend (joining the device thread), waits for in-flight decode jobs and
only then destroys rings, slots and streams.

### 4.4 `update(dt)`

```
1. backend.update(dt)        miniaudio: nothing to poll; null: advance the clock and render (section 8)
2. recover_device()          section 9
3. drain events              FINISHED / STOPPED: mirror the state, awaitingAck = false; LOOPED / UNDERRUN: counters
4. reconcile from atomics    generation first, then state and cursor (covers dropped events)
5. spatial pass              per spatial voice: compute_spatial(); SET_GAIN when |delta| > 1e-4, SET_PAN when > 1e-3,
                             SET_STEP when the relative step change > 1e-3 (Doppler)
6. streaming pass            schedule decode_chunk or run it inline; PRIMING slot with a chunk -> PLAY; seek phases
7. flush spill, stats, null snapshot
```

Every mutating member (`update()`, `end_frame()`, `play()`, `select_device()`, setters, groups) asserts
`core::jobs::is_main_thread()`; only the any-thread readers of section 11 may be called elsewhere.

## 5. Group gain tree

`group_table` (`group_table.h`) owns `MAX_GROUPS` = 32 slots with generations, `MASTER_GROUP = {0, 1}` always
live in slot 0, depth at most `MAX_GROUP_DEPTH` = 8. A group's `effectiveGain` is `gain * parent.effectiveGain`.
`set_gain()` stores the clamped gain, recomputes the subtree depth-first and returns the changed slot indices
(`group_change_list`, a `small_vector<u8, 32>`); the engine turns each into one `SET_GROUP_GAIN { index,
effectiveGain }`. The mixer therefore multiplies one number per voice (`mGroupGain[groupIndex]`) and never walks
the tree. `remove()` reparents children, reports where voices must move (the engine sends `SET_GROUP` per voice)
and bumps the slot generation so stale `group_id`s are rejected; `remove(MASTER_GROUP)` is `INVALID_GROUP`.
`group_desc::parentName` resolves parents by name because settings groups are declared before any id exists;
construction creates them in dependency order. Paused flags live beside the gains (`SET_GROUP_PAUSED`,
`SET_MASTER_PAUSED`): a paused group's voices are skipped without advancing their cursors (a stopping one ends).

## 6. Spatialisation

`spatialisation.h` holds pure, any-thread functions; the engine calls `compute_spatial()` once per spatial voice
per frame in the spatial pass and sends the results as commands, so the callback does no geometry.

| Quantity | Formula (`d = clamp(distance, minDistance, maxDistance)`) |
|---|---|
| `INVERSE` | `min / (min + rolloff * (d - min))` |
| `LINEAR` | `1 - rolloff * (d - min) / (max - min)` clamped to `[0, 1]` |
| `EXPONENTIAL` | `(d / min) ^ -rolloff` |
| `NONE` | `1` |
| pan | listener-right component of the normalised direction to the source; 0 ahead, behind or coincident |
| Doppler | `((c - v_listener . dir) / (c - v_source . dir)) ^ dopplerFactor`, clamped to `[0.5, 2]`; 1 when the factor is 0 or the source is at the listener |

The voice gain sent as `SET_GAIN` is `volume * attenuation * listenerGain`; the group's effective gain is
multiplied in the callback, and `voice_info::effectiveGain` reports the product of all four. A spatial voice with
no position yet is treated as sitting at the listener (gain 1, pan 0). Stereo clips played spatially are
downmixed to mono before panning (A10). `clamp_listener()` normalises `forward` and `up` (making `up`
perpendicular), sanitises non-finite components and clamps the gain to `[0, 2]`.

## 7. Streaming

A `STREAMED` clip keeps the encoded file in memory (`audio_clip::encoded`). `play()` opens a `streaming_decoder`
(a miniaudio decoder over memory) on one of `MAX_STREAMING_VOICES` = 32 stream slots, each with a `stream_ring`
of `STREAM_RING_FRAMES` = 16384 decoded frames (SPSC: producer is the decode job, consumer is the callback;
counters on separate cache lines, `end_of_stream` flag). `decode_chunk` fills `STREAM_CHUNK_FRAMES` = 4096 frames
per run, loop-aware (seeks the decoder back to 0 and continues), and runs as a `core::jobs` job when
`audio_settings::jobs` is set (one job per slot at a time, guarded by an acquire/release `jobInFlight` flag) or
inline inside `update()` otherwise, where the ring is filled completely every frame so a frame of up to 16384 mix
frames never underruns. The open passes the clip's probed `frameCount` so a Vorbis stream is not decoded a second
time just to count it. An empty ring with `end_of_stream` ends the voice; an empty ring without it is an
`UNDERRUN` (silence for the block, counted once per block).

Every streamed `seek()` goes through the `seek_phase` state machine in `streaming_pass()`: the ring and decoder
are reset only once no decode job is in flight and nothing reads the ring (the voice has no mixer slot yet, or the
mixer has *published* `PAUSED` for this occupant; the main-thread mirror alone is not enough while a `PAUSE` is
still in the command ring), then the voice re-primes and resumes. A voice waiting for a stolen slot restarts
from the new decoder position (`streamStartFrame`). After the reset `seekFrame` is the decoder's `position()`,
so a seek the decoder refuses (WARN) reports the frame the audio really continues from. A `seek()` arriving in
`WAIT_PRIME` (ring refilling from the old target, mixer voice still paused) goes back to `WAIT_RESET`.

## 8. Null backend and the virtual clock

`null_backend` has no device, no thread and no OS calls. `start(desc)` resets the virtual clock and fails only
with `INVALID_SETTINGS` (null callback, sample rate outside `[MIN_SAMPLE_RATE, MAX_SAMPLE_RATE]`) or
`DEVICE_START_FAILED` while a `fail_next_starts()` budget remains. `update(dt)` renders `round(dt * sampleRate)`
frames through the registered `mix_callback` in `MIX_BLOCK_FRAMES` blocks, carrying the fractional remainder
(`mFractionalFrames`) so 60 updates of 1/60 s at 48 kHz make exactly 48000 frames and three updates of 1/90 s
make 533, 533, 534; `render_frames(n)` renders exactly `n` frames for block-stepping tests. The engine then writes
the voice / group / listener snapshot into `null_backend_stats` through the `audio_engine_helpers` friend, which is
what `headless_engine::audio_stats()` reads (`08_testing.md` 4.4). `set_capture_frames(n)` keeps the last `n`
stereo frames in `captured()`; `simulate_device_loss()` publishes `LOST` exactly as miniaudio's notification
callback would, and `fail_next_starts(count)` makes the next `count` starts fail and leave the backend `STOPPED`.

The miniaudio backend (`miniaudio_backend.{h,cpp}`) owns an `ma_context` and `ma_device` behind a `unique_ptr
impl`; the data callback runs the mix callback on the device thread, the notification callback stores
`device_state` into an atomic and nothing else, and `update()` has nothing to poll. `start()` turns `STARTING`
into `RUNNING` with a compare-exchange, so a `LOST` the callback stored meanwhile survives for
`recover_device()`. `enumerate_devices()` creates the context lazily. `is_compiled()` is true when the library
was built with miniaudio (`CONJURE_WITH_AUDIO`).

## 9. Device loss and recovery

`recover_device()` in `audio_engine.cpp` runs at the top of every `update()`:

1. The first `LOST` observed puts the engine into its own `recovering` state (WARN "audio device lost"). A
   failed `start()` leaves a backend `STOPPED`, not `LOST`, so the backend's state cannot drive the loop: the
   engine's flag does.
2. While recovering, `stats().deviceState` reports `LOST`, voices are frozen (the callback is not running, so
   cursors do not advance and nothing finishes) and `recoveryTimer` accumulates frame time.
3. Every `recoveryBackoffSeconds` the engine retries `start()` on the configured or default device. Success:
   `deviceRestarts++`, INFO, voices continue. Failure: `recoveryAttempts++`.
4. After `maxRecoveryAttempts` failures the engine replaces the backend with a fresh `null_backend`
   (`fellBackToNull = true`, `last_backend_error()` = the start failure, WARN "falling back to the null backend")
   so voices keep finishing and `is_playing()` stays truthful (A14).

`select_device(name)` reuses the restart path deliberately: stop, restart on the named device, keep the previous
device on failure; voices freeze for the restart and continue afterwards. An unknown non-empty name falls back to
the default device with a WARN (`12_audio.md` 3.9) and counts as success. If neither the new nor the previous
device starts, the engine continues on a fresh null backend with `last_backend_error()` set (the previous
device's failure) and `fellBackToNull = true`, and returns the new device's error. Construction follows the
same rule: an invalid `audio_settings` (`INVALID_SETTINGS`, ERROR log) or a backend that cannot start falls back
to null with `last_backend_error()` set, so `audio_engine` never fails to construct (A1).

## 10. Failure handling

| Situation | Behaviour |
|---|---|
| invalid settings or backend start failure at construction | null backend, `last_backend_error()`, `fellBackToNull` |
| unknown `deviceName` | WARN, default device |
| `select_device()`: neither the new nor the previous device starts | null backend, `last_backend_error()`, `fellBackToNull`; the new device's error returned |
| clip not resident (`asset_ref::get()` null) | `CLIP_NOT_LOADED`; the sync retries next frame |
| clip without its buffer or info mismatch | `INVALID_CLIP` |
| 33rd streamed voice / streamed clip that does not open | `STREAM_SLOT_LIMIT` / `DECODE_FAILED` (slot released), `voicesDropped++` |
| 256 voices busy, nothing stealable | `VOICE_LIMIT_REACHED`, `voicesDropped++`, TRACE |
| command ring full / event ring full | spill, flushed in order / drop, reconcile from atomics |
| stale or invalid `voice_id` / `group_id` | setters no-op with TRACE; `result` functions return `INVALID_VOICE` / `INVALID_GROUP` |
| NaN or infinity in a setter | sanitised at the API boundary (A15); `CONJURE_ASSERT` in Debug |
| forged WAV header (declares more frames than the file holds) | `DECODE_FAILED`; `decode_to_pcm` caps its reserve by the file size |
| blob header mismatch (magic, version, size, channels, storage, a PCM frame count its data does not hold) | `INVALID_CLIP` naming the field; the PCM check divides, so a forged frame count cannot wrap into a huge allocation |
| streamed payload whose format disagrees with its header | `INVALID_CLIP`; `load_clip_blob` opens the payload with the header's `frameCount`, so a Vorbis stream is not decoded to check it |
| PCM-only import settings on a streamed clip | the import succeeds with a warning |
| streamed seek the decoder refuses | WARN; the voice continues, and reports, from the decoder's position |
| stream underrun | silence for the block, `UNDERRUN`, `underruns++` |
| any `audio_engine` mutator (`update()`, `end_frame()`, `play()`, setters, groups) off the main thread | `CONJURE_ASSERT(core::jobs::is_main_thread())` |
| destruction with live retained voices | stopped and released; INFO count |

The callback itself cannot fail: every path above runs on the main thread or on a job.

## 11. Threading model

| Thread | Does |
|---|---|
| Main | every `audio_engine` call except the lock-free readers; command ring producer, event ring consumer; `shared_ptr` acquire / release of PCM and encoded bytes; all logging; backend start / stop / recovery; the only writer of the `voice_mirror` seqlocks and of the null stats snapshot |
| Device (miniaudio data callback) | `mixer::render` only; command ring consumer, event ring producer, stream ring consumer; publishes a slot's generation last |
| miniaudio notification callback | one atomic store of `device_state` |
| Job workers | `decode_chunk` (stream ring producer, one job per slot at a time), importer `import()`, loader `load_cpu` (`decode_to_pcm`) |
| Any | `is_valid`, `is_playing` (atomics), `voice()` (atomics plus the seqlock snapshot), `stats()` (relaxed counters; `outputFormat` fixed at construction), `output_format()`, `pcm_buffer` reads, the `compute_*` functions |

No `std::thread` exists in the module: the device thread belongs to miniaudio and decoding runs on
`core::jobs`. The backend is stopped (device thread joined) before rings, slots and streams are destroyed and
whenever the mixer's sample rate changes.

## 12. Extension points

| Extension | How |
|---|---|
| a new backend | derive `audio_backend`, drive `backend_desc::callback` from your thread or clock, publish `device_state` atomically, add the kind to `audio_backend_kind` and `create_backend()` |
| a new attenuation curve | add to `attenuation_model`, implement in `compute_attenuation()`, extend `CONJURE_REFLECT_ENUM` in `components.h` and the `to_string` table |
| a new per-voice parameter | add it to `play_params` and `voice_info`, a `command_kind`, the mixer field and `apply_command`, the clamp rule, the seqlock republish, and `play_params_from()` if components expose it |
| a new container format | miniaudio decoder flags in `third_party/src/miniaudio_impl.c`, `sniff_encoding`, `clip_encoding`, `CLIP_EXTENSIONS` |
| DSP effects, HRTF, surround | out of scope for v1 (`12_audio.md` 11); a build without `MA_NO_NODE_GRAPH` is the starting point |

`spsc_ring` is a candidate for `core/containers` once a second user exists.

## 13. Tests

`tests/audio/` mirrors every public header (`test_<header>.cpp`, enforced by `check_style.py
--tests-mirror-headers`) and adds `test_mixer.cpp` (the private hot path through `audio/mixer.h`),
`test_source_listener.cpp` (the engine driven the way `engine.audio_sync` will, 120 frames with a moving
listener) and `test_miniaudio_backend.cpp` (`[audio_device]`, requires `precondition::AUDIO_DEVICE`, skips with
exit 77 without a playback device; waits are bounded spins on published state). Everything else runs on the null
backend: `audio_test_support.h` provides `null_settings()`, `step_frames()`, `step_until_ended()` and
`slot_stats()`; `wav_builder.h` generates 16-bit and float WAV files and `pcm_buffer`s from a sine, square or ramp
so no binary asset is needed. `.ogg` / `.mp3` / `.flac` decoding runs behind `precondition::DOWNLOADED_ASSETS`
and expects `tests/.downloaded/audio/tone.{ogg,mp3,flac}`.

| What is pinned | Where |
|---|---|
| 60 updates of 1/60 s render exactly 48000 frames; fractional carry; `MIX_BLOCK_FRAMES` blocks; capture; 256 voices render allocation-free | `test_null_backend.cpp` |
| a 0.5 s clip finishes after 0.5 s and appears in `finished_voices()` once; one-shot slot FREE after `end_frame`; retained stays FINISHED; 5 ms stop ramp without a sample jump; pitch 2 halves the time; 24 kHz clip steps at 0.5; hard-left pan; attenuation floor at `maxDistance`; 256 voices and the 257th steals priority 10; `VOICE_LIMIT_REACHED`; 10000 `set_volume` calls spill and all apply; streamed == PCM sample for sample; 2-worker streaming; `STREAM_SLOT_LIMIT` at 33; hot reload keeps the old buffer; destruction with 100 voices is leak-free | `test_audio_engine.cpp` |
| `regression - audio review N`: recovery keeps retrying after a failed restart; fallback to null after `maxRecoveryAttempts`; a streamed clip that fails to open releases its slot; streamed seeks racing a decode job or a pending PAUSE; STOP on a paused voice; `set_spatial` on a stereo voice | `test_audio_engine.cpp` |
| `regression - code sweep Fnnn`: release or steal under a group or master pause (F022); `resume()` of a `startPaused` voice waiting for a slot (F023); a streamed seek the decoder refuses, and a second seek while the first refills (F325) | `test_audio_engine.cpp` |
| `select_device` keeps the previous device, or falls back to null with `last_backend_error()` when no device starts; mutators called off the main thread assert | `test_audio_engine.cpp` |
| STOP under a paused group or master, and a group pause during a STOP fade (F022) | `test_mixer.cpp` |
| a PCM `frameCount` that wraps the size check (F183); a streamed payload whose format disagrees with its header; the import warning for PCM-only settings on a streamed clip (F321) | `test_clip_asset.cpp` |
| effective gain product; 0.5 in Music (0.5) under master (0.5) renders at 0.125; limits, duplicates, cycles; `remove_group` moves voices and children; paused group freezes cursors; settings groups by `parentName` | `test_mix_groups.cpp` |
| every attenuation model at `minDistance`, LINEAR reaches 0, monotonic curves; pan right / left / ahead / behind / rotated; Doppler approaching / receding / clamp | `test_spatialisation.cpp` |
| ramp step bounded; fractional-cursor interpolation; downmix; output clamp; generation mismatch drops a command; event overflow keeps FINISHED | `test_mixer.cpp` |
| WAV probe / decode within 1e-4 (16-bit) and 1e-6 (float); `forceMono`, `sampleRate`, `normalize`; truncated and random input; streaming reads equal the full decode; `[downloaded]` formats | `test_decoder.cpp` |
| blob header, settings JSON round trip, `cook_clip` storage rules, importer identity and failure message, loader through `memory_asset_source` + `load_blocking`, fallback clip, reload keeps the old `shared_ptr` valid | `test_clip_asset.cpp` |
| 88-byte `audio_source`, field order and offsets, hidden fields, `ASSET_REF` kind, ranges, JSON round trip with a uuid clip | `test_components.cpp` |

```
python tools/build.py --build-dir build/mod_audio --target conjure_audio_tests --test -R audio
```

The design specifies three benchmarks for `tests/benchmarks/bench_audio.cpp` (Development, outside ctest):
`audio.update_256_voices` (< 0.1 ms including `end_frame`), `audio.render_256_voices_480_frames` (< 0.5 ms,
allocation-free) and `audio.decode_wav_10s` (< 20 ms). The harness exists ([Testing](../testing.md#11-benchmarks));
`bench_audio.cpp` is not written yet (see [Open items](#14-open-items)).

> [!TIP]
> When a change touches the mixer, run `test_mixer` and `test_null_backend` first: they fail fast and point at
> the block, ramp or ordering rule you broke. The engine cases then tell you which policy on the main thread
> disagrees with the callback.

## 14. Open items

1. `tests/.downloaded/audio/tone.{ogg,mp3,flac}` must be listed in `tools/test_assets.json`; until CC0 clips are
   listed the `[downloaded]` decoder case skips, and the Vorbis path is untested in CI.
2. `audio_source::playbackRequest`, `isPlaying` and `handle` are `CONJURE_FIELD_HIDDEN` but `core::reflect`
   serialises every described field, so they appear in the reflection JSON. The design's "never serialised" holds
   at the ECS level: the engine registers `handle` as `TRANSIENT` and resets the three on load (`12_audio.md` 2.6,
   8). The module reports this as a documented deviation rather than hiding it.
3. The ECS integration (`audio_subsystem`, `engine.audio_sync`, `engine.audio_finish_one_shots`, the `audio_*`
   script bindings) landed with wave 4 ([Engine module](engine.md) 4, [Bindings module](bindings.md));
   `test_source_listener.cpp` still drives the engine the way the sync does.
4. `tests/benchmarks/bench_audio.cpp` (`12_audio.md` 10) has not been written; the A16 timing targets are
   unverified until it lands.

## Related

- design records: [`12_audio.md`](../../design/12_audio.md) (binding),
  [`09_cross_area_resolutions.md`](../../design/09_cross_area_resolutions.md) R7, R43, R50, R52,
  [`05_assets.md`](../../design/05_assets.md) D4, D14 (asset pointer lifetime, fallback clip),
  [`08_testing.md`](../../design/08_testing.md) 4.4, 5.2, 8.4
- README: `engine/src/audio/README.md`
- 📘 API: [Audio](../../api/audio/index.md), [Playing sounds](../../api/audio/playing-sounds.md),
  [Clips and decoding](../../api/audio/clips-and-decoding.md)
- handbook: [Core module](core.md) (`slot_map`, `small_vector`, `result`, jobs), [Architecture](../architecture.md),
  [Data-oriented design](../data-oriented-design.md), [Testing](../testing.md)
