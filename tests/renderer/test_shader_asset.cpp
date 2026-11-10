#include "conjure/testing/doctest.h"

#include "conjure/testing/temp_directory.h"
#include "renderer_test_support.h"

#include "conjure/renderer/renderer_error.h"
#include "conjure/renderer/shader_asset.h"

#include "conjure/core/types.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/shader_library.h"

#include <memory>
#include <set>
#include <span>
#include <string>
#include <string_view>

// shader_asset.h (docs/design/02_renderer.md 4.2, 3.12): the table of built-in programs the
// renderer instantiates and the validation of a shader library against it.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;
using namespace conjure::testing::renderer_support;

TEST_SUITE("renderer/shader_asset") {

    TEST_CASE("shader_asset - built_in_programs lists every pass program once") {
        const std::span<const shader_program_desc> programs = shader_asset::built_in_programs();
        CHECK(programs.size() == 20);
        std::set<std::string> keys = {};
        for (const shader_program_desc &program : programs) {
            CHECK_FALSE(program.name.empty());
            const bool compute = !program.computeEntry.empty();
            const bool graphics = !program.vertexEntry.empty();
            CHECK(compute != graphics);
            if (compute) {
                CHECK(program.pixelEntry.empty());
                CHECK(program.vertexEntry.empty());
            }
            keys.insert(program.name + "|" + program.pixelDefines + "|" + program.vertexDefines);
        }
        CHECK(keys.size() == programs.size());
        // The permutation axes of the mesh passes are present.
        CHECK(keys.contains("forward_pbr|ALPHA_MASK=0|"));
        CHECK(keys.contains("forward_pbr|ALPHA_MASK=1|"));
        CHECK(keys.contains("shadow_depth||ALPHA_MASK=0"));
        CHECK(keys.contains("shadow_depth|ALPHA_MASK=1|ALPHA_MASK=1"));
        CHECK(keys.contains("depth_prepass|ALPHA_MASK=1|"));
        CHECK(keys.contains("light_cull||"));
        CHECK(keys.contains("ssao_gtao||"));
        CHECK(keys.contains("bloom_up||"));
        CHECK(keys.contains("tonemap||"));
        CHECK(keys.contains("blit||"));
        CHECK(keys.contains("imgui||"));
        CHECK(keys.contains("ibl_brdf_lut||"));
    }

    TEST_CASE("shader_asset - find_built_in matches name and permutation, NOT_FOUND otherwise") {
        core::result<shader_asset> forward = shader_asset::find_built_in("forward_pbr", "ALPHA_MASK=1");
        CONJURE_REQUIRE_OK(forward);
        CHECK(forward.value().desc().name == "forward_pbr");
        CHECK(forward.value().desc().vertexEntry == "vs_main");
        CHECK(forward.value().desc().pixelEntry == "ps_main");
        CHECK(forward.value().desc().pixelDefines == "ALPHA_MASK=1");
        core::result<shader_asset> cull = shader_asset::find_built_in("light_cull");
        CONJURE_REQUIRE_OK(cull);
        CHECK(cull.value().desc().computeEntry == "cs_main");
        core::result<shader_asset> depthOnly = shader_asset::find_built_in("shadow_depth", "", "ALPHA_MASK=0");
        CONJURE_REQUIRE_OK(depthOnly);
        CHECK(depthOnly.value().desc().pixelEntry.empty());
        CONJURE_CHECK_ERROR(shader_asset::find_built_in("forward_pbr", "ALPHA_MASK=2"), core::error_code::NOT_FOUND);
        CONJURE_CHECK_ERROR(shader_asset::find_built_in("forward_pbr"), core::error_code::NOT_FOUND);
        CONJURE_CHECK_ERROR(shader_asset::find_built_in("raytrace"), core::error_code::NOT_FOUND);
        CONJURE_CHECK_ERROR(shader_asset::find_built_in(""), core::error_code::NOT_FOUND);
        const shader_asset copy = forward.value();
        CHECK(copy == forward.value());
        CHECK_FALSE(copy == cull.value());
        CHECK(shader_asset().desc() == shader_program_desc());
    }

    TEST_CASE("shader_asset - validate succeeds for every built-in program against the compiled index") {
        std::unique_ptr<rhi::device> device = make_device();
        std::unique_ptr<rhi::shader_library> shaders = open_shaders(*device);
        for (const shader_program_desc &program : shader_asset::built_in_programs()) {
            CAPTURE(program.name);
            CAPTURE(program.pixelDefines);
            CONJURE_CHECK_OK(shader_asset(program).validate(*shaders));
        }
    }

    TEST_CASE("shader_asset - validate fails with SHADER_NOT_FOUND naming the missing key") {
        temp_directory directory("shaders");
        CONJURE_REQUIRE_OK(directory.write_text("index.json", "{ \"version\": 1, \"shaders\": [] }"));
        std::unique_ptr<rhi::device> device = make_device();
        core::result<std::unique_ptr<rhi::shader_library>> opened = rhi::shader_library::open(
          *device,
          directory.path()
        );
        CONJURE_REQUIRE_OK(opened);
        CHECK(opened.value()->entry_count() == 0);
        core::result<shader_asset> forward = shader_asset::find_built_in("forward_pbr", "ALPHA_MASK=0");
        CONJURE_REQUIRE_OK(forward);
        const core::result<void> validated = forward.value().validate(*opened.value());
        CONJURE_CHECK_ERROR(validated, renderer_error::SHADER_NOT_FOUND);
        CHECK(validated.get_error().message().find("forward_pbr") != std::string::npos);
        CHECK(validated.get_error().message().find("vs_main") != std::string::npos);
        // A compute program names its compute entry.
        core::result<shader_asset> bloom = shader_asset::find_built_in("bloom_down");
        CONJURE_REQUIRE_OK(bloom);
        const core::result<void> bloomValidated = bloom.value().validate(*opened.value());
        CONJURE_CHECK_ERROR(bloomValidated, renderer_error::SHADER_NOT_FOUND);
        CHECK(bloomValidated.get_error().message().find("cs_main") != std::string::npos);
    }
}
