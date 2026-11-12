# platform

The window, its event stream and the per-frame input snapshot (`conjure::platform`, static library
`conjure::platform`, design `docs/design/11_platform.md`, contract `docs/ARCHITECTURE.md` 4.2). Everything
else in the engine - the swapchain, ImGui, scripting's `conjure_input_block`, the editor's `input.inject`, the
test harness - consumes three types: `window`, `event` and `input_state`.

## Main types (`engine/include/conjure/platform/`)

| Header | Contents |
|---|---|
| `window.h` | `window` (abstract; `pump_events() -> std::span<const event>`, sizes in logical and pixel units, title / size / position / fullscreen / cursor mode / clipboard / icon, `native_handle()`, Vulkan helpers, `as_null()`), `window_desc`, `window_flags`, `cursor_mode`, `display_server_hint`, `validate_window_desc`, `validate_window_icon`, `create_window(desc) -> result<unique_ptr<window>>`, `live_window_count` |
| `null_backend.h` | `null_backend final : window` - the virtual window: no OS calls, one "Null Monitor" 1920x1080@60, in-memory clipboard / cursor mode / gamepad names / icon images (`icons()`), test controls `push_event`, `simulate_resize / content_scale / focus / minimise / close / drop_files`, `pending_event_count` |
| `event.h` | the trivially copyable event structs, `input_event` (nine input alternatives) and `event` (plus six window alternatives, same prefix order), `is_input_event`, `to_input_event`, `to_event`, `event_as<T>`, `event_name`, `to_string(event)` |
| `input.h` | `input`: `apply(event)` and `inject(input_event)` run the same state machine, `end_frame()` publishes the snapshot, `snapshot()`, `release_all()`, `skip_next_mouse_delta()`, `input_stats` |
| `input_state.h` | `input_state` (512-key `key_bitset`s for down / pressed / released, modifiers, mouse position / delta / scroll, button masks, four `gamepad_state`, up to 16 text code points per frame); the shape `conjure_input_block` matches (the `key_bitset` shape the frame block copies is pinned in `input_state.cpp`) |
| `key.h`, `mouse_button.h`, `gamepad.h`, `button_action.h` | GLFW-numbered enumerations (`key::SPACE == 32` ... `key::MENU == 348`, `key::DEL`), `to_string` / `parse_*` (case-insensitive), `is_valid_*`, `key_modifier_flags` with `to_string` / `parse_key_modifier_flags` |
| `backend.h` | `backend_kind` (`NULL_BACKEND`, `GLFW`), `parse_backend_kind` (`UNKNOWN_BACKEND`), `is_backend_compiled`, `default_backend_kind` |
| `monitor.h`, `native_handle.h` | `video_mode`, `monitor_info`; `native_handle_kind` (`WINDOWS`, `COCOA`, `X11`, `WAYLAND`), `native_window_handle` |
| `platform_error.h`, `log_category.h`, `platform.h` | `platform_error` bound to the `"platform"` category; `LOG_PLATFORM`; umbrella plus `backend_version_string` |

## Behaviour worth knowing

- **Edges and levels (P4).** `PRESS` on an up key sets `down` and `pressed`; `PRESS` on a down key, `RELEASE` on
  an up key, invalid codes and out-of-range buttons are ignored and counted in `input_stats::ignoredTransitions`;
  `REPEAT` never changes state. Press and release in one frame leave both edges set and `down` clear. A
  `focus_event{false}` (and `release_all()`) releases every key, mouse and gamepad button with a released edge and
  clears the modifiers; GLFW's own synthetic releases on focus loss then hit the no-op rule.
- **`end_frame()`** copies the accumulating state into the published snapshot (`frameIndex` incremented), then
  clears edges, `mouseDelta`, `scrollDelta` and text. Levels (`keysDown`, positions, axes, `connected`,
  `cursorInWindow`, `modifiers`) persist. `snapshot()` is a reference to the published copy.
- **Mouse delta** is skipped for the first move after construction, after a `cursor_enter_event{true}` and after
  `skip_next_mouse_delta()` (call it after `set_cursor_mode`, where the OS repositions the cursor).
- **Resize coalescing (P10).** Both backends deliver at most one `framebuffer_resized_event` per pump, as the last
  element of the batch, carrying the final size; `0 x 0` while minimised.
- **Event spans** are valid until the next `pump_events()`; `drop_files_event::paths` points into a window-owned
  buffer with the same lifetime. Both backends double-buffer: callbacks (glfw) or simulation controls (null) feed
  a *pending* vector that is never cleared at pump start, and the pump swaps it into the *delivered* vector it
  returns. That matters on glfw because Win32 and Cocoa raise the resize, focus and iconify callbacks
  synchronously inside `glfwSetWindowSize`, `glfwSetWindowMonitor` and `glfwShowWindow`, i.e. between two
  pumps; the next pump delivers them. The steady-state pump of both backends allocates nothing (vectors keep
  their capacity; only dropped file paths allocate).
- **One live window per process (v1).** A second `create_window` asserts and returns `INVALID_DESC`.
  `validate_window_desc` rejects a zero size when not fullscreen, a title over 255 bytes and an unknown backend.
