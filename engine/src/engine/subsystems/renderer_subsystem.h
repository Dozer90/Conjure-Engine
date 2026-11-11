#pragma once

#include "engine/engine_services.h"

#include "conjure/engine/engine_subsystem.h"

#include "conjure/core/platform_detect.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER

#include "conjure/assets/asset_manager.h"
#include "conjure/ecs/system.h"
#include "conjure/renderer/render_scene.h"

#include <filesystem>
#include <string_view>

// Startup step 13 (docs/design/06_engine_loop.md 3.4, 09 R7/R8/R10 and section 3): the renderer over the
// device, swapchain and shader library (RENDERER_INIT_FAILED), renderer::register_asset_types, the renderer
// component types registered into the ECS (even when rendering is disabled, so scenes with cameras still
// load) and the asset event subscription that drives upload_* (LOADED / RELOADED) and release_* (UNLOADED).
// Owns the two systems the renderer cannot own because it has no ecs dependency:
//   engine.render_sync (SYNC, MAIN_THREAD | RUNS_WHILE_PAUSED): ticks the timed debug draw;
//   engine.render_extract (EXTRACT, MAIN_THREAD | RUNS_WHILE_PAUSED): cameras -> views, mesh renderers ->
//     instances (render_world_transform, R3), lights, sprites, the environment and the debug draw buffer into
//     the write render_scene the frame loop obtained from begin_extract().

namespace conjure::engine {

    struct render_extract_stats {
        u32 views = 0;              // cameras selected (submitted only when a swapchain exists)
        u32 meshes = 0;
        u32 meshesSkipped = 0;      // mesh renderer without a GPU mesh (asset not loaded yet)
        u32 lights = 0;
        u32 sprites = 0;
        bool environmentSeen = false;
        u32 uploads = 0;            // asset events turned into upload_* calls so far
        u32 releases = 0;           // asset events turned into release_* calls so far
    };

    class renderer_subsystem final : public engine_subsystem {
        public:
            static constexpr std::string_view SYSTEM_SYNC = "engine.render_sync";
            static constexpr std::string_view SYSTEM_EXTRACT = "engine.render_extract";

        private:
            engine_services *mServices = nullptr;
            assets::asset_subscription mSubscription = {};
            render_extract_stats mStats = {};
            f32 mLastDeltaSeconds = 0.0f;

        public:
            explicit renderer_subsystem(engine_services &services);
            ~renderer_subsystem() override;

        public:
            [[nodiscard]] std::string_view name() const noexcept override;
            [[nodiscard]] core::result<void> startup(engine_context &context) override;
            void register_systems(ecs::scheduler &scheduler, engine_context &context) override;
            void begin_frame(engine_context &context) override;
            void shutdown(engine_context &context) override;

            /// <summary>
            /// Counters of the last extraction and of the asset events handled so far.
            /// </summary>
            [[nodiscard]] const render_extract_stats &stats() const noexcept {
                return mStats;
            }

            /// <summary>
            /// Where the shader index is read from: rendering.shaderDirectory, else &lt;engine&gt;/shaders.
            /// </summary>
            [[nodiscard]] static std::filesystem::path shader_directory(
              const engine_config &config,
              const std::filesystem::path &engineDirectory
            );

            /// <summary>
            /// Fills a render_scene from the world (the body of engine.render_extract), public so tests can run
            /// it against a scene of their own.
            /// </summary>
            void extract(ecs::system_context &context, renderer::render_scene &target);

        private:
            void handle_asset_event(const assets::asset_event &event);
            void sync(ecs::system_context &context);
    };
}

#endif
