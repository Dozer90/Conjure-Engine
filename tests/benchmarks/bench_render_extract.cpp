// Extraction scenario (render-settings design 6, docs/design/08_testing.md 7.1): engine.render_extract_8_views runs
// the body of engine.render_extract on an application with a null-device renderer over 8 cameras, 4 of them with a
// post_process override, and 3 render_settings components spread over the settings groups without a tie, so the
// scene settings resolution (candidates, choose_scene_settings, resolve_scene_settings) and the per-camera override
// join are measured on their own, without the rest of a frame. A warmed-up extraction must allocate nothing: the
// scenario fails on any allocation in every configuration, not only where thresholds are enforced.

#include "bench_support.h"

#include "conjure/bench/benchmark.h"
#include "conjure/core/log.h"
#include "conjure/core/math.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/application.h"
#include "conjure/engine/application_desc.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_context.h"
#include "conjure/renderer/components.h"
#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/render_settings.h"
#include "conjure/scene/components.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"
#include "engine/subsystems/renderer_components.h"
#include "engine/subsystems/renderer_subsystem.h"

#include <array>
#include <format>
#include <utility>

using namespace conjure;
using namespace conjure::bench;
using namespace conjure::bench::support;

namespace {

    constexpr u32 CAMERA_COUNT = 8;
    constexpr u32 SETTINGS_COUNT = 3;

    /// <summary>
    /// A root entity of the active scene at 'position' with a fixed scene guid; NULL_ENTITY when the scene refused.
    /// </summary>
    [[nodiscard]] ecs::entity create_entity(scene::scene &active, const math::vec3 &position, u64 guid) {
        const core::result<ecs::entity> created = active.create_entity(core::string_id("bench_extract"));
        if (!created.has_value()) {
            return ecs::NULL_ENTITY;
        }
        scene::local_transform where = {};
        where.position = position;
        if (!active.set_local_transform(created.value(), where).has_value()) {
            return ecs::NULL_ENTITY;
        }
        if (!active.set_guid(created.value(), core::uuid(0xBE4Cu, guid)).has_value()) {
            return ecs::NULL_ENTITY;
        }
        return created.value();
    }

    /// <summary>
    /// The 8 cameras, the even ones with a post_process component overriding one group each (post-process and
    /// SSAO in turn), and the 3 render_settings components: post-process at priority 0, shadows at priority 0 and
    /// SSAO with post-process at priority 1, so every group has one winner and none is tied. False when the scene
    /// or the world refused an entity or a component.
    /// </summary>
    [[nodiscard]] bool build_scene(scene::scene &active, ecs::world &world) {
        for (u32 index = 0; index < CAMERA_COUNT; ++index) {
            const math::vec3 position(static_cast<f32>(index) * 4.0f, 2.0f, 10.0f);
            const ecs::entity camera = create_entity(active, position, index + 1);
            if (camera == ecs::NULL_ENTITY || !world.add<renderer::camera_component>(camera).has_value()) {
                return false;
            }
            if (index % 2 != 0) {
                continue;
            }
            renderer::post_process_component overrides = {};
            overrides.overridePostProcess = index % 4 == 0;
            overrides.overrideSsao = index % 4 != 0;
            overrides.postProcess.exposure = renderer::exposure_mode::MANUAL;
            overrides.ssao.radius = 0.5f;
            if (!world.add<renderer::post_process_component>(camera, overrides).has_value()) {
                return false;
            }
        }

        renderer::render_settings_component mood = {};
        mood.overridePostProcess = true;
        mood.postProcess.tonemap = renderer::tonemap_operator::AGX;
        renderer::render_settings_component level = {};
        level.overrideShadows = true;
        level.shadows.distance = 40.0f;
        renderer::render_settings_component boss = {};
        boss.priority = 1;
        boss.overridePostProcess = true;
        boss.overrideSsao = true;
        boss.postProcess.bloomIntensity = 0.2f;
        const std::array<renderer::render_settings_component, SETTINGS_COUNT> layers = {mood, level, boss};
        for (u32 index = 0; index < SETTINGS_COUNT; ++index) {
            const ecs::entity holder = create_entity(active, math::vec3(0.0f), CAMERA_COUNT + index + 1);
            if (holder == ecs::NULL_ENTITY) {
                return false;
            }
            if (!world.add<renderer::render_settings_component>(holder, layers[index]).has_value()) {
                return false;
            }
        }
        return true;
    }
}

CONJURE_BENCHMARK(engine.render_extract_8_views) {
    if (shader_directory().empty()) {
        context.skip("no shader index: the renderer was built without conjure_shaders");
        return;
    }
    engine::engine_config config = engine::engine_config::headless_defaults();
    config.rendering.shaderDirectory = shader_directory();
    config.rendering.enabled = true;
    config.rendering.backend = engine::rhi_backend_choice::NULL_DEVICE;
    config.rendering.gpuValidation = engine::gpu_validation_level::OFF;
    config.logLevel = core::log_level::WARN;
    config.jobs.workerCount = context.job_thread_count();
    config.modules.enginePlugins = false;
    config.audio.enabled = false;
    config.physics.enabled = false;
    config.scripting.enabled = false;
    engine::application_desc desc = {};
    desc.config = config;

    engine::application app(std::move(desc));
    const core::result<void> initialised = app.initialise();
    if (!initialised.has_value()) {
        context.skip("application: " + initialised.get_error().to_string());
        return;
    }
    engine::engine_context &engineContext = app.context();
    engine::renderer_subsystem *subsystem = engine::renderer_subsystem::find(app.subsystems());
    if (engineContext.renderer == nullptr || subsystem == nullptr) {
        context.skip("the application started without a renderer");
        return;
    }
    if (!build_scene(engineContext.scenes->active(), *engineContext.world)) {
        context.skip("the scene refused a camera or a settings component");
        return;
    }
    // The cameras need their render_world_transform, which the frame's transform phase writes.
    const core::result<void> stepped = app.step_frame();
    if (!stepped.has_value()) {
        context.skip("frame: " + stepped.get_error().to_string());
        return;
    }

    // engine.render_extract's own access, so the views extract() opens are validated as the scheduler's would be.
    engine::render_extract_runner runner(engineContext, *subsystem);
    renderer::render_scene extracted = {};
    auto extract = [&]() {
        runner.run(extracted);
    };

    extract();
    const engine::render_extract_stats &stats = subsystem->stats();
    const bool laidOut = stats.views == CAMERA_COUNT && stats.viewsWithSettingsOverride == CAMERA_COUNT / 2
      && stats.settingsCandidates == SETTINGS_COUNT && stats.orphanPostProcess == 0;
    if (!laidOut) {
        context.fail(std::format(
          "the layout did not extract as built: {} view(s), {} with an override, {} settings candidate(s), {} "
          "orphan(s)",
          stats.views,
          stats.viewsWithSettingsOverride,
          stats.settingsCandidates,
          stats.orphanPostProcess
        ));
        return;
    }

    context.set_items(CAMERA_COUNT);
    context.measure(extract);
    if (context.max_allocations_per_iteration() != 0) {
        context.fail(std::format(
          "a warmed-up extraction allocated {} block(s); its scratch must keep its capacity",
          context.max_allocations_per_iteration()
        ));
    }
    // The application shuts down when it goes out of scope, after the runner that reads its world.
}
