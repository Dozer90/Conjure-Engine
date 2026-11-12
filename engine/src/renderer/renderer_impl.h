#pragma once

#include "renderer/binding_layouts.h"
#include "renderer/culling.h"
#include "renderer/draw_list_builder.h"
#include "renderer/frame_resources.h"
#include "renderer/framebuffer_cache.h"
#include "renderer/gpu_resource_cache.h"
#include "renderer/gpu_types.h"
#include "renderer/passes/passes.h"
#include "renderer/pipeline_cache.h"
#include "renderer/render_graph.h"
#include "renderer/render_target_impl.h"
#include "renderer/shadows/shadow_atlas_allocator.h"
#include "renderer/timer_ring.h"
#include "renderer/transient_pool.h"

#include "conjure/renderer/imgui_renderer.h"
#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/render_target.h"
#include "conjure/renderer/renderer.h"
#include "conjure/renderer/sprite_batch.h"

#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/core/containers/slot_map.h"
#include "conjure/core/types.h"

#include <nvrhi/nvrhi.h>

#include <array>
#include <memory>
#include <span>
#include <thread>
#include <vector>

// The private state of renderer (renderer.cpp). Members are declared in reverse destruction order:
// the pass objects and caches that hold NVRHI handles die before the device-owning rhi objects the
// renderer borrows, and the resource cache dies after the render targets that registered textures in it.
// Every per-frame list below is scratch that persists across frames and is reset in place, so a
// steady-state frame allocates nothing (02_renderer.md "steady-state frames do not allocate").

namespace conjure::renderer {

    struct renderer_state {
        renderer_desc desc = {};
        nvrhi::IDevice *device = nullptr;
        bool lost = false;
        u64 frameIndex = 0;
        std::thread::id renderThread = {};
        render_stats stats = {};

        binding_layouts layouts = {};
        std::unique_ptr<pipeline_cache> pipelines = nullptr;
        std::unique_ptr<framebuffer_cache> framebuffers = nullptr;
        std::unique_ptr<transient_pool> pool = nullptr;
        std::unique_ptr<binding_set_cache> bindingSets = nullptr;
        std::unique_ptr<gpu_resource_cache> resources = nullptr;
        frame_resources frame = {};
        std::unique_ptr<timer_ring> timers = nullptr;
        nvrhi::CommandListHandle commandList = nullptr;
        nvrhi::SamplerHandle shadowSampler = nullptr;
        nvrhi::SamplerHandle linearClampSampler = nullptr;
        nvrhi::SamplerHandle pointClampSampler = nullptr;

        render_scene_pair pair = {};
        core::slot_map<std::unique_ptr<render_target>, render_target_id> targets = {};
        sprite_batcher sprites = {};
        shadow_atlas_allocator atlas;
        std::vector<gpu::instance> instanceData = {};
        std::vector<frame_view_state> views = {};                 // pool; the first preparedViewCount are this frame's
        u32 preparedViewCount = 0;
        std::vector<u32> visible = {};
        cull_scratch cullScratch = {};
        std::vector<u32> lightOrder = {};
        draw_list_builder drawLists = {};
        std::vector<gpu::light> lightData = {};
        std::vector<gpu::shadow_tile> tileData = {};
        std::vector<gpu::shadow_tile> atlasTileData = {};        // scene-wide atlas tiles of this frame
        std::vector<u32> lightTileBase = {};                      // per scene light: first atlas tile or INVALID_INDEX
        std::vector<u32> shadowCasters = {};                      // scene indices of the local casters, by importance
        std::vector<atlas_request> atlasRequests = {};
        std::vector<u32> atlasRequestLights = {};                 // scene light index per request
        core::flat_hash_map<u64, u32> atlasKeyOccurrences = {};  // lights sharing an atlas key this frame
        bool atlasRendered = false;                               // a prepared view owns the atlas pass this frame
        u32 lastViewOverflow = 0;
        u32 lastLightOverflow = 0;
        u32 invalidCameraViews = 0;                               // one bit per view index: its invalid camera WARNed
        static_assert(MAX_VIEWS <= 32, "invalidCameraViews holds one bit per view");
        bool warnedNoSwapchain = false;

        pass_shared shared = {};
        render_graph graph = {};
        std::array<std::unique_ptr<shadow_cascade_pass>, MAX_SHADOW_CASCADES> cascadePasses = {};
        std::unique_ptr<shadow_atlas_pass> atlasPass = nullptr;
        std::unique_ptr<depth_prepass> prepass = nullptr;
        std::unique_ptr<light_cull_pass> lightCull = nullptr;
        std::unique_ptr<ssao_gtao_pass> ssao = nullptr;
        std::unique_ptr<ssao_blur_pass> ssaoBlurH = nullptr;
        std::unique_ptr<ssao_blur_pass> ssaoBlurV = nullptr;
        std::unique_ptr<forward_pass> forwardOpaque = nullptr;
        std::unique_ptr<skybox_pass> skybox = nullptr;
        std::unique_ptr<forward_pass> forwardBlend = nullptr;
        std::unique_ptr<sprite_pass> worldSprites = nullptr;
        std::unique_ptr<debug_draw_pass> debugDraw = nullptr;
        std::unique_ptr<exposure_histogram_pass> exposureHistogram = nullptr;
        std::unique_ptr<exposure_adapt_pass> exposureAdapt = nullptr;
        std::array<std::unique_ptr<bloom_down_pass>, BLOOM_MIP_COUNT> bloomDown = {};
        std::array<std::unique_ptr<bloom_up_pass>, BLOOM_MIP_COUNT - 1> bloomUp = {};
        std::unique_ptr<tonemap_pass> tonemap = nullptr;
        std::unique_ptr<sprite_pass> screenSprites = nullptr;
        std::unique_ptr<blit_pass> blit = nullptr;

        ImDrawData *imguiDrawData = nullptr;
#if defined(CONJURE_BUILD_EDITOR) && CONJURE_BUILD_EDITOR
        std::unique_ptr<imgui_renderer> imgui = nullptr;
#endif

        explicit renderer_state(u32 atlasResolution) :
            atlas(atlasResolution) {}

        /// <summary>
        /// The views prepared this frame, in scene order.
        /// </summary>
        [[nodiscard]] std::span<frame_view_state> prepared_views() noexcept {
            return std::span<frame_view_state>(views.data(), preparedViewCount);
        }

        /// <summary>
        /// The next view state of this frame, reset in place; a state object is created only when
        /// the renderer has never prepared that many views. The caller commits it with
        /// ++preparedViewCount once the view is prepared, or leaves it for the next view when skipped.
        /// </summary>
        [[nodiscard]] frame_view_state &next_view_state() {
            if (views.size() <= preparedViewCount) {
                views.emplace_back();
            }
            frame_view_state &stateOfView = views[preparedViewCount];
            stateOfView.reset();
            return stateOfView;
        }
    };

    struct renderer::impl final : renderer_state {
        using renderer_state::renderer_state;
    };

    /// <summary>
    /// Lets renderer.cpp reach the private state of render_target (the access key named in its header).
    /// </summary>
    struct renderer_access {
        [[nodiscard]] static render_target::impl &state(render_target &target) noexcept {
            return *target.mImpl;
        }
    };
}
