# Windows and events

📘 **Public API wiki** · 🛠 Handbook: [Platform module](../../handbook/modules/platform.md)

How to create the game's window, pump its events once per frame and react to them. Include
`conjure/platform/window.h` for the window, its description and `create_window()`; it pulls in
`conjure/platform/event.h` (the event types), `conjure/platform/backend.h` (which backend sits behind the
window), `conjure/platform/monitor.h` and `conjure/platform/native_handle.h`. `conjure/platform/null_backend.h`
adds the virtual window tests drive. Everything is in `namespace conjure::platform`.

## At a glance

| You want to... | Use |
|---|---|
| Open the game's window | fill a `window_desc`, then `create_window(desc)` |
| Run headless (tests, servers) | `desc.backend = backend_kind::NULL_BACKEND` |
| Process this frame's events | `for (const event &value : target.pump_events())` |
| Look inside an event | `event_as<key_event>(value)` (`nullptr` when it is something else) |
| Know when the player closed the window | `should_close()`, or a `close_event` in the batch |
| Size a swapchain or viewport | `framebuffer_size()` (pixels); `window_size()` is logical, `content_scale()` the ratio |
| Go fullscreen | `set_fullscreen(true, monitorIndex)`; `monitors()` lists the choices |
| Hide or lock the mouse cursor | `set_cursor_mode(cursor_mode::HIDDEN)` / `DISABLED` |
| Copy and paste text | `set_clipboard_text(text)` / `clipboard_text()` |
| Give the window your game's icon | `set_icon(images)` with RGBA8 `core::image`s (16, 32 and 48 px at least) |
| Feed events to a null window in a test | `created->as_null()->push_event(...)`, `simulate_resize(...)`, ... |

## Quick example

```cpp
#include "conjure/platform/window.h"

#include <memory>

using namespace conjure;
using namespace conjure::platform;

void resize_swapchain(math::uvec2 pixels);

core::result<void> run_until_closed() {
    window_desc desc = {};
    desc.title = "My Game";
    desc.width = 1280;      // logical size; the framebuffer may be larger on a high-DPI display
    desc.height = 720;
    CONJURE_TRY_ASSIGN(const std::unique_ptr<window> created, create_window(desc));

    while (!created->should_close()) {
        for (const event &value : created->pump_events()) {
            const framebuffer_resized_event *resized = event_as<framebuffer_resized_event>(value);
            if (resized != nullptr && resized->width > 0 && resized->height > 0) {
                resize_swapchain(math::uvec2(resized->width, resized->height));
            }
        }
        // update and render here; a close_event in the batch has already made should_close() true
    }
    return core::result<void>::success();
}
```

## Describing the window: `window_desc`

`window_desc` is a plain struct with sensible defaults; set only what you need.

| Field | Default | Meaning |
|---|---|---|
| `backend` | `backend_kind::GLFW` | `GLFW` for a real window, `NULL_BACKEND` for the virtual one |
| `title` | `"Conjure"` | UTF-8, at most `MAX_WINDOW_TITLE_BYTES` (255) bytes |
| `width`, `height` | 1600 x 900 | logical size; may be 0 x 0 only when `fullscreen` is true |
| `flags` | `window_flags::DEFAULT` | `RESIZABLE`, `DECORATED`, `VISIBLE`, `FLOATING`, `MAXIMISED`, `SCALE_TO_MONITOR`, `SCALE_FRAMEBUFFER`; `DEFAULT` is all but `FLOATING` and `MAXIMISED` |
| `fullscreen`, `monitorIndex` | false, 0 | start in windowed fullscreen on that monitor |
| `position` | `std::nullopt` | top-left corner in logical desktop coordinates; `nullopt` lets the OS choose (the null window centres itself) |
| `displayServer` | `display_server_hint::ANY` | Linux only: force `X11` or `WAYLAND` |
| `gamepadMappings` | empty | extra SDL game controller database lines for unusual gamepads |
| `nullContentScale` | 1, 1 | null backend only: the content scale it should report |

