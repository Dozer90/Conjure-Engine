#pragma once

#include "renderer/binding_layouts.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/rhi/shader_library.h"
#include "conjure/rhi/texture_format.h"

#include <nvrhi/nvrhi.h>

#include <array>
#include <memory>
#include <unordered_map>

// Every pipeline the renderer uses, created at renderer::create (docs/design/02_renderer.md R7,
// 6: "fail at init, never mid-frame"). Graphics variants are enumerated below; the mesh variants
// exist twice (back-face culled and double-sided). Blit pipelines are the one exception: their
// colour format is the target's, so they are created per format at create (swapchain) and at
// create_render_target, never during a frame.

namespace conjure::renderer {

    enum class graphics_variant : u8 {
        SHADOW_DEPTH = 0,
        SHADOW_DEPTH_MASKED,
        DEPTH_PREPASS,
        DEPTH_PREPASS_MASKED,
        FORWARD_OPAQUE,
        FORWARD_MASKED,
        FORWARD_BLEND,
        SKYBOX,
        WORLD_SPRITES,
        SCREEN_SPRITES,
        DEBUG_DEPTH_TESTED,
        DEBUG_OVERLAY,
        TONEMAP,
        BRDF_LUT,
        COUNT
    };

    enum class compute_variant : u8 {
        LIGHT_CULL = 0,
        SSAO_GTAO,
        SSAO_BLUR,
        EXPOSURE_HISTOGRAM,
        EXPOSURE_ADAPT,
        BLOOM_DOWN,
        BLOOM_UP,
        COUNT
    };

    /// <summary>
    /// Mesh variants exist with back-face culling and without (double-sided materials).
    /// </summary>
    [[nodiscard]] constexpr bool is_mesh_variant(graphics_variant variant) noexcept {
        return variant <= graphics_variant::FORWARD_BLEND;
    }

    [[nodiscard]] const char *to_string(graphics_variant variant) noexcept;
    [[nodiscard]] const char *to_string(compute_variant variant) noexcept;

    class pipeline_cache {
        private:
            struct create_tag {};

            static constexpr usize GRAPHICS_COUNT = static_cast<usize>(graphics_variant::COUNT);
            static constexpr usize COMPUTE_COUNT = static_cast<usize>(compute_variant::COUNT);

            nvrhi::IDevice *mDevice = nullptr;
            rhi::shader_library *mShaders = nullptr;
            const binding_layouts *mLayouts = nullptr;
            std::array<nvrhi::GraphicsPipelineHandle, GRAPHICS_COUNT> mGraphics = {};
            std::array<nvrhi::GraphicsPipelineHandle, GRAPHICS_COUNT> mGraphicsDoubleSided = {};
            std::array<nvrhi::ComputePipelineHandle, COMPUTE_COUNT> mCompute = {};
            std::unordered_map<u8, nvrhi::GraphicsPipelineHandle> mBlit = {};
            nvrhi::InputLayoutHandle mMeshLayout = nullptr;
            nvrhi::InputLayoutHandle mPositionLayout = nullptr;
            nvrhi::InputLayoutHandle mSpriteLayout = nullptr;
            nvrhi::InputLayoutHandle mDebugLayout = nullptr;
            nvrhi::ShaderHandle mBlitVertex = nullptr;
            nvrhi::ShaderHandle mBlitPixel = nullptr;
            u32 mPipelinesCreated = 0;

        public:
            explicit pipeline_cache(create_tag);
            ~pipeline_cache();

            pipeline_cache(const pipeline_cache &) = delete;
            pipeline_cache &operator=(const pipeline_cache &) = delete;

        public:
            /// <summary>
            /// Loads every shader permutation and creates every pipeline. Fails with
            /// renderer_error::SHADER_NOT_FOUND naming the key of a missing blob and
            /// PIPELINE_CREATION_FAILED naming the variant the device refused.
            /// </summary>
            [[nodiscard]] static core::result<std::unique_ptr<pipeline_cache>> create(
              nvrhi::IDevice &device,
              rhi::shader_library &shaders,
              const binding_layouts &layouts
            );

            [[nodiscard]] nvrhi::IGraphicsPipeline *graphics(
              graphics_variant variant,
              bool doubleSided = false
            ) const noexcept;
            [[nodiscard]] nvrhi::IComputePipeline *compute(compute_variant variant) const noexcept;

            /// <summary>
            /// The blit pipeline for a target colour format, created on first request (outside frames).
            /// </summary>
            [[nodiscard]] core::result<nvrhi::IGraphicsPipeline *> blit(rhi::texture_format targetFormat);

            [[nodiscard]] bool has_blit(rhi::texture_format targetFormat) const noexcept;

            /// <summary>
            /// Pipelines created so far (graphics, both cull modes, compute, blits).
            /// </summary>
            [[nodiscard]] u32 pipeline_count() const noexcept {
                return mPipelinesCreated;
            }

            /// <summary>
            /// Number of pipelines create() builds before any blit: every graphics variant, the mesh
            /// variants a second time double-sided, and every compute variant.
            /// </summary>
            [[nodiscard]] static constexpr u32 expected_pipeline_count() noexcept {
                return static_cast<u32>(GRAPHICS_COUNT) + 7 + static_cast<u32>(COMPUTE_COUNT);
            }

            /// <summary>
            /// The framebuffer formats a graphics variant renders into.
            /// </summary>
            [[nodiscard]] static nvrhi::FramebufferInfo framebuffer_info(graphics_variant variant) noexcept;

        private:
            [[nodiscard]] core::result<nvrhi::ShaderHandle> load_shader(
              std::string_view name,
              std::string_view entry,
              std::string_view defines,
              nvrhi::ShaderType type
            );
            [[nodiscard]] core::result<void> create_input_layouts();
            [[nodiscard]] core::result<void> create_graphics(graphics_variant variant);
            [[nodiscard]] core::result<void> create_compute(compute_variant variant);
    };
}
