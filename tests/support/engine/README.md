# conjure_test_support_engine

The engine-level half of the test support library (`docs/design/08_testing.md` D2, 2.3, 3.3, 4.4-4.6, 11):
the fixtures that unit, integration and benchmark code share once `conjure::engine` exists. Namespace
`conjure::testing`, headers under `include/conjure/testing/`, never installed or shipped. Test modules link
it with `conjure_add_test_module(<module> ... ENGINE)` (`cmake/ConjureTesting.cmake`), which picks
`conjure::test_support_engine` or, with `LINK_ENGINE_SHARED`, `conjure::test_support_engine_shared`.

## Main types

| Header | Type | Purpose |
|---|---|---|
| `headless_engine.h` | `headless_engine`, `headless_engine_options` | An `engine::application` on the null window, null rhi device and null audio backend, stepped with an explicit delta (`step_frames`, `step_seconds`); scene loading by uuid, vfs path, absolute path or inline JSON; `world()`, `scenes()`, `active_scene()`, `rhi_stats()`, `renderer_enabled()`, `audio_stats()` (with `CONJURE_WITH_AUDIO`), `checks()`, `world_hash()`, `find_entity()`. Owns a `log_capture` and a `memory_guard` (D6). |
| `input_script.h` | `input_script` | Key, mouse, text and gamepad events scheduled on frame indices; scheduling keeps the view sorted so every reader is allocation-free and `noexcept`. `headless_engine::step_frames` pushes each frame's events into the null window before the frame runs. Frames are absolute (`frame_index() + n` after stepping); late scheduling, for a frame that already ran, fails the case at the next `step_frames`. |
| `world_builder.h` | `spawn_many`, `spawn_hierarchy`, `hierarchy_shape`, `spawn_box_body`, `spawn_script_entity`, `mark_dirty_fraction` | Bulk construction for tests and benchmarks: one-archetype spawns through a command buffer, the 10,000-entity hierarchy (padded or trimmed at its last level), physics bodies, scripted entities, deterministic `transform_dirty` tagging. |
| `protocol_client.h` | `protocol_client`, `protocol_channel`, `scripted_protocol_channel` | The newline-delimited JSON protocol of `04_editor.md` 2.3 from the client side: `send` matches the echoed id and buffers events, `send_raw` proves garbage yields an error document. The channel is the client end of a connection; the scripted channel answers from a handler (which may itself send through the channel). |
| `golden_image.h` | `golden_image`, `golden_options`, `golden_result` | Reference comparison with channel tolerance, mismatch fraction and mean absolute error; `actual.png` / `diff.png` on failure (an alpha-only difference shows grey in the diff); `--conjure-update-golden` writes the reference when it is missing, unreadable or out of date. `render_offscreen` is gated on `CONJURE_TEST_GOLDEN_IMAGES` and, like `render_headless`, is **not functional yet**: it fails with `UNSUPPORTED` until the engine can point a view at an off-screen target headlessly (see below). |
| `log_category.h` | `LOG_TESTING` | The library's log category ("testing"). |

## Threading model

Everything is main-thread only and one `headless_engine` lives at a time (the platform allows one live
window per process, so a second fixture asserts): the fixture drives `application::step_frame()` from the
calling thread, the application's job workers are joined when the fixture destroys it, and the protocol
client runs its channel synchronously. `log_capture` and the allocation counters are the only members job
threads touch.

## Decisions worth knowing

- **Memory baseline after a warm-up.** The first engine start in a process fills permanent caches
  (interned names, reflection registry, component type caches). `headless_engine` therefore starts and
  destroys one throwaway application per distinct configuration before taking its memory baseline;
  everything else must be freed when the application is destroyed. `checkMemory = false` turns the check
  off for helpers whose output outlives the fixture (a fixture used only to author a scene file); the real
  .NET host turns it off too, since the CLR never releases its heap. The first successful native file read
  of a process also pays about a thousand allocations that live until exit, so the first fixture reads one
  scratch file before its baseline.
- **Interned names are budgeted by a measured cost, never a guess.** Names interned during a run (a loaded
  scene's descriptive entity names, a system a test registers) stay in the process-wide `string_id` table
  by design. What one interning costs in live allocations differs between standard libraries and
  configurations (MSVC Debug: three for a name beyond the 15-byte small-string buffer, two inside it;
  release: two and one), so the first fixture of a process interns one short and one long sentinel name
  under a snapshot and keeps the larger cost (`headless_engine::allocations_per_interned_name()`). The
  check then tolerates `newNames * cost`; its failure text names the delta, the number of new names and the
  budget. The remaining imprecision is a short name in a release build, whose budget is one block wider
  than its cost; an exact `string_id` table allocation counter from core would close it.
- **One failure per failure.** A failed doctest assertion keeps a record alive, which the memory check
  would count as a leak. The fixture therefore disarms its `memory_guard` (fresh baseline plus an immediate
  passing check) before every `REQUIRE` it raises from the constructor, turns the memory check off when a
  frame fails or late input is reported, and reports an unallowed `ERROR` itself, once, after the memory
  check (the `log_capture`'s own destructor check is suppressed and the first offending text is kept in
  fixed storage across the check). `conjure_support_engine_child` proves each case shows exactly one failed
  assertion.
- **Trivially copyable options.** A fixture is usually built from a temporary options object that the
  caller destroys after the constructor took the baseline; a heap-owning member (even an empty
  `std::filesystem::path` allocates its iterator proxy in Debug) would read as a negative delta. The
  project directory is therefore a non-owning pointer to a path that outlives the fixture
  (`&scratch.path()`), and the struct is `static_assert`ed trivially copyable.
- **Worker count.** `jobThreadCount` defaults to `environment().jobThreadCount` and is passed to
  `jobs_settings::workerCount` as given: 0 is the engine's inline job mode (every job runs on the calling
  thread, `08_testing.md` D4), which `tests/integration/test_determinism.cpp` uses as its baseline.