`create_window(desc)` returns `result<std::unique_ptr<window>>`. `validate_window_desc(desc)` runs the same checks
without creating anything, which is handy when a settings screen edits a description. `live_window_count()` is 0 or 1.

> [!WARNING]
> Exactly one window may be alive per process. A second `create_window()` while the first exists asserts in Debug
> and Development builds and fails with `INVALID_DESC`. Destroy the first (`unique_ptr` going out of scope) before
> creating another; that is legal any number of times.

### Choosing a backend

`conjure/platform/backend.h` declares `backend_kind` and four helpers:

| Signature (short) | Purpose |
|---|---|
| `std::string_view to_string(backend_kind)` | `"null"` or `"glfw"`, the names used in configuration files |
| `result<backend_kind> parse_backend_kind(std::string_view)` | case-insensitive; fails with `UNKNOWN_BACKEND` |
| `bool is_backend_compiled(backend_kind)` | the null backend always; GLFW only when `CONJURE_PLATFORM_GLFW` was ON |
| `backend_kind default_backend_kind()` | `GLFW` when compiled, else `NULL_BACKEND` |

> [!TIP]
> In a headless configuration set `desc.backend = backend_kind::NULL_BACKEND` explicitly rather than relying on
> `default_backend_kind()`: a developer machine has GLFW compiled in, and a test that opens a real window steals
> focus and fails on a build agent without a display.

## The frame: `pump_events()` and the event variant

Call `pump_events()` exactly once per frame on the main thread. It processes everything the operating system has
queued and returns a `std::span<const event>` in arrival order. `event` (`conjure/platform/event.h`) is a
`std::variant` of small trivially copyable structs:

| Alternative | Payload | Notes |
|---|---|---|
| `key_event` | `keyCode`, `scancode`, `action`, `modifiers` | `action` is `PRESS`, `RELEASE` or `REPEAT` |
| `text_event` | `codepoint` | UTF-32, one per typed character |
| `mouse_move_event` | `position` | logical window coordinates |
| `mouse_button_event` | `button`, `action`, `modifiers` | |
| `mouse_scroll_event` | `offset` | `+y` is away from the user |
| `cursor_enter_event` | `entered` | |
| `gamepad_connection_event` | `gamepadIndex`, `connected` | slots 0 to 3 |
| `gamepad_button_event` | `gamepadIndex`, `button`, `action` | |
| `gamepad_axis_event` | `gamepadIndex`, `axis`, `value` | only when the value changed |
| `framebuffer_resized_event` | `width`, `height` | pixels; at most one per pump, last in the batch; 0 x 0 while minimised |
| `content_scale_changed_event` | `scale` | the window moved to a display with another DPI |
| `focus_event` | `focused` | |
| `minimise_event` | `minimised` | |
| `close_event` | none | the player asked to close; `should_close()` is already true |
| `drop_files_event` | `paths` | files dropped on the window; the span is valid until the next pump |

The first nine are *input events*; `input_event` is the variant of just those, and `input::apply()` consumes them
([Input](input.md)). Helpers: `event_as<T>(value)` returns the payload or `nullptr`; `is_input_event(value)`,
`to_input_event(value)` (`std::optional<input_event>`) and `to_event(inputValue)` convert between the two variants;
`event_name(value)` gives `"key_event"` and `to_string(value)` a full line such as
`key_event{key=SPACE, scancode=57, action=PRESS, modifiers=SHIFT}` for logs and consoles.

> [!TIP]
> Do not track key state from `key_event`s yourself; hand every event to `input::apply()` and query the snapshot.
> `REPEAT` events, focus loss and double presses are already handled there.

> [!WARNING]
> The span returned by `pump_events()` and the `paths` of a `drop_files_event` point into window-owned buffers that
> the *next* `pump_events()` overwrites. Copy an `event` by value if you need it later (it is trivially copyable);
> copy dropped paths into your own strings before the next frame.

## Sizes and high-DPI

