#pragma once

#include "conjure/renderer/environment_asset.h"
#include "conjure/renderer/gpu_ids.h"
#include "conjure/renderer/limits.h"
#include "conjure/renderer/material_asset.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/render_target.h"
#include "conjure/renderer/texture_asset.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/assets/asset_types.h"
#include "conjure/core/api.h"
#include "conjure/core/containers/small_vector.h"
#include "conjure/core/image.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/math.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/shader_library.h"
#include "conjure/rhi/swapchain.h"

#include <memory>

// The renderer (docs/design/02_renderer.md 1, 3, 4.2, 5, 6, 7): a render graph over the rhi that
// turns a render_scene into one command list per frame. Owns the render_scene_pair (R8), every GPU
// mirror of the renderer asset types (R10), the render targets and the ImGui backend (R16).
//
// Threading: create, render, destroy, find_*, create_render_target, read_back, read_back_depth and the
// extract calls belong to the render thread (the main thread in v1). upload_* and release_* may be called
// from any thread: the payload is copied at enqueue time and the GPU objects are created at the start of
// the next render(). Culling, sorting and instance filling run on jobs inside render().

struct ImDrawData;

namespace conjure::renderer {

    class imgui_renderer;

    struct renderer_desc {
        rhi::device *device = nullptr;              // non-owning, must outlive the renderer
        rhi::swapchain *swapchain = nullptr;        // null = headless (views must target render targets)
        rhi::shader_library *shaders = nullptr;     // non-owning
        core::jobs::job_system *jobs = nullptr;     // null = serial culling / sorting
        u32 cascadeResolution = 2048;               // texels per cascade slice
        // Edge of the local-light shadow atlas; clamped to [256, 16384] and rounded down to a power of
        // two (WARN when changed). desc() reports the value given here.
        u32 atlasResolution = 4096;
        bool enableImGui = true;                    // ignored outside editor builds
    };

    /// <summary>
    /// GPU time of one executed pass (Development builds, devices with timer queries).
    /// </summary>
    struct pass_timing {
        const char *name = "";
        f32 gpuMilliseconds = 0.0f;
    };

    /// <summary>
    /// What the last render() did. Fully populated on the null device too, which is how the tests
    /// observe the renderer. The counters of what a frame clamped (viewsSkipped, lightsDropped,
    /// spritesDropped, shadowTilesDropped, settingsClamped) are filled every frame. The log says less: the
    /// WARN about views, lights or sprites beyond their budget, about an unknown render target or about a
    /// settings group out of its range is logged when the problem starts and not again while it lasts
    /// (renderer::render), and dropped shadow tiles are not logged at all. Read the counters to follow a
    /// problem that lasts. A frame whose back buffer could not be acquired counts every view of the scene
    /// as skipped and fills nothing else.
    /// </summary>
    struct CONJURE_API render_stats {
        u32 viewsRendered = 0;
        // views not drawn: beyond MAX_VIEWS, an unknown render target, the swapchain of a headless
        // renderer, or every view of a frame whose back buffer could not be acquired
        u32 viewsSkipped = 0;
        u32 passesExecuted = 0;
        u32 passesCulled = 0;
        u32 drawCalls = 0;
        u32 dispatches = 0;
        // instance records referenced by draws (summed over every pass, view and shadow map)
        u32 instancesDrawn = 0;
        u32 instancesCulled = 0;            // instances rejected by frustum or layer tests (sum over views)
        u32 trianglesDrawn = 0;
        u32 lightsRendered = 0;
        u32 lightsDropped = 0;              // lights beyond MAX_LIGHTS_PER_VIEW (sum over the prepared views)
        u32 shadowCascadesRendered = 0;
        u32 shadowTilesAllocated = 0;
        u32 shadowTilesDropped = 0;
        u32 shadowTilesReused = 0;          // atlas tiles kept at last frame's coordinates (stable shadow maps)
        // settings groups clamped into their ranges (sanitise): each of the scene's groups once per frame,
        // however many views render it, plus each override group of a drawn view (render_view::settingsOverride)
        u32 settingsClamped = 0;
        u32 spritesDrawn = 0;
        // sprites of the scene beyond MAX_SPRITES_PER_FRAME (both spaces, counted once per frame, not per view)
        u32 spritesDropped = 0;
        u32 debugVerticesDrawn = 0;
        u32 uploadsCompleted = 0;
        u32 uploadsPending = 0;
        u32 fallbacksUsed = 0;
        u32 transientTexturesLive = 0;
        u32 transientTexturesCreated = 0;
        u64 uploadBytes = 0;
        // Executed passes in order (every view); holds the largest frame, MAX_VIEWS full view graphs.
        core::small_vector<const char *, MAX_VIEWS * MAX_PASSES_PER_VIEW> passNames = {};
        core::small_vector<pass_timing, 48> passTimings = {};    // Development builds with timer queries

