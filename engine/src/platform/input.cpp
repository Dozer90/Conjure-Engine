#include "conjure/platform/input.h"
#include "conjure/platform/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/jobs/main_thread.h"

#include <optional>
#include <type_traits>
#include <variant>

// The input state machine of docs/design/11_platform.md 3.2 (rules P4). apply() and inject() share
// apply_input(), so the editor's injected events and the window's pumped events can never diverge.

using namespace conjure;
using namespace conjure::platform;

namespace {
    [[nodiscard]] constexpr u32 bit_of(u32 index) noexcept {
        return u32{1} << index;
    }

    /// <summary>
    /// The press / release rule on a u32 mask (mouse buttons, gamepad buttons). Returns false when
    /// the transition changed nothing (press while down, release while up, REPEAT).
    /// </summary>
    [[nodiscard]] bool apply_mask_transition(
      u32 &down,
      u32 &pressed,
      u32 &released,
      u32 bit,
      button_action action
    ) noexcept {
        const bool isDown = (down & bit) != 0;
        if (action == button_action::PRESS && !isDown) {
            down |= bit;
            pressed |= bit;
            return true;
        }
        if (action == button_action::RELEASE && isDown) {
            down &= ~bit;
            released |= bit;
            return true;
        }
        return false;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

input::input() {}

input::~input() {}

/*===========================================================================*/
/*===========================================================================*/
// Public methods

bool input::apply(const event &value) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "input::apply must run on the main thread");

    const std::optional<input_event> inputValue = to_input_event(value);
    if (inputValue.has_value()) {
        apply_input(*inputValue);
        return true;
    }

    // Window events are accepted and ignored, except that losing focus releases everything: GLFW
    // synthesises release events too, and P4 makes the second release of a key a no-op.
    const focus_event *focus = event_as<focus_event>(value);
    if (focus != nullptr && !focus->focused) {
        release_all();
    }
    return false;
}

void input::inject(const input_event &value) {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "input::inject must run on the main thread");
    ++mStats.eventsInjected;
    apply_input(value);
}

void input::inject(const event &value) {
    const std::optional<input_event> inputValue = to_input_event(value);
    if (!inputValue.has_value()) {
        return;
    }
    inject(*inputValue);
}

void input::end_frame() {
    CONJURE_ASSERT(core::jobs::is_main_thread(), "input::end_frame must run on the main thread");

    mPublished = mPending;
    mPublished.frameIndex = ++mFrameCounter;

    // Edges, deltas and text belong to the frame that just ended; levels persist.
    mPending.keysPressed.clear();
    mPending.keysReleased.clear();
    mPending.mouseButtonsPressed = 0;
    mPending.mouseButtonsReleased = 0;
    for (gamepad_state &pad : mPending.gamepads) {
        pad.buttonsPressed = 0;
        pad.buttonsReleased = 0;
    }
    mPending.mouseDelta = math::vec2(0.0f);
    mPending.scrollDelta = math::vec2(0.0f);
    mPending.textCount = 0;
}

const input_state &input::snapshot() const noexcept {
    return mPublished;
}

void input::release_all() noexcept {
    for (usize word = 0; word < key_bitset::WORD_COUNT; ++word) {
        mPending.keysReleased.words[word] |= mPending.keysDown.words[word];
    }
    mPending.keysDown.clear();

    mPending.mouseButtonsReleased |= mPending.mouseButtonsDown;
    mPending.mouseButtonsDown = 0;

    for (gamepad_state &pad : mPending.gamepads) {
        pad.buttonsReleased |= pad.buttonsDown;
        pad.buttonsDown = 0;
    }

    mPending.modifiers = key_modifier_flags::NONE;
}

void input::skip_next_mouse_delta() noexcept {
    mSkipNextMouseDelta = true;
}

const input_stats &input::stats() const noexcept {
    return mStats;
}

/*===========================================================================*/
/*===========================================================================*/
// Private methods

void input::apply_input(const input_event &value) {
    ++mStats.eventsApplied;
    std::visit(
      [this](const auto &alternative) {
          using alternative_type = std::remove_cvref_t<decltype(alternative)>;
          if constexpr (std::is_same_v<alternative_type, key_event>) {
              apply_key(alternative);
          } else if constexpr (std::is_same_v<alternative_type, text_event>) {
              apply_text(alternative);
          } else if constexpr (std::is_same_v<alternative_type, mouse_move_event>) {
              apply_mouse_move(alternative);
          } else if constexpr (std::is_same_v<alternative_type, mouse_button_event>) {
              apply_mouse_button(alternative);
          } else if constexpr (std::is_same_v<alternative_type, mouse_scroll_event>) {
              apply_mouse_scroll(alternative);
          } else if constexpr (std::is_same_v<alternative_type, cursor_enter_event>) {
              apply_cursor_enter(alternative);
          } else if constexpr (std::is_same_v<alternative_type, gamepad_connection_event>) {
              apply_gamepad_connection(alternative);
          } else if constexpr (std::is_same_v<alternative_type, gamepad_button_event>) {
              apply_gamepad_button(alternative);
          } else {
              static_assert(std::is_same_v<alternative_type, gamepad_axis_event>);
              apply_gamepad_axis(alternative);
          }
      },
      value
    );
}

