#pragma once

#include "conjure/rhi/shader_library.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <span>
#include <string>
#include <string_view>

// Shader programs (docs/design/02_renderer.md 4.2, R18). In v1 only the built-in programs exist.
// built_in_programs() is the one list of shaders the renderer loads: renderer::create finds the
// program of every pipeline (and, in editor builds, of the Dear ImGui backend) with find_built_in()
// and checks it with validate() before it creates a device shader of it, so a program missing here
// or a stage missing from the shader library's index fails creation with
// renderer_error::SHADER_NOT_FOUND. The list is not what the build compiles: that is
// engine/shaders/CMakeLists.txt, which the tests hold against this one in both directions. Custom
// user materials through shader_asset are future work.

namespace conjure::renderer {

    struct shader_program_desc {
        std::string name = {};                          // source stem: "forward_pbr"
        std::string vertexEntry = {};                   // "" for compute programs
        std::string pixelEntry = {};                    // "" for depth-only and compute programs
        std::string computeEntry = {};                  // "" for graphics programs
        std::string pixelDefines = {};                  // '|'-separated axis values of the pixel permutation
        std::string vertexDefines = {};                 // the same for the vertex stage; compute stages have none

        [[nodiscard]] bool operator==(const shader_program_desc &other) const noexcept = default;
    };

    class CONJURE_API shader_asset {
        private:
            shader_program_desc mDesc = {};

        public:
            shader_asset();
            explicit shader_asset(shader_program_desc desc);
            ~shader_asset();

            shader_asset(const shader_asset &) = default;
            shader_asset &operator=(const shader_asset &) = default;
            shader_asset(shader_asset &&) noexcept = default;
            shader_asset &operator=(shader_asset &&) noexcept = default;

        public:
            /// <summary>
            /// Every program the renderer loads, one entry per shader permutation: the pass programs
            /// (pipeline variants that differ only in render state share an entry), the blit and the
            /// Dear ImGui backend's program. The ImGui entry is listed in every configuration but is
            /// loaded only by an editor build's renderer created with enableImGui and a swapchain.
            /// </summary>
            [[nodiscard]] static std::span<const shader_program_desc> built_in_programs() noexcept;

            /// <summary>
            /// The built-in program with this name whose pixel and vertex define strings both equal
            /// the given ones ("" matches a stage without defines, it is not a wildcard), or
            /// core::error_code::NOT_FOUND naming all three.
            /// </summary>
            [[nodiscard]] static core::result<shader_asset> find_built_in(
              std::string_view name,
              std::string_view pixelDefines = {},
              std::string_view vertexDefines = {}
            );

            /// <summary>
            /// Succeeds when the library's index has a blob for its backend for every stage of the
            /// program (vertex, pixel, compute, in that order; a stage with an "" entry is skipped).
            /// Fails with renderer_error::SHADER_NOT_FOUND naming the first missing shader key. Reads
            /// the index only: a blob file that is listed but missing on disk is found when the
            /// renderer loads it, not here.
            /// </summary>
            [[nodiscard]] core::result<void> validate(rhi::shader_library &library) const;

            [[nodiscard]] const shader_program_desc &desc() const noexcept {
                return mDesc;
            }

            [[nodiscard]] bool operator==(const shader_asset &other) const noexcept = default;
    };
}