Two sizes are always available. `window_size()` is in logical screen coordinates, the units the OS uses for window
placement and the units `mouse_move_event::position` and `set_cursor_position()` use. `framebuffer_size()` is in
pixels, the size your swapchain and viewport need. `content_scale()` is their ratio per axis (1 on a standard
display, 2 on a Retina display, 1.5 or 1.25 on many Windows laptops). The default flags `SCALE_TO_MONITOR` and
`SCALE_FRAMEBUFFER` make this behave the same on Windows, X11, Wayland and macOS.

`set_size(logicalSize)` asks for a new logical size; the pixel change arrives as a `framebuffer_resized_event` on
the next pump, coalesced with any other resize of that frame. `is_minimised()` is true while the framebuffer is
0 x 0, which is when the engine skips rendering. `set_position(position)` and `position()` work in logical desktop
coordinates; `set_position` returns `FEATURE_UNAVAILABLE` on Wayland, where applications cannot place their windows.
`show()`, `hide()`, `is_visible()`, `is_focused()`, `set_title()` and `title()` do what their names say.

## Fullscreen and monitors

`set_fullscreen(true, monitorIndex)` switches to *windowed fullscreen*: the window covers monitor `monitorIndex` at
its current resolution, with no video-mode change (so a crash never leaves the desktop in a strange mode).
`set_fullscreen(false)` restores the last windowed size. Both deliver a `framebuffer_resized_event` on the next pump.
`is_fullscreen()` reports the state; `MONITOR_NOT_FOUND` leaves everything unchanged.

`monitors()` returns a `std::span<const monitor_info>`, primary first, refreshed by `pump_events()` when a monitor is
plugged in or out. `monitor_info` (`conjure/platform/monitor.h`) carries `name`, `isPrimary`, `position` (logical
desktop coordinates), `currentMode` (a `video_mode`: `width`, `height`, `refreshRateHz`, in pixels), `contentScale`
and `workArea` (logical, excluding task bars). `video_modes(monitorIndex)` lists every mode a monitor supports, for a
settings screen; it is empty for an unknown index. The null window has one monitor, `"Null Monitor"`,
1920 x 1080 at 60 Hz.

## Cursor modes

| `cursor_mode` | Behaviour | Typical use |
|---|---|---|
| `NORMAL` | visible, free | menus, editor |
| `HIDDEN` | invisible while over the window, still free | custom in-game cursor |
| `DISABLED` | hidden and locked; `mouseDelta` keeps accumulating without bounds | first-person camera |
| `CAPTURED` | visible but confined to the window | strategy games; not every display server supports it |

`set_cursor_mode(mode)` returns `FEATURE_UNAVAILABLE` and leaves the mode unchanged where the display server lacks
it (`CAPTURED` outside Windows and macOS, `DISABLED` on a Wayland compositor without pointer locking).
`current_cursor_mode()` reads it back; `to_string(mode)` and `parse_cursor_mode(name)` convert for configuration.
`set_raw_mouse_motion(true)` asks for unaccelerated motion while `DISABLED` (`FEATURE_UNAVAILABLE` when the system
has none); `set_cursor_position(logicalPosition)` warps the cursor.

