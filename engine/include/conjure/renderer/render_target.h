#pragma once

#include "conjure/renderer/gpu_ids.h"

#include "conjure/rhi/texture_format.h"

#include "conjure/core/api.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <memory>

// Off-screen targets (docs/design/02_renderer.md 4.2, R12): the editor viewport and the AI agent's
// screenshots render into one and read it back synchronously through renderer::read_back. Created,
// resized and destroyed through the renderer; this class exposes only what the editor needs.

namespace conjure::renderer {

    struct renderer_access;

    struct render_target_desc {
        math::uvec2 size = math::uvec2(0, 0);
        rhi::texture_format colourFormat = rhi::texture_format::RGBA8_SRGB;
        bool withDepth = true;

        [[nodiscard]] bool operator==(const render_target_desc &other) const noexcept = default;
    };

    class CONJURE_API render_target {
        public:
            /// <summary>
            /// The private state (engine/src/renderer/render_target_impl.h), reachable through renderer_access.
            /// </summary>
            struct impl;

        private:
            std::unique_ptr<impl> mImpl = nullptr;

            friend struct renderer_access;

        public:
            render_target();
            ~render_target();

            render_target(const render_target &) = delete;
            render_target &operator=(const render_target &) = delete;

        public:
            /// <summary>
            /// The description as created or last resized.
            /// </summary>
            [[nodiscard]] const render_target_desc &desc() const noexcept;

            /// <summary>
            /// Current size in pixels.
            /// </summary>
            [[nodiscard]] math::uvec2 size() const noexcept;

            /// <summary>
            /// The colour texture registered with the renderer's texture table, so the editor can bind
            /// it through imgui_renderer::register_texture.
            /// </summary>
            [[nodiscard]] gpu_texture_id colour_texture() const noexcept;

            [[nodiscard]] bool has_depth() const noexcept;

            /// <summary>
            /// Number of frames rendered into this target since creation or the last resize.
            /// </summary>
            [[nodiscard]] u64 frames_rendered() const noexcept;
    };

    /// <summary>
    /// True for the colour formats a render target may use: RGBA8_UNORM, RGBA8_SRGB, BGRA8_SRGB,
    /// RGBA16_FLOAT and R11G11B10_FLOAT.
    /// </summary>
    [[nodiscard]] CONJURE_API bool is_render_target_colour_format(rhi::texture_format format) noexcept;
}
