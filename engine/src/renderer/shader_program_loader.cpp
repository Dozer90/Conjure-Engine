#include "renderer/shader_program_loader.h"
#include "conjure/renderer/renderer_error.h"

#include "conjure/core/error.h"

#include <string>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    /// <summary>
    /// The entry point and define string a program gives one stage; 'entry' is "" when the program
    /// has no such stage.
    /// </summary>
    struct program_stage {
        std::string_view label = "";
        std::string_view entry = "";
        std::string_view defines = "";
    };

    [[nodiscard]] program_stage stage_of(const shader_program_desc &desc, nvrhi::ShaderType stage) noexcept {
        if (stage == nvrhi::ShaderType::Vertex) {
            return {"vertex", desc.vertexEntry, desc.vertexDefines};
        }
        if (stage == nvrhi::ShaderType::Pixel) {
            return {"pixel", desc.pixelEntry, desc.pixelDefines};
        }
        if (stage == nvrhi::ShaderType::Compute) {
            return {"compute", desc.computeEntry, ""};
        }
        return {"requested", "", ""};
    }
}

core::result<shader_asset> conjure::renderer::resolve_built_in_program(
  rhi::shader_library &library,
  std::string_view name,
  std::string_view pixelDefines,
  std::string_view vertexDefines
) {
    core::result<shader_asset> program = shader_asset::find_built_in(name, pixelDefines, vertexDefines);
    if (!program.has_value()) {
        return core::make_error(renderer_error::SHADER_NOT_FOUND, "{}", program.get_error().message());
    }
    CONJURE_TRY(program.value().validate(library));
    return program;
}

core::result<nvrhi::ShaderHandle> conjure::renderer::create_program_shader(
  nvrhi::IDevice &device,
  rhi::shader_library &library,
  const shader_asset &program,
  nvrhi::ShaderType stage
) {
    const shader_program_desc &desc = program.desc();
    const program_stage wanted = stage_of(desc, stage);
    if (wanted.entry.empty()) {
        return core::make_error(
          renderer_error::SHADER_NOT_FOUND,
          "shader program '{}' has no {} stage",
          desc.name,
          wanted.label
        );
    }
    rhi::shader_key key = {};
    key.name = desc.name;
    key.entry = wanted.entry;
    key.defines = wanted.defines;
    core::result<rhi::shader_blob> blob = library.get(key);
    if (!blob.has_value()) {
        return core::make_error(
          renderer_error::SHADER_NOT_FOUND,
          "shader '{}' entry '{}' defines '{}': {}",
          key.name,
          key.entry,
          key.defines,
          blob.get_error().message()
        );
    }
    nvrhi::ShaderDesc shaderDesc = {};
    shaderDesc.shaderType = stage;
    shaderDesc.entryName = std::string(key.entry);
    shaderDesc.debugName = desc.name + "." + std::string(key.entry);
    if (!key.defines.empty()) {
        shaderDesc.debugName += "." + std::string(key.defines);
    }
    nvrhi::ShaderHandle shader = device.createShader(
      shaderDesc,
      blob.value().bytes.data(),
      blob.value().bytes.size()
    );
    if (shader == nullptr) {
        return core::make_error(
          renderer_error::PIPELINE_CREATION_FAILED,
          "shader '{}' entry '{}' defines '{}' was refused by the device",
          key.name,
          key.entry,
          key.defines
        );
    }
    return shader;
}
