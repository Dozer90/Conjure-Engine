#pragma once

#include "renderer/binding_layouts.h"
#include "renderer/draw_list_builder.h"
#include "renderer/frame_resources.h"
#include "renderer/gpu_resource_cache.h"
#include "renderer/gpu_types.h"
#include "renderer/pipeline_cache.h"
#include "renderer/render_graph.h"

#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/sprite_batch.h"

#include "conjure/core/types.h"

#include <nvrhi/nvrhi.h>

#include <array>
#include <span>
#include <vector>

// The passes of one view (docs/design/02_renderer.md 3.3). Every pass object lives for the lifetime
// of the renderer; the renderer fills pass_shared for the current view, adds the passes that have
// work to the graph in declaration order, and the graph culls what nothing reads. Passes talk to each
// other only through view_graph_resources, which the first writer of each resource fills during setup.

namespace conjure::renderer {

    /// <summary>
    /// A contiguous region of the instance buffer with its sorted batches and the resolved mesh and
    /// material of every batch.
    /// </summary>
    struct draw_region {
        u32 firstInstance = 0;
        u32 instanceCount = 0;
        std::vector<draw_batch> batches = {};
        std::vector<const gpu_mesh *> meshes = {};
        std::vector<const gpu_material *> materials = {};

        void clear() noexcept {
            firstInstance = 0;
            instanceCount = 0;
            batches.clear();
            meshes.clear();
            materials.clear();
        }
    };

    /// <summary>
    /// A shadow render of one cascade slice or one atlas tile.
    /// </summary>
    struct shadow_draw_list {
        u32 tileIndex = 0;                      // index into the view's shadow tile buffer
        draw_region region = {};
        u32 atlasX = 0;                         // atlas placement (atlas tiles only)
        u32 atlasY = 0;
        u32 atlasSize = 0;

        void clear() noexcept {
            tileIndex = 0;
            region.clear();
            atlasX = 0;
            atlasY = 0;
            atlasSize = 0;
        }
    };

    /// <summary>
    /// What the renderer resolved for one view before building its graph. The renderer keeps these
    /// objects alive across frames and resets them in place (reset()), so the draw regions and the
    /// atlas tile lists keep their capacity and a steady-state frame allocates nothing.
    /// </summary>
    struct frame_view_state {
        const render_view *view = nullptr;
        u32 viewIndex = 0;
        math::urect viewport = {};
        math::uvec2 targetSize = math::uvec2(1, 1);
        rhi::texture_format targetFormat = rhi::texture_format::RGBA8_SRGB;
        nvrhi::ITexture *targetTexture = nullptr;
        // The render target's depth when this view owns it this frame (render_target.h): imported as the
        // graph's depth instead of a transient one. Null for the swapchain, a target without depth and a
        // later view of the frame into the same target.
        nvrhi::ITexture *targetDepth = nullptr;
        gpu::view_constants constants = {};
        math::frustum frustum = {};
        bool postProcess = true;
        bool autoExposure = false;
        bool bloom = false;
        bool ssao = false;
        bool shadows = false;
        bool hasEnvironment = false;
        bool drawDebug = false;
        const gpu_environment *environment = nullptr;

        draw_region main = {};                  // opaque and masked batches first, blended after
        u32 opaqueBatchCount = 0;
        std::array<shadow_draw_list, MAX_SHADOW_CASCADES> cascades = {};
        u32 cascadeCount = 0;
        std::vector<shadow_draw_list> atlasTiles = {};   // pool of lists; the first atlasTileCount are this frame's
        u32 atlasTileCount = 0;
        u32 lightCount = 0;
        u32 directionalLightCount = 0;
        u32 localLightCount = 0;
        bool worldSprites = false;
        bool screenSprites = false;
        u32 debugDepthTestedVertices = 0;
        u32 debugOverlayVertices = 0;

        bool hdrCleared = false;
        bool depthCleared = false;

        /// <summary>
        /// The atlas tiles this frame renders through the view (shadow_atlas_pass).
        /// </summary>
        [[nodiscard]] std::span<const shadow_draw_list> atlas_tiles() const noexcept {
            return std::span<const shadow_draw_list>(atlasTiles.data(), atlasTileCount);
        }

        /// <summary>
        /// The next atlas tile list of this frame, reset in place; a list object is created only when
        /// the view has never rendered that many tiles.
        /// </summary>
        [[nodiscard]] shadow_draw_list &push_atlas_tile() {
            if (atlasTiles.size() <= atlasTileCount) {
                atlasTiles.emplace_back();
            }
            shadow_draw_list &list = atlasTiles[atlasTileCount];
            ++atlasTileCount;
            list.clear();
            return list;
        }

