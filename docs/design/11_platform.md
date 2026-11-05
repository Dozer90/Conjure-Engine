# 11 - Platform module design (`conjure::platform`)

Status: **binding** (owner: lead architect). Module `conjure_platform`, wave 2, namespace `conjure::platform`,
public headers `engine/include/conjure/platform/`, tests `tests/platform/` (`conjure_platform_tests`). Expands the
contract of `ARCHITECTURE.md` 4.2 and the needs stated in `06_engine_loop.md` 3.1/7/8, `02_renderer.md` 4.1/8,
`04_editor.md` 8, `08_testing.md` 5.2/8.4 and `03_scripting.md` 8 into an implementable design. Where an area
document spells a platform name differently, this document wins (R30). Depends on `conjure_core` only; GLFW 3.4 is a
private dependency of the `glfw` backend and never appears in a public header (CONVENTIONS 2).

---

## 1. Overview and decisions

The platform module owns three things: a **window** (one per process in v1) that turns OS events into a flat,
trivially copyable `event` stream once per frame; an **input** accumulator that turns that stream into an immutable
per-frame `input_state` snapshot with edge (pressed/released) and level (down) semantics; and the **backend**
behind the window: `glfw` (real OS window through GLFW 3.4) or `null` (virtual window, no OS calls). Everything else
in the engine - the swapchain, ImGui, scripting's `conjure_input_block`, the editor's `input.inject` command, the
test harness's `input_script` - consumes these three types and nothing else.

