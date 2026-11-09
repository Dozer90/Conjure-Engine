#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/memory_guard.h"
#include "platform_test_support.h"

#include "conjure/platform/input.h"
#include "conjure/platform/null_backend.h"
#include "conjure/platform/window.h"

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::platform;
using namespace conjure::platform::test_support;

namespace {
    /// <summary>
    /// What the engine's frame step 3 (06_engine_loop.md 3.1) does with one pump, recorded for
    /// assertions: layers see every event, input gets them, window events drive quit / resize / pacer.
    /// </summary>
    struct frame_result {
        bool quitRequested = false;
        std::optional<math::uvec2> swapchainResize = std::nullopt;
        u32 resizeCount = 0;
        bool pacerUnfocused = false;
        std::vector<std::string> droppedPaths = {};
        usize eventCount = 0;
    };

    [[nodiscard]] frame_result run_frame(window &target, input &accumulator) {
        frame_result result = {};
        const std::span<const event> events = target.pump_events();
        result.eventCount = events.size();
        for (const event &value : events) {
            accumulator.apply(value);
            if (event_as<close_event>(value) != nullptr) {
                result.quitRequested = true;
            }
            if (const framebuffer_resized_event *resized = event_as<framebuffer_resized_event>(value)) {
                ++result.resizeCount;
                if (resized->width > 0 && resized->height > 0) {
                    result.swapchainResize = math::uvec2(resized->width, resized->height);
                }
            }
            if (const focus_event *focus = event_as<focus_event>(value)) {
                result.pacerUnfocused = !focus->focused;
            }
            if (const drop_files_event *drop = event_as<drop_files_event>(value)) {
                for (const std::string &path : drop->paths) {
                    result.droppedPaths.push_back(path);
                }
            }
        }
        accumulator.end_frame();
        return result;
    }
}

TEST_SUITE("platform/window_events") {

    TEST_CASE("window_events - an idle frame pumps nothing and advances the input frame") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        input accumulator = {};

        const frame_result frame = run_frame(*created, accumulator);
        CHECK(frame.eventCount == 0);
        CHECK_FALSE(frame.quitRequested);
        CHECK_FALSE(frame.swapchainResize.has_value());
        CHECK(accumulator.snapshot().frameIndex == 1);
    }

    TEST_CASE("window_events - two simulated resizes deliver one coalesced swapchain resize") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        null_backend &backend = *created->as_null();
        input accumulator = {};

        backend.simulate_resize(800, 600);
        backend.simulate_resize(1024, 768);
        const frame_result frame = run_frame(*created, accumulator);
        CHECK(frame.eventCount == 1);
        CHECK(frame.resizeCount == 1);
        REQUIRE(frame.swapchainResize.has_value());
        CHECK(*frame.swapchainResize == math::uvec2(1024, 768));
        CHECK(created->framebuffer_size() == math::uvec2(1024, 768));
    }

    TEST_CASE("window_events - minimising delivers a 0 x 0 resize the engine does not act on") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        null_backend &backend = *created->as_null();
        input accumulator = {};

        backend.simulate_minimise(true);
        const frame_result frame = run_frame(*created, accumulator);
        CHECK(frame.resizeCount == 1);
        CHECK_FALSE(frame.swapchainResize.has_value());
        CHECK(created->is_minimised());
        CHECK(created->pump_events().empty());     // pumping while minimised returns normally
    }

    TEST_CASE("window_events - losing focus releases held keys through input::apply and pauses the pacer") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        null_backend &backend = *created->as_null();
        input accumulator = {};

        backend.push_event(make_key(key::W, button_action::PRESS));
        backend.push_event(make_mouse_button(mouse_button::LEFT, button_action::PRESS));
        static_cast<void>(run_frame(*created, accumulator));
        CHECK(accumulator.snapshot().is_key_down(key::W));
        CHECK(accumulator.snapshot().is_mouse_button_down(mouse_button::LEFT));

        backend.simulate_focus(false);
        const frame_result frame = run_frame(*created, accumulator);
        CHECK(frame.pacerUnfocused);
        CHECK_FALSE(created->is_focused());
        CHECK_FALSE(accumulator.snapshot().is_key_down(key::W));
        CHECK(accumulator.snapshot().is_key_released(key::W));
        CHECK(accumulator.snapshot().is_mouse_button_released(mouse_button::LEFT));

        backend.simulate_focus(true);
        const frame_result regained = run_frame(*created, accumulator);
        CHECK_FALSE(regained.pacerUnfocused);
        CHECK_FALSE(accumulator.snapshot().keysReleased.any());
    }

    TEST_CASE("window_events - a close request reaches the engine as a quit") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        input accumulator = {};

        created->as_null()->simulate_close();
        const frame_result frame = run_frame(*created, accumulator);
        CHECK(frame.quitRequested);
        CHECK(created->should_close());
    }

    TEST_CASE("window_events - dropped files arrive with their paths for the frame") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        input accumulator = {};

        const std::vector<std::string> paths = {"models/robot.gltf", "textures/robot.png"};
        created->as_null()->simulate_drop_files(paths);
        const frame_result frame = run_frame(*created, accumulator);
        CHECK(frame.droppedPaths == paths);
        CHECK(accumulator.stats().eventsApplied == 0);   // a drop is a window event
    }

    TEST_CASE("window_events - a full frame mixes input and window events in pump order") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        null_backend &backend = *created->as_null();
        input accumulator = {};

        backend.push_event(make_mouse_move(10.0f, 10.0f));
        backend.push_event(make_key(key::SPACE, button_action::PRESS, key_modifier_flags::SHIFT));
        backend.push_event(make_text(' '));
        backend.push_event(make_scroll(0.0f, 1.0f));
        backend.simulate_resize(640, 480);
        backend.simulate_focus(false);
        backend.simulate_close();

        const frame_result frame = run_frame(*created, accumulator);
        CHECK(frame.eventCount == 7);
        CHECK(frame.quitRequested);
        CHECK(frame.pacerUnfocused);
        REQUIRE(frame.swapchainResize.has_value());
        CHECK(*frame.swapchainResize == math::uvec2(640, 480));

        const input_state &state = accumulator.snapshot();
        CHECK(state.frameIndex == 1);
        CHECK(state.mousePosition == math::vec2(10.0f, 10.0f));
        // Focus loss arrived after the press, so the key shows both edges and is no longer down.
        CHECK(state.is_key_pressed(key::SPACE));
        CHECK(state.is_key_released(key::SPACE));
        CHECK_FALSE(state.is_key_down(key::SPACE));
        CHECK(state.scrollDelta == math::vec2(0.0f, 1.0f));
        REQUIRE(state.text().size() == 1);
        CHECK(state.text()[0] == ' ');
        CHECK(accumulator.stats().eventsApplied == 4);
    }
}
