#pragma once

#include "engine/engine_services.h"

#include "conjure/engine/engine_subsystem.h"

#include "conjure/core/platform_detect.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER

#include "conjure/assets/asset_manager.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/renderer/components.h"
#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/render_settings.h"

#include <array>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

// Startup step 13 (docs/design/06_engine_loop.md 3.4, 09 R7/R8/R10 and section 3): the project's rendering
// defaults (project.json "rendering.defaults", read before anything else and published as
// engine_context::renderDefaults, also when rendering is disabled), the renderer over the device, swapchain and
// shader library (RENDERER_INIT_FAILED), renderer::register_asset_types, the renderer component types registered
// into the ECS (even when rendering is disabled, so scenes with cameras still load) and the asset event
// subscription that drives upload_* (LOADED / RELOADED) and release_* (UNLOADED).
// Owns the two systems the renderer cannot own because it has no ecs dependency:
//   engine.render_sync (SYNC, MAIN_THREAD | RUNS_WHILE_PAUSED): ticks the timed debug draw;
//   engine.render_extract (EXTRACT, MAIN_THREAD | RUNS_WHILE_PAUSED): cameras (with their post_process
//     overrides) -> views, the project defaults with the per-group winners of the render_settings components ->
//     the scene's settings groups, mesh renderers -> instances (render_world_transform, R3), lights, sprites,
//     the environment and the debug draw buffer into the write render_scene the frame loop obtained from
//     begin_extract().

namespace conjure::engine {

    struct render_extract_stats {
        u32 views = 0;              // cameras selected (submitted only when a swapchain exists)
        u32 meshes = 0;
        u32 meshesSkipped = 0;      // mesh renderer without a GPU mesh (asset not loaded yet)
        u32 lights = 0;
        u32 sprites = 0;
        bool environmentSeen = false;
        u32 settingsCandidates = 0;         // enabled render_settings components, whether they override a group or not
        u32 viewsWithSettingsOverride = 0;  // selected views whose camera's post_process overrides a group
        u32 orphanPostProcess = 0;          // enabled post_process components on entities without a camera
        u32 uploads = 0;            // asset events turned into upload_* calls so far
        u32 releases = 0;           // asset events turned into release_* calls so far
    };

    class renderer_subsystem final : public engine_subsystem {
        public:
            static constexpr std::string_view SYSTEM_SYNC = "engine.render_sync";
            static constexpr std::string_view SYSTEM_EXTRACT = "engine.render_extract";

        private:
            /// <summary>
            /// A camera seen by the extraction, ordered by priority then by world order before selection.
            /// </summary>
            struct camera_candidate {
                renderer::render_view view = {};
                i32 priority = 0;
                bool isMain = false;
                u32 order = 0;
            };

        private:
            engine_services *mServices = nullptr;
            assets::asset_subscription mSubscription = {};
            render_extract_stats mStats = {};
            std::vector<camera_candidate> mCameras = {};    // extraction scratch, keeps its capacity across frames
            renderer::render_defaults mDefaults = {};       // project.json "rendering.defaults" (renderDefaults)
            // Extraction scratch like mCameras; emptied after every extraction, so it never holds a component
            // pointer across frames.
            std::vector<renderer::scene_settings_candidate> mSettingsCandidates = {};
            // Per settings group: two or more candidates tied at the top priority in the last extraction.
            std::array<bool, renderer::SETTINGS_GROUP_COUNT> mAmbiguousLatch = {};
            bool mOrphanPostProcessLatch = false;           // the last extraction saw a post_process without a camera
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
            /// The renderer subsystem among an application's subsystems (application::subsystems()), or nullptr
            /// when there is none.
            /// </summary>
            [[nodiscard]] static renderer_subsystem *find(
              std::span<const std::unique_ptr<engine_subsystem>> subsystems
            ) noexcept;

            /// <summary>
            /// Where the shader index is read from: rendering.shaderDirectory, else &lt;engine&gt;/shaders.
            /// </summary>
            [[nodiscard]] static std::filesystem::path shader_directory(
              const engine_config &config,
              const std::filesystem::path &engineDirectory
            );

            /// <summary>
            /// The description of engine.render_extract: the components it reads (resolved against a registry
            /// through resolveAccess) and a run that extracts into the frame loop's write scene. Public so
            /// render_extract_runner can run extract() outside the scheduler with the same access.
            /// </summary>
            [[nodiscard]] ecs::system_desc describe_extract();

            /// <summary>
            /// Fills a render_scene from the world (the body of engine.render_extract), public so tests can run
            /// it against a scene of their own: the views of the selected cameras, each with its post_process
            /// override groups; the scene's post-process, shadow and SSAO groups, which are the project defaults
            /// with each group that a render_settings component overrides replaced by that group's winner
            /// (renderer::choose_scene_settings); the instances, lights, sprites, environment and debug draw.
            /// Logs one WARN per group when two or more candidates start to tie at its top priority, and one when
            /// post_process components on entities without a camera start to be seen; each WARN comes again only
            /// after an extraction without its condition. Needs the renderer (register_systems adds the system
            /// only then); a steady state allocates nothing.
            /// </summary>
            void extract(ecs::system_context &context, renderer::render_scene &target);

        private:
            void extract_scene_settings(ecs::system_context &context, renderer::render_scene &target);
            void warn_ambiguous_groups(const renderer::scene_settings_choice &choice);
            void handle_asset_event(const assets::asset_event &event);
            void sync(ecs::system_context &context);
    };

    /// <summary>
    /// Runs the body of engine.render_extract outside the scheduler, into a render_scene of the caller's, through a
    /// system_context with the access the system declares (describe_extract, resolved against the context's
    /// component registry), so the views extract() opens are validated as the scheduler's would be. For tests and
    /// benchmarks: a renderer without a swapchain never hands back the scene the frame loop extracts. The scratch,
    /// the command buffer and the frame counter are kept across runs, so a steady state allocates nothing. The
    /// context's world and registry and the subsystem must outlive the runner: application::shutdown() destroys
    /// them, so a runner goes out of scope before its application shuts down.
    /// </summary>
    class render_extract_runner {
        private:
            ecs::world *mWorld = nullptr;
            renderer_subsystem *mSubsystem = nullptr;
            ecs::system_desc mDesc = {};
            ecs::system_scratch mScratch = {};
            ecs::command_buffer mCommands;      // bound to the world by the constructor: it has no default
            ecs::frame_info mFrame = {};
            u64 mSinceVersion = 0;

        public:
            render_extract_runner(engine_context &context, renderer_subsystem &subsystem);
            ~render_extract_runner();

            render_extract_runner(const render_extract_runner &) = delete;
            render_extract_runner &operator=(const render_extract_runner &) = delete;
            render_extract_runner(render_extract_runner &&) = delete;
            render_extract_runner &operator=(render_extract_runner &&) = delete;

        public:
            /// <summary>
            /// Clears 'target' (keeping its capacity), advances the frame index and the world version, and
            /// extracts the world into it.
            /// </summary>
            void run(renderer::render_scene &target);
    };
}

#endif
