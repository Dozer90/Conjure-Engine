# Platform: windows, input and events

📘 **Public API wiki** · 🛠 Handbook: [Platform module](../../handbook/modules/platform.md)

The platform module puts the game's window on screen, turns what the player does with the keyboard, mouse and
gamepads into a flat list of events once per frame, and folds that list into one per-frame *input snapshot* that
your game code queries. Everything lives in `namespace conjure::platform`; the umbrella header
`conjure/platform/platform.h` includes every header named on these pages and adds `backend_version_string()`.

## What you get

| Type | What it is | Page |
|---|---|---|
| `window` | the game's window: size, title, fullscreen, cursor mode, clipboard, and `pump_events()` | [Windows and events](windows-and-events.md) |
| `event` | one thing that happened this frame: a key, a mouse move, a resize, a close request | [Windows and events](windows-and-events.md) |
| `input` and `input_state` | the accumulator that applies events, and the snapshot it publishes (`is_key_pressed(key::SPACE)`) | [Input](input.md) |

Three rules hold everywhere:

1. **One window per process.** `create_window()` makes it; a second call while the first is alive is a programmer
   error.
2. **Everything happens on the main thread**, once per frame, in this order: `pump_events()`, then `input::apply()`
   for every event, then `input::end_frame()`. Reading `input::snapshot()` is allowed from any thread between two
   `end_frame()` calls.
3. **Nothing at frame time can fail.** Only setup calls (`create_window`, `set_fullscreen`, `set_cursor_mode`,
   the clipboard) return a `result`; pumping, applying and reading the snapshot never do.

## What the engine does for you

When your game runs through the engine's application loop, the engine owns the window and the `input` instance
and runs the frame steps above for you: it creates the window from the project's window settings, pumps events,
lets the editor and overlay layers see them first, feeds the rest to `input`, and acts on the window events
itself (a `close_event` quits, a `framebuffer_resized_event` resizes the swapchain, a `focus_event` slows the frame
pacer). Gameplay code and scripts only ever read the published snapshot.

You use this module directly when you drive the window yourself: a tool, a test, a custom runtime, or anything that
needs the raw event list (an in-game console that wants text input, a drag-and-drop importer that wants
`drop_files_event`).

> [!TIP]
> If all you want is "is the jump key down this frame", skip straight to [Input](input.md). The window and event
> pages matter when you create the window yourself or react to window-level events.

## Two backends

A `window` is always created through the same factory; what sits behind it is chosen by `window_desc::backend`.

| Backend | `backend_kind` | What it does | When you use it |
|---|---|---|---|
| GLFW | `GLFW` (the default) | a real operating-system window through GLFW 3.4: Windows, macOS, Linux (X11 and Wayland) | the game and the editor |
| Null | `NULL_BACKEND` | a virtual window: no operating-system calls, no display server needed, events are whatever you queue | tests, headless servers, continuous integration |

Both backends deliver the same events with the same meaning and produce byte-identical input snapshots for the same
input, so code and tests written against the null window behave the same on a real one. `default_backend_kind()`
tells you which one a build prefers (`GLFW` when it was compiled in, else `NULL_BACKEND`), and `is_backend_compiled()`
whether a given one exists in this build.

## Pages

| Page | Covers |
|---|---|
| [Windows and events](windows-and-events.md) | `window_desc` and `create_window()`, choosing a backend, `pump_events()` and every `event` alternative, logical versus pixel sizes, fullscreen and monitors, cursor modes, the clipboard, the native handle and Vulkan helpers, the null backend's simulation controls |
| [Input](input.md) | `input` and `input_state`, pressed / released / down / held semantics, keys and modifiers, mouse, gamepads, text input, focus loss, injecting input in tests, the bit-level layout |

## Errors you may meet

Setup calls report failure through `result<T>` with a code from `platform_error` (`conjure/platform/platform_error.h`,
category `"platform"`). Test for one with `error.is(platform_error::X)`; see
📘 API: [Errors and results](../core/errors-and-results.md).

| Code | When |
|---|---|
| `BACKEND_NOT_COMPILED` | `window_desc::backend` asks for GLFW in a build made with `CONJURE_PLATFORM_GLFW=OFF` |
| `UNKNOWN_BACKEND` | `parse_backend_kind()` got a name other than `"null"` or `"glfw"` |
| `INIT_FAILED` | GLFW could not start (no display server, missing libraries); the message quotes GLFW's text |
| `INVALID_DESC` | zero size without fullscreen, a title over 255 bytes, an unknown backend value, or a window is already alive |
| `WINDOW_CREATE_FAILED` | the operating system refused the window |
| `MONITOR_NOT_FOUND` | `monitorIndex` is past the end of `monitors()` |
| `VULKAN_UNSUPPORTED` / `SURFACE_CREATE_FAILED` | Vulkan helpers on the null backend or without a Vulkan loader; surface creation failed |
| `CLIPBOARD_UNAVAILABLE` | the clipboard holds no text, or the platform refused |
| `FEATURE_UNAVAILABLE` | a cursor mode, raw mouse motion or window positioning the display server does not offer (Wayland) |

> [!NOTE]
> Examples on these pages open with `using namespace conjure;` and `using namespace conjure::platform;` for brevity
> and take math types (`math::vec2`, `math::uvec2`, `math::ivec2`) from 📘 API: [Math](../core/math.md). In your
> own headers, qualify.

## See also

- 📘 API: [Glossary](../glossary.md)
- 📘 API: [Jobs and threading](../core/jobs.md) for what "the main thread" means to the engine
- 🛠 Handbook: [Platform module](../../handbook/modules/platform.md) for the GLFW backend's internals, the input state
  machine and the conformance tests
