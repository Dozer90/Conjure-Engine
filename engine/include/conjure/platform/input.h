#pragma once

#include "conjure/platform/event.h"
#include "conjure/platform/input_state.h"

#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <concepts>

// The backend-independent input accumulator (docs/design/11_platform.md 3.2, P3, P4). apply()
// (engine frame step 3) and inject() (editor, tests) run the same state machine; end_frame()
// publishes the snapshot by copy and clears the edges. Main thread only, except snapshot(), which any
// thread may read between two end_frame() calls.

namespace conjure::platform {

    /// <summary>
    /// Counters kept across the lifetime of an input instance (diagnostics overlay, tests).
    /// </summary>
    struct input_stats {
        u64 eventsApplied = 0;              // input events processed by apply() or inject()
        u64 eventsInjected = 0;             // the subset that came through inject()
        u64 textCodepointsDropped = 0;      // text beyond MAX_TEXT_CODEPOINTS_PER_FRAME in one frame
        u64 ignoredTransitions = 0;         // press while down, release while up, invalid key / button / index
    };

    class CONJURE_API input {
        private:
            input_state mPending = {};      // accumulating; written by apply / inject
            input_state mPublished = {};    // immutable between end_frame() calls; returned by snapshot()
            input_stats mStats = {};
            u64 mFrameCounter = 0;
            bool mSkipNextMouseDelta = true;

        public:
            input();
            ~input();

            input(const input &) = delete;
            input &operator=(const input &) = delete;

        public:
            /// <summary>
            /// Applies one pumped event. Returns true when it was an input event; window events return
            /// false (a focus_event with focused == false still triggers release_all()).
            /// </summary>
            bool apply(const event &value);

            /// <summary>
            /// Same state machine as apply(); the source for tests and the editor's input.inject (P3).
            /// </summary>
            void inject(const input_event &value);

            /// <summary>
            /// Convenience overload: injects the event when it is an input event, ignores it otherwise.
            /// </summary>
            void inject(const event &value);

            /// <summary>
            /// Injects one concrete input alternative (key_event, mouse_move_event, ...) without the
            /// caller spelling the variant; both variants convert from the same structs, so this
            /// overload is what makes inject(make_key(...)) unambiguous.
            /// </summary>
            template <typename EventType>
                requires std::constructible_from<input_event, const EventType &> &&
                  (!std::same_as<EventType, input_event>) && (!std::same_as<EventType, event>)
            void inject(const EventType &value) {
                inject(input_event(value));
            }

            /// <summary>
            /// Publishes the frame's snapshot (frameIndex incremented) and clears edges, deltas, scroll
            /// and text in the accumulating state. Levels persist.
            /// </summary>
            void end_frame();

            /// <summary>
            /// The snapshot published by the last end_frame(). Readable from any thread until the next
            /// end_frame(); never hold the reference across one.
            /// </summary>
            [[nodiscard]] const input_state &snapshot() const noexcept;

            /// <summary>
            /// Releases every down key, mouse button and gamepad button with a released edge this frame
            /// and clears the modifiers (focus loss, the editor's input.clear).
            /// </summary>
            void release_all() noexcept;

            /// <summary>
            /// Makes the next mouse_move_event set the position without adding to mouseDelta. The
            /// window owner calls this after a cursor mode change, where the OS repositions the cursor
            /// and the first report would otherwise read as a jump.
            /// </summary>
            void skip_next_mouse_delta() noexcept;

            [[nodiscard]] const input_stats &stats() const noexcept;

        private:
            void apply_input(const input_event &value);
            void apply_key(const key_event &value);
            void apply_text(const text_event &value);
            void apply_mouse_move(const mouse_move_event &value);
            void apply_mouse_button(const mouse_button_event &value);
            void apply_mouse_scroll(const mouse_scroll_event &value);
            void apply_cursor_enter(const cursor_enter_event &value);
            void apply_gamepad_connection(const gamepad_connection_event &value);
            void apply_gamepad_button(const gamepad_button_event &value);
            void apply_gamepad_axis(const gamepad_axis_event &value);
            [[nodiscard]] gamepad_state *gamepad_for(u8 index);
    };
}
