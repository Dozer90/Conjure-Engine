# Clips and decoding

📘 **Public API wiki** · 🛠 Handbook: [Audio module](../../handbook/modules/audio.md)

A **clip** is the samples of one sound. This page covers the clip types (`conjure/audio/clip.h`), the
`conjure.audio.clip` asset type with its importer, loader and import settings (`conjure/audio/clip_asset.h`),
the decoders behind them (`conjure/audio/decoder.h`), and the two backends the engine can run on: the backend
interface (`conjure/audio/audio_backend.h`) and the headless `null_backend` that tests and servers use
(`conjure/audio/null_backend.h`). Everything is in `namespace conjure::audio`.

## At a glance

| You want to... | Use |
|---|---|
| Use a sound file in the game | drop a `.wav`, `.ogg`, `.mp3` or `.flac` into the project; it imports as `conjure.audio.clip` |
| Reference it from code or a component | `assets::asset_ref<audio_clip>` |
| Keep a long track out of memory | import setting `"storage": "streamed"` (automatic above 10 s) |
| Make a 3D effect cheaper | import setting `"forceMono": true` |
| Build a clip from samples you generated | `pcm_buffer`, then `make_pcm_clip()` |
| Decode a file in a tool | `decode_to_pcm(bytes, options)`; `probe(bytes)` for just the format |
| Decode incrementally | `streaming_decoder` |
| Run headless and assert on what was rendered | `audio_backend_kind::NULL_BACKEND`, `null_backend::stats()` and `captured()` |

## Quick example

```cpp
#include "conjure/audio/clip.h"
#include "conjure/audio/decoder.h"

#include <memory>
#include <span>

using namespace conjure;
using namespace conjure::audio;

// A tool that turns any supported file into a clip ready for audio_engine::play().
core::result<audio_clip> clip_from_file(std::span<const byte> fileBytes) {
    decode_options options = {};
    options.forceMono = true;                               // what a 3D effect wants
    CONJURE_TRY_ASSIGN(pcm_buffer decoded, decode_to_pcm(fileBytes, options));
    audio_clip clip = make_pcm_clip(std::make_shared<const pcm_buffer>(std::move(decoded)));
    clip.info.encoding = sniff_encoding(fileBytes);         // make_pcm_clip assumes WAV
    return clip;
}
```

## Clips

| Type | What it holds |
|---|---|
| `pcm_buffer` | immutable interleaved `f32` samples with an `audio_format`; `frame_count()`, `samples()`, `duration_seconds()` |
| `clip_info` | `format`, `frameCount`, `encoding`, `storage`; `duration_seconds()`; everything known without the samples |
| `audio_clip` | the asset payload: `info` plus `pcm` (a `shared_ptr<const pcm_buffer>`, PCM storage) or `encoded` (a `shared_ptr<const vector<byte>>`, STREAMED storage); `is_streamed()`, `is_valid()`, `duration_seconds()` |

`make_pcm_clip(pcm, encoding = WAV)` fills `info` from a buffer; `make_fallback_clip()` is the 0.1 s silent mono
clip the asset system serves when a load fails. `CLIP_ASSET_TYPE_NAME` is `"conjure.audio.clip"`.

> [!NOTE]
> Clips share their samples by `shared_ptr`. Every voice playing a clip holds its own reference, so when a hot
> reload replaces the asset the voices that are already playing finish on the old samples and the new ones
> start on the new. You never have to stop sounds before reloading.

## The asset type

Any `.wav`, `.ogg`, `.mp3` or `.flac` under the project imports as a `conjure.audio.clip` through
`audio_clip_importer` (id `CLIP_IMPORTER_ID` = `"conjure.audio.clip"`, version `CLIP_IMPORTER_VERSION` = 1) and
loads through `audio_clip_loader` into an `audio_clip`. The engine registers both at startup with
`register_asset_types(types, importers)`; a standalone tool does the same.

