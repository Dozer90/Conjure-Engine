> [!IMPORTANT]
> The [`audio_engine`](audio/audio-engine/audio-engine.md) is main-thread owned. Construction, destruction,
> mutators, device selection, group changes, `update()`, and `end_frame()` must run there.

`is_valid()`, `is_playing()`, `voice()`, `stats()`, and `output_format()` are the documented read-only exceptions.

The real device callback runs on the miniaudio thread. The null backend invokes the same callback on the main
thread. Streaming decode jobs finish before their state is released; only one job may produce into a stream's
ring at a time.

> [!WARNING]
> Callback work must not allocate, acquire locks, or log. The real device callback runs independently of the
> main thread, so main-thread-only operations must not be called from it.

Group pause changes are sent as individual commands. A subtree is therefore not paused atomically across
different groups; voices requiring sample synchronisation should share a group or use the whole-mixer pause.