        /// <summary>
        /// Resets every counter and list.
        /// </summary>
        void reset() noexcept;

        /// <summary>
        /// True when a pass of this name executed.
        /// </summary>
        [[nodiscard]] bool executed(std::string_view passName) const noexcept;

        /// <summary>
        /// Number of executed passes with this name.
        /// </summary>
        [[nodiscard]] u32 count(std::string_view passName) const noexcept;
    };

    class CONJURE_API renderer {
        private:
            struct impl;
            struct create_tag {};

            std::unique_ptr<impl> mImpl = nullptr;

        public:
            explicit renderer(create_tag);
            ~renderer();

            renderer(const renderer &) = delete;
            renderer &operator=(const renderer &) = delete;

        public:
            /// <summary>
            /// Creates every pipeline, binding layout, fallback resource and the BRDF LUT up front. A
            /// missing shader permutation fails creation with renderer_error::SHADER_NOT_FOUND naming the
            /// key; a device refusal with PIPELINE_CREATION_FAILED or RESOURCE_CREATION_FAILED; a missing
            /// device or shader library with INVALID_ARGUMENT.
            /// </summary>
            [[nodiscard]] static core::result<std::unique_ptr<renderer>> create(const renderer_desc &desc);

            /// <summary>
            /// Queues an upload; safe from any thread. The payload bytes are copied into the upload record
            /// at enqueue time (R10). The returned id resolves to the fallback until the upload has run at
            /// the start of a later render(). Uploading again under an asset id already known replaces the
            /// GPU mirror and keeps the id.
            /// </summary>
            [[nodiscard]] gpu_mesh_id upload_mesh(const assets::asset_id &id, const mesh_asset &mesh);
            [[nodiscard]] gpu_texture_id upload_texture(const assets::asset_id &id, const texture_asset &texture);
            [[nodiscard]] gpu_material_id upload_material(const assets::asset_id &id, const material_asset &material);
            [[nodiscard]] gpu_environment_id upload_environment(
              const assets::asset_id &id,
              const environment_asset &env
            );

            /// <summary>
            /// Releases a GPU mirror; the id becomes stale at once, the GPU objects die bufferCount + 1
            /// frames later. Safe from any thread. Unknown or stale ids are ignored.
            /// </summary>
            void release_mesh(gpu_mesh_id id);
            void release_texture(gpu_texture_id id);
            void release_material(gpu_material_id id);
            void release_environment(gpu_environment_id id);

            /// <summary>
            /// Returns the gpu id previously uploaded for the asset, or an invalid id. Render thread.
            /// </summary>
            [[nodiscard]] gpu_mesh_id find_mesh(const assets::asset_id &id) const noexcept;
            [[nodiscard]] gpu_texture_id find_texture(const assets::asset_id &id) const noexcept;
            [[nodiscard]] gpu_material_id find_material(const assets::asset_id &id) const noexcept;
            [[nodiscard]] gpu_environment_id find_environment(const assets::asset_id &id) const noexcept;