| Storage | What the cooked asset holds | Plays as |
|---|---|---|
| `PCM` | decoded `f32` samples at the source rate (or the `sampleRate` you set) | mixed directly; cheapest per voice |
| `STREAMED` | the original encoded file, byte for byte | decoded 4096 frames at a time while playing, on the job system; at most 32 at once |

The cooked payload is a 32-byte `clip_blob_header` (`CJAC`, version, storage, channels, encoding, sample rate,
frame count, data size) followed by the data. `cook_clip(bytes, settings)` and `load_clip_blob(blob)` are the
pure functions behind the importer and loader, and `read_clip_blob_header(blob)` validates a header; tools can
call them directly, as the example shows.

```cpp
#include "conjure/audio/clip_asset.h"

#include <span>
#include <vector>

using namespace conjure;
using namespace conjure::audio;

// Cook a file the way the importer does and load it back; what a test or an offline tool needs.
core::result<audio_clip> cook_and_load(std::span<const byte> fileBytes, bool stream) {
    clip_import_settings settings = {};
    settings.storage = stream ? clip_storage_setting::STREAMED : clip_storage_setting::PCM;
    CONJURE_TRY_ASSIGN(std::vector<byte> blob, cook_clip(fileBytes, settings));
    return load_clip_blob(blob);
}
```

> [!TIP]
> Leave `storage` on `auto`: effects stay PCM, music and long ambience become streamed at the 10 s threshold.
> Force `pcm` only for a long clip you play many times at once (streamed voices are limited to 32), and
> `streamed` only when memory is tight.

> [!WARNING]
> A `const audio_clip *` from `asset_ref::get()` is valid until the end of the next frame, like every asset
> pointer. Keep the `asset_ref`, or copy the `shared_ptr` out of the clip, never the raw pointer.

### Import settings

The importer reads a JSON object (`clip_import_settings`, round-tripped by `from_json()`, `to_json()` and
`to_document()`); missing keys keep their defaults, bad values fail the import with `INVALID_SETTINGS`.

| Key | Default | Meaning |
|---|---|---|
| `"storage"` | `"auto"` | `"auto"`, `"pcm"` or `"streamed"`; `auto` streams above the threshold |
| `"streamThresholdSeconds"` | `10.0` | the `auto` cut-over |
| `"forceMono"` | `false` | downmix to one channel (PCM only); recommended for 3D effects |
| `"sampleRate"` | `0` | resample at import (PCM only); `0` keeps the source rate |
| `"normalize"` | `false` | scale the peak to 1.0 (PCM only) |

> [!NOTE]
> `forceMono`, `sampleRate` and `normalize` do not apply to streamed clips, which keep the file untouched.
> Cooked PCM keeps the source sample rate unless you set one; the engine resamples while playing.

## Decoding

| Format | Extension | Notes |
|---|---|---|
| WAV | `.wav` | 16-bit and 32-bit float PCM; the only format whose decode is bit-identical across platforms |
| Ogg Vorbis | `.ogg` | the container reports no length, so `probe()` counts frames once |
| MP3 | `.mp3` | decode may differ by a few least-significant bits between builds |
| FLAC | `.flac` | lossless |

| Function | Purpose |
|---|---|
| `clip_encoding sniff_encoding(bytes)` | which container the magic bytes say; `UNKNOWN` otherwise |
| `result<clip_info> probe(bytes)` | format and length without decoding everything (`storage` is reported as `STREAMED`) |
| `result<pcm_buffer> decode_to_pcm(bytes, decode_options)` | the whole file as `f32`; `decode_options { forceMono, sampleRate, normalize }` apply in that order |
| `streaming_decoder` | `open(bytes)` or `open(bytes, knownFrameCount)`, `read(span<f32>)` returns frames written (0 at the end), `seek(frame)`, `position()`, `info()`, `close()` |

