#include "conjure/testing/doctest.h"

#include "conjure/rhi/backend.h"

#include "conjure/core/error.h"

#include <array>
#include <string_view>

using namespace conjure;
using namespace conjure::rhi;

TEST_SUITE("rhi/backend") {

    TEST_CASE("graphics_backend - to_string names every backend in lower case") {
        CHECK(to_string(graphics_backend::NULL_DEVICE) == "null");
        CHECK(to_string(graphics_backend::D3D12) == "d3d12");
        CHECK(to_string(graphics_backend::VULKAN) == "vulkan");
        CHECK(to_string(graphics_backend::COUNT) == "null");
    }

    TEST_CASE("graphics_backend - parse_graphics_backend accepts any case and surrounding whitespace") {
        CONJURE_REQUIRE_OK(parse_graphics_backend("null"));
        CHECK(parse_graphics_backend("null").value() == graphics_backend::NULL_DEVICE);
        CHECK(parse_graphics_backend("D3D12").value() == graphics_backend::D3D12);
        CHECK(parse_graphics_backend("  Vulkan\n").value() == graphics_backend::VULKAN);
    }

    TEST_CASE("graphics_backend - parse_graphics_backend rejects unknown names") {
        const std::array<std::string_view, 5> rejected = {"", "d3d11", "metal", "null device", "nul"};
        for (const std::string_view name : rejected) {
            CAPTURE(name);
            const core::result<graphics_backend> parsed = parse_graphics_backend(name);
            CONJURE_CHECK_ERROR(parsed, core::error_code::INVALID_ARGUMENT);
            CHECK(parsed.get_error().message().find("unknown graphics backend") != std::string::npos);
        }
    }

    TEST_CASE("graphics_backend - the round trip through to_string and parse is the identity") {
        const std::array<graphics_backend, 3> backends = {
            graphics_backend::NULL_DEVICE,
            graphics_backend::D3D12,
            graphics_backend::VULKAN,
        };
        for (const graphics_backend backend : backends) {
            CAPTURE(to_string(backend));
            const core::result<graphics_backend> parsed = parse_graphics_backend(to_string(backend));
            CONJURE_REQUIRE_OK(parsed);
            CHECK(parsed.value() == backend);
        }
    }

    TEST_CASE("graphics_backend - the null device is always compiled and COUNT never is") {
        CHECK(is_backend_compiled(graphics_backend::NULL_DEVICE));
        CHECK_FALSE(is_backend_compiled(graphics_backend::COUNT));
    }

    TEST_CASE("graphics_backend - default_graphics_backend prefers D3D12, then Vulkan, then null") {
        const graphics_backend chosen = default_graphics_backend();
        CHECK(is_backend_compiled(chosen));
        if (is_backend_compiled(graphics_backend::D3D12)) {
            CHECK(chosen == graphics_backend::D3D12);
        } else if (is_backend_compiled(graphics_backend::VULKAN)) {
            CHECK(chosen == graphics_backend::VULKAN);
        } else {
            CHECK(chosen == graphics_backend::NULL_DEVICE);
        }
    }

    TEST_CASE("device_capabilities - defaults describe an unknown 16k adapter without queues") {
        const device_capabilities capabilities = {};
        CHECK(capabilities.backend == graphics_backend::NULL_DEVICE);
        CHECK(capabilities.adapterName.empty());
        CHECK(capabilities.maxTextureDimension2d == 16384);
        CHECK(capabilities.dedicatedVideoMemoryBytes == 0);
        CHECK_FALSE(capabilities.computeQueue);
        CHECK_FALSE(capabilities.timerQueries);
        CHECK_FALSE(capabilities.softwareAdapter);
    }
}