void input::apply_key(const key_event &value) {
    // The modifiers travel with every key event, including those for keys without a code.
    mPending.modifiers = value.modifiers;

    if (!is_valid_key(value.keyCode)) {
        ++mStats.ignoredTransitions;
        return;
    }
    if (value.action == button_action::REPEAT) {
        return;
    }

    const bool isDown = mPending.keysDown.test(value.keyCode);
    if (value.action == button_action::PRESS && !isDown) {
        mPending.keysDown.set(value.keyCode);
        mPending.keysPressed.set(value.keyCode);
        return;
    }
    if (value.action == button_action::RELEASE && isDown) {
        mPending.keysDown.reset(value.keyCode);
        mPending.keysReleased.set(value.keyCode);
        return;
    }
    ++mStats.ignoredTransitions;
}

void input::apply_text(const text_event &value) {
    if (mPending.textCount >= MAX_TEXT_CODEPOINTS_PER_FRAME) {
        ++mStats.textCodepointsDropped;
        return;
    }
    mPending.textCodepoints[mPending.textCount] = value.codepoint;
    ++mPending.textCount;
}

void input::apply_mouse_move(const mouse_move_event &value) {
    // The first report after construction, a cursor re-entry or a cursor mode change is a position,
    // not a movement: the cursor got there without crossing the window.
    if (mSkipNextMouseDelta) {
        mSkipNextMouseDelta = false;
    } else {
        mPending.mouseDelta += value.position - mPending.mousePosition;
    }
    mPending.mousePosition = value.position;
}

void input::apply_mouse_button(const mouse_button_event &value) {
    mPending.modifiers = value.modifiers;

    if (!is_valid_mouse_button(value.button)) {
        ++mStats.ignoredTransitions;
        return;
    }
    if (value.action == button_action::REPEAT) {
        return;
    }

    const bool changed = apply_mask_transition(
      mPending.mouseButtonsDown,
      mPending.mouseButtonsPressed,
      mPending.mouseButtonsReleased,
      bit_of(static_cast<u32>(value.button)),
      value.action
    );
    if (!changed) {
        ++mStats.ignoredTransitions;
    }
}

void input::apply_mouse_scroll(const mouse_scroll_event &value) {
    mPending.scrollDelta += value.offset;
}

void input::apply_cursor_enter(const cursor_enter_event &value) {
    mPending.cursorInWindow = value.entered;
    if (value.entered) {
        mSkipNextMouseDelta = true;
    }
}

void input::apply_gamepad_connection(const gamepad_connection_event &value) {
    gamepad_state *pad = gamepad_for(value.gamepadIndex);
    if (pad == nullptr) {
        return;
    }

    if (value.connected) {
        // A freshly connected pad starts clean, whatever a stale slot held.
        *pad = gamepad_state();
        pad->connected = true;
        return;
    }

    // Disconnecting releases every held button (edges this frame) and centres the axes.
    pad->buttonsReleased |= pad->buttonsDown;
    pad->buttonsDown = 0;
    pad->axes.fill(0.0f);
    pad->connected = false;
}

void input::apply_gamepad_button(const gamepad_button_event &value) {
    gamepad_state *pad = gamepad_for(value.gamepadIndex);
    if (pad == nullptr) {
        return;
    }
    if (!is_valid_gamepad_button(value.button)) {
        ++mStats.ignoredTransitions;
        return;
    }
    if (value.action == button_action::REPEAT) {
        return;
    }

    const bool changed = apply_mask_transition(
      pad->buttonsDown,
      pad->buttonsPressed,
      pad->buttonsReleased,
      bit_of(static_cast<u32>(value.button)),
      value.action
    );
    if (!changed) {
        ++mStats.ignoredTransitions;
    }
}

void input::apply_gamepad_axis(const gamepad_axis_event &value) {
    gamepad_state *pad = gamepad_for(value.gamepadIndex);
    if (pad == nullptr) {
        return;
    }
    if (!is_valid_gamepad_axis(value.axis)) {
        ++mStats.ignoredTransitions;
        return;
    }
    pad->axes[static_cast<usize>(value.axis)] = value.value;
}

gamepad_state *input::gamepad_for(u8 index) {
    // An out-of-range index is a programmer error on the injecting side (the backends never produce
    // one), so it asserts; the event is then dropped and counted.
    if (index >= MAX_GAMEPADS) {
        CONJURE_ASSERT(false, "gamepad index {} out of range (MAX_GAMEPADS = {})", index, MAX_GAMEPADS);
        ++mStats.ignoredTransitions;
        return nullptr;
    }
    return &mPending.gamepads[index];
}