        /// <summary>
        /// Back to the default values with every list emptied and its capacity kept.
        /// </summary>
        void reset() noexcept {
            view = nullptr;
            viewIndex = 0;
            viewport = math::urect{};
            targetSize = math::uvec2(1, 1);
            targetFormat = rhi::texture_format::RGBA8_SRGB;
            targetTexture = nullptr;
            targetDepth = nullptr;
            constants = gpu::view_constants{};
            frustum = math::frustum{};
            postProcess = true;
            autoExposure = false;
            bloom = false;
            ssao = false;
            shadows = false;
            hasEnvironment = false;
            drawDebug = false;
            environment = nullptr;
            main.clear();
            opaqueBatchCount = 0;
            for (shadow_draw_list &cascade : cascades) {
                cascade.clear();
            }
            cascadeCount = 0;
            atlasTileCount = 0;
            lightCount = 0;
            directionalLightCount = 0;
            localLightCount = 0;
            worldSprites = false;
            screenSprites = false;
            debugDepthTestedVertices = 0;
            debugOverlayVertices = 0;
            hdrCleared = false;
            depthCleared = false;
        }
    };

    /// <summary>
    /// Graph resources of the current view, filled by the passes that create or import them.
    /// </summary>
    struct view_graph_resources {
        // The target's depth, imported before the passes when the view owns it (frame_view_state::targetDepth);
        // otherwise a transient created by the first pass that needs a depth buffer.
        graph_resource depth = INVALID_GRAPH_RESOURCE;
        graph_resource viewNormals = INVALID_GRAPH_RESOURCE;
        graph_resource hdr = INVALID_GRAPH_RESOURCE;
        graph_resource ldr = INVALID_GRAPH_RESOURCE;
        graph_resource target = INVALID_GRAPH_RESOURCE;
        graph_resource aoHalf = INVALID_GRAPH_RESOURCE;
        graph_resource aoDepthHalf = INVALID_GRAPH_RESOURCE;     // half-resolution linear view depth of the AO
        graph_resource aoBlurredH = INVALID_GRAPH_RESOURCE;
        graph_resource aoBlurred = INVALID_GRAPH_RESOURCE;
        graph_resource tileLists = INVALID_GRAPH_RESOURCE;
        graph_resource shadowCascades = INVALID_GRAPH_RESOURCE;
        graph_resource shadowAtlas = INVALID_GRAPH_RESOURCE;
        graph_resource histogram = INVALID_GRAPH_RESOURCE;
        graph_resource exposureState = INVALID_GRAPH_RESOURCE;
        graph_resource instances = INVALID_GRAPH_RESOURCE;
        graph_resource lights = INVALID_GRAPH_RESOURCE;
        graph_resource shadowTiles = INVALID_GRAPH_RESOURCE;
        graph_resource spriteVertices = INVALID_GRAPH_RESOURCE;
        graph_resource debugVertices = INVALID_GRAPH_RESOURCE;
        std::array<graph_resource, BLOOM_MIP_COUNT> bloomMips = {};
        bool ldrIsTarget = false;
    };

    /// <summary>
    /// Everything a pass reaches, set by the renderer before the graph of a view is built.
    /// </summary>
    struct pass_shared {
        nvrhi::IDevice *device = nullptr;
        const pipeline_cache *pipelines = nullptr;
        gpu_resource_cache *resources = nullptr;
        frame_resources *frame = nullptr;
        binding_set_cache *bindingSets = nullptr;
        const binding_layouts *layouts = nullptr;
        const render_scene *scene = nullptr;
        frame_view_state *view = nullptr;
        view_graph_resources graph = {};
        std::span<const sprite_run> spriteRuns = {};
        nvrhi::ISampler *shadowSampler = nullptr;
        nvrhi::ISampler *linearClampSampler = nullptr;
        nvrhi::ISampler *pointClampSampler = nullptr;
        u64 frameIndex = 0;
        // nvrhi::BindingSetDesc holds a std::vector, so a description built per pass per frame would
        // allocate; every builder fills this one scratch instead (binding_desc()), whose capacity
        // persists. Mutable because the builders are const views of the shared state.
        mutable nvrhi::BindingSetDesc bindingScratch = {};

        /// <summary>
        /// The scratch binding set description, emptied and ready to be filled; valid until the next
        /// call, so a pass fills it, looks the set up in the cache and only then builds the next one.
        /// </summary>
        [[nodiscard]] nvrhi::BindingSetDesc &binding_desc() const noexcept {
            bindingScratch.bindings.clear();
            bindingScratch.trackLiveness = true;
            return bindingScratch;
        }

