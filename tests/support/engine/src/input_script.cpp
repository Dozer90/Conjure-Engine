#include "conjure/testing/input_script.h"

#include "conjure/core/assert.h"

#include <algorithm>
#include <cstddef>
#include <iterator>

using namespace conjure;
using namespace conjure::testing;

/*===========================================================================*/
/*===========================================================================*/
// Helpers

namespace {
    [[nodiscard]] bool valid_gamepad_index(u32 padIndex) noexcept {
        if (padIndex < platform::MAX_GAMEPADS) {
            return true;
        }
        CONJURE_ASSERT(
          false,
          "input_script: gamepad index {} is out of range (max {})",
          padIndex,
          platform::MAX_GAMEPADS
        );
        return false;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

input_script::input_script() {}

input_script::~input_script() {}


/*===========================================================================*/
/*===========================================================================*/
// Scheduling

void input_script::key(u64 frame, platform::key keyCode, platform::button_action action) {
    platform::key_event event = {};
    event.keyCode = keyCode;
    event.action = action;
    push(frame, platform::input_event(event));
}

void input_script::tap_key(u64 frame, platform::key keyCode, u64 holdFrames) {
    key(frame, keyCode, platform::button_action::PRESS);
    key(frame + std::max<u64>(holdFrames, 1), keyCode, platform::button_action::RELEASE);
}

void input_script::text(u64 frame, u32 codepoint) {
    platform::text_event event = {};
    event.codepoint = codepoint;
    push(frame, platform::input_event(event));
}

void input_script::mouse_move(u64 frame, f32 x, f32 y) {
    platform::mouse_move_event event = {};
    event.position = math::vec2(x, y);
    push(frame, platform::input_event(event));
}

void input_script::mouse_button(u64 frame, platform::mouse_button button, platform::button_action action) {
    platform::mouse_button_event event = {};
    event.button = button;
    event.action = action;
    push(frame, platform::input_event(event));
}

void input_script::mouse_scroll(u64 frame, f32 x, f32 y) {
    platform::mouse_scroll_event event = {};
    event.offset = math::vec2(x, y);
    push(frame, platform::input_event(event));
}

void input_script::gamepad_connection(u64 frame, u32 padIndex, bool connected) {
    if (!valid_gamepad_index(padIndex)) {
        return;
    }
    platform::gamepad_connection_event event = {};
    event.gamepadIndex = static_cast<u8>(padIndex);
    event.connected = connected;
    push(frame, platform::input_event(event));
}

void input_script::gamepad_button(u64 frame, u32 padIndex, platform::gamepad_button button) {
    gamepad_button(frame, padIndex, button, platform::button_action::PRESS);
    gamepad_button(frame + 1, padIndex, button, platform::button_action::RELEASE);
}

void input_script::gamepad_button(
  u64 frame,
  u32 padIndex,
  platform::gamepad_button button,
  platform::button_action action
) {
    if (!valid_gamepad_index(padIndex)) {
        return;
    }
    platform::gamepad_button_event event = {};
    event.gamepadIndex = static_cast<u8>(padIndex);
    event.button = button;
    event.action = action;
    push(frame, platform::input_event(event));
}

void input_script::gamepad_axis(u64 frame, u32 padIndex, platform::gamepad_axis axis, f32 value) {
    if (!valid_gamepad_index(padIndex)) {
        return;
    }
    platform::gamepad_axis_event event = {};
    event.gamepadIndex = static_cast<u8>(padIndex);
    event.axis = axis;
    event.value = value;
    push(frame, platform::input_event(event));
}

void input_script::push(u64 frame, const platform::input_event &event) {
    if (frame < mFirstOpenFrame) {
        ++mLateEventCount;
    }

    // After the last event already on this frame: a PRESS scheduled before a RELEASE on the same frame
    // stays a press-then-release.
    const auto position = std::upper_bound(mFrames.begin(), mFrames.end(), frame);
    const usize index = static_cast<usize>(std::distance(mFrames.begin(), position));
    mFrames.insert(position, frame);
    mEvents.insert(mEvents.begin() + static_cast<std::ptrdiff_t>(index), event);
}


/*===========================================================================*/
/*===========================================================================*/
// Delivery bookkeeping

void input_script::close_frames_before(u64 frame) noexcept {
    mFirstOpenFrame = std::max(mFirstOpenFrame, frame);
}


/*===========================================================================*/
/*===========================================================================*/
// Reading

std::span<const platform::input_event> input_script::events_for(u64 frame) const noexcept {
    const auto first = std::lower_bound(mFrames.begin(), mFrames.end(), frame);
    const auto last = std::upper_bound(first, mFrames.end(), frame);
    const usize begin = static_cast<usize>(std::distance(mFrames.begin(), first));
    const usize count = static_cast<usize>(std::distance(first, last));
    return std::span<const platform::input_event>(mEvents).subspan(begin, count);
}

std::optional<u64> input_script::last_frame() const noexcept {
    if (mFrames.empty()) {
        return std::nullopt;
    }
    return mFrames.back();
}

void input_script::clear() noexcept {
    mFrames.clear();
    mEvents.clear();
}