| Id | Decision | Rationale |
|---|---|---|
| P1 | `window` is an abstract class created through `platform::create_window(const window_desc &) -> result<std::unique_ptr<window>>`; the backend is chosen by `window_desc::backend` (`backend_kind::GLFW` or `NULL_BACKEND`). The concrete classes `glfw_window` (private, `src/platform/glfw/`) and `null_backend` (public, so tests can drive it) are the only implementations. | One factory, one virtual interface, two implementations: the engine's `platform_subsystem` (startup step 6) and every test construct the window the same way; `headless_engine` holds a `platform::null_backend *` (`08_testing.md` 4.4). |
| P2 | `pump_events()` returns `std::span<const event>` into a window-owned `std::vector<event>` that is cleared, never shrunk, each frame. `event` is a `std::variant` of trivially copyable structs (`static_assert`ed); the one payload with external data (`drop_files_event`) points into a window-owned buffer valid until the next pump. | Allocation-free steady state after the first frame (`08_testing.md` allocation gates); callers iterate a span, never own events; layers can copy an event by value (`layer::on_event`). |
| P3 | `input` is backend-independent. `apply(const event &)` (engine frame step 3) and `inject(const input_event &)` (editor, tests) run the **same** function; `end_frame()` publishes `input_state` by copy and clears the edges. The `glfw` and `null` backends only differ in where events come from. | `08_testing.md` 8.4 demands identical edge/level behaviour on both backends; making `inject` *be* `apply` makes that a tautology rather than a test target. |
| P4 | Edge/level semantics are fixed: `PRESS` on an up key sets `down` and `pressed`; `PRESS` on a down key is ignored; `RELEASE` on a down key clears `down` and sets `released`; `RELEASE` on an up key is ignored; `REPEAT` never changes state (it is only visible as an event). Press and release within one frame leave `pressed` and `released` both set and `down` clear, so no tap is lost. Focus loss releases everything (`release_all()`). | Deterministic and testable ("pressed for exactly one frame then held"); OS key-repeat and GLFW's synthetic releases on focus loss cannot double-count. |
| P5 | Key codes use GLFW's numbering (`key::SPACE == 32`, ..., `key::MENU == 348`, `KEY_COUNT = 512`); mouse buttons, gamepad buttons and axes likewise mirror `GLFW_MOUSE_BUTTON_*`, `GLFW_GAMEPAD_BUTTON_*`, `GLFW_GAMEPAD_AXIS_*`. `key::UNKNOWN = 0` replaces `GLFW_KEY_UNKNOWN (-1)`. | The glfw backend maps with a range check instead of a table; the null backend, the C# `Key` enum and the editor protocol share one stable numbering. |
| P6 | The snapshot's key bitsets are `key_bitset { std::array<u64, 8> words; }` (512 bits) and gamepads are `std::array<gamepad_state, 4>` with six `f32` axes and `u32` button masks - the same shape as `conjure_input_block` (`03_scripting.md` 2.1). | The scripting subsystem copies the snapshot field by field into the frame block without conversion; the layout is asserted by `static_assert`s on sizes. |
| P7 | GLFW is initialised **once per process**: the first `create_window(GLFW)` calls `glfwInit()`, a main-thread reference count tracks live glfw windows, and `glfwTerminate()` runs when the last one is destroyed. The error callback is installed before `glfwInit()` and routes every GLFW error to `CONJURE_LOG_ERROR(LOG_PLATFORM, ...)` and into a last-error string that `make_error` messages quote. | GLFW is a process-global library; two owners would double-init or terminate under each other. Tests that create and destroy windows repeatedly stay correct because GLFW documents re-initialisation after terminate. |
| P8 | Every platform call is **main-thread only** (`CONJURE_ASSERT(core::jobs::is_main_thread())` in Debug on `create_window`, `pump_events`, `inject`, `end_frame`, every window setter, clipboard); the only cross-thread read is `input::snapshot()` between two `end_frame()` calls. No thread is created by this module. | GLFW requires it; the engine already runs the pump and all orchestration on main (`06_engine_loop.md` D1, 5). |
| P9 | Sizes come in two coordinate systems and both are always available: `window_size()` in logical screen coordinates and `framebuffer_size()` in pixels; `content_scale()` is their ratio per axis. Mouse positions are reported in logical coordinates. The glfw backend sets `GLFW_SCALE_TO_MONITOR` and `GLFW_SCALE_FRAMEBUFFER` (3.4) so high-DPI behaves the same on Windows, X11, Wayland and macOS. | The swapchain needs pixels, ImGui and picking need logical coordinates plus the scale; exposing both removes every per-OS branch outside the backend. |
| P10 | Resize events are **coalesced**: a pump that saw several framebuffer size callbacks appends exactly one `framebuffer_resized_event` with the final size at the end of the pump. A `0 x 0` framebuffer (minimised) is delivered as-is; `is_minimised()` becomes true. | The engine resizes the swapchain per event (`06_engine_loop.md` 3.1 step 3); ten resizes in one drag must not mean ten swapchain rebuilds. |
| P11 | Gamepads are polled, not event driven: each pump calls `glfwGetGamepadState` for the up to four connected gamepad-mapped joysticks and diffs against the previous poll, emitting `gamepad_button_event`s (edges) and `gamepad_axis_event`s (changed values). Joysticks without a gamepad mapping are ignored. Triggers are remapped from GLFW's `[-1, 1]` to `[0, 1]`; sticks stay `[-1, 1]`; no dead zone is applied here. | GLFW has no gamepad button events; polling once per frame on main is the documented pattern. Dead zones are a gameplay/bindings concern and must be identical on both backends, which is only possible if the platform layer applies none. |
| P12 | Fullscreen is **windowed fullscreen** (`glfwSetWindowMonitor` with the monitor's current video mode); monitors are enumerated through `window::monitors()` and selected by index in `window_desc::monitorIndex` or `set_fullscreen(true, index)`. No video-mode switching in v1. | No mode switches means no desktop corruption on crash and no DXGI/Vulkan exclusive-mode interaction with NVRHI; the current mode is what every modern engine defaults to. |
| P13 | Clipboard is text only (`clipboard_text()` / `set_clipboard_text()`), UTF-8; the null backend keeps an in-memory string. Cursor modes are `NORMAL`, `HIDDEN`, `DISABLED`, `CAPTURED` (GLFW 3.4) plus optional raw mouse motion. | Exactly what ImGui, the editor and a first-person camera need; images and files in the clipboard are future work. |
| P14 | Naming: struct fields camelCase (R31), class members `mCamelCase`, flags `enum class <x>_flags : uN { NONE = 0, ... }` + `CONJURE_ENUM_FLAGS`, no enumerator named after a Windows macro (`key::DEL`, not `DELETE`; R56 - the C# `Key.Delete` name is unaffected). The ABI enums of `15_bindings.md` 2.4 (`conjure_button_action`, `conjure_cursor_mode`) carry exactly these enumerator values. Public classes with out-of-line members and `create_window` carry `CONJURE_API` (`07_modules.md` D1). | CONVENTIONS 1 and 3; GLFW is linked exactly once into `conjure_shared`. |

What was rejected: a callback-based event API (hides ordering, allocates closures, forces re-entrancy rules on
every consumer); per-event heap allocation or `std::function` sinks; SDL (GLFW is a fixed decision); a platform
thread for the pump (GLFW forbids it); `std::bitset<512>` for key state (not guaranteed trivially copyable, no
fixed word layout for the C ABI mirror); a dead zone in `input` (would differ from what scripts see on `null`).

---

## 2. Data model

### 2.1 Enumerations (`backend.h`, `button_action.h`, `key.h`, `mouse_button.h`, `gamepad.h`)

```cpp
namespace conjure::platform {
    enum class backend_kind : u8 { NULL_BACKEND = 0, GLFW = 1, COUNT };
    enum class button_action : u8 { RELEASE = 0, PRESS = 1, REPEAT = 2 };   // GLFW_RELEASE/PRESS/REPEAT

    inline constexpr u32 KEY_COUNT = 512;
    enum class key : u16 {                   // GLFW_KEY_* values; UNKNOWN replaces GLFW_KEY_UNKNOWN (-1)
        UNKNOWN = 0, SPACE = 32, APOSTROPHE = 39, COMMA = 44, MINUS, PERIOD, SLASH,
        NUM_0 = 48, NUM_1, NUM_2, NUM_3, NUM_4, NUM_5, NUM_6, NUM_7, NUM_8, NUM_9,
        SEMICOLON = 59, EQUAL = 61, A = 65, B, C, /* ... */ Z = 90,
        LEFT_BRACKET = 91, BACKSLASH, RIGHT_BRACKET, GRAVE_ACCENT = 96, WORLD_1 = 161, WORLD_2 = 162,
        ESCAPE = 256, ENTER, TAB, BACKSPACE, INSERT, DEL /* DELETE is a Windows macro */, RIGHT, LEFT, DOWN, UP,
        PAGE_UP, PAGE_DOWN, HOME, END, CAPS_LOCK = 280, SCROLL_LOCK, NUM_LOCK, PRINT_SCREEN, PAUSE,
        F1 = 290, F2, /* ... */ F25 = 314, KP_0 = 320, /* ... */ KP_9 = 329, KP_DECIMAL, KP_DIVIDE, KP_MULTIPLY,
        KP_SUBTRACT, KP_ADD, KP_ENTER, KP_EQUAL, LEFT_SHIFT = 340, LEFT_CONTROL, LEFT_ALT, LEFT_SUPER,
        RIGHT_SHIFT, RIGHT_CONTROL, RIGHT_ALT, RIGHT_SUPER, MENU = 348, LAST = MENU
    };
    enum class key_modifier_flags : u8 {     // GLFW_MOD_* values
        NONE = 0, SHIFT = 1 << 0, CONTROL = 1 << 1, ALT = 1 << 2, SUPER = 1 << 3, CAPS_LOCK = 1 << 4, NUM_LOCK = 1 << 5
    };
    CONJURE_ENUM_FLAGS(key_modifier_flags);

    inline constexpr u32 MOUSE_BUTTON_COUNT = 8;
    enum class mouse_button : u8 { LEFT = 0, RIGHT = 1, MIDDLE = 2, BUTTON_4, BUTTON_5, BUTTON_6, BUTTON_7, BUTTON_8, COUNT };

    inline constexpr u32 MAX_GAMEPADS = 4;
    inline constexpr u32 GAMEPAD_BUTTON_COUNT = 15;
    inline constexpr u32 GAMEPAD_AXIS_COUNT = 6;
    enum class gamepad_button : u8 {         // GLFW_GAMEPAD_BUTTON_* (SDL game controller layout)
        A = 0, B, X, Y, LEFT_BUMPER, RIGHT_BUMPER, BACK, START, GUIDE, LEFT_THUMB, RIGHT_THUMB,
        DPAD_UP, DPAD_RIGHT, DPAD_DOWN, DPAD_LEFT, COUNT
    };
    enum class gamepad_axis : u8 { LEFT_X = 0, LEFT_Y, RIGHT_X, RIGHT_Y, LEFT_TRIGGER, RIGHT_TRIGGER, COUNT };
}
```

Each enum header offers `[[nodiscard]] std::string_view to_string(<enum>) noexcept` (enumerator name, `""` for
unknown values) and `[[nodiscard]] std::optional<<enum>> parse_<enum>(std::string_view) noexcept` (case-insensitive;
used by the editor protocol's `input.inject` and by `project.json`). `key.h` additionally has
`[[nodiscard]] constexpr bool is_valid_key(u32 code) noexcept` (`0 < code <= LAST`).

### 2.2 Events (`event.h`)

```cpp
namespace conjure::platform {
    struct key_event            { key keyCode = key::UNKNOWN; i32 scancode = 0; button_action action = button_action::RELEASE; key_modifier_flags modifiers = key_modifier_flags::NONE; };
    struct text_event           { u32 codepoint = 0; };                        // UTF-32, from glfwSetCharCallback
    struct mouse_move_event     { math::vec2 position = math::vec2(0.0f); };   // logical window coordinates
    struct mouse_button_event   { mouse_button button = mouse_button::LEFT; button_action action = button_action::RELEASE; key_modifier_flags modifiers = key_modifier_flags::NONE; };
    struct mouse_scroll_event   { math::vec2 offset = math::vec2(0.0f); };     // +y = away from the user
    struct cursor_enter_event   { bool entered = false; };
    struct gamepad_connection_event { u8 gamepadIndex = 0; bool connected = false; };
    struct gamepad_button_event { u8 gamepadIndex = 0; gamepad_button button = gamepad_button::A; button_action action = button_action::RELEASE; };
    struct gamepad_axis_event   { u8 gamepadIndex = 0; gamepad_axis axis = gamepad_axis::LEFT_X; f32 value = 0.0f; };
    struct framebuffer_resized_event { u32 width = 0; u32 height = 0; };       // pixels; 0 x 0 while minimised
    struct content_scale_changed_event { math::vec2 scale = math::vec2(1.0f); };
    struct focus_event          { bool focused = false; };
    struct minimise_event       { bool minimised = false; };
    struct close_event          {};
    struct drop_files_event     { std::span<const std::string> paths = {}; };  // valid until the next pump_events()

    using input_event = std::variant<key_event, text_event, mouse_move_event, mouse_button_event, mouse_scroll_event,
                                     cursor_enter_event, gamepad_connection_event, gamepad_button_event, gamepad_axis_event>;
    using event = std::variant<key_event, text_event, mouse_move_event, mouse_button_event, mouse_scroll_event,
                               cursor_enter_event, gamepad_connection_event, gamepad_button_event, gamepad_axis_event,
                               framebuffer_resized_event, content_scale_changed_event, focus_event, minimise_event,
                               close_event, drop_files_event>;
    static_assert(std::is_trivially_copyable_v<event>);

    [[nodiscard]] bool is_input_event(const event &value) noexcept;
    [[nodiscard]] std::optional<input_event> to_input_event(const event &value) noexcept;
    [[nodiscard]] event to_event(const input_event &value) noexcept;
    template <typename EventType> [[nodiscard]] const EventType *event_as(const event &value) noexcept;  // std::get_if
}
```

Both variants list the input alternatives in the same order, so `to_event` / `to_input_event` are index remaps,
not switches. `event_kind` is not a separate enum: `event.index()` is the kind, and `to_string(const event &)`
(for the editor console and `[window]` diagnostics) formats the alternative name and payload.

### 2.3 Snapshot (`input_state.h`)

```cpp
namespace conjure::platform {
    struct key_bitset {                                   // 512 bits, same layout as conjure_input_block::keys_*
        std::array<u64, 8> words = {};
        [[nodiscard]] constexpr bool test(key code) const noexcept;
        constexpr void set(key code) noexcept;  constexpr void reset(key code) noexcept;  constexpr void clear() noexcept;
        [[nodiscard]] constexpr bool any() const noexcept;  [[nodiscard]] constexpr u32 count() const noexcept;
    };
    struct gamepad_state {
        bool connected = false;
        u32 buttonsDown = 0; u32 buttonsPressed = 0; u32 buttonsReleased = 0;     // bit i = gamepad_button(i)
        std::array<f32, GAMEPAD_AXIS_COUNT> axes = {};                           // sticks [-1, 1], triggers [0, 1]
        [[nodiscard]] bool is_down(gamepad_button) const noexcept; is_pressed(...); is_released(...);
        [[nodiscard]] f32 axis(gamepad_axis) const noexcept;
    };
    inline constexpr u32 MAX_TEXT_CODEPOINTS_PER_FRAME = 16;
    struct input_state {
        u64 frameIndex = 0;                               // incremented by every end_frame()
        key_bitset keysDown = {}; key_bitset keysPressed = {}; key_bitset keysReleased = {};
        key_modifier_flags modifiers = key_modifier_flags::NONE;                 // from the latest key/mouse event
        math::vec2 mousePosition = math::vec2(0.0f);      // logical coordinates
        math::vec2 mouseDelta = math::vec2(0.0f);         // sum of movement this frame (works with cursor_mode::DISABLED)
        math::vec2 scrollDelta = math::vec2(0.0f);        // sum of scroll offsets this frame
        u32 mouseButtonsDown = 0; u32 mouseButtonsPressed = 0; u32 mouseButtonsReleased = 0;
        bool cursorInWindow = true;
        std::array<gamepad_state, MAX_GAMEPADS> gamepads = {};
        std::array<u32, MAX_TEXT_CODEPOINTS_PER_FRAME> textCodepoints = {}; u32 textCount = 0;
        [[nodiscard]] bool is_key_down(key) const noexcept; is_key_pressed(key); is_key_released(key); is_key_held(key) /* down && !pressed */;
        [[nodiscard]] bool is_mouse_button_down(mouse_button) const noexcept; is_mouse_button_pressed(...); is_mouse_button_released(...);
        [[nodiscard]] const gamepad_state &gamepad(u32 index) const noexcept;   // asserts index < MAX_GAMEPADS
        [[nodiscard]] std::span<const u32> text() const noexcept;
    };
    static_assert(std::is_trivially_copyable_v<input_state>);
    static_assert(sizeof(key_bitset) == 64);
}
```

### 2.4 Window description, monitors, native handle (`window.h`, `monitor.h`, `native_handle.h`)

```cpp
namespace conjure::platform {
    enum class window_flags : u8 {
        NONE = 0, RESIZABLE = 1 << 0, DECORATED = 1 << 1, VISIBLE = 1 << 2, FLOATING = 1 << 3, MAXIMISED = 1 << 4,
        SCALE_TO_MONITOR = 1 << 5, SCALE_FRAMEBUFFER = 1 << 6,
        DEFAULT = RESIZABLE | DECORATED | VISIBLE | SCALE_TO_MONITOR | SCALE_FRAMEBUFFER
    };
    CONJURE_ENUM_FLAGS(window_flags);
    enum class display_server_hint : u8 { ANY = 0, X11, WAYLAND };          // Linux only; GLFW_PLATFORM init hint
    enum class cursor_mode : u8 { NORMAL = 0, HIDDEN, DISABLED, CAPTURED }; // GLFW_CURSOR_* (CAPTURED is 3.4)

    struct window_desc {
        backend_kind backend = backend_kind::GLFW;
        std::string title = "Conjure";
        u32 width = 1600; u32 height = 900;               // logical size; the engine passes window_settings (06 2.3)
        window_flags flags = window_flags::DEFAULT;
        bool fullscreen = false; u32 monitorIndex = 0;    // windowed fullscreen on that monitor (P12)
        std::optional<math::ivec2> position = std::nullopt;   // nullopt = let the OS / centre on the monitor
        display_server_hint displayServer = display_server_hint::ANY;
        std::string_view gamepadMappings = {};            // extra SDL_GameControllerDB lines (glfwUpdateGamepadMappings)
        math::vec2 nullContentScale = math::vec2(1.0f);  // null backend only: reported content scale
    };

    struct video_mode { u32 width = 0; u32 height = 0; u32 refreshRateHz = 0; };
    struct monitor_info {
        std::string name = {}; bool isPrimary = false;
        math::ivec2 position = math::ivec2(0);           // virtual desktop, logical coordinates
        video_mode currentMode = {};                      // pixels
        math::vec2 contentScale = math::vec2(1.0f);
        math::urect workArea = {};                        // logical coordinates, excludes task bars
    };

    enum class native_handle_kind : u8 { NONE = 0, WIN32, COCOA, X11, WAYLAND };
    struct native_window_handle {
        native_handle_kind kind = native_handle_kind::NONE;
        void *window = nullptr;     // HWND | NSWindow * | wl_surface *   (X11 uses x11Window)
        void *display = nullptr;    // HINSTANCE | nullptr | Display * | wl_display *
        u64 x11Window = 0;          // X11 Window id (an unsigned long, not a pointer)
    };
}
```

---

## 3. Algorithms and flow

### 3.1 Frame (main thread; `06_engine_loop.md` 3.1 step 3)

```
events = window.pump_events()         glfw: glfwPollEvents() fills mEvents through callbacks; then gamepad poll
                                             (P11); then one coalesced framebuffer_resized_event if any (P10)
                                      null:  swap(mPending, mDelivered); return mDelivered
for e in events:
    if layers.dispatch_event(e): continue          (ImGui / editor consumed it)
    input.apply(e)                                 (window events are accepted and ignored except focus_event)
    close_event             -> engine request_quit(OK)
    framebuffer_resized_event (w, h > 0) -> swapchain.resize ; renderer.on_swapchain_resized
    focus_event             -> pacer.set_unfocused(!focused)
input.end_frame()                                  publishes the snapshot read by scripting / bindings / systems
```

`pump_events()` must be called exactly once per frame; calling it twice is legal (the second call returns the new
batch) but the engine never does. The span returned by the previous call is invalidated by the next call.

### 3.2 Input state machine (`input::apply`, identical for `inject`)

```
key_event k:          if k.keyCode == UNKNOWN: ignore for bitsets (still stored modifiers)
                      PRESS   && !down[k]: down.set, pressed.set
                      RELEASE &&  down[k]: down.reset, released.set
                      REPEAT: nothing          modifiers = k.modifiers
mouse_button_event:   same rule on u32 masks; modifiers updated
mouse_move_event:     mouseDelta += position - mousePosition (skipped on the first event after a cursor_mode change
                      or cursor re-entry to avoid a jump); mousePosition = position
mouse_scroll_event:   scrollDelta += offset
cursor_enter_event:   cursorInWindow = entered
text_event:           if textCount < 16: textCodepoints[textCount++] = codepoint   (excess dropped, counted in stats)
gamepad_connection:   connected = flag; on connect zero the pad; on disconnect release every down button (edges) and zero axes
gamepad_button_event: same press/release rule on the pad's masks
gamepad_axis_event:   axes[axis] = value (level only)
focus_event(false):   release_all()     (every down key / mouse / gamepad button gets a released edge this frame)
other window events:  ignored; apply returns false
```

`end_frame()`: `mPublished = mPending; mPublished.frameIndex = ++mFrameCounter;` then in `mPending` clear
`keysPressed`, `keysReleased`, `*Pressed`, `*Released` masks (keys, mouse, every gamepad), `mouseDelta`,
`scrollDelta`, `textCount`. Levels (`keysDown`, positions, axes, `connected`, `cursorInWindow`, `modifiers`)
persist. The copy is one `memcpy` of roughly 600 bytes; `snapshot()` returns `const input_state &` to `mPublished`.

`release_all()` is public (the editor's `input.clear` command, `04_editor.md` 4.4) and is also what a `focus_event`
with `focused == false` triggers. Because GLFW itself synthesises release events on focus loss, the same key may be
released twice in one frame; rule P4 makes the second release a no-op.

### 3.3 GLFW window creation (`glfw_window::create`)

1. If no glfw window is alive: `glfwSetErrorCallback(glfw_error_callback)`; init hints `GLFW_PLATFORM` from
   `displayServer` (Linux; `GLFW_ANY_PLATFORM` otherwise), `GLFW_COCOA_CHDIR_RESOURCES = GLFW_FALSE` (GLFW would
   otherwise change the working directory on macOS and break project-relative paths); `glfwInit()`; failure ->
   `platform_error::INIT_FAILED` with the last GLFW error text. Log `glfwGetVersionString()` and `glfwGetPlatform()`
   at INFO.
2. Window hints: `GLFW_CLIENT_API = GLFW_NO_API` (NVRHI owns the graphics API; never an OpenGL context),
   `GLFW_RESIZABLE/DECORATED/VISIBLE/FLOATING/MAXIMIZED` from `flags`, `GLFW_SCALE_TO_MONITOR`,
   `GLFW_SCALE_FRAMEBUFFER` (3.4), `GLFW_FOCUS_ON_SHOW = GLFW_TRUE`, `GLFW_POSITION_X/Y` (3.4) when `position` is set.
3. Resolve `monitorIndex` against `glfwGetMonitors` when `fullscreen` (`MONITOR_NOT_FOUND` if out of range);
   `glfwCreateWindow(width, height, title, monitor, nullptr)`; failure -> `WINDOW_CREATE_FAILED`.
4. `glfwSetWindowUserPointer(this)`; install callbacks: key, char, cursor pos, cursor enter, mouse button, scroll,
   drop, framebuffer size, window content scale, focus, iconify, close, joystick (process-wide; dispatched to the
   single live window). Callbacks only ever run inside `glfwPollEvents`, i.e. inside `pump_events` on main.
5. Read the initial `framebuffer_size`, `window_size`, `content_scale`, focus and iconified state; scan joysticks
   `GLFW_JOYSTICK_1..16`, assign up to four `glfwJoystickIsGamepad` ones to slots in id order and queue a
   `gamepad_connection_event` for each so the first pump reports them. Apply `gamepadMappings` if non-empty.
6. `mEvents.reserve(256)`, `mDropPaths.reserve(8)`. The reference count is incremented; the destructor decrements it
   and calls `glfwTerminate()` at zero.

### 3.4 GLFW pump (`glfw_window::pump_events`)

```
mEvents.clear(); mDropPaths.clear(); mPendingResize.reset()
glfwPollEvents()                                  callbacks push events in arrival order; framebuffer size callbacks
                                                  only record mPendingResize; drop callback copies paths into mDropPaths
                                                  and pushes one drop_files_event spanning them
for slot in 0..3 with a joystick id:
    GLFWgamepadstate s; if !glfwGetGamepadState(id, &s): continue   (disconnect arrives through the joystick callback)
    for each button: if s.buttons[b] != prev.buttons[b]: push gamepad_button_event(PRESS/RELEASE)
    for each axis: v = remap(s.axes[a]); if v != prev.axes[a]: push gamepad_axis_event(v)
    prev = s
if mPendingResize: push framebuffer_resized_event(*mPendingResize); mMinimised = (w == 0 || h == 0)
return mEvents
```

Steady state: zero heap allocations (vectors keep capacity; `std::string` paths allocate only when files are
dropped). `should_close()` is `glfwWindowShouldClose`; `request_close()` sets it, mirroring the null backend.

### 3.5 Null backend (`null_backend`)

Holds `mLogicalSize`, `mContentScale`, `mFocused = true`, `mMinimised = false`, `mFullscreen`, `mCursorMode`,
`mClipboard`, one virtual monitor `{"Null Monitor", 1920 x 1080 @ 60, scale 1}`, `mPending` and `mDelivered` event
vectors. Test controls (all main thread) queue events for the next pump: `push_event(const event &)`,
`simulate_resize(u32 width, u32 height)` (logical; framebuffer = `round(size * scale)`; queues
`framebuffer_resized_event`), `simulate_content_scale(math::vec2)`, `simulate_focus(bool)`, `simulate_minimise(bool)`,
`simulate_close()`, `simulate_drop_files(std::span<const std::string>)`. `pump_events` swaps the two vectors and
returns the delivered one; with nothing queued it returns an empty span, which is what `06_engine_loop.md` 7 states.
`set_fullscreen(true)` resizes to the virtual monitor mode and queues the resize event; `set_size`, `set_title`,
`set_cursor_mode`, clipboard setters update state so getters round-trip (so the bindings' `input_clipboard_*` and
`input_set_cursor_mode` are real implementations on the null window, not stubs; R57). `gamepad_name(i)` returns
`"Null Gamepad <i>"` for a slot a test connected through a `gamepad_connection_event`, else an empty view.

---

## 4. Public API (`engine/include/conjure/platform/`)

All headers: `#pragma once`, Conjure + standard includes only, `/// <summary>` on public methods, `CONJURE_API` on
classes with out-of-line definitions and on free functions defined in `.cpp`.

| Header | Contents |
|---|---|
| `log_category.h` | `CONJURE_DECLARE_LOG_CATEGORY(LOG_PLATFORM);` (defined as `"platform"` in `platform.cpp`) |
| `platform_error.h` | `enum class platform_error : u16`, `platform_error_name`, `CONJURE_DECLARE_ERROR_CATEGORY(platform_error, "platform", platform_error_name)` |
| `backend.h` | `backend_kind`, `to_string`, `parse_backend_kind`, `is_backend_compiled`, `default_backend_kind` |
| `button_action.h` | `button_action`, `to_string`, `parse_button_action` |
| `key.h` | `key`, `key_modifier_flags`, `KEY_COUNT`, `is_valid_key`, `to_string`, `parse_key` |
| `mouse_button.h` | `mouse_button`, `MOUSE_BUTTON_COUNT`, `to_string`, `parse_mouse_button` |
| `gamepad.h` | `gamepad_button`, `gamepad_axis`, `MAX_GAMEPADS`, `GAMEPAD_*_COUNT`, `to_string`, parsers |
| `event.h` | the event structs, `input_event`, `event`, `is_input_event`, `to_input_event`, `to_event`, `event_as<T>`, `to_string(const event &)` |
| `input_state.h` | `key_bitset`, `gamepad_state`, `input_state`, `MAX_TEXT_CODEPOINTS_PER_FRAME` |
| `input.h` | `class input`, `input_stats` |
| `monitor.h` | `video_mode`, `monitor_info` |
| `native_handle.h` | `native_handle_kind`, `native_window_handle` |
| `window.h` | `window_flags`, `display_server_hint`, `cursor_mode`, `window_desc`, `class window`, `create_window` |
| `null_backend.h` | `class null_backend final : public window` with the simulation controls of 3.5 |
| `platform.h` | umbrella include; `[[nodiscard]] std::string_view backend_version_string(backend_kind) noexcept` (`glfwGetVersionString()` / `"null"`) |

### 4.1 `platform_error.h` and `backend.h`

```cpp
namespace conjure::platform {
    enum class platform_error : u16 {
        BACKEND_NOT_COMPILED = 1,   // backend_kind requested but CONJURE_PLATFORM_GLFW is OFF
        UNKNOWN_BACKEND,            // parse_backend_kind: name is neither "null" nor "glfw"
        INIT_FAILED,                // glfwInit failed (no display server, missing libraries); message has GLFW text
        INVALID_DESC,               // zero size when not fullscreen, title longer than 255 bytes, second live window (Distribution)
        WINDOW_CREATE_FAILED,       // glfwCreateWindow returned null
        MONITOR_NOT_FOUND,          // monitorIndex >= monitors().size()
        VULKAN_UNSUPPORTED,         // glfwVulkanSupported() false, or any Vulkan call on the null backend
        SURFACE_CREATE_FAILED,      // glfwCreateWindowSurface returned a VkResult != VK_SUCCESS (code in message)
        CLIPBOARD_UNAVAILABLE,      // glfwGetClipboardString returned null (no text / platform error)
        FEATURE_UNAVAILABLE,        // raw mouse motion, cursor_mode::CAPTURED or window positioning unsupported here
        COUNT
    };
    [[nodiscard]] std::string_view platform_error_name(platform_error code) noexcept;
}
CONJURE_DECLARE_ERROR_CATEGORY(conjure::platform::platform_error, "platform", conjure::platform::platform_error_name);

namespace conjure::platform {
    [[nodiscard]] std::string_view to_string(backend_kind kind) noexcept;                       // "null" | "glfw"
    [[nodiscard]] core::result<backend_kind> parse_backend_kind(std::string_view name) noexcept;  // UNKNOWN_BACKEND
    [[nodiscard]] bool is_backend_compiled(backend_kind kind) noexcept;
    [[nodiscard]] backend_kind default_backend_kind() noexcept;   // GLFW when compiled, else NULL_BACKEND
}
```

Errors are built with `core::make_error(platform_error::X, "...")`; generic conditions use `core::error_code`
(`INVALID_ARGUMENT` for a bad gamepad index, `NOT_FOUND` for an unknown key name in a parser result).

### 4.2 `window.h`

```cpp
namespace conjure::platform {
    class null_backend;

    class CONJURE_API window {
        public:
            window();
            virtual ~window();
            window(const window &) = delete;
            window &operator=(const window &) = delete;

        public:
            [[nodiscard]] virtual backend_kind backend() const noexcept = 0;

            /// <summary>Processes OS events and returns this frame's batch; the span is valid until the next call.</summary>
            [[nodiscard]] virtual std::span<const event> pump_events() = 0;

            [[nodiscard]] virtual bool should_close() const noexcept = 0;
            virtual void request_close() noexcept = 0;

            [[nodiscard]] virtual math::uvec2 framebuffer_size() const noexcept = 0;   // pixels
            [[nodiscard]] virtual math::uvec2 window_size() const noexcept = 0;        // logical
            [[nodiscard]] virtual math::vec2 content_scale() const noexcept = 0;
            [[nodiscard]] virtual bool is_minimised() const noexcept = 0;
            [[nodiscard]] virtual bool is_focused() const noexcept = 0;
            [[nodiscard]] virtual bool is_visible() const noexcept = 0;
            [[nodiscard]] virtual bool is_fullscreen() const noexcept = 0;

            virtual void set_title(std::string_view title) = 0;
            [[nodiscard]] virtual std::string_view title() const noexcept = 0;
            virtual void set_size(math::uvec2 logicalSize) = 0;
            [[nodiscard]] virtual core::result<void> set_position(math::ivec2 position) = 0;   // FEATURE_UNAVAILABLE on Wayland
            [[nodiscard]] virtual math::ivec2 position() const noexcept = 0;
            virtual void show() = 0;
            virtual void hide() = 0;

            /// <summary>Windowed fullscreen on monitors()[monitorIndex] with its current video mode (P12).</summary>
            [[nodiscard]] virtual core::result<void> set_fullscreen(bool fullscreen, u32 monitorIndex = 0) = 0;
            [[nodiscard]] virtual std::span<const monitor_info> monitors() const noexcept = 0;   // refreshed by pump_events
            [[nodiscard]] virtual std::span<const video_mode> video_modes(u32 monitorIndex) const noexcept = 0;

            [[nodiscard]] virtual core::result<void> set_cursor_mode(cursor_mode mode) = 0;
            [[nodiscard]] virtual cursor_mode current_cursor_mode() const noexcept = 0;
            [[nodiscard]] virtual core::result<void> set_raw_mouse_motion(bool enabled) = 0;
            virtual void set_cursor_position(math::vec2 logicalPosition) = 0;

            /// <summary>Human-readable name of a connected gamepad slot (glfwGetGamepadName); empty when not connected (R57).</summary>
            [[nodiscard]] virtual std::string_view gamepad_name(u32 index) const noexcept = 0;

            /// <summary>Opaque per-OS handles for rhi (D3D12 swapchain, MoltenVK). kind == NONE on the null backend.</summary>
            [[nodiscard]] virtual native_window_handle native_handle() const noexcept = 0;

            /// <summary>Instance extensions Vulkan needs to present to this window; empty on the null backend.</summary>
            [[nodiscard]] virtual std::span<const char *const> required_vulkan_instance_extensions() const noexcept = 0;

            /// <summary>
            /// Creates a VkSurfaceKHR for VkInstance 'instance' and stores it in *surface (both are 64-bit handles
            /// passed as opaque pointers; no Vulkan header is involved). VULKAN_UNSUPPORTED on the null backend.
            /// </summary>
            [[nodiscard]] virtual core::result<void> create_vulkan_surface(void *instance, void **surface) const = 0;

            [[nodiscard]] virtual core::result<std::string> clipboard_text() const = 0;
            [[nodiscard]] virtual core::result<void> set_clipboard_text(std::string_view text) = 0;

            /// <summary>This window as the null backend, or nullptr for a real window.</summary>
            [[nodiscard]] virtual null_backend *as_null() noexcept;
    };

    /// <summary>Creates the window for desc.backend. Main thread only. Exactly one live window per process in v1 (asserted).</summary>
    [[nodiscard]] CONJURE_API core::result<std::unique_ptr<window>> create_window(const window_desc &desc);
}
```

### 4.3 `input.h`

```cpp
namespace conjure::platform {
    struct input_stats { u64 eventsApplied = 0; u64 eventsInjected = 0; u64 textCodepointsDropped = 0; u64 ignoredTransitions = 0; };

    class CONJURE_API input {
        private:
            input_state mPending = {};      // accumulating; written by apply/inject
            input_state mPublished = {};    // immutable between end_frame() calls; returned by snapshot()
            input_stats mStats = {};
            u64 mFrameCounter = 0;
            bool mSkipNextMouseDelta = true;

        public:
            input();
            ~input();

        public:
            /// <summary>Applies one pumped event. Returns true when it was an input event (window events return false).</summary>
            bool apply(const event &value);

            /// <summary>Same state machine as apply(); the source for tests and the editor's input.inject (P3).</summary>
            void inject(const input_event &value);
            void inject(const event &value);   // convenience; ignores non-input events

            /// <summary>Publishes the frame's snapshot and clears edges, deltas, scroll and text.</summary>
            void end_frame();

            /// <summary>The snapshot published by the last end_frame(). Readable from any thread until the next end_frame().</summary>
            [[nodiscard]] const input_state &snapshot() const noexcept;

            /// <summary>Releases every down key and button with a released edge this frame (focus loss, input.clear).</summary>
            void release_all() noexcept;

            [[nodiscard]] const input_stats &stats() const noexcept;
    };
}
```

### 4.4 `null_backend.h`

`class CONJURE_API null_backend final : public window` implements every virtual of 4.2 as described in 3.5 and adds
`push_event(const event &)`, `simulate_resize(u32, u32)`, `simulate_content_scale(math::vec2)`, `simulate_focus(bool)`,
`simulate_minimise(bool)`, `simulate_close()`, `simulate_drop_files(std::span<const std::string>)`,
`[[nodiscard]] usize pending_event_count() const noexcept`, and `static std::unique_ptr<null_backend> create(const window_desc &)`
(what `create_window` calls for `NULL_BACKEND`). `as_null()` returns `this`.

### 4.5 Files

`engine/src/platform/`: `platform.cpp` (log category, `create_window`, backend queries, version string),
`platform_error.cpp`, `enum_names.cpp` (all `to_string`/`parse_*`), `event.cpp`, `input_state.cpp`, `input.cpp`,
`null_backend.cpp`, `glfw/glfw_window.{h,cpp}` (window + callbacks + gamepad poll), `glfw/glfw_context.{h,cpp}`
(init reference count, error callback, last error, joystick dispatch), `glfw/glfw_native.cpp` (the only file
including `glfw3native.h`, one `#if CONJURE_PLATFORM_*` block per OS, CONVENTIONS 3), `README.md`. CMake:
`conjure_add_module(platform SOURCES ... PUBLIC_DEPS conjure_core PRIVATE_DEPS $<$<BOOL:${CONJURE_PLATFORM_GLFW}>:conjure::thirdparty::glfw>)`
with the new option `CONJURE_PLATFORM_GLFW` (default ON; OFF yields a null-only library for server builds;
listed in `ARCHITECTURE.md` 9, R59), private definition `CONJURE_PLATFORM_HAS_GLFW=0|1`. `Dependencies.cmake` pins GLFW `3.4` with `GLFW_BUILD_DOCS/EXAMPLES/TESTS
OFF`, `GLFW_INSTALL OFF`, `GLFW_BUILD_WAYLAND ON` and `GLFW_BUILD_X11 ON` on Linux (CI installs `libwayland-dev
libxkbcommon-dev wayland-protocols xorg-dev`), `GLFW_LIBRARY_TYPE STATIC`.

---

## 5. Threading and ownership

| What | Thread | Notes |
|---|---|---|
| `create_window`, `~window`, `pump_events`, every window setter/getter that touches GLFW, clipboard, `monitors()` | main only | asserted in Debug via `core::jobs::is_main_thread()`; GLFW's own rule |
| `input::apply/inject/end_frame/release_all` | main only | the editor's protocol server polls on main (R38), so `input.inject` is main too |
| `input::snapshot()` | any | returns a reference to data written only inside `end_frame()`; readers must not hold it across an `end_frame()` (the engine reads it in frame step 5, scripting copies it into the frame block) |
| GLFW callbacks, joystick callback | main, inside `glfwPollEvents` | they only append to `mEvents`; no callback calls back into the engine |
| Threads created | none | `08_testing.md` 8.1 and CONVENTIONS 3 |

Ownership: `engine::application` owns the `window` and the `input` through `engine_services` (`platform_subsystem`,
startup step 6, shut down last, R22) and exposes `engine_context::window` / `input` as non-owning pointers. The
window owns its event and drop-path buffers, the monitor list and (glfw) the `GLFWwindow *`; GLFW itself is owned by
the process-wide reference count in `glfw_context`. Nothing else holds a `GLFWwindow *`: `rhi` receives only
`native_window_handle` and the two Vulkan helpers. One live window per process is a v1 rule (`CONJURE_ASSERT` on the
second `create_window`); multi-window is listed in section 11.

---

## 6. Failure handling

| Situation | Behaviour |
|---|---|
| `backend == GLFW` but `CONJURE_PLATFORM_GLFW` is OFF | `create_window` -> `BACKEND_NOT_COMPILED`; the engine falls back to `NULL_BACKEND` only when `config.headless` |
| `glfwInit` fails (no `DISPLAY`/`WAYLAND_DISPLAY`, missing `libwayland`/X11 libraries, unsupported platform) | `INIT_FAILED` with GLFW's error text; `glfwTerminate` is not called (init did not happen). `[window]` tests gate on `precondition::REAL_WINDOW` so CI never hits this |
| `glfwCreateWindow` fails | `WINDOW_CREATE_FAILED`; the init reference is released (terminate if it was the first window); engine maps it to `engine_error::WINDOW_CREATE_FAILED` |
| `window_desc` invalid (zero size when not fullscreen, title > 255 bytes) | `INVALID_DESC` naming the field; nothing created |
| `monitorIndex` out of range | `MONITOR_NOT_FOUND`; `set_fullscreen` leaves the window unchanged |
| `glfwVulkanSupported()` false / surface creation fails | `VULKAN_UNSUPPORTED` / `SURFACE_CREATE_FAILED` with the `VkResult` value; rhi reports `DEVICE_CREATION_FAILED` |
| Clipboard has no text or the platform refused | `CLIPBOARD_UNAVAILABLE`; ImGui's callback returns an empty string |
| `cursor_mode::CAPTURED`, raw mouse motion, `set_position` unsupported (Wayland) | `FEATURE_UNAVAILABLE`, state unchanged, `WARN` once per feature |
| Any GLFW error callback invocation | `CONJURE_LOG_ERROR(LOG_PLATFORM, "GLFW {} ({}): {}", name, code, description)`; tests that provoke one use `allow_errors()` |
| Key code outside `[1, 348]`, mouse button >= 8, gamepad index >= 4 in an event | ignored for state, counted in `input_stats::ignoredTransitions`; `CONJURE_ASSERT` in Debug for the gamepad index (programmer error on the injecting side) |
| More than 16 text codepoints in one frame | extra dropped, `textCodepointsDropped++` |
| `pump_events` while minimised | returns normally; the engine skips submit (`06_engine_loop.md` 6) |
| Second `create_window` while one is alive | `CONJURE_ASSERT`; in Distribution returns `INVALID_DESC` |

No frame-time call returns an error; `pump_events`, `apply`, `end_frame` and `snapshot` cannot fail.

---

## 7. Headless and null behaviour

- `null_backend` created by `create_window({backend = NULL_BACKEND, width = 1280, height = 720})` under
  `engine_config::headless_defaults()` (R33). No OS call of any kind; valgrind/ASan/TSan clean; constructible in a
  process without a display server.
- `pump_events()` returns an empty span unless a test queued events; `should_close()` false until `request_close()`
  or `simulate_close()` (`06_engine_loop.md` 7); `is_minimised()` false, `is_focused()` true, `is_visible()` follows
  `window_flags::VISIBLE`, `content_scale()` = `nullContentScale`, `framebuffer_size()` = `round(window_size * scale)`.
- `native_handle().kind == NONE`; `required_vulkan_instance_extensions()` empty; `create_vulkan_surface` ->
  `VULKAN_UNSUPPORTED` (rhi uses its `null` device headless, never a surface).
- `input` has no backend: `inject` on a null-backed engine produces exactly the snapshot `apply` would on glfw
  (`08_testing.md` 8.4), and `test_backend_conformance` proves it by feeding the same event list through both.
- Determinism: the null window and `input` contain no clock, random state or OS query; the same injected sequence
  yields byte-identical snapshots across runs and platforms (`frameIndex` included).

---

## 8. Integration points

**Needs from `core` (wave 1):** `result<T>` / `error` / `make_error` / `CONJURE_DECLARE_ERROR_CATEGORY`;
`CONJURE_DECLARE/DEFINE_LOG_CATEGORY`, `CONJURE_LOG_*`; `CONJURE_ASSERT`; `enum_flags.h`; `math` (`vec2`, `ivec2`,
`uvec2`, `urect`); `core::jobs::is_main_thread()`; `core::str::iequals` for the parsers; `platform_detect.h`
(`CONJURE_PLATFORM_*` for `glfw_native.cpp`); `api.h` (`CONJURE_API`). No containers beyond `std::vector`/`std::array`.

**Offers to `engine` (`06_engine_loop.md` 3.1, 3.4, 8):** `create_window` for `platform_subsystem` (step 6; maps
`window_settings {title, width, height, fullscreen, resizable}` to `window_desc`, `backend` = `NULL_BACKEND` when
`headless`), `pump_events` / `apply` / `end_frame` / `snapshot` for frame step 3, `close_event`,
`framebuffer_resized_event`, `focus_event` for quit/resize/pacer, `is_minimised` for step 11, `engine_context::window`
and `input`.

**Offers to `rhi` (`02_renderer.md` 4.1, 8):** `window::native_handle()` for the D3D12 swapchain (`HWND`) and MoltenVK
(`NSWindow *`), `required_vulkan_instance_extensions()` and `create_vulkan_surface(void *, void **)` wrapping
`glfwGetRequiredInstanceExtensions` / `glfwCreateWindowSurface`, `framebuffer_size()` for `swapchain_desc`;
`device_desc::window` is a `const platform::window *`.

**Offers to `renderer` / `engine::imgui_layer` (R9):** the event stream for `ImGui_ImplConjure` input (key, text,
mouse, scroll, focus), `content_scale()` for `DisplayFramebufferScale`, clipboard getters/setters for
`ImGuiPlatformIO::Platform_GetClipboardTextFn/SetClipboardTextFn`, `set_cursor_mode` for `ImGuiConfigFlags_NoMouseCursorChange`.

**Offers to `scripting` / `bindings` (`03_scripting.md` 8, R26):** `input_state` whose `key_bitset::words`,
mouse fields and `gamepad_state` map one-to-one onto `conjure_input_block` / `conjure_gamepad_state`; the scripting
subsystem copies `snapshot()` in frame step 5; `bindings` `input_*` entries read the same snapshot, call
`input::inject`, `window::set_cursor_mode / current_cursor_mode / gamepad_name / clipboard_text / set_clipboard_text`
(all of which work on the null backend, R57) and pass `button_action` / `cursor_mode` values unchanged through
`conjure_button_action` / `conjure_cursor_mode` (R56); the C# `Key`, `MouseButton`, `GamepadButton`, `GamepadAxis`
enums are generated from `key.h`, `mouse_button.h`, `gamepad.h` (numbering P5).

**Offers to `editor` (`04_editor.md` 8):** `input::inject` on both backends for `input.inject` (the editor maps
`action: "press" | "release" | "tap"` and `holdFrames` onto PRESS/RELEASE events on specific frames), `release_all`
for `input.clear`, `parse_key` / `parse_mouse_button` / `parse_gamepad_*` for protocol strings, clipboard,
`drop_files_event` for drag-and-drop asset import, `monitors()` for `project.set_settings` window options.

**Offers to `testing` (`08_testing.md` 2.3, 4.5):** `null_backend` (held by `headless_engine`), `input_event` for
`input_script::events_for(frame)`, which `headless_engine::step_frames` feeds to `input::inject` before each
`step_frame()`.

**Offers to `modules` (`07_modules.md` D1):** GLFW linked once into `conjure_shared`; plugins reach the window only
through `engine_context::window`.

---

## 9. Implementation plan

1. `log_category.h`, `platform_error.h`, `backend.h`, `button_action.h`, `key.h`, `mouse_button.h`, `gamepad.h` with
   `enum_names.cpp`; tests for names, parsers and the GLFW-numbering invariants (`key::SPACE == 32`,
   `key::MENU == 348`, `gamepad_axis::COUNT == 6`). Core only.
2. `event.h` + `event.cpp` (`is_input_event`, remaps, `to_string`); `static_assert`s; `test_event`.
3. `input_state.h` (`key_bitset`, `gamepad_state`, helpers) and `input.h` / `input.cpp` with the state machine of 3.2;
   `test_input_state`, `test_input` (every rule of P4 as a case).
4. `monitor.h`, `native_handle.h`, `window.h` (interface + `window_desc` validation in `platform.cpp`),
   `null_backend.h/.cpp`; `create_window` dispatching to the null backend; `test_window`, `test_null_backend`,
   `test_window_events`, `test_backend_selection` (null part).
5. CMake: `CONJURE_PLATFORM_GLFW` option, GLFW 3.4 in `Dependencies.cmake`, Linux CI packages; `conjure_header_check(platform)`.
6. `glfw/glfw_context` (init reference count, error callback, last error string, joystick dispatch).
7. `glfw/glfw_window` creation (3.3), callbacks, pump (3.4) with resize coalescing and the gamepad poll; destruction
   order; `glfw_native.cpp` per OS.
8. Fullscreen, monitors, cursor modes, raw mouse motion, clipboard, Vulkan helpers.
9. `test_backend_conformance` as a `TEST_CASE_TEMPLATE` over factories (null always; glfw under
   `CONJURE_TEST_REQUIRE(precondition::REAL_WINDOW)`, tag `[window]`); run it manually on all three OSes.
10. `bench_platform.cpp` (allocation-free pump and apply of 10,000 injected events; snapshot copy cost).
11. `engine/src/platform/README.md` (purpose, types, threading, how to test). (`06_engine_loop.md` 7 already reads
    "returns an empty span unless a test queued events", R60.)

Definition of done (`ARCHITECTURE.md` 11, wave 2): `null` and `glfw` backends pass the same conformance test;
`inject` edge/level semantics identical; every header has its test file; `conjure_platform_tests` green on all PR
jobs without a display; `[window]` cases pass locally on Windows, Ubuntu (X11 and Wayland) and macOS.

---

## 10. Tests (`tests/platform/`, target `conjure_platform_tests`, label `platform`)

One file per public header (R16, `08_testing.md` 5.1) plus the topic files that `08_testing.md` 5.2 names. Every
fixture owns a `memory_guard` and a `log_capture`; no test needs a display unless tagged `[window]`.

| File | Cases (minimum) |
|---|---|
| `test_log_category.cpp` | category registered as `"platform"`, level change visible |
| `test_platform_error.cpp` | every enumerator has a name; `make_error(platform_error::X).is(X)`; category name `"platform"`; `to_string` format |
| `test_backend.cpp` | `parse_backend_kind("glfw" / "GLFW" / "null")`, `"vulkan"` -> `UNKNOWN_BACKEND`; `is_backend_compiled(NULL_BACKEND)` always true; `default_backend_kind` consistent with `is_backend_compiled` |
| `test_button_action.cpp` | values 0/1/2, names, parser |
| `test_key.cpp` | GLFW numbering invariants, `is_valid_key(0) == false`, `is_valid_key(348)`, `is_valid_key(349) == false`, `to_string`/`parse_key` round trip for every key, `DEL` spelled, modifier flags operators |
| `test_mouse_button.cpp`, `test_gamepad.cpp` | numbering, counts, names, parsers |
| `test_event.cpp` | `event` trivially copyable; `to_input_event` round trip for each input alternative and `nullopt` for window events; `event_as<T>`; `to_string` of every alternative |
| `test_input_state.cpp` | `key_bitset` set/reset/test/count across word boundaries (keys 63, 64, 348); `sizeof(key_bitset) == 64`; helpers; `is_key_held`; default state all zero |
| `test_input.cpp` | press -> `pressed` and `down` for one frame, then `held` only; release -> `released` one frame; press+release same frame -> both edges, not down; double press ignored; release when up ignored; REPEAT no change; mouse delta accumulates, scroll accumulates, both reset; text up to 16 then dropped and counted; gamepad connect/disconnect releases buttons and zeroes axes; `focus_event(false)` releases all; `release_all` edges; `frameIndex` increments; `snapshot` unchanged until `end_frame`; `inject` == `apply` for a scripted sequence (same final snapshot bytes); allocation-free apply/end_frame under `CONJURE_CHECK_NO_ALLOCATIONS` |
| `test_monitor.cpp`, `test_native_handle.cpp` | defaults; null monitor list has one primary 1920x1080@60; `native_handle_kind::NONE` on null |
| `test_window.cpp` | `window_desc` defaults; `INVALID_DESC` for zero size and long title; `create_window(NULL_BACKEND)` happy path; `as_null()` non-null; title/size/cursor/clipboard round trips; `set_fullscreen` resizes to the virtual mode and `MONITOR_NOT_FOUND` for index 1; `required_vulkan_instance_extensions` empty; `create_vulkan_surface` -> `VULKAN_UNSUPPORTED`; second window asserts (`CONJURE_CHECK_ASSERTS`) |
| `test_null_backend.cpp` | 1280x720 under headless size; `pump_events` empty by default; `push_event` delivered exactly once on the next pump and the span invalidated by the following pump; `simulate_resize` -> one `framebuffer_resized_event` with `framebuffer == round(size * scale)`; `simulate_content_scale`; `simulate_focus(false)` -> `focus_event`; `simulate_minimise`; `simulate_close` -> `close_event` and `should_close()`; `simulate_drop_files` paths valid until next pump; pump after the first frame allocation-free |
| `test_platform.cpp` | umbrella compiles alone; `backend_version_string(NULL_BACKEND) == "null"` |
| `test_window_events.cpp` | full frame of 3.1 against a null window: resize delivered and coalesced (two `simulate_resize` -> last size only), focus loss releases held keys through `input::apply`, close requested, drop files |
| `test_backend_selection.cpp` | `create_window` with `GLFW` when not compiled -> `BACKEND_NOT_COMPILED`; `parse_backend_kind` error; `window_settings` -> `window_desc` mapping used by the engine (headless -> null, 1280x720) |
| `test_backend_conformance.cpp` | `TEST_CASE_TEMPLATE` over `null_factory` and `glfw_factory` (`[window]`, `CONJURE_TEST_REQUIRE(precondition::REAL_WINDOW)`): create/destroy twice in one process (P7), `framebuffer_size` consistent with `window_size * content_scale`, title round trip, `set_cursor_mode` round trip, clipboard round trip, `monitors()` non-empty with one primary, `native_handle().kind` matches the OS, `request_close` -> `should_close`, the same injected `input_event` list yields byte-identical `input_state` on both backends, `required_vulkan_instance_extensions` non-empty on glfw when `glfwVulkanSupported` |
| `tests/benchmarks/bench_platform.cpp` | 10,000 injected key/mouse events applied + `end_frame` allocation-free and under 50 us; `snapshot()` copy under 0.2 us; null pump with 1,000 queued events |

---

## 11. Out of scope / future

- Multiple windows (editor floating viewports); the interface is per-window already, only the one-live-window
  assert and the joystick-callback dispatch need changing.
- Exclusive fullscreen with video-mode switching; `video_modes()` exists so a settings UI can prepare for it.
- Cursor images and shapes (`glfwCreateCursor`, standard shapes), window icons, drag-and-drop *out* of the window.
- Clipboard images and file lists; IME composition events (GLFW has no IME API in 3.4).
- Gamepad rumble, battery, touchpads and gyros (not in GLFW); touch and pen input.
- Dead zones, action maps and rebinding (bindings / gameplay layer); input recording and replay beyond
  `input_script` (test support).
- Blocking pumps (`glfwWaitEventsTimeout`) for an idle editor; the frame pacer sleeps instead.
- A non-GLFW native backend (Win32/Cocoa/Wayland direct) or SDL; `backend_kind` leaves room for it.
