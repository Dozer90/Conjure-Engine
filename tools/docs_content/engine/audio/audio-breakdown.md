### Frame coordination

The [`audio_engine`](audio/audio-engine/audio-engine.md) owns the backend and keeps the main-thread voice and
group state separate from the mixer. Its private implementation lives in `engine/src/audio/audio_engine.cpp`.
The application's audio subsystem connects it to scene components; the audio library itself does not depend on ECS.

Call `update()` once per frame to advance the backend, drain events, reconcile voice state, compute spatial
parameters, and service streaming. `end_frame()` releases completed one-shots and publishes the finished-voice
list. The following helper is a complete, compilable example for an already-created engine; call it on the main thread.

```cpp
#include "conjure/audio/audio_engine.h"

void advance_audio_frame(conjure::audio::audio_engine &audio, conjure::f32 deltaSeconds) {
    audio.update(deltaSeconds);
    audio.end_frame();
}
```

### One mixer, two drivers

`engine/src/audio/mixer.cpp` renders interleaved floating-point samples without allocating, locking, or logging
inside the callback. The miniaudio backend drives the callback from its device thread.

> [!NOTE]
> The [`null_backend`](/reference/handbook/engine/audio/null-backend.html) drives the same mixer on the main
> thread using a virtual clock, so headless tests can inspect the output without opening a real device.

### Memory and voice lifetime

Voice handles pair an index with a generation, so a reused slot does not make an old handle valid again.
The main thread owns a table of 512 voice entries; the mixer has 256 voice slots. The additional entries let a
new voice retain its own identity while it waits for a stolen slot to finish fading out.

One-shots are reclaimed by `end_frame()` after the mixer acknowledges completion. Retained voices stay owned
until `release()` is called. Per-entry atomic state and seqlock snapshots support the documented read-only queries.

### Commands, events, and recovery

A single-producer/single-consumer ring sends commands to the mixer. When that ring fills, commands spill into
a main-thread vector and are flushed later. Mixer events travel through a second ring; if it overflows, the
main thread reconciles terminal states from per-slot atomics. Both directions carry generations to reject stale work.

Device loss freezes progress while the engine retries the device using the configured backoff and attempt count.
If recovery cannot restore it, playback continues on the null backend. Counters and the mirrored device state are
available through [`audio_engine_stats`](audio/audio-engine/audio-engine-stats.md).

### Gain, spatialisation, and streaming

The main thread combines voice volume, attenuation, and listener gain. The mixer applies the group's effective
gain and ramps changes to avoid discontinuities. Mono spatial sources use constant-power panning; pitch combines
the clip-to-mix sample-rate ratio, requested pitch, and Doppler ratio.

Streaming voices decode into bounded SPSC rings. An optional job system performs decode work; without it,
`update()` performs that work inline. Seeking coordinates the decoder and mixer through pause, reset, and prime
states so a ring is not reset while either side is using it.
