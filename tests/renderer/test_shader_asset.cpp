#include "conjure/testing/doctest.h"

#include "conjure/testing/temp_directory.h"
#include "renderer_test_support.h"

#include "conjure/renderer/renderer_error.h"
#include "conjure/renderer/shader_asset.h"

#include "conjure/core/types.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/shader_library.h"

#include <array>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <string_view>

// shader_asset.h (docs/design/02_renderer.md 4.2, 3.12, R18): the table of built-in programs, which is
// the list of shaders the renderer loads (test_pipeline_cache.cpp holds the pipelines against it), its
// agreement with the compiled index, and the validation of a shader library against a program.

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

    TEST_CASE("shader_asset - the built-in programs and the compiled index list the same shaders") {
        // Two lists remain (design R18): engine/shaders/CMakeLists.txt, which the index is built from,
        // and the built-in table. "validate succeeds for every built-in program" proves every stage of
        // the table is compiled; this case proves nothing else is, apart from the rhi module's own
        // fullscreen-triangle pair.
        std::unique_ptr<rhi::device> device = make_device();
        std::unique_ptr<rhi::shader_library> shaders = open_shaders(*device);
        std::set<std::string> stages = {};
        const auto add = [&stages](const std::string &name, const std::string &entry, const std::string &defines) {
            if (!entry.empty()) {
                stages.insert(name + "/" + entry + "[" + defines + "]");
            }
        };
        for (const shader_program_desc &program : shader_asset::built_in_programs()) {
            CAPTURE(program.name);
            CHECK(program.name != "fullscreen_triangle");
            add(program.name, program.vertexEntry, program.vertexDefines);
            add(program.name, program.pixelEntry, program.pixelDefines);
            add(program.name, program.computeEntry, "");
        }
        CHECK(shaders->contains({"fullscreen_triangle", "vs_main", ""}));
        CHECK(shaders->contains({"fullscreen_triangle", "ps_main", ""}));
        CHECK(shaders->entry_count() == stages.size() + 2);
    }

    TEST_CASE("shader_asset - validate fails with SHADER_NOT_FOUND naming the missing key") {
        temp_directory directory("shaders");
        std::unique_ptr<rhi::device> device = make_device();
        SUBCASE("an empty index lacks the first stage: the vertex stage, or the compute stage of a compute program") {
            std::unique_ptr<rhi::shader_library> empty = open_written_shaders(*device, directory);
            core::result<shader_asset> forward = shader_asset::find_built_in("forward_pbr", "ALPHA_MASK=0");
            CONJURE_REQUIRE_OK(forward);
            const core::result<void> validated = forward.value().validate(*empty);
            CONJURE_CHECK_ERROR(validated, renderer_error::SHADER_NOT_FOUND);
            CHECK(validated.get_error().message().find("forward_pbr") != std::string::npos);
            CHECK(validated.get_error().message().find("vs_main") != std::string::npos);
            core::result<shader_asset> bloom = shader_asset::find_built_in("bloom_down");
            CONJURE_REQUIRE_OK(bloom);
            const core::result<void> bloomValidated = bloom.value().validate(*empty);
            CONJURE_CHECK_ERROR(bloomValidated, renderer_error::SHADER_NOT_FOUND);
            CHECK(bloomValidated.get_error().message().find("cs_main") != std::string::npos);
        }
        SUBCASE("a program with its vertex stage but another pixel permutation names the pixel key") {
            const std::array<index_stage, 2> stages = {{
              {"forward_pbr", "vs_main", "vertex", ""},
              {"forward_pbr", "ps_main", "pixel", "ALPHA_MASK=1"},
            }};
            std::unique_ptr<rhi::shader_library> partial = open_written_shaders(*device, directory, stages);
            core::result<shader_asset> masked = shader_asset::find_built_in("forward_pbr", "ALPHA_MASK=1");
            CONJURE_REQUIRE_OK(masked);
            CONJURE_CHECK_OK(masked.value().validate(*partial));
            core::result<shader_asset> solid = shader_asset::find_built_in("forward_pbr", "ALPHA_MASK=0");
            CONJURE_REQUIRE_OK(solid);
            const core::result<void> validated = solid.value().validate(*partial);
            CONJURE_CHECK_ERROR(validated, renderer_error::SHADER_NOT_FOUND);
            const std::string message = validated.get_error().message();
            CHECK(message.find("shader 'forward_pbr'") != std::string::npos);
            CHECK(message.find("entry 'ps_main'") != std::string::npos);
            CHECK(message.find("defines 'ALPHA_MASK=0'") != std::string::npos);
        }
    }

    TEST_CASE("shader_asset - validate reads the index only, a listed blob whose file is missing still validates") {
        temp_directory directory("listed_shaders");
        std::unique_ptr<rhi::device> device = make_device();
        const std::array<index_stage, 1> stages = {{{"light_cull", "cs_main", "compute", "", false}}};
        std::unique_ptr<rhi::shader_library> listed = open_written_shaders(*device, directory, stages);
        core::result<shader_asset> cull = shader_asset::find_built_in("light_cull");
        CONJURE_REQUIRE_OK(cull);
        CONJURE_CHECK_OK(cull.value().validate(*listed));
        CHECK(listed->loaded_blob_count() == 0);
    }
}
