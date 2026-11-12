#pragma once

#include "conjure/core/types.h"
#include "conjure/platform/button_action.h"
#include "conjure/platform/event.h"
#include "conjure/platform/gamepad.h"
#include "conjure/platform/key.h"
#include "conjure/platform/mouse_button.h"

#include <optional>
#include <span>
#include <vector>

// Scripted input for headless runs (docs/design/08_testing.md 4.5, D5). Events are scheduled on frame
// indices and handed to the null window right before the frame runs, so a case states its input up
// front and the engine sees exactly the key, mouse and gamepad transitions a player would have made.
//
//   headless_engine engine = {};
//   engine.input().key(10, platform::key::W, platform::button_action::PRESS);
//   engine.input().key(12, platform::key::W, platform::button_action::RELEASE);
//   engine.step_frames(20);
//
// Events may be scheduled in any order; events_for() returns them sorted by frame, and events that
// share a frame keep their insertion order. Scheduling keeps the sorted view current (an insertion is
// O(n), fine for a test script), so every reader is allocation-free and honestly noexcept. The script is
// a plain container: it never touches a window itself (headless_engine::step_frames does), so it can also
// drive platform::input::inject.
//
// Frames are absolute frame indices, never offsets from "now": with headless_engine, frame_index() is the
// next frame to run, so a case that has already stepped schedules at engine.frame_index() + n. Whoever
// delivers the events closes each frame it ran (close_frames_before); an event scheduled before the first
// open frame is still stored but counted as late (late_event_count), because nothing will deliver it.
// clear() forgets the events but keeps the late count and the first open frame.

namespace conjure::testing {

    class input_script {
        private:
            std::vector<u64> mFrames = {};                          // sorted, parallel to mEvents
            std::vector<platform::input_event> mEvents = {};        // sorted by frame, insertion order within
            u64 mFirstOpenFrame = 0;                                // frames below it already ran
            u64 mLateEventCount = 0;                                // events scheduled below mFirstOpenFrame

        public:
            input_script();
            ~input_script();

            input_script(const input_script &) = default;
            input_script &operator=(const input_script &) = default;
            input_script(input_script &&) noexcept = default;
            input_script &operator=(input_script &&) noexcept = default;

        public:
            /*---------------------------------------------------------------*/
            // Scheduling

            /// <summary>
            /// A key transition at 'frame' (scancode 0, no modifiers).
            /// </summary>
            void key(u64 frame, platform::key keyCode, platform::button_action action);

            /// <summary>
            /// A key tap: PRESS at 'frame' and RELEASE at frame + holdFrames (at least one frame later).
            /// </summary>
            void tap_key(u64 frame, platform::key keyCode, u64 holdFrames = 1);

            /// <summary>
            /// A UTF-32 text codepoint at 'frame'.
            /// </summary>
            void text(u64 frame, u32 codepoint);

            /// <summary>
            /// The cursor moving to (x, y) in logical window coordinates at 'frame'.
            /// </summary>
            void mouse_move(u64 frame, f32 x, f32 y);

            /// <summary>
            /// A mouse button transition at 'frame' (no modifiers).
            /// </summary>
            void mouse_button(u64 frame, platform::mouse_button button, platform::button_action action);

            /// <summary>
            /// A scroll of (x, y) wheel offsets at 'frame' (+y = away from the user).
            /// </summary>
            void mouse_scroll(u64 frame, f32 x, f32 y);

            /// <summary>
            /// A gamepad (dis)connection at 'frame'. Indices at or above platform::MAX_GAMEPADS assert and
            /// schedule nothing.
            /// </summary>
            void gamepad_connection(u64 frame, u32 padIndex, bool connected);

            /// <summary>
            /// A gamepad button tap: PRESS at 'frame' and RELEASE one frame later. Indices at or above
            /// platform::MAX_GAMEPADS assert and schedule nothing.
            /// </summary>
            void gamepad_button(u64 frame, u32 padIndex, platform::gamepad_button button);

            /// <summary>
            /// One gamepad button transition at 'frame'.
            /// </summary>
            void gamepad_button(
              u64 frame,
              u32 padIndex,
              platform::gamepad_button button,
              platform::button_action action
            );

            /// <summary>
            /// A gamepad axis value at 'frame' (sticks [-1, 1], triggers [0, 1]; the platform clamps).
            /// </summary>
            void gamepad_axis(u64 frame, u32 padIndex, platform::gamepad_axis axis, f32 value);

            /// <summary>
            /// Any input event verbatim at 'frame'. The one call that allocates: it places the event after
            /// every event already scheduled for that frame. A frame before the first open frame is stored
            /// too but counted as late (late_event_count), since nothing will deliver it.
            /// </summary>
            void push(u64 frame, const platform::input_event &event);

            /*---------------------------------------------------------------*/
            // Delivery bookkeeping

            /// <summary>
            /// Marks every frame below 'frame' as already run: events scheduled for one of them from now on
            /// count as late. The first open frame never moves backwards.
            /// </summary>
            void close_frames_before(u64 frame) noexcept;

            /// <summary>
            /// Events scheduled for a frame that had already run when they were scheduled; clear() keeps it.
            /// </summary>
            [[nodiscard]] u64 late_event_count() const noexcept {
                return mLateEventCount;
            }

            /*---------------------------------------------------------------*/
            // Reading (never allocate; spans stay valid until the next scheduling call)

            /// <summary>
            /// Every event scheduled for exactly 'frame', in insertion order.
            /// </summary>
            [[nodiscard]] std::span<const platform::input_event> events_for(u64 frame) const noexcept;

            /// <summary>
            /// The frame of every scheduled event, sorted (insertion order within a frame).
            /// </summary>
            [[nodiscard]] std::span<const u64> frames() const noexcept {
                return mFrames;
            }

            /// <summary>
            /// The events frames() refers to, in the same order.
            /// </summary>
            [[nodiscard]] std::span<const platform::input_event> events() const noexcept {
                return mEvents;
            }

            /// <summary>
            /// Number of scheduled events.
            /// </summary>
            [[nodiscard]] usize event_count() const noexcept {
                return mFrames.size();
            }

            [[nodiscard]] bool empty() const noexcept {
                return mFrames.empty();
            }

            /// <summary>
            /// The latest frame any event is scheduled for; nullopt when the script is empty.
            /// </summary>
            [[nodiscard]] std::optional<u64> last_frame() const noexcept;

            /// <summary>
            /// Forgets every event; the late count and the first open frame stay.
            /// </summary>
            void clear() noexcept;
    };
}
