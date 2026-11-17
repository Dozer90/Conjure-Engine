#pragma once

#include "conjure/renderer/shader_asset.h"

#include "conjure/core/result.h"
#include "conjure/rhi/shader_library.h"

#include <nvrhi/nvrhi.h>

#include <string_view>

// How the renderer gets its device shaders (docs/design/02_renderer.md R18, 3.12; code sweep F395).
// pipeline_cache and imgui_renderer never name a shader blob themselves: they resolve a program of
// shader_asset's built-in table, which checks every stage of it against the shader library, and
// then create the stages they need. The table is therefore the one C++ list of what the renderer
// loads; engine/shaders/CMakeLists.txt is the list of what the build compiles.

namespace conjure::renderer {

    /// <summary>
    /// The built-in program with this name and permutation, after shader_asset::validate() found every
    /// stage of it in the library. Fails with renderer_error::SHADER_NOT_FOUND when the built-in
    /// table has no such program (the message names the program and both define strings) or when
    /// the library lacks the blob of a stage (the message names the shader key). Reads the index
    /// only: no blob file is opened and no device object is created.
    /// </summary>
    [[nodiscard]] core::result<shader_asset> resolve_built_in_program(
      rhi::shader_library &library,
      std::string_view name,
      std::string_view pixelDefines = {},
      std::string_view vertexDefines = {}
    );

    /// <summary>
    /// Creates the device shader of one stage of a program: 'stage' is nvrhi::ShaderType::Vertex,
    /// Pixel or Compute. Fails with renderer_error::SHADER_NOT_FOUND when the program has no such
    /// stage ("" entry, any other ShaderType included) or the library cannot hand out the blob (not
    /// in the index, or the blob file is unreadable or empty), and with PIPELINE_CREATION_FAILED
    /// when the device refuses the bytecode.
    /// </summary>
    [[nodiscard]] core::result<nvrhi::ShaderHandle> create_program_shader(
      nvrhi::IDevice &device,
      rhi::shader_library &library,
      const shader_asset &program,
      nvrhi::ShaderType stage
    );
}