- **Renderer on the null device.** `enableRenderer` needs the shader index `conjure_shaders` writes
  (`CONJURE_TEST_SHADER_DIR`); without the shader target the renderer stays off, `rhi_stats()` is zero,
  `renderer_enabled()` is false and construction logs a WARN on `LOG_TESTING` saying why.
- **Off-screen rendering is not functional yet.** A headless `engine::application` has no swapchain, and
  `renderer_subsystem` submits camera views only to the swapchain (`engine/src/engine/subsystems/
  renderer_subsystem.cpp`), so a render target created by `golden_image::render_headless` would be read
  back untouched (on WARP or lavapipe a golden comparison would then compare an untouched texture). The
  function fails with `UNSUPPORTED` instead; it needs an engine hook such as
  `renderer_subsystem::set_headless_target(render_target_id)` or `engine_config.rendering.headlessTarget`
  that points the extracted views at an off-screen target when no swapchain exists.
- **The real .NET host.** `enableScripting` without `useNullScriptHost` starts the CLR with the managed
  SDK this tree built (`CONJURE_TEST_MANAGED_DIR`, from `conjure_managed_sdk`); the `DOTNET_RUNTIME`
  precondition probe this library installs holds only when hostfxr is found *and* `Conjure.Managed.dll` is
  there, so `CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME)` skips cleanly on a machine without the SDK.
- **Inline scenes.** `load_scene_json` writes the text into the fixture's own in-memory file system and
  loads it through `scene_manager::load_file`, so the scene has a record, an id and can be unloaded.
- **Names without interning.** `find_entity` hashes its argument (`string_id::from_hash`) and
  `spawn_hierarchy` / `spawn_box_body` / `spawn_script_entity` use hashed ids, so building worlds inside
  the fixture's memory guard does not grow the process-wide name table.
- **Protocol client transport.** The client speaks through the `protocol_channel` interface. `editor_core`
  exists (wave 4, `conjure/editor/protocol/loopback_transport.h`), but the in-process and TCP factories of
  the design (`connect_in_process` over `editor::loopback_transport`, and `connect_tcp`) and the design-11
  cases "tcp ping on ephemeral port" and "closed port returns error" are still owed
  (`docs/design/09_cross_area_resolutions.md` 4 item 15(e)). The `LOOPBACK_TCP` precondition probe is
  installed by the editor tests (`tests/editor/editor_test_support.cpp`), not by this library.
  `scripted_protocol_channel::send_line` collects a handler's lines in a list local to the call, so a
  handler that forwards through the same channel loses nothing.
- **Hierarchy plans never overflow.** `spawn_hierarchy` and `hierarchy_entity_count` share one plan in
  fixed storage (at most `MAX_HIERARCHY_DEPTH` levels): per-level arithmetic saturates, a depth above 0
  with zero children per node is unusable (it used to predict more entities than were spawned), and a plan
  beyond a `u32` of entities is unusable too, so the two functions always agree.
- **Public headers are checked alone.** The library registers its headers in `CONJURE_PUBLIC_HEADERS` and
  calls `conjure_header_check(... INCLUDE_ROOT include/)`, the same standalone compile every engine module
  gets.

## How to test

```
python tools/build.py --build-dir build/<dir> --target conjure_support_engine_tests --test -R support_engine
```

The self-tests live in `tests/support/tests/engine/` (one `test_<header>.cpp` per public header). The golden
update-mode cases re-run the executable with `--conjure-update-golden` on the reference cases (missing, out of
date, resized, unreadable, passing), which is why the test target receives its own path as
`CONJURE_SUPPORT_ENGINE_TESTS_PATH`; each reference case therefore has an update-mode half, and the whole
executable also passes when run in update mode. The fixture-failure cases run `conjure_support_engine_child`
(`support_engine_child.cpp`, never registered with CTest), whose cases fail on purpose from a constructor, a
destructor or `step_frames`; the parent checks the exit code, the failure text and that exactly one assertion
failed. The `[scripting]` case that starts the real .NET host skips (exit 77) when
`precondition::DOTNET_RUNTIME` does not hold; the probe case checks that the precondition holds exactly when
hostfxr and the managed SDK (`CONJURE_SUPPORT_ENGINE_TEST_MANAGED_DIR`) are present. To prove the
`CONJURE_WITH_AUDIO=OFF` guards, configure a second directory with `-DCONJURE_WITH_AUDIO=OFF` and build the
same target.
