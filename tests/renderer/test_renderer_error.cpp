#include "conjure/testing/doctest.h"

#include "conjure/renderer/renderer_error.h"

#include "conjure/core/error.h"
#include "conjure/core/result.h"
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
}