            /// <summary>
            /// Creates an off-screen target. Fails with INVALID_ARGUMENT for a zero size or an unsupported
            /// colour format and RESOURCE_CREATION_FAILED / PIPELINE_CREATION_FAILED when the device refuses.
            /// </summary>
            [[nodiscard]] core::result<render_target_id> create_render_target(const render_target_desc &desc);

            /// <summary>
            /// Recreates the target's textures at a new size (waiting for the GPU to go idle). The target id
            /// and its colour_texture() id stay the same; the id resolves to the new texture from here on.
            /// Fails with NOT_FOUND for an unknown id and INVALID_ARGUMENT for a zero size.
            /// </summary>
            [[nodiscard]] core::result<void> resize_render_target(render_target_id id, math::uvec2 size);

            /// <summary>
            /// Destroys a target; unknown ids are ignored.
            /// </summary>
            void destroy_render_target(render_target_id id);

            /// <summary>
            /// The target, or nullptr for an unknown id. The pointer is valid until destroy_render_target.
            /// </summary>
            [[nodiscard]] render_target *get_render_target(render_target_id id) noexcept;

            /// <summary>
            /// Clears the write slot of the owned render_scene_pair and returns it for EXTRACT (R8).
            /// </summary>
            [[nodiscard]] render_scene &begin_extract();

            /// <summary>
            /// Appends the views queued by submit_view() to the scene being written, then swaps the pair;
            /// the extracted scene is immutable from here on.
            /// </summary>
            void end_extract();

            /// <summary>
            /// Queues a view that no scene camera stands for: the editor's viewport camera, a screenshot
            /// through a chosen camera. The next end_extract() appends the queued views to the extracted
            /// scene, after the scene's own, and forgets them, so each call renders the view for one
            /// frame; call it again every frame the view should be drawn, at any point before that
            /// frame's end_extract(). The views render in submission order and before the ImGui pass, so
            /// a panel may show a render target in the frame it was drawn into. A view normally names a
            /// render target (an invalid one means the swapchain); one whose target was destroyed is
            /// skipped and counted in render_stats::viewsSkipped every frame, with a WARN when that
            /// starts (render). Render thread.
            /// </summary>
            void submit_view(const render_view &view);

            /// <summary>
            /// Views queued by submit_view() that no end_extract() has taken yet.
            /// </summary>
            [[nodiscard]] u32 submitted_view_count() const noexcept;

            /// <summary>
            /// Renders the last extracted scene: render(pair.read()). Render thread only.
            /// </summary>
            [[nodiscard]] core::result<void> render_frame();

            /// <summary>
            /// Renders one scene. Render thread only. Fails only with rhi_error::DEVICE_LOST; everything
            /// else is clamped and logged so a bad scene never aborts a frame. A problem that lasts is
            /// logged (WARN) by the frame in which it starts and not again until a frame has seen it end,
            /// whatever its size does in between: more views than MAX_VIEWS, more lights than
            /// MAX_LIGHTS_PER_VIEW, more sprites than MAX_SPRITES_PER_FRAME; per settings group, a scene
            /// group (render_scene::postProcess, shadows, ssao) out of its range; per view index and group,
            /// a camera that had to be clamped, an override group out of its range
            /// (render_view::settingsOverride) or a render target the renderer does not know; a swapchain
            /// that refuses the acquire (the frame is skipped) or the present. Out-of-range settings are
            /// clamped (sanitise): the scene's groups once per frame, a view's override groups once per
            /// view. Only a frame that looks at a problem can see it end: a frame skipped by a refused
            /// acquire looks at nothing else, the per-view problems are looked at only while the scene has
            /// a view at that index (the camera and the override groups only when that view is drawn), and
            /// the light budget only when a view is drawn. A view with debugMode AMBIENT_OCCLUSION renders
            /// the SSAO term instead of its lit output; a view with any debugMode but NONE draws no shadows
            /// (as SKIP_SHADOWS).
            /// A view targeting the swapchain of a renderer without one is logged once per renderer.
            /// stats() counts what every frame clamped.
            /// </summary>
            [[nodiscard]] core::result<void> render(const render_scene &scene);

