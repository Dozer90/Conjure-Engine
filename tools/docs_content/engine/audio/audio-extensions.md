Implement [`audio_backend`](/reference/handbook/engine/audio/audio-backend.html) to supply another playback driver.
Keep device handling behind that interface and preserve the callback's format and threading contract.

> [!CAUTION]
> A custom backend must stop its callback thread before the engine state is destroyed. Calling the callback
> with a dangling user pointer has undefined behaviour.

Clip import and loading integrate with the asset database through the audio clip asset type. Scene integration
uses [`audio_source`](/reference/handbook/engine/audio/components.html) and audio listeners, while the engine's
audio subsystem owns the ECS scheduling. Applications can provide a job system through
[`audio_settings`](audio/audio-engine/audio-settings.md) to move streaming decode work off the main thread.