Errors: `INVALID_CLIP` (empty input), `UNSUPPORTED_FORMAT` (unknown magic), `DECODE_FAILED` (the decoder refused
the data; the message names the reason). Random bytes never assert. Decoding is the same on every backend and
callable from any thread; one `streaming_decoder` is used by one thread at a time.

> [!WARNING]
> `streaming_decoder` reads the bytes you passed to `open()` on every `read()`: keep them alive until `close()`.
> Opening an Ogg Vorbis stream without a known length decodes the whole file once to count it; pass
> `info.frameCount` from a probed clip to skip that.

## Backends

A backend (`audio_backend`) owns the device, or a virtual clock, and drives the mixer callback. `audio_engine`
picks one from `audio_settings::backend`:

| `audio_backend_kind` | What it does |
|---|---|
| `AUTO` | `MINIAUDIO` when compiled in and a device opens, otherwise `NULL_BACKEND`; the engine never fails to construct |
| `MINIAUDIO` | the real device through miniaudio: a device thread calls the mixer, the OS converts formats |
| `NULL_BACKEND` | no device, no thread, no OS calls; rendering happens inside `update()` on the main thread |

`is_backend_compiled(kind)` says whether a kind exists in this build and `create_backend(kind)` makes a stopped
one (`UNSUPPORTED` when it is not compiled in); most code never calls either and lets the engine choose. The
interface itself (`start(backend_desc)`, `stop()`, `state()`, `output_format()`, `enumerate_devices()`,
`update(dt)`) matters only if you write a backend; `backend_desc` names the mix format, period, device and the
`mix_callback` to drive.

## The null backend for tests

`null_backend` renders the real mixer on the main thread for `round(dt * sampleRate)` frames every `update(dt)`
(the fractional remainder carries over, so 60 updates of 1/60 s make exactly 48000 frames). Cursors, loops,
ramps, group gains and clamping behave exactly as on hardware, deterministically, and decoding is identical.
After each update the engine writes a snapshot into `stats()`:

| `null_backend_stats` field | Meaning |
|---|---|
| `virtualTimeSeconds`, `framesRendered`, `renderCalls` | the virtual clock |
| `activeVoices`, `underruns`, `commandsApplied` | mixer counters |
| `listener` | the `listener_state` in effect |
| `voices[MAX_VOICES]` | per mixer slot: `state`, `cursorFrames`, `gain`, `pan`, `step`, `loop`, `groupIndex` |
| `groups` | per group: `id`, `name`, `gain`, `effectiveGain`, `paused` |

`set_capture_frames(n)` keeps the last `n` rendered stereo frames in `captured()` so a test can assert on
sample values; `virtual_time()` is the seconds rendered since `start()`. `simulate_device_loss()` and
`fail_next_starts(count)` exercise the engine's device-loss recovery without hardware.

```cpp
#include "conjure/audio/audio_engine.h"
#include "conjure/audio/null_backend.h"

#include <memory>
#include <vector>

using namespace conjure;
using namespace conjure::audio;

// Headless: a 0.1 s clip ends after six 1/60 s frames and renders at the volume asked for.
bool one_shot_ends_on_time() {
    audio_settings settings = {};
    settings.backend = audio_backend_kind::NULL_BACKEND;
    audio_engine engine(settings);
    null_backend &backend = *engine.null_backend_instance();

    audio_format format = {};                               // 48 kHz mono
    format.channels = 1;
    const std::shared_ptr<const pcm_buffer> pcm =
      std::make_shared<const pcm_buffer>(format, std::vector<f32>(4800, 0.5f));
    play_params params = {};
    params.volume = 0.5f;
    const core::result<voice_id> voice = engine.play(pcm, params);
    if (!voice.has_value()) {
        return false;
    }

    backend.set_capture_frames(800);                        // keep the last frame's samples
    engine.update(1.0f / 60.0f);
    engine.end_frame();
    const bool gainApplied = backend.stats().voices[0].gain == 0.5f;

    u32 frames = 1;
    while (engine.is_playing(voice.value()) && frames < 20) {
        engine.update(1.0f / 60.0f);
        engine.end_frame();
        ++frames;
    }
    return gainApplied && frames == 6 && !engine.is_valid(voice.value());   // one-shot: id is stale now
}
```

