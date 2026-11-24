#include "conjure/testing/doctest.h"

#include "conjure/renderer/renderer_error.h"

#include "conjure/core/error.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/rhi/rhi_error.h"

#include <string_view>

using namespace conjure;
using namespace conjure::renderer;

TEST_SUITE("renderer/renderer_error") {

    TEST_CASE("renderer_error - renderer_error_name names every code and nothing else") {
        CHECK(renderer_error_name(renderer_error::SHADER_NOT_FOUND) == "SHADER_NOT_FOUND");
        CHECK(renderer_error_name(renderer_error::PIPELINE_CREATION_FAILED) == "PIPELINE_CREATION_FAILED");
        CHECK(renderer_error_name(renderer_error::RESOURCE_CREATION_FAILED) == "RESOURCE_CREATION_FAILED");
        CHECK(renderer_error_name(renderer_error::DECODE_FAILED) == "DECODE_FAILED");
        CHECK(renderer_error_name(renderer_error::INVALID_ASSET) == "INVALID_ASSET");
        CHECK(renderer_error_name(renderer_error::INVALID_SETTINGS) == "INVALID_SETTINGS");
        CHECK(renderer_error_name(renderer_error::COUNT) == "");
        CHECK(renderer_error_name(static_cast<renderer_error>(0)) == "");
        CHECK(renderer_error_name(static_cast<renderer_error>(200)) == "");
    }

    TEST_CASE("renderer_error - make_error binds the codes to the renderer category") {
        const core::error failure = core::make_error(
          renderer_error::SHADER_NOT_FOUND,
          "shader '{}' is missing",
          "forward_pbr"
        );
        CHECK(failure.category_name() == "renderer");
        CHECK(failure.code_name() == "SHADER_NOT_FOUND");
        CHECK(failure.message() == "shader 'forward_pbr' is missing");
        CHECK(failure.is(renderer_error::SHADER_NOT_FOUND));
        CHECK_FALSE(failure.is(renderer_error::INVALID_ASSET));
        CHECK_FALSE(failure.is(core::error_code::NOT_FOUND));
        CHECK_FALSE(failure.is(rhi::rhi_error::DEVICE_LOST));
        CHECK(failure.is_category<renderer_error>());
        CHECK_FALSE(failure.is_category<rhi::rhi_error>());
    }

    TEST_CASE("renderer_error - a result carrying a renderer error reports it through the testing helpers") {
        const core::result<int> failed = core::make_error(renderer_error::INVALID_ASSET, "mesh: short read");
        CONJURE_CHECK_ERROR(failed, renderer_error::INVALID_ASSET);
        CHECK(testing::describe_error(failed).find("INVALID_ASSET") != std::string::npos);
        CHECK(testing::describe_error(failed).find("short read") != std::string::npos);
    }

    TEST_CASE("renderer_error - INVALID_SETTINGS is the last code and reports through the renderer category") {
        CHECK(static_cast<u16>(renderer_error::INVALID_SETTINGS) + 1 == static_cast<u16>(renderer_error::COUNT));
        const core::error failure = core::make_error(
          renderer_error::INVALID_SETTINGS,
          "{}: {}",
          "rendering/defaults/ssao/radius",
          "20 is outside [0.05, 10]"
        );
        CHECK(failure.category_name() == "renderer");
        CHECK(failure.code_name() == "INVALID_SETTINGS");
        CHECK(failure.is(renderer_error::INVALID_SETTINGS));
        CHECK_FALSE(failure.is(renderer_error::INVALID_ASSET));
        CHECK(failure.message() == "rendering/defaults/ssao/radius: 20 is outside [0.05, 10]");
    }

    TEST_CASE("renderer_error - errors are built with core::make_error, no factory of the module's own") {
        // The error pattern of code sweep F625, which this module already followed: a guard, not a regression.
        CHECK_FALSE(testing::declares_own_make_error<renderer_error>);
    }
}