        /// <summary>
        /// The viewport and scissor covering the view's viewport.
        /// </summary>
        [[nodiscard]] nvrhi::ViewportState viewport_state() const noexcept;

        /// <summary>
        /// Binding set 0 of the mesh passes for the current view; 'ambientOcclusion' is the blurred
        /// AO texture or nullptr (white) and 'ambientOcclusionDepth' its half-resolution view depth
        /// or nullptr (white).
        /// </summary>
        [[nodiscard]] nvrhi::IBindingSet *mesh_view_set(
          nvrhi::ITexture *ambientOcclusion,
          nvrhi::ITexture *ambientOcclusionDepth = nullptr
        ) const;

        /// <summary>
        /// Binding set of the fullscreen layout: b0 view constants, t0 / t1 textures, t2 buffer, s0 / s1.
        /// Null textures and buffers fall back to the white texture and the exposure state buffer.
        /// </summary>
        [[nodiscard]] nvrhi::IBindingSet *fullscreen_set(
          nvrhi::ITexture *texture0,
          nvrhi::ITexture *texture1,
          nvrhi::IBuffer *buffer2,
          nvrhi::ISampler *sampler0,
          nvrhi::ISampler *sampler1
        ) const;

        /// <summary>
        /// Clears the HDR / depth attachments the first time a pass of the view renders into them.
        /// </summary>
        void clear_targets_once(pass_context &context, nvrhi::IFramebuffer *framebuffer, bool hdr, bool depth) const;

        /// <summary>
        /// Binding sets from the cache for a compute pass.
        /// </summary>
        [[nodiscard]] nvrhi::IBindingSet *compute_set(
          nvrhi::IBindingLayout *layout,
          const nvrhi::BindingSetDesc &desc
        ) const;
    };

    /// <summary>
    /// Which pipeline family a mesh region is drawn with.
    /// </summary>
    enum class mesh_pass_kind : u8 {
        SHADOW,
        PREPASS,
        FORWARD_OPAQUE,
        FORWARD_BLEND
    };

    /// <summary>
    /// Issues the batches of a region: one drawIndexed per batch with the mesh's buffers and the
    /// material's binding set (set 1). The draw flags go into the push constants.
    /// </summary>
    void draw_mesh_region(
      pass_context &context,
      const pass_shared &shared,
      nvrhi::IFramebuffer *framebuffer,
      const nvrhi::ViewportState &viewport,
      nvrhi::IBindingSet *viewSet,
      const draw_region &region,
      mesh_pass_kind kind,
      u32 drawFlags
    );

    /*-----------------------------------------------------------------------*/
    // Pass classes

    class shadow_cascade_pass final : public render_pass {
        private:
            pass_shared &mShared;
            u32 mCascade = 0;
            std::array<char, 24> mName = {};

        public:
            shadow_cascade_pass(pass_shared &shared, u32 cascade);
            ~shadow_cascade_pass() override;

        public:
            [[nodiscard]] const char *name() const noexcept override;
            void setup(pass_builder &builder) override;
            void execute(pass_context &context) override;
    };

    class shadow_atlas_pass final : public render_pass {
        private:
            pass_shared &mShared;

        public:
            explicit shadow_atlas_pass(pass_shared &shared);
            ~shadow_atlas_pass() override;

        public:
            [[nodiscard]] const char *name() const noexcept override;
            void setup(pass_builder &builder) override;
            void execute(pass_context &context) override;
    };

    class depth_prepass final : public render_pass {
        private:
            pass_shared &mShared;

        public:
            explicit depth_prepass(pass_shared &shared);
            ~depth_prepass() override;

        public:
            [[nodiscard]] const char *name() const noexcept override;
            void setup(pass_builder &builder) override;
            void execute(pass_context &context) override;
    };

    class light_cull_pass final : public render_pass {
        private:
            pass_shared &mShared;

        public:
            explicit light_cull_pass(pass_shared &shared);
            ~light_cull_pass() override;

        public:
            [[nodiscard]] const char *name() const noexcept override;
            void setup(pass_builder &builder) override;
            void execute(pass_context &context) override;
    };

    class ssao_gtao_pass final : public render_pass {
        private:
            pass_shared &mShared;

        public:
            explicit ssao_gtao_pass(pass_shared &shared);
            ~ssao_gtao_pass() override;