> [!IMPORTANT]
> After changing the cursor mode, call `input::skip_next_mouse_delta()`: the operating system repositions the cursor
> and the first report would otherwise read as a huge camera jump. See [Input](input.md#mouse).

## Clipboard

`set_clipboard_text(text)` replaces the clipboard's text (UTF-8) and `clipboard_text()` reads it back as
`result<std::string>`. The read fails with `CLIPBOARD_UNAVAILABLE` when the clipboard holds no text at all (an image,
or nothing yet on the null window); an empty string that was copied counts as text. Both work on the null backend,
which keeps an in-memory string.

## Window icon

`set_icon(images)` gives the window the icon the OS shows in its title bar, task switcher and taskbar. Pass one
square RGBA8 `core::image` (`conjure/core/image.h`) per size; the OS picks the closest size for each place, so pass
16, 32 and 48 px at least. The pixels are copied, and an empty span restores the default icon.

```cpp
#include "conjure/core/image.h"
#include "conjure/platform/platform_error.h"
#include "conjure/platform/window.h"

#include <utility>
#include <vector>

using namespace conjure;

// Loads the game's icon files and gives them to the window; platforms without window icons are fine.
core::result<void> apply_game_icon(platform::window &target) {
    std::vector<core::image> icons = {};
    for (const char *path : {"icon_16.png", "icon_32.png", "icon_48.png"}) {
        CONJURE_TRY_ASSIGN(core::image icon, core::image::load_file(path));
        icons.push_back(std::move(icon));
    }
    core::result<void> applied = target.set_icon(icons);
    if (!applied.has_value() && applied.get_error().is(platform::platform_error::FEATURE_UNAVAILABLE)) {
        return core::result<void>::success();
    }
    return applied;
}
```

The images are checked first, on every platform, by `validate_window_icon(images)`: at most
`MAX_WINDOW_ICON_IMAGES` (16), none empty and none larger than `MAX_WINDOW_ICON_DIMENSION` (256) on either side,
else `INVALID_ARGUMENT` and the icon is unchanged. Windows and X11 then show the icon. macOS and Wayland answer
`FEATURE_UNAVAILABLE`, without a log line: there the window shows the icon of the application bundle or of the
desktop file instead.

> [!TIP]
> On Windows, an executable that carries an icon resource named `GLFW_ICON` shows it from the first frame,
> before any code runs, and Explorer uses it for the file. The editor does both; see
> 🛠 Handbook: [Logo and application icon](../../handbook/brand.md).

## Native handle and Vulkan helpers

These exist for the rendering layer; games rarely touch them. `native_handle()` returns a `native_window_handle`
(`conjure/platform/native_handle.h`): `kind` is `WINDOWS`, `COCOA`, `X11`, `WAYLAND` or `NONE` (null backend);
`window` holds the `HWND`, `NSWindow *` or `wl_surface *`, `display` the `HINSTANCE`, `Display *` or `wl_display *`,
and `x11Window` the X11 window id (an integer, not a pointer). `required_vulkan_instance_extensions()` lists what a
`VkInstance` needs to present to this window (empty on the null backend or without a Vulkan loader), and
`create_vulkan_surface(instance, &surface)` creates a `VkSurfaceKHR` through opaque pointers so no Vulkan header is
needed here; it fails with `INVALID_ARGUMENT` for null pointers, `VULKAN_UNSUPPORTED` on the null backend or
without a loader, and `SURFACE_CREATE_FAILED` otherwise.

`gamepad_name(index)` is the human-readable name of the gamepad in slot `index` (`"Xbox Controller"`), empty while
the slot is not connected.

## The null backend

`conjure/platform/null_backend.h` is the window every headless test uses. Create it through `create_window()` with
`backend = NULL_BACKEND` (or `null_backend::create(desc)` directly), then reach the simulation controls through
`created->as_null()`, which returns `nullptr` for a real window. Nothing is delivered until the next
`pump_events()`, exactly as with a real window; `pending_event_count()` tells you what is queued.

| Control | Effect on the next pump |
|---|---|
| `push_event(value)` | delivers any `event` verbatim, in order |
| `simulate_resize(width, height)` | changes the logical size; one `framebuffer_resized_event` with `round(size * scale)` pixels |
| `simulate_content_scale(scale)` | a `content_scale_changed_event`, plus a resize when the pixel size changed |
| `simulate_focus(focused)` | a `focus_event`; `is_focused()` changes immediately |
| `simulate_minimise(minimised)` | a `minimise_event` and a 0 x 0 (or restored) resize |
| `simulate_close()` | a `close_event`; `should_close()` is true immediately |
| `simulate_drop_files(paths)` | copies the paths and delivers one `drop_files_event` spanning them |

```cpp
#include "conjure/platform/null_backend.h"
#include "conjure/platform/window.h"

#include <memory>

using namespace conjure;
using namespace conjure::platform;

void resize_is_coalesced(window &created) {
    null_backend &backend = *created.as_null();    // nullptr for a real window
    backend.simulate_resize(800, 600);
    backend.simulate_resize(1024, 768);

    const std::span<const event> events = created.pump_events();
    const framebuffer_resized_event *resized = event_as<framebuffer_resized_event>(events.back());
    // events.size() == 1, resized->width == 1024, resized->height == 768
}
```

The null window reports `is_focused() == true`, `is_visible()` from `window_flags::VISIBLE`,
`framebuffer_size() == round(window_size() * nullContentScale)`, `native_handle().kind == NONE`, and its
`set_*` calls update state so the getters round-trip. It contains no clock, random state or OS query, so the same
sequence of controls produces the same events on every machine. `cursor_position()` and `raw_mouse_motion()` read
back what `set_cursor_position()` and `set_raw_mouse_motion()` stored.

## API summary

| Signature (short) | Purpose |
|---|---|
| `result<std::unique_ptr<window>> create_window(const window_desc &)` | the factory; main thread; one live window |
| `result<void> validate_window_desc(const window_desc &)` / `u32 live_window_count()` | checks without creating; windows alive |
| `std::span<const event> pump_events()` | this frame's events; valid until the next call |
| `bool should_close()` / `void request_close()` | close request from the player or from code |
| `math::uvec2 framebuffer_size()` / `math::uvec2 window_size()` / `math::vec2 content_scale()` | pixels, logical units, ratio |
| `bool is_minimised()` / `is_focused()` / `is_visible()` / `is_fullscreen()` | state |
| `void set_title(std::string_view)` / `std::string_view title()` | title |
| `result<void> set_icon(std::span<const core::image>)` / `result<void> validate_window_icon(...)` | window icon; its checks |
| `void set_size(math::uvec2)` / `result<void> set_position(math::ivec2)` / `math::ivec2 position()` | geometry in logical units |
| `void show()` / `void hide()` | visibility |
| `result<void> set_fullscreen(bool, u32 monitorIndex = 0)` | windowed fullscreen on a monitor |
| `std::span<const monitor_info> monitors()` / `std::span<const video_mode> video_modes(u32)` | monitors and their modes |
| `result<void> set_cursor_mode(cursor_mode)` / `cursor_mode current_cursor_mode()` | cursor behaviour |
| `result<void> set_raw_mouse_motion(bool)` / `void set_cursor_position(math::vec2)` | raw motion; warp |
| `std::string_view gamepad_name(u32 index)` | connected gamepad's name, else empty |
| `native_window_handle native_handle()` | per-OS handles for the renderer |
| `std::span<const char *const> required_vulkan_instance_extensions()` / `result<void> create_vulkan_surface(void *, void **)` | Vulkan presentation helpers |
| `result<std::string> clipboard_text()` / `result<void> set_clipboard_text(std::string_view)` | text clipboard |
| `null_backend *as_null()` | the simulation controls, or `nullptr` |
| `template <typename T> const T *event_as(const event &)` | payload access |
| `bool is_input_event(const event &)` / `std::optional<input_event> to_input_event(const event &)` / `event to_event(const input_event &)` | variant conversions |
| `std::string_view event_name(const event &)` / `std::string to_string(const event &)` | diagnostics |
| `std::string_view to_string(cursor_mode)` / `std::optional<cursor_mode> parse_cursor_mode(std::string_view)` | cursor mode names |

## See also

- 📘 API: [Input](input.md): turning the event batch into the per-frame snapshot
- 📘 API: [Platform overview](index.md): the two backends and the error codes
- 📘 API: [Errors and results](../core/errors-and-results.md) for `result<T>` and `CONJURE_TRY_ASSIGN`
- 📘 API: [Glossary](../glossary.md)
- 🛠 Handbook: [Logo and application icon](../../handbook/brand.md): how the editor builds and sets its icon
- 🛠 Handbook: [Platform module](../../handbook/modules/platform.md): GLFW lifetime, event double buffering, resize
  coalescing, the gamepad poll
