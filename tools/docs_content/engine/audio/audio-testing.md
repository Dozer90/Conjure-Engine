`tests/audio/` exercises construction, settings validation, voice and group lifetimes, spatialisation, decoding,
streaming, device recovery, and the null backend. Most cases are headless; real-device checks are separately gated.

> [!TIP]
> For this header, start with `tests/audio/test_audio_engine.cpp`. It covers backend selection, invalid settings,
> playback state, bulk updates, and diagnostics.

Structs without a dedicated test file are also exercised by the engine and integration suites; do not infer a
test file solely from a type name.

```bash
python tools/build.py --target conjure_audio_tests --test -L audio
```

