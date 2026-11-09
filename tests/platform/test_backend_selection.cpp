#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/memory_guard.h"
#include "platform_test_support.h"

#include "conjure/platform/backend.h"
#include "conjure/platform/platform_error.h"
#include "conjure/platform/window.h"

#include <memory>
#include <string>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::platform;
using namespace conjure::platform::test_support;

namespace {
    /// <summary>
    /// The engine's window_settings (06_engine_loop.md 2.3) as platform_subsystem maps them onto a
    /// window_desc (11_platform.md 8): headless picks the null backend, the rest copies through.
    /// </summary>
    struct window_settings {
        std::string title = "Conjure";
        u32 width = 1600;
        u32 height = 900;
        bool fullscreen = false;
        bool resizable = true;
        bool vsync = true;
    };

    [[nodiscard]] window_settings headless_window_settings() {
        window_settings settings = {};
        settings.width = 1280;
        settings.height = 720;
        return settings;
    }

    [[nodiscard]] window_desc to_window_desc(const window_settings &settings, bool headless) {
        window_desc desc = {};
        desc.backend = headless ? backend_kind::NULL_BACKEND : default_backend_kind();
        desc.title = settings.title;
        desc.width = settings.width;
        desc.height = settings.height;
        desc.fullscreen = settings.fullscreen;
        desc.flags = settings.resizable ? window_flags::DEFAULT : (window_flags::DEFAULT & ~window_flags::RESIZABLE);
        return desc;
    }
}

TEST_SUITE("platform/backend_selection") {

    TEST_CASE("backend_selection - requesting the glfw backend when it is not compiled fails cleanly") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        window_desc desc = {};
        desc.backend = backend_kind::GLFW;

        if (is_backend_compiled(backend_kind::GLFW)) {
            // The glfw backend exists in this build; creating a real window belongs to the [window]
            // conformance cases. The compiled-in answer must agree with the default choice.
            CHECK(default_backend_kind() == backend_kind::GLFW);
            CHECK(backend_version_string(backend_kind::GLFW).find("not compiled") == std::string_view::npos);
            return;
        }

        const result<std::unique_ptr<window>> created = create_window(desc);
        CONJURE_CHECK_ERROR(created, platform_error::BACKEND_NOT_COMPILED);
        CHECK(created.get_error().message().find("glfw") != std::string::npos);
        CHECK(default_backend_kind() == backend_kind::NULL_BACKEND);
        CHECK(backend_version_string(backend_kind::GLFW) == "glfw (not compiled)");
        CHECK(live_window_count() == 0);
    }

    TEST_CASE("backend_selection - an unknown backend name is UNKNOWN_BACKEND and a bad kind INVALID_DESC") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        CONJURE_CHECK_ERROR(parse_backend_kind("sdl"), platform_error::UNKNOWN_BACKEND);

        window_desc desc = null_desc();
        desc.backend = static_cast<backend_kind>(5);
        CONJURE_CHECK_ERROR(create_window(desc), platform_error::INVALID_DESC);
        CHECK(live_window_count() == 0);
    }

    TEST_CASE("backend_selection - headless window_settings map to a 1280 x 720 null window") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const window_desc desc = to_window_desc(headless_window_settings(), true);
        CHECK(desc.backend == backend_kind::NULL_BACKEND);
        CHECK(desc.width == 1280);
        CHECK(desc.height == 720);
        CHECK(desc.title == "Conjure");
        CHECK_FALSE(desc.fullscreen);
        CHECK(has_flag(desc.flags, window_flags::RESIZABLE));

        const std::unique_ptr<window> created = create_null_window(desc);
        CHECK(created->backend() == backend_kind::NULL_BACKEND);
        CHECK(created->window_size() == math::uvec2(1280, 720));
        CHECK(created->title() == "Conjure");
        CHECK(created->pump_events().empty());
    }

    TEST_CASE("backend_selection - non-headless window_settings pick the default backend and copy the fields") {
        window_settings settings = {};
        settings.title = "My Game";
        settings.width = 1920;
        settings.height = 1080;
        settings.fullscreen = true;
        settings.resizable = false;

        const window_desc desc = to_window_desc(settings, false);
        CHECK(desc.backend == default_backend_kind());
        CHECK(is_backend_compiled(desc.backend));
        CHECK(desc.title == "My Game");
        CHECK(desc.width == 1920);
        CHECK(desc.height == 1080);
        CHECK(desc.fullscreen);
        CHECK_FALSE(has_flag(desc.flags, window_flags::RESIZABLE));
        CHECK(has_flag(desc.flags, window_flags::DECORATED));
        CONJURE_CHECK_OK(validate_window_desc(desc));
    }

    TEST_CASE("backend_selection - a parsed configuration name selects the backend of a description") {
        testing::memory_guard guard = {};
        testing::log_capture logs = {};
        const result<backend_kind> parsed = parse_backend_kind("NULL");
        CONJURE_REQUIRE_OK(parsed);
        window_desc desc = null_desc();
        desc.backend = parsed.value();
        const std::unique_ptr<window> created = create_null_window(desc);
        CHECK(created->backend() == backend_kind::NULL_BACKEND);
        CHECK(to_string(created->backend()) == "null");
    }
}
