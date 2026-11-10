#pragma once

#include "conjure/rhi/shader_library.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <span>
#include <string>
#include <string_view>

// Shader programs (docs/design/02_renderer.md 4.2). In v1 only the built-in programs exist: the
// list below is the one renderer::create instantiates, and validate() proves a shader_library carries
// every permutation of a program. Custom user materials through shader_asset are future work.

namespace conjure::renderer {

    struct shader_program_desc {
        std::string name = {};                          // source stem: "forward_pbr"
        std::string vertexEntry = {};                   // "" for compute programs
        std::string pixelEntry = {};                    // "" for depth-only and compute programs
        std::string computeEntry = {};                  // "" for graphics programs
        std::string pixelDefines = {};                  // '|'-separated axis values of the pixel permutation
        std::string vertexDefines = {};

        [[nodiscard]] bool operator==(const shader_program_desc &other) const noexcept = default;
    };

    class shader_asset {
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
            /// Every program the renderer creates at start-up, one entry per shader permutation it
            /// needs (pipeline variants reuse entries).
            /// </summary>
            [[nodiscard]] static std::span<const shader_program_desc> built_in_programs() noexcept;

            /// <summary>
            /// The built-in program with this name and permutation, or NOT_FOUND.
            /// </summary>
            [[nodiscard]] static core::result<shader_asset> find_built_in(
              std::string_view name,
              std::string_view pixelDefines = {},
              std::string_view vertexDefines = {}
            );

            /// <summary>
            /// True when every stage of the program is present in the library. Fails with
            /// renderer_error::SHADER_NOT_FOUND naming the missing key.
            /// </summary>
            [[nodiscard]] core::result<void> validate(rhi::shader_library &library) const;

            [[nodiscard]] const shader_program_desc &desc() const noexcept {
                return mDesc;
            }

            [[nodiscard]] bool operator==(const shader_asset &other) const noexcept = default;
    };
}