- **GLFW backend** (`src/platform/glfw/`, built when `CONJURE_PLATFORM_GLFW`, default ON): `glfw_context` owns the
  process-wide init reference count, the error callback (every GLFW error is logged at ERROR and kept as the last
  error text quoted by `INIT_FAILED` / `WINDOW_CREATE_FAILED` / `CLIPBOARD_UNAVAILABLE` messages) and the joystick /
  monitor callback dispatch; `glfw_window` holds the `GLFWwindow`, its callbacks, the gamepad poll (diff against
  the previous `glfwGetGamepadState`, triggers remapped to `[0, 1]`, no dead zone) and the monitor list (rebuilt
  only when a monitor callback fires). `glfw_native.cpp` is the only file including `glfw3native.h`;
  `glfw_vulkan.cpp` the only one that sees Vulkan handle types (declared locally, no SDK dependency; 64-bit only,
  asserted, and an ODR caveat once the real `vulkan_core.h` is compiled into the same program - see the file
  header). A cursor mode the display server lacks (`CAPTURED` outside Win32/Cocoa, `DISABLED` on a Wayland
  compositor without pointer locking) and unsupported raw mouse motion report `FEATURE_UNAVAILABLE` (WARN once
  per mode / feature, state unchanged); any other GLFW error from `glfwSetInputMode` is
  `core::error_code::BACKEND_FAILURE`. On Wayland, where clients have no window position and cannot warp the
  cursor, the backend never asks GLFW: `set_position` returns `FEATURE_UNAVAILABLE`, `position()` returns the
  stored (never set) position and `set_cursor_position` warns once and does nothing, so no ERROR line is logged
  per call. The windowed size restored when leaving fullscreen is the last size the OS *confirmed* through the
  window size callback, not the last one requested (the OS may clamp); the position restored is the last windowed
  one, and a window created fullscreen without `window_desc::position` is centred on its monitor's work area (the
  monitor origin it reported while fullscreen would put the title bar off screen). Windowed fullscreen only (P12).
  A gamepad that found all four slots taken gets the first slot that frees: after a pump has delivered the slot's
  disconnect, the next pump rescans the joysticks before it polls, so the connect arrives one pump later, the
  disconnect's released edges are never wiped by the reset on connect and `gamepad_name()` stays empty until the
  connect is delivered.
- **Null backend** reports `framebuffer_size() == round(window_size() * nullContentScale)`, the same content scale
  on its virtual monitor, `is_focused() == true`, `is_visible()` following `window_flags::VISIBLE`,
  `native_handle().kind == NONE`, no Vulkan extensions and `VULKAN_UNSUPPORTED` from `create_vulkan_surface`. Its
  error contracts mirror glfw: `clipboard_text()` is `CLIPBOARD_UNAVAILABLE` until `set_clipboard_text` has been
  called (an empty copy counts as text, as on the OS clipboards) and `create_vulkan_surface` rejects a null
  `surface` or `instance` with `core::error_code::INVALID_ARGUMENT` before answering `VULKAN_UNSUPPORTED`.
  `gamepad_name(i)` is `"Null Gamepad <i>"` once a `gamepad_connection_event` for slot `i` has been delivered.
  Leaving fullscreen restores the last windowed position, not one set while fullscreen, as on glfw.
- `CONJURE_PLATFORM_GLFW=OFF` builds a null-only library: `create_window(GLFW)` returns `BACKEND_NOT_COMPILED`,
  `is_backend_compiled(GLFW)` is false and `default_backend_kind()` is `NULL_BACKEND`. The editor requires the
  option ON (checked in the root `CMakeLists.txt`).

## Threading model

Every window call (`create_window`, destruction, `pump_events`, every setter and getter that touches GLFW,
clipboard, monitors) and every `input` mutation (`apply`, `inject`, `end_frame`, `release_all`) is **main thread
only** and asserts `core::jobs::is_main_thread()` in Debug and Development. `input::snapshot()` may be read from
any thread between two `end_frame()` calls; the reference must not be held across one. GLFW callbacks run on the
main thread inside `glfwPollEvents` and only append to the window's event vector. The module creates no threads.

## Dependencies

`conjure::core` (public): `result` / `error` / `CONJURE_DECLARE_ERROR_CATEGORY`, logging, assertions,
`enum_flags.h`, math (`vec2`, `ivec2`, `uvec2`, `urect`), `core::jobs::is_main_thread`, `core::str::iequals`,
`platform_detect.h`. `conjure::thirdparty::glfw` (private, GLFW 3.4, pinned in `third_party/CMakeLists.txt`) when
`CONJURE_PLATFORM_GLFW`.

## Testing

`tests/platform/` builds `conjure_platform_tests`: one file per public header plus `test_window_events.cpp`
(the engine's frame step 3 against a null window), `test_backend_selection.cpp` (option gating, the
`window_settings -> window_desc` mapping) and `test_backend_conformance.cpp` (`TEST_CASE_TEMPLATE` over a null
and a glfw factory; the glfw instantiations carry `[window]` in their name, require `CONJURE_TEST_WINDOW=1` and
skip with exit 77 otherwise, so CI never opens a window). The conformance suite covers the events that
programmatic changes produce (`set_size`, `set_fullscreen` round trip, `show` / `hide`), not just the state they
leave behind, because the glfw instantiation is the only place a setter-driven event can be observed; the show /
hide case briefly shows the window. Review fixes carry `regression - platform-review-<n> ...` cases. Everything
else is headless and deterministic.

```
python tools/build.py --build-dir build/mod_platform --target conjure_platform_tests --test -R platform
CONJURE_TEST_WINDOW=1 build/mod_platform/bin/conjure_platform_tests -ts="platform/backend_conformance"
```