            /// <summary>
            /// Resizes the swapchain the renderer presents to; the engine calls it for a framebuffer resize
            /// instead of swapchain::resize. The renderer holds framebuffers over the back buffers (the
            /// view targets and the ImGui pass), and a swapchain cannot recreate buffers that are still
            /// referenced, so this waits for the GPU, drops those framebuffers and the binding sets and
            /// only then resizes. Fails with INVALID_ARGUMENT without a swapchain and otherwise with what
            /// swapchain::resize reports; after a failure the caches are empty and refill on the next frame.
            /// </summary>
            [[nodiscard]] core::result<void> resize_swapchain(math::uvec2 size);

            /// <summary>
            /// Device loss: stops recording and drops the GPU mirrors; every later render() fails with
            /// DEVICE_LOST and the engine quits (R8).
            /// </summary>
            void on_device_lost();

            /// <summary>
            /// Editor builds: draw data produced by engine::imgui_layer, drawn after the views of the
            /// next render() into the swapchain. Ignored without a swapchain or outside editor builds.
            /// </summary>
            void set_imgui_draw_data(ImDrawData *drawData) noexcept;

            /// <summary>
            /// Registers this module's asset types (mesh, texture, material, model, environment) and
            /// importers (glTF, texture, .cmat, .hdr); called by the engine's renderer_subsystem. The
            /// optional job system lets the environment importer bake faces in parallel. Errors are those
            /// of the two registries.
            /// </summary>
            [[nodiscard]] static core::result<void> register_asset_types(
              assets::asset_type_registry &types,
              assets::importer_registry &importers,
              core::jobs::job_system *jobs = nullptr
            );

            /// <summary>
            /// Synchronous readback of a render target's colour texture as RGBA8 (waits for the GPU).
            /// On the null device the image is zero-filled and the right size. Fails with NOT_FOUND for
            /// an unknown target, rhi_error::DEVICE_LOST on a lost device and
            /// renderer_error::RESOURCE_CREATION_FAILED when the staging texture cannot be created or mapped.
            /// </summary>
            [[nodiscard]] core::result<core::image> read_back(render_target_id target);

            /// <summary>
            /// Synchronous readback of a render target's depth (waits for the GPU): the scene depth the first
            /// view of the last frame with a view into the target drew (render_target.h), reverse-Z. A target
            /// no view has rendered into since its creation or last resize reads as all zeros (the far plane)
            /// without a GPU copy, and on the null device every value is zero. Fails with NOT_FOUND
            /// for an unknown target, INVALID_ARGUMENT for a target created without depth,
            /// rhi_error::DEVICE_LOST on a lost device and renderer_error::RESOURCE_CREATION_FAILED when the
            /// staging texture cannot be created or mapped. Render thread.
            /// </summary>
            [[nodiscard]] core::result<depth_read_back> read_back_depth(render_target_id target);

            /// <summary>
            /// Statistics of the last render().
            /// </summary>
            [[nodiscard]] const render_stats &stats() const noexcept;

            /// <summary>
            /// The ImGui backend; null in Distribution builds, without a swapchain or when disabled.
            /// </summary>
            [[nodiscard]] imgui_renderer *imgui() noexcept;

            /// <summary>
            /// The device the renderer was created on.
            /// </summary>
            [[nodiscard]] rhi::device &device() const noexcept;

            /// <summary>
            /// The description the renderer was created from.
            /// </summary>
            [[nodiscard]] const renderer_desc &desc() const noexcept;

            /// <summary>
            /// True after on_device_lost() or after the device reported loss during a frame.
            /// </summary>
            [[nodiscard]] bool is_lost() const noexcept;

            /// <summary>
            /// Uploads queued but not yet flushed by a render().
            /// </summary>
            [[nodiscard]] u32 pending_upload_count() const;

            /// <summary>
            /// Number of frames rendered.
            /// </summary>
            [[nodiscard]] u64 frame_count() const noexcept;
    };
}
