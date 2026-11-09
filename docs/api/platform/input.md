# Input

📘 **Public API wiki** · 🛠 Handbook: [Platform module](../../handbook/modules/platform.md)

How to ask "is the jump key down this frame" and get the same answer on every platform and in every test. Include
`conjure/platform/input.h` for the `input` accumulator; it brings in `conjure/platform/input_state.h` (the snapshot
you query) and the enumerations `conjure/platform/key.h`, `conjure/platform/mouse_button.h`,
`conjure/platform/gamepad.h` and `conjure/platform/button_action.h`. Everything is in `namespace conjure::platform`.

## At a glance

| You want to... | Use |
|---|---|
| React once when a key goes down | `snapshot().is_key_pressed(key::SPACE)` |
| Act every frame while a key is held | `snapshot().is_key_down(key::W)` |
| React once when a key comes up | `snapshot().is_key_released(key::E)` |
| Move a first-person camera | `snapshot().mouseDelta` with `cursor_mode::DISABLED` |
| Read a stick or trigger | `snapshot().gamepad(0).axis(gamepad_axis::LEFT_X)` |
| Read typed text for a console or name field | `snapshot().text()` |
| Know whether Shift or Ctrl is held | `has_flag(snapshot().modifiers, key_modifier_flags::SHIFT)` |
| Drive input from a test or a script | `input.inject(event)` |
| Print or parse a key name | `to_string(key::F5)`, `parse_key("f5")` |

## Quick example

```cpp
#include "conjure/platform/input.h"
#include "conjure/platform/window.h"

using namespace conjure;
using namespace conjure::platform;

struct player {
    math::vec2 position = math::vec2(0.0f);
    bool jumping = false;
};

void run_frame(window &target, input &accumulator, player &hero) {
    for (const event &value : target.pump_events()) {
        accumulator.apply(value);               // input events update the state; window events are ignored
    }
    accumulator.end_frame();                    // publish this frame's snapshot, clear the edges

    const input_state &state = accumulator.snapshot();
    if (state.is_key_pressed(key::SPACE)) {     // true for exactly the frame the key went down
        hero.jumping = true;
    }
    if (state.is_key_down(key::D)) {            // true for every frame the key is held
        hero.position.x += 1.0f;
    }
    hero.position.x += state.gamepad(0).axis(gamepad_axis::LEFT_X);     // [-1, 1]; 0 when not connected
}
```

## Pressed, released, down and held

The snapshot keeps two kinds of fact for every key and button. **Levels** describe a state and persist across
frames: `is_key_down()` is true from the frame the key went down to the frame it came up. **Edges** describe a
transition and are true for exactly one frame: `is_key_pressed()` in the frame it went down, `is_key_released()` in
the frame it came up. `is_key_held()` is "down but not pressed this frame". The same four exist for mouse buttons
(`is_mouse_button_down/pressed/released`) and gamepad buttons (`gamepad_state::is_down/pressed/released`).

| Event arrives while the key is... | Effect |
|---|---|
| `PRESS` while up | `down` and `pressed` set |
| `PRESS` while already down | ignored (counted in `stats().ignoredTransitions`) |
| `RELEASE` while down | `down` cleared, `released` set |
| `RELEASE` while already up | ignored |
| `REPEAT` (OS key repeat) | no change to any state; only visible as a `key_event` |
| press and release within one frame | `pressed` and `released` both set, `down` clear: a fast tap is never lost |
| `focus_event{false}`, or `release_all()` | every down key and button gets a `released` edge; modifiers cleared |

`end_frame()` copies the accumulating state into the snapshot (`frameIndex` goes up by one) and then clears the
edges, `mouseDelta`, `scrollDelta` and the typed text; levels, positions, axes and `modifiers` stay.

> [!WARNING]
> `snapshot()` changes only inside `end_frame()`. Events applied this frame are invisible until then, so always
> run the three steps in order: `pump_events()`, `apply()` for each event, `end_frame()`, and read the snapshot
> after that. The reference `snapshot()` returns stays valid until the next `end_frame()`; never hold it across
> one. Other threads may read the snapshot between two `end_frame()` calls, but `apply`, `inject`, `end_frame`
> and `release_all` are main thread only.

## Keys and modifiers

`key` (`conjure/platform/key.h`) names every key with GLFW's numbering (`key::SPACE == 32`, `key::A == 65`,
`key::ESCAPE == 256`, `key::F1 == 290`, `key::KP_0 == 320`, `key::LEFT_SHIFT == 340`, `key::MENU == 348`), so the
same numbers appear in configuration files, the editor protocol and the C# `Key` enum. Letters and digits are
`key::A` to `key::Z` and `key::NUM_0` to `key::NUM_9`; the keypad is `key::KP_*`; the Delete key is `key::DEL`.
`key::UNKNOWN` (0) is what a key without a code reports and never sets any bit. `is_valid_key(code)` is true for
1 to `key::LAST`.

