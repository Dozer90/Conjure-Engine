#include "conjure/renderer/shader_asset.h"
#include "conjure/renderer/renderer_error.h"

#include "conjure/core/error.h"

#include <array>
#include <utility>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    /// <summary>
    /// Every shader permutation the renderer needs, as (name, vertexEntry, pixelEntry, computeEntry,
    /// pixelDefines, vertexDefines). Kept in step with engine/shaders/CMakeLists.txt and
    /// engine/src/renderer/pipeline_cache.cpp.
    /// </summary>
    const std::array<shader_program_desc, 20> &program_table() {
        static const std::array<shader_program_desc, 20> table = {{
          {"shadow_depth", "vs_main", "", "", "", "ALPHA_MASK=0"},
          {"shadow_depth", "vs_main", "ps_main", "", "ALPHA_MASK=1", "ALPHA_MASK=1"},
          {"depth_prepass", "vs_main", "ps_main", "", "ALPHA_MASK=0", ""},
          {"depth_prepass", "vs_main", "ps_main", "", "ALPHA_MASK=1", ""},
          {"forward_pbr", "vs_main", "ps_main", "", "ALPHA_MASK=0", ""},
          {"forward_pbr", "vs_main", "ps_main", "", "ALPHA_MASK=1", ""},
          {"skybox", "vs_main", "ps_main", "", "", ""},
          {"sprite", "vs_main", "ps_main", "", "", ""},
          {"debug_draw", "vs_main", "ps_main", "", "", ""},
          {"tonemap", "vs_main", "ps_main", "", "", ""},
          {"blit", "vs_main", "ps_main", "", "", ""},
          {"ibl_brdf_lut", "vs_main", "ps_main", "", "", ""},
          {"imgui", "vs_main", "ps_main", "", "", ""},
          {"light_cull", "", "", "cs_main", "", ""},
          {"ssao_gtao", "", "", "cs_main", "", ""},
          {"ssao_blur", "", "", "cs_main", "", ""},
          {"exposure_histogram", "", "", "cs_main", "", ""},
          {"exposure_adapt", "", "", "cs_main", "", ""},
          {"bloom_down", "", "", "cs_main", "", ""},
          {"bloom_up", "", "", "cs_main", "", ""},
        }};
        return table;
    }
}

shader_asset::shader_asset() {}

shader_asset::shader_asset(shader_program_desc desc) :
    mDesc(std::move(desc)) {}

shader_asset::~shader_asset() {}

std::span<const shader_program_desc> shader_asset::built_in_programs() noexcept {
    return program_table();
}

core::result<shader_asset> shader_asset::find_built_in(
  std::string_view name,
  std::string_view pixelDefines,
  std::string_view vertexDefines
) {
    for (const shader_program_desc &program : program_table()) {
        if (program.name == name && program.pixelDefines == pixelDefines && program.vertexDefines == vertexDefines) {
            return shader_asset(program);
        }
    }
    return core::make_error(
      core::error_code::NOT_FOUND,
      "shader_asset: no built-in program '{}' with defines '{}' / '{}'",
      name,
      pixelDefines,
      vertexDefines
    );
}

core::result<void> shader_asset::validate(rhi::shader_library &library) const {
    const auto check = [&library, this](std::string_view entry, std::string_view defines) -> core::result<void> {
        if (entry.empty()) {
            return core::result<void>();
        }
        rhi::shader_key key = {};
        key.name = mDesc.name;
        key.entry = entry;
        key.defines = defines;
        if (!library.contains(key)) {
            return core::make_error(
              renderer_error::SHADER_NOT_FOUND,
              "shader '{}' entry '{}' defines '{}' is not in the shader library",
              key.name,
              key.entry,
              key.defines
            );
        }
        return core::result<void>();
    };
    CONJURE_TRY(check(mDesc.vertexEntry, mDesc.vertexDefines));
    CONJURE_TRY(check(mDesc.pixelEntry, mDesc.pixelDefines));
    CONJURE_TRY(check(mDesc.computeEntry, ""));
    return core::result<void>();
}