> [!TIP]
> Write gameplay tests against the null backend and read `engine.null_backend_instance()->stats()` instead of
> listening: it is what the engine's headless test fixture exposes as `audio_stats()`. Pass
> `audio_settings::jobs = nullptr` and streamed clips decode inline, so no job system is needed either.

> [!WARNING]
> `null_backend_instance()` is `nullptr` on the miniaudio backend. Request `NULL_BACKEND` explicitly in tests
> rather than relying on `AUTO`, which uses the real device on a developer machine.

## API summary

| Signature (short) | Purpose |
|---|---|
| `pcm_buffer(audio_format, vector<f32> samples)`; `format()`, `frame_count()`, `samples()`, `duration_seconds()` | immutable samples |
| `struct clip_info { audio_format format; u64 frameCount; clip_encoding encoding; clip_storage storage; }` | metadata |
| `struct audio_clip { clip_info info; shared_ptr<const pcm_buffer> pcm; shared_ptr<const vector<byte>> encoded; }` with `is_streamed()`, `is_valid()`, `duration_seconds()` | the asset payload |
| `audio_clip make_pcm_clip(shared_ptr<const pcm_buffer>, clip_encoding = WAV)` / `audio_clip make_fallback_clip()` | construction helpers |
| `struct clip_import_settings { clip_storage_setting storage; f64 streamThresholdSeconds; bool forceMono; u32 sampleRate; bool normalize; }` with `from_json()`, `to_json()`, `to_document()` | importer settings |
| `result<vector<byte>> cook_clip(span<const byte>, clip_import_settings)` / `result<audio_clip> load_clip_blob(span<const byte>)` / `result<clip_blob_header> read_clip_blob_header(span<const byte>)` | the import and load logic |
| `bool is_clip_extension(string_view)`, `CLIP_EXTENSIONS`, `CLIP_IMPORTER_ID`, `CLIP_IMPORTER_VERSION`, `CLIP_BLOB_VERSION` | identity |
| `class audio_clip_importer : assets::asset_importer` / `class audio_clip_loader : assets::typed_asset_loader<audio_clip, audio_clip>` | the asset adapters |
| `result<void> register_asset_types(assets::asset_type_registry &, assets::importer_registry &)` | register type, loader and importer |
| `clip_encoding sniff_encoding(span<const byte>)` / `result<clip_info> probe(span<const byte>)` / `result<pcm_buffer> decode_to_pcm(span<const byte>, decode_options)` | decoding |
| `streaming_decoder`: `open(bytes)`, `open(bytes, u64 knownFrameCount)`, `usize read(span<f32>)`, `result<void> seek(u64)`, `info()`, `position()`, `is_open()`, `close()` | incremental decoding |
| `class audio_backend`: `kind()`, `start(backend_desc)`, `stop()`, `state()`, `output_format()`, `enumerate_devices()`, `update(f32)` | the backend interface |
| `bool is_backend_compiled(audio_backend_kind)` / `result<unique_ptr<audio_backend>> create_backend(audio_backend_kind)` | backend factory |
| `class null_backend final : audio_backend`: `stats()`, `set_capture_frames(u32)`, `captured()`, `virtual_time()`, `render_frames(u32)`, `simulate_device_loss()`, `fail_next_starts(u32)` | headless backend |

## See also

- 📘 API: [Playing sounds](playing-sounds.md) for `audio_engine`, `play_params` and groups
- 📘 API: [Audio](index.md) for the components
- 📘 API: [Errors and results](../core/errors-and-results.md) for `result<T>`, `CONJURE_TRY_ASSIGN`
- 📘 API: [JSON](../core/json.md) for the import settings document
- 🛠 Handbook: [Audio module](../../handbook/modules/audio.md)
