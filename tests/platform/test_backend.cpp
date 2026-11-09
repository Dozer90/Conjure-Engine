#include "conjure/testing/doctest.h"

#include "platform_test_support.h"

#include "conjure/platform/backend.h"
#include "conjure/platform/platform_error.h"

#include <string_view>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::platform;

TEST_SUITE("platform/backend") {

    TEST_CASE("backend_kind - values are stable and named") {
        CHECK(static_cast<u8>(backend_kind::NULL_BACKEND) == 0);
        CHECK(static_cast<u8>(backend_kind::GLFW) == 1);
        CHECK(static_cast<u8>(backend_kind::COUNT) == 2);
        CHECK(to_string(backend_kind::NULL_BACKEND) == "null");
        CHECK(to_string(backend_kind::GLFW) == "glfw");
        CHECK(to_string(backend_kind::COUNT).empty());
        CHECK(to_string(static_cast<backend_kind>(200)).empty());
    }

    TEST_CASE("backend_kind - parse_backend_kind accepts the two names in any case") {
        result<backend_kind> glfw = parse_backend_kind("glfw");
        CONJURE_REQUIRE_OK(glfw);
        CHECK(glfw.value() == backend_kind::GLFW);

        result<backend_kind> upper = parse_backend_kind("GLFW");
        CONJURE_REQUIRE_OK(upper);
        CHECK(upper.value() == backend_kind::GLFW);

        result<backend_kind> null = parse_backend_kind("null");
        CONJURE_REQUIRE_OK(null);
        CHECK(null.value() == backend_kind::NULL_BACKEND);

        result<backend_kind> mixed = parse_backend_kind("Null");
        CONJURE_REQUIRE_OK(mixed);
        CHECK(mixed.value() == backend_kind::NULL_BACKEND);
    }

    TEST_CASE("backend_kind - parse_backend_kind rejects unknown names with UNKNOWN_BACKEND") {
        const result<backend_kind> vulkan = parse_backend_kind("vulkan");
        CONJURE_CHECK_ERROR(vulkan, platform_error::UNKNOWN_BACKEND);
        CHECK(vulkan.get_error().message().find("vulkan") != std::string::npos);

        CONJURE_CHECK_ERROR(parse_backend_kind(""), platform_error::UNKNOWN_BACKEND);
        CONJURE_CHECK_ERROR(parse_backend_kind(" glfw"), platform_error::UNKNOWN_BACKEND);
        CONJURE_CHECK_ERROR(parse_backend_kind("null_backend"), platform_error::UNKNOWN_BACKEND);
    }

    TEST_CASE("backend_kind - to_string and parse_backend_kind round trip") {
        for (u8 code = 0; code < static_cast<u8>(backend_kind::COUNT); ++code) {
            CAPTURE(code);
            const backend_kind kind = static_cast<backend_kind>(code);
            const result<backend_kind> parsed = parse_backend_kind(to_string(kind));
            CONJURE_REQUIRE_OK(parsed);
            CHECK(parsed.value() == kind);
        }
    }

    TEST_CASE("backend_kind - the null backend is always compiled") {
        CHECK(is_backend_compiled(backend_kind::NULL_BACKEND));
        CHECK_FALSE(is_backend_compiled(backend_kind::COUNT));
        CHECK_FALSE(is_backend_compiled(static_cast<backend_kind>(77)));
    }

    TEST_CASE("backend_kind - default_backend_kind is consistent with is_backend_compiled") {
        const backend_kind chosen = default_backend_kind();
        CHECK(is_backend_compiled(chosen));
        if (is_backend_compiled(backend_kind::GLFW)) {
            CHECK(chosen == backend_kind::GLFW);
        } else {
            CHECK(chosen == backend_kind::NULL_BACKEND);
        }
    }
}
