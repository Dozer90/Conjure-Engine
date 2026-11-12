#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/memory_guard.h"
#include "platform_test_support.h"

#include "conjure/platform/monitor.h"
#include "conjure/platform/null_backend.h"
#include "conjure/platform/window.h"

#include <memory>
#include <span>

using namespace conjure;
using namespace conjure::platform;
using namespace conjure::platform::test_support;

TEST_SUITE("platform/monitor") {

    TEST_CASE("video_mode - defaults are zero and compare by value") {
        const video_mode mode = {};
        CHECK(mode.width == 0);
        CHECK(mode.height == 0);
        CHECK(mode.refreshRateHz == 0);
        video_mode other = {};
        CHECK(mode == other);
        other.refreshRateHz = 60;
        CHECK_FALSE(mode == other);
    }

    TEST_CASE("monitor_info - defaults describe an unnamed secondary monitor at the origin") {
        const monitor_info info = {};
        CHECK(info.name.empty());
        CHECK_FALSE(info.isPrimary);
        CHECK(info.position == math::ivec2(0));
        CHECK(info.currentMode == video_mode());
        CHECK(info.contentScale == math::vec2(1.0f));
        CHECK(info.workArea == math::urect());
        CHECK(info.workArea.is_empty());
        monitor_info other = {};
        CHECK(info == other);
        other.name = "Second";
        CHECK_FALSE(info == other);
    }

    TEST_CASE("monitor_info - the null window reports one primary 1920 x 1080 @ 60 Hz monitor") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const std::unique_ptr<window> created = create_null_window();

        const std::span<const monitor_info> monitors = created->monitors();
        REQUIRE(monitors.size() == 1);
        CHECK(monitors[0].name == "Null Monitor");
        CHECK(monitors[0].isPrimary);
        CHECK(monitors[0].position == math::ivec2(0));
        CHECK(monitors[0].currentMode.width == 1920);
        CHECK(monitors[0].currentMode.height == 1080);
        CHECK(monitors[0].currentMode.refreshRateHz == 60);
        CHECK(monitors[0].contentScale == math::vec2(1.0f));
        CHECK(monitors[0].workArea.x == 0);
        CHECK(monitors[0].workArea.y == 0);
        CHECK(monitors[0].workArea.width == 1920);
        CHECK(monitors[0].workArea.height == 1080);

        const std::span<const video_mode> modes = created->video_modes(0);
        REQUIRE(modes.size() == 1);
        CHECK(modes[0] == monitors[0].currentMode);
        CHECK(created->video_modes(1).empty());
        CHECK(created->video_modes(0xFFFFFFFFu).empty());
    }

    TEST_CASE("monitor_info - the null monitor reports the content scale the window was created with") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        window_desc desc = null_desc();
        desc.nullContentScale = math::vec2(2.0f, 1.5f);
        const std::unique_ptr<window> created = create_null_window(desc);

        // The window sits on the virtual monitor, so the two scales agree from creation on, as they do after
        // simulate_content_scale.
        const std::span<const monitor_info> monitors = created->monitors();
        REQUIRE(monitors.size() == 1);
        CHECK(monitors[0].contentScale == math::vec2(2.0f, 1.5f));
        CHECK(monitors[0].contentScale == created->content_scale());
    }
}