        public:
            [[nodiscard]] const char *name() const noexcept override;
            void setup(pass_builder &builder) override;
            void execute(pass_context &context) override;
    };

    class ssao_blur_pass final : public render_pass {
        private:
            pass_shared &mShared;
            bool mVertical = false;

        public:
            ssao_blur_pass(pass_shared &shared, bool vertical);
            ~ssao_blur_pass() override;

        public:
            [[nodiscard]] const char *name() const noexcept override;
            void setup(pass_builder &builder) override;
            void execute(pass_context &context) override;
    };

    class forward_pass final : public render_pass {
        private:
            pass_shared &mShared;
            bool mBlended = false;

        public:
            forward_pass(pass_shared &shared, bool blended);
            ~forward_pass() override;

        public:
            [[nodiscard]] const char *name() const noexcept override;
            void setup(pass_builder &builder) override;
            void execute(pass_context &context) override;
    };

    class skybox_pass final : public render_pass {
        private:
            pass_shared &mShared;

        public:
            explicit skybox_pass(pass_shared &shared);
            ~skybox_pass() override;

        public:
            [[nodiscard]] const char *name() const noexcept override;
            void setup(pass_builder &builder) override;
            void execute(pass_context &context) override;
    };

    class sprite_pass final : public render_pass {
        private:
            pass_shared &mShared;
            sprite_space mSpace = sprite_space::WORLD;

        public:
            sprite_pass(pass_shared &shared, sprite_space space);
            ~sprite_pass() override;

        public:
            [[nodiscard]] const char *name() const noexcept override;
            void setup(pass_builder &builder) override;
            void execute(pass_context &context) override;
    };

    class debug_draw_pass final : public render_pass {
        private:
            pass_shared &mShared;

        public:
            explicit debug_draw_pass(pass_shared &shared);
            ~debug_draw_pass() override;

        public:
            [[nodiscard]] const char *name() const noexcept override;
            void setup(pass_builder &builder) override;
            void execute(pass_context &context) override;
    };

    class exposure_histogram_pass final : public render_pass {
        private:
            pass_shared &mShared;

        public:
            explicit exposure_histogram_pass(pass_shared &shared);
            ~exposure_histogram_pass() override;

        public:
            [[nodiscard]] const char *name() const noexcept override;
            void setup(pass_builder &builder) override;
            void execute(pass_context &context) override;
    };

    class exposure_adapt_pass final : public render_pass {
        private:
            pass_shared &mShared;

        public:
            explicit exposure_adapt_pass(pass_shared &shared);
            ~exposure_adapt_pass() override;

        public:
            [[nodiscard]] const char *name() const noexcept override;
            void setup(pass_builder &builder) override;
            void execute(pass_context &context) override;
    };

    class bloom_down_pass final : public render_pass {
        private:
            pass_shared &mShared;
            u32 mMip = 0;
            std::array<char, 24> mName = {};

        public:
            bloom_down_pass(pass_shared &shared, u32 mip);
            ~bloom_down_pass() override;

        public:
            [[nodiscard]] const char *name() const noexcept override;
            void setup(pass_builder &builder) override;
            void execute(pass_context &context) override;
    };

    class bloom_up_pass final : public render_pass {
        private:
            pass_shared &mShared;
            u32 mMip = 0;
            std::array<char, 24> mName = {};

        public:
            bloom_up_pass(pass_shared &shared, u32 mip);
            ~bloom_up_pass() override;

        public:
            [[nodiscard]] const char *name() const noexcept override;
            void setup(pass_builder &builder) override;
            void execute(pass_context &context) override;
    };

    class tonemap_pass final : public render_pass {
        private:
            pass_shared &mShared;

        public:
            explicit tonemap_pass(pass_shared &shared);
            ~tonemap_pass() override;

        public:
            [[nodiscard]] const char *name() const noexcept override;
            void setup(pass_builder &builder) override;
            void execute(pass_context &context) override;
    };

    class blit_pass final : public render_pass {
        private:
            pass_shared &mShared;
            nvrhi::IGraphicsPipeline *mPipeline = nullptr;

        public:
            explicit blit_pass(pass_shared &shared);
            ~blit_pass() override;

        public:
            [[nodiscard]] const char *name() const noexcept override;
            void setup(pass_builder &builder) override;
            void execute(pass_context &context) override;

            /// <summary>
            /// The blit pipeline matching the target format, chosen by the renderer per view.
            /// </summary>
            void set_pipeline(nvrhi::IGraphicsPipeline *pipeline) noexcept {
                mPipeline = pipeline;
            }
    };
}
