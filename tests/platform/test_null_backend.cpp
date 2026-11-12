#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/memory_guard.h"
#include "platform_test_support.h"

#include "conjure/platform/null_backend.h"
#include "conjure/platform/window.h"

#include <memory>
#include <span>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::platform;
using namespace conjure::platform::test_support;

namespace {
    [[nodiscard]] null_backend &as_null_ref(const std::unique_ptr<window> &created) {
        null_backend *backend = created->as_null();
        REQUIRE(backend != nullptr);
        return *backend;
    }

    [[nodiscard]] const framebuffer_resized_event *last_resize(std::span<const event> events) {
        if (events.empty()) {
            return nullptr;
        }
        return event_as<framebuffer_resized_event>(events.back());
    }
}

TEST_SUITE("platform/null_backend") {

    TEST_CASE("null_backend - the headless size is 1280 x 720 and nothing is pending") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        null_backend &backend = as_null_ref(created);

        CHECK(backend.backend() == backend_kind::NULL_BACKEND);
        CHECK(backend.window_size() == math::uvec2(1280, 720));
        CHECK(backend.framebuffer_size() == math::uvec2(1280, 720));
        CHECK(backend.pending_event_count() == 0);
        CHECK_FALSE(backend.should_close());
        CHECK(null_backend::VIRTUAL_MONITOR_WIDTH == 1920);
        CHECK(null_backend::VIRTUAL_MONITOR_HEIGHT == 1080);
        CHECK(null_backend::VIRTUAL_MONITOR_REFRESH_HZ == 60);
    }

    TEST_CASE("null_backend - create builds the window directly") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<null_backend> created = null_backend::create(null_desc(320, 200));
        REQUIRE(created != nullptr);
        CHECK(created->window_size() == math::uvec2(320, 200));
        CHECK(created->as_null() == created.get());
        CHECK(live_window_count() == 1);
    }

    TEST_CASE("null_backend - pump_events is empty by default, frame after frame") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        for (u32 frame = 0; frame < 3; ++frame) {
            CAPTURE(frame);
            CHECK(created->pump_events().empty());
        }
    }

    TEST_CASE("null_backend - push_event is delivered exactly once on the next pump in order") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        null_backend &backend = as_null_ref(created);

        backend.push_event(make_key(key::A, button_action::PRESS));
        backend.push_event(make_mouse_move(1.0f, 2.0f));
        backend.push_event(make_key(key::A, button_action::RELEASE));
        CHECK(backend.pending_event_count() == 3);

        const std::span<const event> first = created->pump_events();
        REQUIRE(first.size() == 3);
        CHECK(backend.pending_event_count() == 0);
        const key_event *press = event_as<key_event>(first[0]);
        REQUIRE(press != nullptr);
        CHECK(press->keyCode == key::A);
        CHECK(press->action == button_action::PRESS);
        CHECK(event_as<mouse_move_event>(first[1]) != nullptr);
        const key_event *release = event_as<key_event>(first[2]);
        REQUIRE(release != nullptr);
        CHECK(release->action == button_action::RELEASE);

        CHECK(created->pump_events().empty());
    }

    TEST_CASE("null_backend - events pushed after a pump wait for the following pump") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        null_backend &backend = as_null_ref(created);

        backend.push_event(make_key(key::A, button_action::PRESS));
        const std::span<const event> first = created->pump_events();
        REQUIRE(first.size() == 1);
        const event copy = first[0];    // layers copy events by value; the span dies with the next pump

        backend.push_event(make_key(key::B, button_action::PRESS));
        const std::span<const event> second = created->pump_events();
        REQUIRE(second.size() == 1);
        CHECK(event_as<key_event>(second[0])->keyCode == key::B);
        CHECK(event_as<key_event>(copy)->keyCode == key::A);
    }

    TEST_CASE("null_backend - simulate_resize delivers one framebuffer_resized_event in pixels") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        window_desc desc = null_desc();
        desc.nullContentScale = math::vec2(1.5f, 1.5f);
        const std::unique_ptr<window> created = create_null_window(desc);
        null_backend &backend = as_null_ref(created);
        CHECK(created->framebuffer_size() == math::uvec2(1920, 1080));

        backend.simulate_resize(1001, 601);
        CHECK(backend.pending_event_count() == 1);
        CHECK(created->window_size() == math::uvec2(1001, 601));
        CHECK(created->framebuffer_size() == math::uvec2(1502, 902));   // round(1501.5), round(901.5)

        const std::span<const event> events = created->pump_events();
        REQUIRE(events.size() == 1);
        const framebuffer_resized_event *resized = last_resize(events);
        REQUIRE(resized != nullptr);
        CHECK(resized->width == 1502);
        CHECK(resized->height == 902);
        CHECK(created->pump_events().empty());
    }

    TEST_CASE("null_backend - several resizes in one frame are coalesced into the final size") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        null_backend &backend = as_null_ref(created);

        backend.push_event(make_key(key::A, button_action::PRESS));
        backend.simulate_resize(100, 100);
        backend.simulate_resize(200, 200);
        created->set_size(math::uvec2(300, 300));
        backend.push_event(make_key(key::A, button_action::RELEASE));
        CHECK(backend.pending_event_count() == 3);

        const std::span<const event> events = created->pump_events();
        REQUIRE(events.size() == 3);
        CHECK(event_as<key_event>(events[0]) != nullptr);
        CHECK(event_as<key_event>(events[1]) != nullptr);
        const framebuffer_resized_event *resized = last_resize(events);
        REQUIRE(resized != nullptr);
        CHECK(resized->width == 300);
        CHECK(resized->height == 300);
    }

    TEST_CASE("null_backend - simulate_content_scale changes the scale and the framebuffer") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window(null_desc(1000, 500));
        null_backend &backend = as_null_ref(created);

        backend.simulate_content_scale(math::vec2(2.0f, 2.0f));
        CHECK(created->content_scale() == math::vec2(2.0f, 2.0f));
        CHECK(created->window_size() == math::uvec2(1000, 500));
        CHECK(created->framebuffer_size() == math::uvec2(2000, 1000));
        CHECK(created->monitors()[0].contentScale == math::vec2(2.0f, 2.0f));

        const std::span<const event> events = created->pump_events();
        REQUIRE(events.size() == 2);
        const content_scale_changed_event *scaled = event_as<content_scale_changed_event>(events[0]);
        REQUIRE(scaled != nullptr);
        CHECK(scaled->scale == math::vec2(2.0f, 2.0f));
        const framebuffer_resized_event *resized = last_resize(events);
        REQUIRE(resized != nullptr);
        CHECK(resized->width == 2000);
        CHECK(resized->height == 1000);

        // The same scale again changes no size: only the scale event is delivered.
        backend.simulate_content_scale(math::vec2(2.0f, 2.0f));
        const std::span<const event> again = created->pump_events();
        REQUIRE(again.size() == 1);
        CHECK(event_as<content_scale_changed_event>(again[0]) != nullptr);
    }

    TEST_CASE("null_backend - simulate_focus toggles is_focused and delivers focus events") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        null_backend &backend = as_null_ref(created);

        backend.simulate_focus(false);
        CHECK_FALSE(created->is_focused());
        std::span<const event> events = created->pump_events();
        REQUIRE(events.size() == 1);
        const focus_event *lost = event_as<focus_event>(events[0]);
        REQUIRE(lost != nullptr);
        CHECK_FALSE(lost->focused);

        backend.simulate_focus(true);
        CHECK(created->is_focused());
        events = created->pump_events();
        REQUIRE(events.size() == 1);
        const focus_event *gained = event_as<focus_event>(events[0]);
        REQUIRE(gained != nullptr);
        CHECK(gained->focused);
    }

    TEST_CASE("null_backend - simulate_minimise zeroes the framebuffer and restores it") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        null_backend &backend = as_null_ref(created);

        backend.simulate_minimise(true);
        CHECK(created->is_minimised());
        CHECK(created->framebuffer_size() == math::uvec2(0));
        CHECK(created->window_size() == math::uvec2(1280, 720));
        std::span<const event> events = created->pump_events();
        REQUIRE(events.size() == 2);
        const minimise_event *minimised = event_as<minimise_event>(events[0]);
        REQUIRE(minimised != nullptr);
        CHECK(minimised->minimised);
        const framebuffer_resized_event *zero = last_resize(events);
        REQUIRE(zero != nullptr);
        CHECK(zero->width == 0);
        CHECK(zero->height == 0);

        backend.simulate_minimise(true);    // already minimised: nothing queued
        CHECK(backend.pending_event_count() == 0);

        backend.simulate_minimise(false);
        CHECK_FALSE(created->is_minimised());
        CHECK(created->framebuffer_size() == math::uvec2(1280, 720));
        events = created->pump_events();
        REQUIRE(events.size() == 2);
        CHECK_FALSE(event_as<minimise_event>(events[0])->minimised);
        const framebuffer_resized_event *restored = last_resize(events);
        REQUIRE(restored != nullptr);
        CHECK(restored->width == 1280);
        CHECK(restored->height == 720);
    }

    TEST_CASE("null_backend - simulate_close delivers a close_event and sets should_close") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        null_backend &backend = as_null_ref(created);

        backend.simulate_close();
        CHECK(created->should_close());
        const std::span<const event> events = created->pump_events();
        REQUIRE(events.size() == 1);
        CHECK(event_as<close_event>(events[0]) != nullptr);
        CHECK(created->should_close());
    }

    TEST_CASE("null_backend - simulate_drop_files paths stay valid until the next pump") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        null_backend &backend = as_null_ref(created);

        {
            const std::vector<std::string> paths = {"assets/a.png", "assets/b.gltf"};
            backend.simulate_drop_files(paths);
            const std::vector<std::string> more = {"c.wav"};
            backend.simulate_drop_files(more);
        }   // the caller's buffers are gone before the pump

        const std::span<const event> events = created->pump_events();
        REQUIRE(events.size() == 2);
        const drop_files_event *first = event_as<drop_files_event>(events[0]);
        REQUIRE(first != nullptr);
        REQUIRE(first->paths.size() == 2);
        CHECK(first->paths[0] == "assets/a.png");
        CHECK(first->paths[1] == "assets/b.gltf");
        const drop_files_event *second = event_as<drop_files_event>(events[1]);
        REQUIRE(second != nullptr);
        REQUIRE(second->paths.size() == 1);
        CHECK(second->paths[0] == "c.wav");

        CHECK(created->pump_events().empty());
    }

    TEST_CASE("null_backend - a drop_files_event pushed through push_event is copied too") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        null_backend &backend = as_null_ref(created);

        {
            const std::vector<std::string> paths = {"x.json"};
            drop_files_event dropped = {};
            dropped.paths = paths;
            backend.push_event(dropped);
        }
        const std::span<const event> events = created->pump_events();
        REQUIRE(events.size() == 1);
        const drop_files_event *drop = event_as<drop_files_event>(events[0]);
        REQUIRE(drop != nullptr);
        REQUIRE(drop->paths.size() == 1);
        CHECK(drop->paths[0] == "x.json");
    }

    TEST_CASE("null_backend - gamepad_name reports connected slots after the connection is delivered") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        null_backend &backend = as_null_ref(created);

        backend.push_event(make_gamepad_connection(1, true));
        CHECK(created->gamepad_name(1).empty());
        static_cast<void>(created->pump_events());
        CHECK(created->gamepad_name(1) == "Null Gamepad 1");
        CHECK(created->gamepad_name(0).empty());

        backend.push_event(make_gamepad_connection(1, false));
        static_cast<void>(created->pump_events());
        CHECK(created->gamepad_name(1).empty());

        backend.push_event(make_gamepad_connection(200, true));     // out of range: ignored by the window
        static_cast<void>(created->pump_events());
        for (u32 index = 0; index < MAX_GAMEPADS; ++index) {
            CHECK(created->gamepad_name(index).empty());
        }
    }

    TEST_CASE("null_backend - set_size and set_fullscreen queue the coalesced resize") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        null_backend &backend = as_null_ref(created);

        created->set_size(math::uvec2(1280, 720));  // unchanged: nothing queued
        CHECK(backend.pending_event_count() == 0);

        CONJURE_CHECK_OK(created->set_fullscreen(true));
        const std::span<const event> events = created->pump_events();
        REQUIRE(events.size() == 1);
        const framebuffer_resized_event *resized = last_resize(events);
        REQUIRE(resized != nullptr);
        CHECK(resized->width == 1920);
        CHECK(resized->height == 1080);
    }

    TEST_CASE("null_backend - icons holds deep copies of the last successful set_icon") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        null_backend &backend = as_null_ref(created);
        CHECK(backend.icons().empty());

        std::vector<core::image> first = {core::image(16, 16)};
        first[0].set_pixel(3, 4, core::rgba8{1, 2, 3, 4});
        CONJURE_REQUIRE_OK(created->set_icon(first));
        first.clear();
        REQUIRE(backend.icons().size() == 1);
        CHECK(backend.icons()[0].pixel(3, 4) == core::rgba8{1, 2, 3, 4});

        const std::vector<core::image> second = {core::image(24, 24), core::image(48, 48), core::image(64, 64)};
        CONJURE_REQUIRE_OK(created->set_icon(second));
        REQUIRE(backend.icons().size() == 3);
        CHECK(backend.icons()[2].height() == 64);

        // Setting the window's own icons again is a supported no-op, not a self-overwrite.
        CONJURE_REQUIRE_OK(created->set_icon(backend.icons()));
        REQUIRE(backend.icons().size() == 3);
        CHECK(backend.icons()[0].width() == 24);
        CHECK(backend.icons()[2].height() == 64);
    }

    TEST_CASE("null_backend - the pump allocates nothing after the first frame") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();
        null_backend &backend = as_null_ref(created);

        backend.push_event(make_key(key::A, button_action::PRESS));
        backend.simulate_resize(640, 480);
        static_cast<void>(created->pump_events());

        for (u32 frame = 0; frame < 3; ++frame) {
            CAPTURE(frame);
            CONJURE_CHECK_NO_ALLOCATIONS(backend.push_event(make_key(key::A, button_action::PRESS)));
            CONJURE_CHECK_NO_ALLOCATIONS(backend.push_event(make_mouse_move(1.0f, 1.0f)));
            CONJURE_CHECK_NO_ALLOCATIONS(backend.simulate_resize(700 + frame, 400));
            CONJURE_CHECK_NO_ALLOCATIONS(backend.simulate_focus(frame % 2 == 0));
            std::span<const event> events = {};
            CONJURE_CHECK_NO_ALLOCATIONS(events = created->pump_events());
            CHECK(events.size() == 4);
        }
    }

    TEST_CASE("null_backend - the window is deterministic: no OS, clock or random state") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        std::vector<std::string> firstRun = {};
        std::vector<std::string> secondRun = {};
        for (std::vector<std::string> *run : {&firstRun, &secondRun}) {
            const std::unique_ptr<window> created = create_null_window();
            null_backend &backend = as_null_ref(created);
            backend.push_event(make_key(key::W, button_action::PRESS));
            backend.simulate_resize(800, 600);
            backend.simulate_focus(false);
            for (const event &value : created->pump_events()) {
                run->push_back(to_string(value));
            }
            run->push_back(std::string(created->title()));
            run->push_back(std::to_string(created->position().x) + "," + std::to_string(created->position().y));
        }
        CHECK(firstRun == secondRun);
        CHECK(firstRun.size() == 5);
    }
}
