#include "conjure/testing/doctest.h"

#include "conjure/platform/platform.h"

#include <string_view>

using namespace conjure;
using namespace conjure::platform;

TEST_SUITE("platform/platform") {

    TEST_CASE("platform - the umbrella header exposes every public type") {
        // Compiling these declarations is the test: the umbrella must pull in every header.
        const window_desc desc = {};
        const input_state state = {};
        const key_bitset bits = {};
        const monitor_info monitor = {};
        const native_window_handle handle = {};
        const event value = key_event();
        const input_stats stats = {};
        CHECK(desc.backend == backend_kind::GLFW);
        CHECK(state.frameIndex == 0);
        CHECK_FALSE(bits.any());
        CHECK_FALSE(monitor.isPrimary);
        CHECK(handle.kind == native_handle_kind::NONE);
        CHECK(is_input_event(value));
        CHECK(stats.eventsApplied == 0);
        CHECK(std::string_view(LOG_PLATFORM.name()) == "platform");
        CHECK(platform_error_name(platform_error::INIT_FAILED) == "INIT_FAILED");
    }

    TEST_CASE("platform - backend_version_string names the null backend") {
        CHECK(backend_version_string(backend_kind::NULL_BACKEND) == "null");
    }

    TEST_CASE("platform - backend_version_string of the glfw backend reflects whether it was compiled") {
        const std::string_view version = backend_version_string(backend_kind::GLFW);
        CHECK_FALSE(version.empty());
        if (is_backend_compiled(backend_kind::GLFW)) {
            CHECK(version.find("3.4") != std::string_view::npos);
        } else {
            CHECK(version == "glfw (not compiled)");
        }
    }

    TEST_CASE("platform - backend_version_string is empty for unknown kinds") {
        CHECK(backend_version_string(backend_kind::COUNT).empty());
        CHECK(backend_version_string(static_cast<backend_kind>(99)).empty());
    }
}