`to_string(key)` gives the enumerator name (`"SPACE"`, `"DEL"`, `"KP_ENTER"`); `parse_key(name)` reads it back
case-insensitively and returns `std::nullopt` for anything else, including `"UNKNOWN"` and the alias `"LAST"`.

`snapshot().modifiers` is a `key_modifier_flags` set (`SHIFT`, `CONTROL`, `ALT`, `SUPER`, `CAPS_LOCK`, `NUM_LOCK`)
taken from the most recent key or mouse button event; test it with `has_flag()`. `to_string(modifiers)` prints
`"SHIFT|CONTROL"` and `parse_key_modifier_flags("shift|alt")` parses the same form.

> [!TIP]
> For a "hold to sprint" binding use `is_key_down(key::LEFT_SHIFT)` rather than the `SHIFT` modifier flag: the flag
> only updates when another key or button event arrives.

## Mouse

`mousePosition` is the cursor in logical window coordinates (see
[Sizes and high-DPI](windows-and-events.md#sizes-and-high-dpi)). `mouseDelta` is the sum of all movement this frame
and `scrollDelta` the sum of all scroll offsets (`+y` away from the user); both reset at `end_frame()`.
`cursorInWindow` follows `cursor_enter_event`. Buttons are `mouse_button::LEFT`, `RIGHT`, `MIDDLE` and `BUTTON_4` to
`BUTTON_8` (`conjure/platform/mouse_button.h`, with `to_string` / `parse_mouse_button` and
`is_valid_mouse_button`), read through `is_mouse_button_down/pressed/released`.

The first mouse move after construction, after the cursor re-enters the window and after
`skip_next_mouse_delta()` sets the position without adding to the delta, because the cursor got there without
crossing the window.

> [!IMPORTANT]
> Call `skip_next_mouse_delta()` right after `window::set_cursor_mode()`: the operating system moves the cursor
> when the mode changes, and without the skip the first report of the new position reads as a jump. With
> `cursor_mode::DISABLED` the position is unbounded and `mouseDelta` is what you want for a camera.

## Gamepads

Up to `MAX_GAMEPADS` (4) gamepads occupy slots 0 to 3; `snapshot().gamepad(index)` returns the slot's
`gamepad_state` (an out-of-range index asserts). A slot is `connected` after a `gamepad_connection_event`; a
disconnect releases every held button with an edge and zeroes the axes, and reconnecting starts from a clean slot.
`window::gamepad_name(index)` gives the device's name.

`gamepad_button` and `gamepad_axis` (`conjure/platform/gamepad.h`) follow the SDL game controller layout: buttons
`A`, `B`, `X`, `Y`, `LEFT_BUMPER`, `RIGHT_BUMPER`, `BACK`, `START`, `GUIDE`, `LEFT_THUMB`, `RIGHT_THUMB`,
`DPAD_UP/RIGHT/DOWN/LEFT`; axes `LEFT_X`, `LEFT_Y`, `RIGHT_X`, `RIGHT_Y` in `[-1, 1]` and `LEFT_TRIGGER`,
`RIGHT_TRIGGER` in `[0, 1]`. `axis(axisId)` returns 0 for an invalid axis; both enumerations have `to_string`,
`parse_gamepad_button` / `parse_gamepad_axis` and `is_valid_gamepad_button` / `is_valid_gamepad_axis`. Joysticks
without a gamepad mapping are ignored; `window_desc::gamepadMappings` adds mappings for unusual devices.

> [!TIP]
> No dead zone is applied here, so a resting stick may read 0.02. Apply your own dead zone in the game's bindings
> layer; keeping it out of the platform layer is what makes injected test input and real input identical.

## Text input

`snapshot().text()` is a `std::span<const u32>` of the UTF-32 code points typed this frame, in order, at most
`MAX_TEXT_CODEPOINTS_PER_FRAME` (16); anything beyond that is dropped and counted in `stats().textCodepointsDropped`.
Use it for consoles and text fields; use key events for gameplay. Text arrives as `text_event`s already translated by
the operating system's keyboard layout, so `key::A` and the typed character need not match.

## Focus loss

When the window loses focus, `apply()` sees the `focus_event{false}` and calls `release_all()`: every down key, mouse
button and gamepad button gets a `released` edge in that frame and `modifiers` is cleared, so nothing stays stuck
when the player alt-tabs away. The OS also sends its own releases; those hit the "release while up" rule and are
ignored. `release_all()` is public for the same purpose in code (the editor's *clear input* command).

## Injecting input in tests

`inject()` runs exactly the same state machine as `apply()`; the only difference is the counter
`stats().eventsInjected`. There are two ways to script input against a headless window:

```cpp
#include "conjure/platform/input.h"
#include "conjure/platform/null_backend.h"
#include "conjure/platform/window.h"

using namespace conjure;
using namespace conjure::platform;

key_event make_key(key code, button_action action) {
    key_event value = {};
    value.keyCode = code;
    value.action = action;
    return value;
}

void tap_space(input &accumulator) {
    accumulator.inject(make_key(key::SPACE, button_action::PRESS));    // a concrete struct injects directly
    accumulator.inject(make_key(key::SPACE, button_action::RELEASE));
    accumulator.end_frame();
    // snapshot().is_key_pressed(SPACE) and is_key_released(SPACE) are true, is_key_down(SPACE) is false
}

void press_through_the_window(window &created, input &accumulator) {
    created.as_null()->push_event(make_key(key::W, button_action::PRESS));
    for (const event &value : created.pump_events()) {
        accumulator.apply(value);           // the real frame path, event by event
    }
    accumulator.end_frame();
}
```

`inject` accepts an `input_event`, any one of its concrete structs (`key_event`, `mouse_move_event`, ...), or an
`event` (window alternatives are ignored). Going through `null_backend::push_event()` and `pump_events()` also
exercises window events such as focus loss and resizes (see
[The null backend](windows-and-events.md#the-null-backend)). The module's conformance tests feed the same event list
through a null window and a real one and require byte-identical snapshots, so what you test headless is what the
player gets.

> [!NOTE]
> The same injected sequence yields the same snapshot on every run and every platform, `frameIndex` included: the
> accumulator holds no clock, no random state and no OS query.

## Under the hood

The snapshot is a plain trivially copyable struct you may read field by field when a helper is not enough:
`keysDown`, `keysPressed` and `keysReleased` are `key_bitset`s (512 bits as `std::array<u64, 8> words`, bit *i* of
word *w* is key `64 * w + i`, with `test`, `set`, `reset`, `clear`, `any` and `count`); `mouseButtonsDown/Pressed/Released`
and each `gamepad_state::buttonsDown/Pressed/Released` are `u32` masks with bit *i* for button *i*;
`gamepad_state::axes` is `std::array<f32, 6>`; `textCodepoints` and `textCount` back `text()`. Comparing two
snapshots with `==` compares every field. The scripting layer copies this struct into the C# frame block as is, so
its layout is fixed. `stats()` returns `input_stats`: `eventsApplied`, `eventsInjected`, `textCodepointsDropped` and
`ignoredTransitions`, for a diagnostics overlay.

## API summary

| Signature (short) | Purpose |
|---|---|
| `bool apply(const event &)` | applies a pumped event; true for an input event; `focus_event{false}` releases all |
| `void inject(const input_event &)` / `inject(const event &)` / `template <typename T> inject(const T &)` | the same state machine for tests and the editor |
| `void end_frame()` | publishes the snapshot, clears edges, deltas, scroll and text |
| `const input_state &snapshot()` | the published snapshot; readable from any thread until the next `end_frame()` |
| `void release_all()` / `void skip_next_mouse_delta()` | release everything with edges; ignore the next move's delta |
| `const input_stats &stats()` | lifetime counters |
| `input_state`: `frameIndex`, `keysDown/Pressed/Released`, `modifiers`, `mousePosition`, `mouseDelta`, `scrollDelta`, `mouseButtons*`, `cursorInWindow`, `gamepads`, `textCodepoints`, `textCount` | the data |
| `bool is_key_down(key)` / `is_key_pressed(key)` / `is_key_released(key)` / `is_key_held(key)` | key queries |
| `bool is_mouse_button_down(mouse_button)` / `_pressed` / `_released` | mouse button queries |
| `const gamepad_state &gamepad(u32 index)` / `std::span<const u32> text()` | gamepad slot; typed text |
| `gamepad_state`: `connected`, `is_down(gamepad_button)` / `is_pressed` / `is_released`, `f32 axis(gamepad_axis)` | one gamepad |
| `key_bitset`: `test(key)`, `set(key)`, `reset(key)`, `clear()`, `any()`, `count()`, `words` | the 512-bit set |
| `button_action`: `RELEASE`, `PRESS`, `REPEAT` with `to_string` / `parse_button_action` | what happened to a key or button |
| `key`, `key_modifier_flags`, `mouse_button`, `gamepad_button`, `gamepad_axis` with `to_string` / `parse_*` / `is_valid_*` | the enumerations |
| `KEY_COUNT` (512), `MOUSE_BUTTON_COUNT` (8), `MAX_GAMEPADS` (4), `GAMEPAD_BUTTON_COUNT` (15), `GAMEPAD_AXIS_COUNT` (6), `MAX_TEXT_CODEPOINTS_PER_FRAME` (16) | the limits |

## See also

- 📘 API: [Windows and events](windows-and-events.md): where the events come from, cursor modes, the null backend
- 📘 API: [Platform overview](index.md)
- 📘 API: [Data types](../core/data-types.md) for flag enumerations and `has_flag()`
- 📘 API: [Glossary](../glossary.md)
- 🛠 Handbook: [Platform module](../../handbook/modules/platform.md): the state machine in full and the layout the
  scripting frame block mirrors
