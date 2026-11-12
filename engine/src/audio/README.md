# audio

Mixer, voices, mix groups, 3D spatialisation, the `conjure.audio.clip` asset type with its decoder,
importer and loader, and two backends: `miniaudio` (real devices) and `null` (headless). Binding
design: `docs/design/12_audio.md`; cross-area decisions R7, R43, R50 and R52 of
`docs/design/09_cross_area_resolutions.md`. Target `conjure_audio` (alias `conjure::audio`),
namespace `conjure::audio`, public dependencies `conjure::core` and `conjure::assets`, private
dependency `conjure::thirdparty::miniaudio`. The module never includes `ecs`: the engine's
`audio_subsystem` registers the components and owns `engine.audio_sync` / `engine.audio_finish_one_shots`.

## Public headers (`engine/include/conjure/audio/`)

| Header | Contents |
|---|---|
| `audio.h` | umbrella |
| `log_category.h` | `LOG_AUDIO` (`"audio"`) |
| `audio_error.h` | `audio_error` (18 codes), `audio_error_name`, category `"audio"` |
| `audio_types.h` | constants (`MAX_VOICES` 256, `MAX_GROUPS` 32, `MIX_CHANNELS` 2, `MIX_BLOCK_FRAMES` 256, ring sizes), `sample_format`, `clip_storage`, `clip_encoding`, `audio_backend_kind`, `device_state`, `voice_state`, `attenuation_model`, `audio_format`, `to_string`, `is_active_state`, `is_terminal_state` |
| `clip.h` | `pcm_buffer` (immutable interleaved f32, shared by clip and voices), `clip_info`, `audio_clip`, `make_pcm_clip`, `make_fallback_clip`, `CONJURE_DECLARE_ASSET_TYPE(audio_clip, "conjure.audio.clip")` |
| `decoder.h` | `decode_options`, `sniff_encoding`, `probe`, `decode_to_pcm`, `streaming_decoder` |
| `clip_asset.h` | `clip_blob_header` (32 bytes, `CJAC`), `clip_import_settings` (JSON), `cook_clip`, `read_clip_blob_header`, `load_clip_blob`, `audio_clip_importer`, `audio_clip_loader`, `register_asset_types` |
| `voice.h` | `voice_id` (slot key), `play_params` + `clamp_play_params`, `voice_info` |
| `mix_groups.h` | `group_id`, `MASTER_GROUP`, `group_desc` (parent by id or by `parentName`), `group_info`, `clamp_group_gain` |
| `spatialisation.h` | `listener_state`, `attenuation_params`, `spatial_result`, `compute_attenuation`, `compute_pan`, `compute_doppler_ratio`, `compute_spatial`, `clamp_attenuation`, `clamp_listener` |
| `components.h` | `audio_source` (88 bytes, field order = the C# `AudioSourceData` mirror; `clip` is an `assets::asset_ref<audio_clip>` reflected as `ASSET_REF`), `play_params_from(source, group)`, `source_handle`, `audio_playback_request`, `audio_listener` + reflection of these and of `attenuation_model` |
| `audio_device.h` | `mix_callback`, `audio_device_info`, `backend_desc`, `NULL_DEVICE_NAME` |
| `audio_backend.h` | `audio_backend` interface, `is_backend_compiled`, `create_backend` |
| `null_backend.h` | `null_voice_stats`, `null_group_stats`, `null_backend_stats`, `null_backend` (`update`, `render_frames`, capture, `virtual_time`) |
| `audio_engine.h` | `audio_settings` (+ `validate`), `voice_transform`, `audio_engine_stats`, `audio_engine` |

Private (`engine/src/audio/`): `spsc_ring.h`, `commands.h`, `stream_ring.{h,cpp}`, `mixer.{h,cpp}`,
`group_table.{h,cpp}`, `miniaudio_backend.{h,cpp}`, `decoder_impl.cpp` (the last two are the only
files that include `miniaudio.h`), one `.cpp` per public header.

## How it works

- **One mixer, two drivers.** `mixer::render(out, frames)` is the only mixing code. The miniaudio
  data callback calls it on the device thread; `null_backend::update(dt)` calls it on the main
  thread for `round(dt * sampleRate)` frames (fraction carried), in `MIX_BLOCK_FRAMES` blocks. The
  null backend therefore produces bit-identical output to the real one and tests assert on
  captured samples and on the `null_backend_stats` snapshot the engine writes after each update.
- **Lock-free rings.** Main thread -> mixer: `spsc_ring<voice_command, 8192>` (spilled into a vector
  when full, flushed by `update()`, never dropped). Mixer -> main: `spsc_ring<voice_event, 4096>`; an
  overflow is counted and `update()` reconciles from the per-slot atomics (state, cursor,
  generation), so a FINISHED / STOPPED can never be lost. Every command and event carries the mixer
  slot generation; both sides drop mismatches, which makes slot reuse ABA-free.
- **Voices.** The main thread owns a fixed table of 512 entries (twice the mixer slots, so a voice
  waiting for a stolen slot has an id of its own) with atomic generation and state, which is what
  `is_valid` / `is_playing` read from any thread; `voice()` adds a per-entry seqlock snapshot of
  the mirror (`voice_mirror`) that the main thread republishes after `play()`, every setter that
  changes `voice_info` and every `update()`. `play()` clamps the parameters (A15), rejects a stale
  group with `INVALID_GROUP`, takes a free mixer slot or steals the lowest-priority
  non-looping-retained voice (2 ms fade; the new voice waits in PRIMING), pushes PLAY and mirrors
  the state. One-shots are erased by `end_frame()` once acknowledged; retained voices live until
  `release()`. `finished_voices()` publishes every voice that ended during the frame exactly once.
  The mixer publishes a slot's generation last (after its fields and state, release), so the
  main-thread reconcile pass that compares the generation first can never pair a new occupant
  with the previous occupant's terminal state.
- **Gain, pan, pitch.** The main thread computes `volume * attenuation * listenerGain` per voice and
  sends it as `SET_GAIN` (ramped over 5 ms per sample inside the callback); the callback multiplies
  the group's effective gain from a 32-entry table that `SET_GROUP_GAIN` writes. Mono and downmixed
  spatial voices use constant-power panning (`cos` / `sin` of `(pan + 1) * PI / 4`); flat stereo voices
  use a balance law so a stereo clip at gain `g` renders at amplitude `g`; `set_spatial()` on a
  stereo voice sends `SET_DOWNMIX` so the law follows the flag (A10). Pitch is a per-voice step
  `pitch * clipRate / mixRate * dopplerRatio` through a linear interpolator; cooked PCM keeps its
  source rate (A9). `STOP` on a PAUSED voice, or on a voice of a paused group or under the master
  pause, ends it at once (it is silent already) instead of fading it back in; a frozen voice could
  not run its fade, so it would otherwise keep its slot until the unpause. `PLAY` starts paused
  from the voice's `userPaused` flag alone (seeded from `startPaused`), so `resume()` while a voice
  waits in PRIMING starts it playing.
- **Streaming.** STREAMED clips keep the encoded file; `play()` opens a `streaming_decoder` on one of
  32 stream slots (attached to the voice before the open so a clip that fails to decode releases
  the slot again), each with a 16384-frame SPSC `stream_ring`. The open passes the clip's probed
  `frameCount`, so a Vorbis stream is not decoded once more on the main thread just to count it.
  `decode_chunk` (4096 frames, loop-aware) runs as a `core::jobs` job when `audio_settings::jobs`
  is set, or inline inside `update()` otherwise (the ring is then filled completely each frame, so
  a frame of up to 16384 mix frames never underruns). Every streamed seek goes through the
  `seek_phase` state machine in `streaming_pass()`: the ring and decoder are reset only once no
  decode job is in flight and nothing reads the ring (the voice has no mixer slot yet, or the mixer
  has *published* PAUSED for this occupant; the main-thread mirror alone is not enough while a
  PAUSE is still in the command ring), then the voice re-primes and resumes. A voice without a
  mixer slot restarts from the new decoder position (`streamStartFrame`). After the reset the
  target becomes the decoder's `position()`, so a seek the decoder refuses (WARN) continues and
  reports from where the decoder stands; a second seek during `WAIT_PRIME` resets the ring again.
- **Groups.** `group_table` keeps the tree (depth <= 8), recomputes effective gains depth-first on
  every change and reports the changed slots; `remove_group` reparents children and voices and bumps
  the slot generation. Settings groups name their parents (`group_desc::parentName`) because ids do
  not exist before construction.
- **Device loss.** The miniaudio notification callback only stores `device_state`. The first LOST
  that `update()` observes puts the engine into its own `recovering` state (a failed `start()`
  leaves a backend STOPPED, not LOST, so the backend's state cannot drive the loop); it then
  retries `start()` every `recoveryBackoffSeconds` of frame time, with voices frozen and
  `stats().deviceState == LOST`, until a start succeeds (`deviceRestarts`) or `maxRecoveryAttempts`
  failures swap in a `null_backend` (`fellBackToNull`, WARN) so voices keep finishing (A14).
  `null_backend::simulate_device_loss()` / `fail_next_starts()` exercise the path headlessly.
  Construction never fails: invalid settings or a backend that cannot start fall back to null with
  `last_backend_error()` set; so does `select_device()` when neither the new nor the previous
  device starts. The miniaudio `start()` turns `STARTING` into `RUNNING` with a compare-exchange,
  so a LOST the notification callback stored meanwhile is not overwritten.
- **Decoding.** miniaudio's decoders (WAV, MP3, FLAC built in; Ogg Vorbis through `stb_vorbis`
  compiled into `third_party/src/miniaudio_impl.c`). Vorbis in push mode reports no length, so
  `probe` counts the frames once when the container does not say; `streaming_decoder::open(bytes,
  knownFrameCount)` skips that count when the caller already knows the length. Project files are
  untrusted input: a WAV header that declares more frames than the file can hold (half a byte per
  sample at worst) fails with `DECODE_FAILED`, and `decode_to_pcm` caps its reserve by the file
  size instead of trusting the declared length.
- **Clip asset.** `cook_clip` produces the 32-byte `CJAC` header plus f32 samples (PCM) or the
  original file (STREAMED; `auto` switches above `streamThresholdSeconds`). `load_clip_blob` validates
  every header field against the payload (the PCM frame count by division, so a forged count cannot
  wrap into a huge allocation) and opens streamed bytes to check their format against the header
  (with the header's frame count, so a Vorbis stream is not counted). `audio_clip_importer` and
  `audio_clip_loader` are thin adapters; the importer warns when `forceMono`, `sampleRate` or
  `normalize` is set on a clip it cooks as STREAMED. The loader needs no engine (A6, R43) and its
  fallback is the 0.1 s silent clip.

## Threading model

| Thread | Does |
|---|---|
| Main | every `audio_engine` call except the lock-free readers; every mutating member asserts `core::jobs::is_main_thread()`; the only writer of the `voice_mirror` seqlocks |
| Device (miniaudio data callback) | `mixer::render` only: no allocation, lock, log, asset access or decoding; publishes a slot's generation last |
| miniaudio notification callback | one atomic store of `device_state` |
| Job workers | `decode_chunk` (one job per stream slot at a time, `jobInFlight` acquire/release), importer `import`, loader `load_cpu` |
| Any | `is_valid`, `is_playing` (atomics), `voice()` (atomics plus the seqlock snapshot), `stats()` (relaxed counters; `outputFormat` fixed at construction), `output_format()`, `pcm_buffer` reads, `compute_*` |

The backend is stopped (device thread joined) before the engine's rings, slots and streams are
destroyed; in-flight decode jobs are waited for in the destructor.

## Open items (design deviations tracked explicitly)

1. **`tests/.downloaded/audio/tone.{ogg,mp3,flac}`** must be listed in `tools/test_assets.json`;
   the `[downloaded]` decoder case fails when the downloaded set is present but a tone file is
   missing, so the Vorbis path cannot go untested silently. Until CC0 clips are listed the case
   skips (`precondition::DOWNLOADED_ASSETS`).
2. **Hidden fields and JSON.** `audio_source::playbackRequest`, `isPlaying` and `handle` are
   `CONJURE_FIELD_HIDDEN` (inspector) but `core::reflect` serialises every described field, so they
   appear in the reflection JSON; the design's "never serialised" holds at the ECS level, where the
   engine registers `handle` as `TRANSIENT` and resets the three on load (12_audio.md 2.6, 8).

`audio_source`, `play_params_from` and `audio_engine::apply_source` (12_audio.md 2.6, 4.4, R50) landed with
the wave 2 integration once `assets` reflected `asset_ref<T>` as `ASSET_REF` and `core::reflect` learnt
the 24-byte handle; `apply_source(voice, source, group)` is `apply_params(voice, play_params_from(source,
group))`, the group being the caller's `find_group(source.group)` (a null name is MASTER, an invalid group
leaves the voice's group unchanged). The `[audio_device]` cases require `precondition::AUDIO_DEVICE`
(probe in `tests/audio/audio_test_support.cpp`) and skip with exit 77 on a machine without a playback
device.

## Testing

`tests/audio/` mirrors every public header (`test_<header>.cpp`) and adds `test_mixer.cpp` (private
hot path through `audio/mixer.h`), `test_source_listener.cpp` (the engine driven the way
`engine.audio_sync` will) and `test_miniaudio_backend.cpp` (`[audio_device]`, skips without a device
through `precondition::AUDIO_DEVICE`; its waits are bounded spins on conditions the device publishes). Clip data is generated
in-test by `wav_builder.h` (16-bit and float WAV, sine / square / ramp); `.ogg` / `.mp3` / `.flac`
decode runs behind `precondition::DOWNLOADED_ASSETS` and expects
`tests/.downloaded/audio/tone.{ogg,mp3,flac}`. Everything runs on the null backend, whose
`simulate_device_loss()` / `fail_next_starts()` hooks cover device-loss recovery and the null
fallback. The `regression - audio review N ...` cases pin the bugs the module review found
(recovery stall, stream slot leak, forged WAV length, STOP on a paused voice, stereo `set_spatial`,
streamed seeks racing a decode job or a pending PAUSE); the `regression - code sweep Fnnn ...`
cases pin the code sweep fixes (STOP under a paused group or master, `resume()` of a waiting
`startPaused` voice, a wrapping PCM frame count, a refused or retargeted streamed seek).

```
python tools/build.py --build-dir build/mod_audio --target conjure_audio_tests --test -R audio
```
