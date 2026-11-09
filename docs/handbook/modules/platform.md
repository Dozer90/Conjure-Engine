# Platform module

🛠 **Engineering handbook** · 📘 API: [Platform: windows, input and events](../../api/platform/index.md)

`conjure::platform` (static library `conjure::platform`, wave 2) owns the process's one window, the flat event
stream it produces once per frame and the backend-independent `input` accumulator that turns that stream into the
per-frame `input_state` snapshot. Everything above it (the swapchain, ImGui, the scripting frame block, the editor's
`input.inject`, the test harness) consumes those three types and nothing else. This chapter is for engineers who
change the backends, the input state machine or the tests; users read the wiki pages. The binding design is
[`../../design/11_platform.md`](../../design/11_platform.md); where it and the code differ, the code wins and the
difference is noted here.

## Contents

- [1. Shape and position](#1-shape-and-position)
- [2. Public headers](#2-public-headers)
- [3. The GLFW backend](#3-the-glfw-backend)
- [4. Event double buffering and resize coalescing](#4-event-double-buffering-and-resize-coalescing)
- [5. The input state machine](#5-the-input-state-machine)
- [6. The null backend](#6-the-null-backend)
- [7. Threading and ownership](#7-threading-and-ownership)
- [8. Extension points](#8-extension-points)
- [9. Failure handling](#9-failure-handling)
- [10. Tests](#10-tests)
- [11. Open items](#11-open-items)
- [Related](#related)

## 1. Shape and position

The module depends on `conjure::core` only (`result`, logging, asserts, `enum_flags.h`, math, `jobs::is_main_thread`,
`str::iequals`, `platform_detect.h`) and sits in wave 2 beside `ecs`, `rhi`, `assets`, `audio` and `modules`; `rhi`
is its only engine consumer today (`native_handle()`, the Vulkan helpers, `framebuffer_size()`). GLFW 3.4
(`conjure::thirdparty::glfw`, pinned in `third_party/CMakeLists.txt`) is a *private* dependency of the glfw backend
and never appears in a public header.

| File | Role |
|---|---|
| `engine/src/platform/platform.cpp` | `LOG_PLATFORM`, `window` base (live-window count), `validate_window_desc`, `create_window`, backend queries, `backend_version_string` |
| `enum_names.cpp`, `platform_error.cpp`, `event.cpp`, `input_state.cpp` | every `to_string` / `parse_*`, the error category, event conversions and formatting, the layout `static_assert`s |
| `input.cpp` | the state machine of section 5 |
| `null_backend.cpp` | the virtual window of section 6 |
| `glfw/glfw_context.{h,cpp}` | process-wide GLFW state: init reference count, error callback and last error, joystick and monitor callback dispatch |
| `glfw/glfw_window.{h,cpp}` | the `GLFWwindow`, its callbacks, the pump, the gamepad poll, monitors, cursor and clipboard |
| `glfw/glfw_native.{h,cpp}` | the only file that includes `glfw3native.h`; one `CONJURE_PLATFORM_*` block per OS |
| `glfw/glfw_vulkan.cpp` | `create_surface` with locally declared Vulkan handle types (no SDK dependency) |

`CONJURE_PLATFORM_GLFW` (default ON) adds the `glfw/` sources and the GLFW dependency and defines
`CONJURE_PLATFORM_HAS_GLFW=1` privately; OFF yields a null-only library for server builds, in which
`create_window(GLFW)` returns `BACKEND_NOT_COMPILED`, `is_backend_compiled(GLFW)` is false,
`default_backend_kind()` is `NULL_BACKEND` and `backend_version_string(GLFW)` is `"glfw (not compiled)"`. The
editor requires the option ON (checked in the root `CMakeLists.txt`).

## 2. Public headers

| Header (`conjure/platform/`) | Contents | Wiki page |
|---|---|---|
| `window.h` | `window_flags`, `display_server_hint`, `cursor_mode` (+ `to_string`, `parse_cursor_mode`), `MAX_WINDOW_TITLE_BYTES`, `window_desc`, abstract `window`, `validate_window_desc`, `create_window`, `live_window_count` | Windows and events |
| `null_backend.h` | `null_backend final : window`, `create()`, the `simulate_*` controls, `push_event`, `pending_event_count`, `cursor_position`, `raw_mouse_motion` | Windows and events, Input |
| `backend.h` | `backend_kind`, `to_string`, `parse_backend_kind`, `is_backend_compiled`, `default_backend_kind` | Windows and events |
| `event.h` | the fifteen event structs, `input_event` (nine alternatives), `event` (same nine first, then six window alternatives), `INPUT_EVENT_COUNT`, `EVENT_COUNT`, `is_input_event`, `to_input_event`, `to_event`, `event_as<T>`, `event_name`, `to_string` | Windows and events |
| `monitor.h`, `native_handle.h` | `video_mode`, `monitor_info`; `native_handle_kind`, `native_window_handle` | Windows and events |
| `input.h` | `input_stats`, `input` (`apply`, three `inject` overloads, `end_frame`, `snapshot`, `release_all`, `skip_next_mouse_delta`, `stats`) | Input |
| `input_state.h` | `key_bitset`, `gamepad_state`, `MAX_TEXT_CODEPOINTS_PER_FRAME`, `input_state` with its query helpers | Input |
| `key.h`, `mouse_button.h`, `gamepad.h`, `button_action.h` | GLFW-numbered enumerations, counts, `is_valid_*`, `to_string`, `parse_*`; `key_modifier_flags` with `to_string` / `parse_key_modifier_flags` | Input |
| `platform_error.h`, `log_category.h`, `platform.h` | `platform_error` bound to category `"platform"`; `LOG_PLATFORM`; umbrella plus `backend_version_string` | overview |

Since the design was written the code gained `validate_window_desc`, `live_window_count`, `event_name`,
`INPUT_EVENT_COUNT` / `EVENT_COUNT`, `is_valid_mouse_button` / `is_valid_gamepad_*`, `to_string` /
`parse_cursor_mode`, `to_string` / `parse_key_modifier_flags`, `null_backend::cursor_position` /
`raw_mouse_motion` and the public `null_backend` constructor; `is_valid_key` exists for both `u32` and `key`.

## 3. The GLFW backend

### 3.1 Process-wide lifetime (`glfw_context`)

GLFW is a process-global library, so `glfw_context` keeps a reference count of live glfw windows. The first
`acquire()` installs the error callback *before* `glfwInit()`, applies the init hints (`GLFW_PLATFORM` from
`window_desc::displayServer` on Linux, `GLFW_ANY_PLATFORM` elsewhere; `GLFW_COCOA_CHDIR_RESOURCES = GLFW_FALSE` so
macOS keeps the working directory) and calls `glfwInit()`; failure is `INIT_FAILED` quoting the last GLFW error and
takes no reference. The last `release()` removes the joystick and monitor callbacks and calls `glfwTerminate()`;
GLFW documents re-initialisation after terminate, so tests that create and destroy windows repeatedly are correct.
`glfw_window::create` acquires before constructing the object; the destructor releases, so a failed `initialise`
also releases through the destructor of the partially built window.

The error callback logs every GLFW error as `CONJURE_LOG_ERROR(LOG_PLATFORM, "GLFW <name> (<code>): <text>")` and
records code, text and a summary; `clear_last_error()` / `last_error_code()` bracket a GLFW call whose failure is
reported only through the callback (`glfwSetInputMode`, the clipboard). The joystick and monitor callbacks are
process-wide and are forwarded to the single active window (`set_active_window`).

### 3.2 Window creation (`glfw_window::initialise`)

1. Reserve both halves of every double buffer (`mPending` / `mDelivered` 256 events, drop paths and ranges 8) so the
   steady state never allocates.
2. Read `glfwGetPlatform()`: on Wayland, `mPositionSupported` and `mCursorWarpSupported` are false and the
   corresponding GLFW calls are skipped rather than attempted (each would log an ERROR line).
3. Window hints: `GLFW_CLIENT_API = GLFW_NO_API` (NVRHI owns the graphics API), the `window_flags` bits,
   `GLFW_FOCUS_ON_SHOW`, `GLFW_POSITION_X/Y` when `position` is set and supported.
4. Fullscreen: resolve `monitorIndex` against `glfwGetMonitors` (`MONITOR_NOT_FOUND`), take the monitor's current
   video mode as the size, remember the requested size as the windowed size to restore.
5. `glfwCreateWindow` (`WINDOW_CREATE_FAILED` with the last error), user pointer, the thirteen callbacks.
6. Record the initial position and windowed size, `GLFW_ICONIFIED`, apply `gamepadMappings`
   (`glfwUpdateGamepadMappings`, WARN when rejected), query `glfwGetRequiredInstanceExtensions` only when
   `glfwVulkanSupported()` (the query itself errors otherwise), become the active window, scan joysticks
   `GLFW_JOYSTICK_1..LAST` and queue a `gamepad_connection_event` per gamepad-mapped one, build the monitor list.

### 3.3 Gamepad poll

GLFW has no gamepad button events, so each pump calls `glfwGetGamepadState` for every assigned slot and diffs against
the previous raw state: a changed button emits a `gamepad_button_event` (`PRESS` / `RELEASE`), a changed axis a
`gamepad_axis_event` with triggers remapped from `[-1, 1]` to `[0, 1]`. A freshly assigned slot starts from
`rest_state()` (triggers at -1) so connection produces no spurious axis events while buttons already held still
produce their press edges. Joysticks without an SDL mapping are ignored; a fifth gamepad logs a WARN. No dead zone
is applied (P11): it would make null and glfw input differ.

### 3.4 Monitors, high-DPI and fullscreen

The monitor list is rebuilt only when the monitor callback marks it dirty (and once at creation): `glfwGetMonitors`,
primary flag, position, current mode, content scale, work area and the full mode list per monitor. `window_size()`,
`framebuffer_size()` and `content_scale()` are live GLFW queries; `SCALE_TO_MONITOR` and `SCALE_FRAMEBUFFER` (3.4)
make logical and pixel sizes behave identically on Win32, X11, Wayland and Cocoa. Fullscreen is windowed fullscreen
(`glfwSetWindowMonitor` with the current mode); leaving it restores `mWindowedSize`, which is the last size the OS
*confirmed* through the window size callback while windowed, not the last one requested (the OS may clamp). The
`mFullscreen` flag is flipped before `glfwSetWindowMonitor` so the size callback it raises synchronously knows
whether to record a windowed size.

### 3.5 Optional features

`set_cursor_mode` brackets `glfwSetInputMode` with the last-error helpers: `GLFW_FEATURE_UNAVAILABLE` /
`GLFW_FEATURE_UNIMPLEMENTED` become `FEATURE_UNAVAILABLE` with one WARN per feature (`mFeatureWarned`) and the
stored mode unchanged; any other GLFW error is `core::error_code::BACKEND_FAILURE`. Raw mouse motion checks
`glfwRawMouseMotionSupported` first; window positioning and cursor warping return early on Wayland.

### 3.6 Native handle and Vulkan surface

`glfw_native.cpp` exposes exactly the native API of the build's OS (`GLFW_EXPOSE_NATIVE_WIN32` / `COCOA` /
`X11` + `WAYLAND`) and fills `native_window_handle` (`HWND` + `HINSTANCE`; `NSWindow *`; X11 window id + `Display *`
or `wl_surface *` + `wl_display *`, chosen at run time on Linux). `glfw_vulkan.cpp` declares the 64-bit handle
shapes `VkInstance`, `VkSurfaceKHR`, `VkResult` and `PFN_vkGetInstanceProcAddr` locally and defines `VK_VERSION_1_0`
so `glfw3.h` declares `glfwCreateWindowSurface`; a `static_assert` pins 64-bit pointers. Argument checks
(`INVALID_ARGUMENT` for a null `surface` or `instance`, `*surface` cleared first) come before `glfwVulkanSupported`
(`VULKAN_UNSUPPORTED`) and the call (`SURFACE_CREATE_FAILED` with the `VkResult`); the null backend performs the
same checks in the same order so the rhi sees one contract.

> [!NOTE]
> The local Vulkan declarations are an ODR caveat once a real `vulkan_core.h` is compiled into the same program
> (two definitions of `::VkResult`, harmless in practice: both are int-sized and only `VK_SUCCESS` is compared).
> They are to be replaced by a pinned header-only Vulkan-Headers dependency with `VK_NO_PROTOTYPES`.

## 4. Event double buffering and resize coalescing

Both backends hold a *pending* and a *delivered* event vector plus matching drop-path and drop-range vectors.
Callbacks (glfw) and simulation controls (null) append to pending; `pump_events()` clears the delivered buffers,
runs `glfwPollEvents()` and the gamepad poll (glfw only), appends the coalesced resize, swaps pending into
delivered and returns a span over it. Pending is *never* cleared at pump start: on Win32 and Cocoa GLFW raises the
resize, focus and iconify callbacks synchronously inside `glfwSetWindowSize`, `glfwSetWindowMonitor`,
`glfwShowWindow` and `glfwHideWindow`, i.e. between two pumps, and those events must survive until the next pump
(regression `platform-review-1`). The previous batch dies with the clear, which is why the span and
`drop_files_event::paths` are documented as valid until the next pump.

Resizes are coalesced (P10): the framebuffer size callback only records `mPendingResize`; the pump appends exactly
one `framebuffer_resized_event` with the final size as the *last* element and sets `mMinimised` from a 0 x 0
size. Drop paths are recorded as `(offset, count)` ranges while pending because the path vector may still grow;
the spans are patched after the swap (`patch_drop_spans`). In the steady state nothing allocates: vectors keep
their capacity and only dropped file paths allocate (asserted by the null backend test and the benchmark).

## 5. The input state machine

`apply(event)` narrows to `input_event` and calls `apply_input`; `inject(input_event)` calls the same function after
counting itself, so the two can never diverge (P3). Window events return false from `apply` and are ignored except
`focus_event{false}`, which calls `release_all()`.

| Event | Rule |
|---|---|
| `key_event` | `modifiers` always updated; invalid code (0 or > `key::LAST`) counted and dropped; `REPEAT` no-op; `PRESS` while up sets `down` + `pressed`; `RELEASE` while down clears `down`, sets `released`; anything else counted in `ignoredTransitions` |
| `mouse_button_event` | the same rule on the `u32` masks through `apply_mask_transition`; invalid button counted |
| `mouse_move_event` | `mouseDelta += position - mousePosition` unless `mSkipNextMouseDelta` (set at construction, on `cursor_enter_event{true}` and by `skip_next_mouse_delta()`), then `mousePosition = position` |
| `mouse_scroll_event` / `cursor_enter_event` | `scrollDelta += offset`; `cursorInWindow = entered` |
| `text_event` | appended while `textCount < 16`, else `textCodepointsDropped++` |
| `gamepad_connection_event` | connect resets the slot to a clean `gamepad_state` with `connected = true`; disconnect releases held buttons (edges), zeroes axes, clears `connected` |
| `gamepad_button_event` / `gamepad_axis_event` | mask rule / level assignment; invalid button or axis counted |
| any gamepad event with `gamepadIndex >= MAX_GAMEPADS` | `CONJURE_ASSERT` (programmer error on the injecting side; backends never produce one), counted, dropped |

`end_frame()` is `mPublished = mPending; mPublished.frameIndex = ++mFrameCounter;` followed by clearing
`keysPressed`, `keysReleased`, every `*Pressed` / `*Released` mask, `mouseDelta`, `scrollDelta` and `textCount` in
`mPending`. `release_all()` ORs each `down` set into the matching `released` set, clears the `down` sets and
`modifiers`; GLFW's own synthetic releases on focus loss then hit the no-op rule. `snapshot()` returns a reference to
`mPublished`.

`input_state.cpp` pins the layout the scripting frame block (`conjure_input_block`, `03_scripting.md` 2.1) mirrors:
`sizeof(key_bitset) == 64`, `alignof == 8`, `sizeof(gamepad_state) == 40` with `buttonsDown` at 4 and `axes` at 16,
`keysDown` / `keysPressed` / `keysReleased` at offsets 8 / 72 / 136, `sizeof(input_state) <= 512`. Changing any of
these is an ABI change for the C# side.

## 6. The null backend

`null_backend` holds the logical size, windowed size, content scale, position, cursor position, focus, minimised,
visible, fullscreen and close flags, the cursor mode, the raw-motion flag, an in-memory clipboard with a
"has text" flag, one virtual monitor (`"Null Monitor"`, 1920 x 1080 at 60 Hz, scale 1) and per-slot gamepad
connection flags. `framebuffer_size()` is `round(logical * scale)`, or 0 x 0 while minimised; `position` defaults to
the window centred on the virtual monitor. Every setter updates state so the getters round-trip (R57), which makes
the bindings' clipboard and cursor-mode entries real implementations on the null window rather than stubs.

The simulation controls queue into the same double buffer as section 4: `simulate_resize`, `set_size`,
`set_fullscreen`, `simulate_minimise` and a scale change that alters the pixel size all set the single coalesced
`mPendingResize`; `pending_event_count()` includes it. `push_event` re-routes a `drop_files_event` through
`simulate_drop_files` so the paths are copied, and the pump applies delivered `gamepad_connection_event`s to the
connection flags behind `gamepad_name()` (`"Null Gamepad <i>"`). Error contracts mirror glfw: `clipboard_text()` is
`CLIPBOARD_UNAVAILABLE` until something (even an empty string) was copied, `create_vulkan_surface` checks its
pointers before answering `VULKAN_UNSUPPORTED`, `set_fullscreen(true, 1)` is `MONITOR_NOT_FOUND`. There is no
clock, random state or OS query anywhere, so the same control sequence is byte-identical across runs and platforms.

## 7. Threading and ownership

| What | Thread | Notes |
|---|---|---|
| `create_window`, `~window`, `pump_events`, every window setter and getter that touches GLFW, clipboard, `monitors()` | main only | `CONJURE_ASSERT(core::jobs::is_main_thread())` in Debug and Development; GLFW's own rule |
| `input::apply` / `inject` / `end_frame` / `release_all` | main only | asserted; the editor's protocol server polls on main (R38) |
| `input::snapshot()` | any | a reference to data written only inside `end_frame()`; never hold it across one |
| GLFW callbacks, joystick and monitor callbacks | main, inside `glfwPollEvents` or a setter | they only append to `mPending` or set flags; nothing calls back into the engine |
| Threads created | none | |

Ownership: the window owns its event and drop-path buffers, the monitor list and (glfw) the `GLFWwindow *`; GLFW
itself is owned by the reference count in `glfw_context`. `platform.cpp` keeps the live-window count in the `window`
base class constructor and destructor; `create_window` asserts and returns `INVALID_DESC` when a window is already
alive. The engine's `platform_subsystem` (startup step 6, shut down last) will own the window and the `input` and
expose them as non-owning pointers; nothing else ever holds a `GLFWwindow *`.

## 8. Extension points

- **A new backend** (`backend_kind` leaves room): implement `window`, add the kind to `is_backend_compiled`,
  `default_backend_kind`, `backend_version_string`, `to_string` / `parse_backend_kind` and the `create_window`
  dispatch, and add a factory to `test_backend_conformance.cpp` so it runs the same suite. Everything above the
  factory is backend-agnostic; `input` needs no change.
- **A new event alternative**: append to `event` (after the window alternatives, or to both variants at the same
  index for an input alternative), extend `event.cpp` (`event_name`, `to_string`, the remaps if the input list
  changed), `input::apply_input` for an input alternative, and `test_event.cpp`'s `every_event` list, which pins
  the alternative order.
- **Gamepad mappings**: `window_desc::gamepadMappings` takes SDL game controller database lines; no code change.
- **Cursor images, window icons, IME, touch**: out of scope for v1 (section 11); they would be new virtuals on
  `window` with null implementations that round-trip.

## 9. Failure handling

| Situation | Behaviour |
|---|---|
| `backend == GLFW` with `CONJURE_PLATFORM_GLFW=OFF` | `BACKEND_NOT_COMPILED`; the engine falls back to the null backend only when headless |
| `glfwInit` fails | `INIT_FAILED` with GLFW's text; no reference taken, no terminate |
| `glfwCreateWindow` fails | `WINDOW_CREATE_FAILED`; the reference is released by the destructor of the partially built window |
| invalid `window_desc` | `INVALID_DESC` naming the field (`validate_window_desc`); nothing created |
| `monitorIndex` out of range | `MONITOR_NOT_FOUND` from `create_window` or `set_fullscreen`; window unchanged |
| Vulkan helpers | `INVALID_ARGUMENT` (null pointers), `VULKAN_UNSUPPORTED`, `SURFACE_CREATE_FAILED` with the `VkResult` |
| clipboard without text or refused | `CLIPBOARD_UNAVAILABLE` quoting the GLFW error |
| unsupported cursor mode, raw motion, positioning | `FEATURE_UNAVAILABLE`, state unchanged, one WARN per feature; other `glfwSetInputMode` errors are `BACKEND_FAILURE` |
| any GLFW error callback | `CONJURE_LOG_ERROR(LOG_PLATFORM, ...)`; tests that provoke one call `logs.allow_errors()` |
| invalid key, button, axis, or gamepad index in an event | dropped and counted in `ignoredTransitions`; the gamepad index also asserts |
| more than 16 text code points in a frame | dropped and counted |
| second `create_window` while one is alive | `CONJURE_ASSERT`, then `INVALID_DESC` |

`pump_events`, `apply`, `inject`, `end_frame` and `snapshot` cannot fail.

## 10. Tests

`tests/platform/` builds `conjure_platform_tests` (label `platform`): one file per public header plus three topic
files. `platform_test_support.h` provides `null_desc()` (1280 x 720, the headless default), `create_null_window()`
and `make_key` / `make_mouse_button` / `make_mouse_move` / `make_scroll` / `make_text` / `make_gamepad_*` /
`make_focus` builders; every fixture owns a `memory_guard` and a `log_capture`.

| File | Proves |
|---|---|
| `test_input.cpp` | every rule of section 5 as a case, `inject == apply` on a scripted sequence (same snapshot bytes), determinism across instances, allocation-free `apply` / `end_frame` / `release_all` |
| `test_input_state.cpp` | `key_bitset` across word boundaries (63, 64, 348), the helpers, `gamepad()` asserting out of range, `text()` never reading past the array |
| `test_window.cpp`, `test_null_backend.cpp` | `window_desc` defaults and validation, the null happy path, round trips, fullscreen, Vulkan and clipboard contracts, one live window, each simulation control, coalescing, allocation-free pump, determinism |
| `test_window_events.cpp` | the engine's frame step 3 against a null window: coalesced resize, 0 x 0 on minimise, focus loss releasing keys through `apply`, close, dropped files |
| `test_backend_selection.cpp` | option gating, `UNKNOWN_BACKEND`, the `window_settings -> window_desc` mapping the engine will use |
| `test_backend_conformance.cpp` | `TEST_CASE_TEMPLATE` over `null_factory` and `glfw_factory`: create twice, size relation, title, cursor modes, clipboard, monitors, native handle kind, close, `set_size` / `set_fullscreen` / `show` / `hide` deliver their events, the same injected sequence yields the same snapshot, Vulkan helpers |
| `test_event.cpp`, `test_key.cpp`, `test_mouse_button.cpp`, `test_gamepad.cpp`, `test_button_action.cpp`, `test_backend.cpp`, `test_monitor.cpp`, `test_native_handle.cpp`, `test_platform_error.cpp`, `test_log_category.cpp`, `test_platform.cpp` | numbering invariants, names and parsers, variant order and formatting, defaults, the umbrella |

The glfw instantiations of the conformance suite carry `[window]` in their name, require
`CONJURE_TEST_REQUIRE(precondition::REAL_WINDOW)` (`CONJURE_TEST_WINDOW=1`) and skip with exit 77 otherwise, so CI
never opens a window; `glfw_factory::warm_up` creates one window before a case takes its memory baseline because the
first `glfwInit` / `glfwTerminate` pair and the first `glfwVulkanSupported` keep a few allocations alive until exit.
The suite checks the *events* setters produce, not only the state they leave, because the glfw instantiation is the
only place a setter-driven event can be observed; the show / hide case briefly shows the window. Review fixes carry
`regression - platform-review-<n> ...` cases.

```
python tools/build.py --build-dir build/mod_platform --target conjure_platform_tests --test -R platform
CONJURE_TEST_WINDOW=1 build/mod_platform/bin/conjure_platform_tests -ts="platform/backend_conformance"
```

> [!TIP]
> Run the `[window]` cases on all three operating systems (and on both X11 and Wayland) before changing anything in
> `glfw_window.cpp`; the null half of the suite cannot see a display-server regression.

## 11. Open items

- Multiple windows (editor floating viewports): the interface is per-window already; the one-live-window assert
  and the joystick / monitor callback dispatch to a single active window need changing.
- Replace the local Vulkan declarations in `glfw_vulkan.cpp` with a pinned Vulkan-Headers dependency (3.6).
- Exclusive fullscreen with mode switching (`video_modes()` exists for a settings UI), cursor images and shapes,
  window icons, clipboard images and file lists, IME composition, touch and pen, gamepad rumble: `11_platform.md` 11.
- `tests/benchmarks/bench_platform.cpp` (10,000 injected events under 50 us, snapshot copy under 0.2 us) is listed
  in the design's plan and not yet in the tree.

## Related

- design records: [`../../design/11_platform.md`](../../design/11_platform.md) (the binding contract),
  [`../../design/06_engine_loop.md`](../../design/06_engine_loop.md) 3.1 (frame step 3, headless behaviour),
  [`../../design/08_testing.md`](../../design/08_testing.md) 8.4 (identical edge / level behaviour on both backends),
  [`../../design/03_scripting.md`](../../design/03_scripting.md) 2.1 (`conjure_input_block`)
- module README: `engine/src/platform/README.md`
- 📘 API: [Platform overview](../../api/platform/index.md),
  [Windows and events](../../api/platform/windows-and-events.md), [Input](../../api/platform/input.md)
- handbook: [Architecture](../architecture.md), [Testing](../testing.md), [Modules: core](core.md)
