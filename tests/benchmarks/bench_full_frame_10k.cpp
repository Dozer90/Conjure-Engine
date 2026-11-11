// Full-frame scenarios through engine::application headless (docs/design/08_testing.md 7.1, 06_engine_loop.md
// 10 "bench_engine_frame"): one step_frame() over 10,000 moving mesh renderers extracted onto the null
// device, over 10,000 awake rigid bodies resting on a plane, and over 10,000 scripted movers on the real
// CLR. Besides the frame total every engine phase (and the submit) is reported from frame_stats.

#include "bench_support.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/bench/benchmark.h"
#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_source.h"
#include "conjure/assets/asset_types.h"
#include "conjure/assets/cooked_blob.h"
#include "conjure/core/json/document.h"
#include "conjure/core/log.h"
#include "conjure/core/math.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/chunk_view.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/ecs/view.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/application.h"
#include "conjure/engine/application_desc.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_context.h"
#include "conjure/engine/engine_phase.h"
#include "conjure/engine/frame_stats.h"
#include "conjure/engine/project.h"
#include "conjure/scene/components.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/renderer/components.h"
    #include "conjure/renderer/mesh_asset.h"
    #include "conjure/renderer/renderer.h"
    #include "engine/subsystems/renderer_components.h"
#endif
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #include "conjure/physics/components.h"
    #include "conjure/physics/physics_world.h"
#endif
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    #include "conjure/scripting/script_class_registry.h"
    #include "conjure/scripting/script_runtime.h"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::bench;
using namespace conjure::bench::support;

namespace {

    constexpr u32 GRID_SIDE = 100;
    constexpr u32 ASSET_LOAD_FRAME_LIMIT = 32;      // may exceed SETTLE_FRAMES: reserve() counts from size()
    constexpr u32 SETTLE_FRAMES = 30;
    constexpr std::string_view MOVER_CLASS = "Conjure.TestScripts.Mover";

    /// <summary>
    /// A headless application plus the per-phase samples read from frame_stats after every frame.
    /// </summary>
    class frame_fixture {
        public:
            std::unique_ptr<engine::application> app = nullptr;
            std::array<std::vector<f64>, ecs::SYSTEM_PHASE_COUNT> phaseSamples = {};
            std::vector<f64> submitSamples = {};
            std::string failure = {};               // start() or the first step_frame() error, with its frame
            u32 failedFrames = 0;                   // step_frame() calls that returned an error
            u32 frames = 0;                         // step_frame() calls so far

        public:
            frame_fixture() = default;

            ~frame_fixture() {
                if (app != nullptr) {
                    app->shutdown();
                }
            }

            frame_fixture(const frame_fixture &) = delete;
            frame_fixture &operator=(const frame_fixture &) = delete;

        public:
            /// <summary>
            /// The headless configuration every scenario starts from: null backends, the benchmark worker
            /// count, WARN logging, no engine plugins; rendering, physics and scripting are opted in per scenario.
            /// </summary>
            [[nodiscard]] static engine::engine_config base_config(const bench_context &context) {
                engine::engine_config config = engine::engine_config::headless_defaults();
                config.rendering.shaderDirectory = shader_directory();
                config.logLevel = core::log_level::WARN;
                config.jobs.workerCount = context.job_thread_count();
                config.modules.enginePlugins = false;
                config.audio.enabled = false;
                config.physics.enabled = false;
                config.scripting.enabled = false;
                return config;
            }

            /// <summary>
            /// Constructs and initialises the application; 'failure' names the error when false.
            /// </summary>
            [[nodiscard]] bool start(engine::application_desc desc) {
                app = std::make_unique<engine::application>(std::move(desc));
                const core::result<void> initialised = app->initialise();
                if (!initialised.has_value()) {
                    failure = "application: " + initialised.get_error().to_string();
                    app.reset();
                    return false;
                }
                return true;
            }

            /// <summary>
            /// One frame; the phase times of the committed sample are appended. A frame that returns an
            /// error is counted and the first one is kept in 'failure': a frame the engine cut short must
            /// not be timed as a good one.
            /// </summary>
            void step() {
                ++frames;
                const core::result<void> stepped = app->step_frame();
                if (!stepped.has_value()) {
                    ++failedFrames;
                    if (failure.empty()) {
                        failure = std::format("frame {}: {}", frames, stepped.get_error().to_string());
                    }
                }
                const engine::frame_sample &sample = app->stats().latest();
                for (u32 phase = 0; phase < ecs::SYSTEM_PHASE_COUNT; ++phase) {
                    phaseSamples[phase].push_back(static_cast<f64>(sample.phaseMilliseconds[phase]));
                }
                submitSamples.push_back(static_cast<f64>(sample.submitMilliseconds));
            }

            /// <summary>
            /// Reserves headroom for 'additionalFrames' more samples in every vector (on top of what the
            /// settle and asset-load frames already appended) so the measured frames never grow them.
            /// </summary>
            void reserve(u32 additionalFrames) {
                for (std::vector<f64> &samples : phaseSamples) {
                    samples.reserve(samples.size() + additionalFrames);
                }
                submitSamples.reserve(submitSamples.size() + additionalFrames);
            }

            /// <summary>
            /// The summed capacity of every sample vector: unchanged across the measured frames when
            /// reserve() did its job.
            /// </summary>
            [[nodiscard]] usize sample_capacity() const noexcept {
                usize total = submitSamples.capacity();
                for (const std::vector<f64> &samples : phaseSamples) {
                    total += samples.capacity();
                }
                return total;
            }

            /// <summary>
            /// Reports the last 'measured' samples of every phase and of the submit as sub-phases.
            /// </summary>
            void publish_phases(bench_context &context, u32 measured) const {
                for (u32 phase = 0; phase < ecs::SYSTEM_PHASE_COUNT; ++phase) {
                    const std::string_view name = engine::to_string(static_cast<ecs::system_phase>(phase));
                    context.add_phase(name, tail(phaseSamples[phase], measured));
                }
                context.add_phase("SUBMIT", tail(submitSamples, measured));
            }

            [[nodiscard]] scene::scene &active() {
                return app->context().scenes->active();
            }

            [[nodiscard]] ecs::world &world() {
                return *app->context().world;
            }

            /// <summary>
            /// A root entity with a local transform at 'position'; NULL_ENTITY when the scene refused.
            /// </summary>
            [[nodiscard]] ecs::entity create_posed(std::string_view name, const math::vec3 &position) {
                const core::result<ecs::entity> created = active().create_entity(core::string_id(name));
                if (!created.has_value()) {
                    return ecs::NULL_ENTITY;
                }
                scene::local_transform where = {};
                where.position = position;
                if (!active().set_local_transform(created.value(), where).has_value()) {
                    return ecs::NULL_ENTITY;
                }
                return created.value();
            }

        private:
            [[nodiscard]] static std::span<const f64> tail(const std::vector<f64> &samples, u32 count) {
                const usize keep = std::min<usize>(samples.size(), count);
                return std::span<const f64>(samples).subspan(samples.size() - keep, keep);
            }
    };

    /// <summary>
    /// The world position of grid cell 'index' on a GRID_SIDE x GRID_SIDE grid of 2 m cells at height y,
    /// centred on x = 0 and running from z = -2 towards -z.
    /// </summary>
    [[nodiscard]] math::vec3 grid_position(u32 index, f32 y) noexcept {
        const f32 x = (static_cast<f32>(index % GRID_SIDE) - static_cast<f32>(GRID_SIDE) * 0.5f) * 2.0f;
        const f32 z = -(static_cast<f32>(index / GRID_SIDE) * 2.0f) - 2.0f;
        return math::vec3(x, y, z);
    }

    /// <summary>
    /// Runs the measure loop over step_frame and publishes the phases. A frame that failed before the
    /// loop is an unmet precondition (SKIPPED); one that fails inside it, a quit request, or a sample
    /// vector that grew in spite of the reservation fails the scenario (the figures would be of frames
    /// the engine cut short, or of the fixture's own allocations).
    /// </summary>
    void measure_frames(bench_context &context, frame_fixture &fixture, u64 items) {
        if (!fixture.failure.empty()) {
            context.skip(fixture.failure);
            return;
        }
        fixture.reserve(context.options().warmupIterations + context.options().measuredIterations);
        const usize capacityBefore = fixture.sample_capacity();
        const u32 failedBefore = fixture.failedFrames;
        context.set_items(items);
        context.measure([&]() {
            fixture.step();
        });
        fixture.publish_phases(context, context.options().measuredIterations);
        if (fixture.failedFrames != failedBefore) {
            context.fail(std::format(
              "{} frame(s) returned an error during the warm-up or measured loop; first: {}",
              fixture.failedFrames - failedBefore,
              fixture.failure
            ));
        }
        if (fixture.app->is_quit_requested()) {
            context.fail("the application requested quit during the measured frames");
        }
        if (fixture.sample_capacity() != capacityBefore) {
            context.fail("the fixture's sample vectors grew during the measured frames");
        }
    }
}

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER

namespace {

    [[nodiscard]] assets::cooked_blob cube_blob(const assets::asset_id &id) {
        assets::cooked_blob_header header = {};
        header.type = assets::asset_type_traits<renderer::mesh_asset>::id();
        header.typeVersion = renderer::MESH_BLOB_VERSION;
        header.importerVersion = 1;
        header.id = id;
        return assets::make_cooked_blob(header, {}, renderer::mesh_asset::unit_cube().write_blob());
    }

    /// <summary>
    /// Every local_transform bobs on a sine of the frame time so the propagation and the extraction see
    /// 10,000 moved entities per frame (a PARALLEL system over chunks).
    /// </summary>
    [[nodiscard]] ecs::system_desc make_mover_system() {
        return ecs::make_system<scene::local_transform>(
          core::string_id("bench.move_renderers"),
          ecs::system_phase::UPDATE,
          [](ecs::system_context &system) {
              const f32 time = static_cast<f32>(system.frame().time);
              system.view<scene::local_transform>().parallel_for_each_chunk(
                system,
                [time](ecs::chunk_view &chunk, ecs::command_buffer &) {
                    const std::span<scene::local_transform> transforms = chunk.get<scene::local_transform>();
                    for (u32 row = 0; row < chunk.count(); ++row) {
                        transforms[row].position.y = std::sin(time + transforms[row].position.x * 0.1f);
                    }
                }
              );
          }
        );
    }
}

CONJURE_BENCHMARK(engine.full_frame_10k_transform_render) {
    if (shader_directory().empty()) {
        context.skip("no shader index: the renderer was built without conjure_shaders");
        return;
    }
    engine::engine_config config = frame_fixture::base_config(context);
    config.rendering.enabled = true;
    config.rendering.backend = engine::rhi_backend_choice::NULL_DEVICE;
    config.rendering.gpuValidation = false;
    engine::application_desc desc = {};
    desc.config = config;

    frame_fixture fixture = {};
    if (!fixture.start(std::move(desc))) {
        context.skip(fixture.failure);
        return;
    }
    engine::engine_context &engineContext = fixture.app->context();
    if (engineContext.renderer == nullptr) {
        context.skip("the application started without a renderer");
        return;
    }

    // The cube mesh comes from a memory source beside the project's cache; the first frames load it.
    const assets::asset_id meshId = core::uuid::generate();
    std::unique_ptr<assets::memory_asset_source> source = std::make_unique<assets::memory_asset_source>();
    source->add(cube_blob(meshId), "assets://meshes/bench_cube.mesh");
    engineContext.assets->add_source(std::move(source));
    assets::asset_handle meshHandle = engineContext.assets->load(meshId);
    for (u32 frame = 0; frame < ASSET_LOAD_FRAME_LIMIT && meshHandle.state() != assets::asset_state::LOADED; ++frame) {
        fixture.step();
    }
    if (meshHandle.state() != assets::asset_state::LOADED) {
        context.skip("the cube mesh did not load");
        return;
    }

    const ecs::entity camera = fixture.create_posed("camera", math::vec3(0.0f, 20.0f, 10.0f));
    if (camera == ecs::NULL_ENTITY || !fixture.world().add<renderer::camera_component>(camera).has_value()) {
        context.skip("the scene refused the camera");
        return;
    }
    renderer::mesh_renderer_component meshRenderer = {};
    meshRenderer.mesh = engineContext.assets->bind<renderer::mesh_asset>(meshId);
    const u32 count = context.entity_count();
    for (u32 index = 0; index < count; ++index) {
        const ecs::entity cube = fixture.create_posed("cube", grid_position(index % (GRID_SIDE * GRID_SIDE), 0.0f));
        if (cube == ecs::NULL_ENTITY ||
          !fixture.world().add<renderer::mesh_renderer_component>(cube, meshRenderer).has_value()) {
            context.skip("the scene refused a mesh renderer");
            return;
        }
    }
    if (!engineContext.scheduler->add_system(make_mover_system()).has_value()) {
        context.skip("the scheduler refused the mover system");
        return;
    }

    measure_frames(context, fixture, count);
    // The benchmark's reference goes before shutdown: the manager asserts on live handles.
    meshHandle = assets::asset_handle{};
}

#endif

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS

namespace {

    constexpr std::string_view PHYSICS_PROJECT_JSON =
      "{\n"
      "  \"schema_version\": 1,\n"
      "  \"name\": \"bench_full_frame_physics\",\n"
      "  \"assets_root\": \"assets\",\n"
      "  \"physics\": { \"allow_sleeping\": false, \"time_before_sleep_seconds\": 0.3 }\n"
      "}\n";

    /// <summary>
    /// A box body component pair at a position; false when the world refused.
    /// </summary>
    [[nodiscard]] bool add_box_body(
      ecs::world &world,
      ecs::entity entity,
      physics::motion_type motion,
      const math::vec3 &halfExtents
    ) {
        physics::rigid_body body = {};
        body.motionType = motion;
        body.flags = physics::rigid_body_flags::NONE;
        physics::box_collider collider = {};
        collider.halfExtents = halfExtents;
        return world.add<physics::rigid_body>(entity, body).has_value() &&
          world.add<physics::box_collider>(entity, collider).has_value();
    }
}

CONJURE_BENCHMARK(engine.full_frame_10k_physics) {
    // Sleeping is a physics_settings field the engine reads from project.json (R53): a scratch project
    // keeps the 10,000 bodies awake for the whole run.
    scratch_directory projectDirectory = {};
    if (!projectDirectory.write_text("project.json", PHYSICS_PROJECT_JSON) ||
      !projectDirectory.write_text("assets/readme.txt", "benchmark scratch project\n")) {
        context.skip("the scratch project could not be written");
        return;
    }
    core::result<engine::project> loaded = engine::project::load(projectDirectory.path());
    if (!loaded.has_value()) {
        context.skip("project: " + loaded.get_error().to_string());
        return;
    }
    engine::engine_config config = frame_fixture::base_config(context);
    config.physics.enabled = true;
    config.projectPath = loaded.value().root_directory();
    loaded.value().apply_to(config);
    engine::application_desc desc = {};
    desc.config = config;
    desc.project = std::move(loaded).value();

    frame_fixture fixture = {};
    if (!fixture.start(std::move(desc))) {
        context.skip(fixture.failure);
        return;
    }
    if (fixture.app->context().physics == nullptr) {
        context.skip("the application started without a physics world");
        return;
    }

    const ecs::entity ground = fixture.create_posed("ground", math::vec3(0.0f, -0.5f, 0.0f));
    if (ground == ecs::NULL_ENTITY ||
      !add_box_body(fixture.world(), ground, physics::motion_type::STATIC, math::vec3(400.0f, 0.5f, 400.0f))) {
        context.skip("the scene refused the ground");
        return;
    }
    const u32 count = context.entity_count();
    for (u32 index = 0; index < count; ++index) {
        const ecs::entity box = fixture.create_posed("box", grid_position(index % (GRID_SIDE * GRID_SIDE), 0.75f));
        if (box == ecs::NULL_ENTITY ||
          !add_box_body(fixture.world(), box, physics::motion_type::DYNAMIC, math::vec3(0.5f))) {
            context.skip("the scene refused a box body");
            return;
        }
    }
    for (u32 frame = 0; frame < SETTLE_FRAMES; ++frame) {
        fixture.step();
    }
    if (fixture.app->context().physics->body_count() != count + 1) {
        context.skip("not every body was created");
        return;
    }

    measure_frames(context, fixture, count);
}

#endif

#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING

CONJURE_BENCHMARK(engine.full_frame_10k_scripts) {
    std::string reason = {};
    if (!dotnet_available(reason)) {
        context.skip(reason);
        return;
    }
    engine::engine_config config = frame_fixture::base_config(context);
    config.scripting.enabled = true;
    config.scripting.required = true;
    config.scripting.useNullHost = false;
    config.scripting.hotReload = false;
    config.scripting.managedDirectory = managed_directory();
    config.scripting.gameAssembly = test_scripts_assembly();
    engine::application_desc desc = {};
    desc.config = config;

    frame_fixture fixture = {};
    if (!fixture.start(std::move(desc))) {
        context.skip(fixture.failure);
        return;
    }
    scripting::script_runtime *runtime = fixture.app->context().scripting;
    if (runtime == nullptr || !runtime->is_assembly_loaded()) {
        context.skip("the application started without the test scripts");
        return;
    }

    const u64 moverHash = scripting::script_name_hash(MOVER_CLASS);
    const core::json::document fields = core::json::document::object();
    const u32 count = context.entity_count();
    for (u32 index = 0; index < count; ++index) {
        const ecs::entity mover = fixture.create_posed("mover", grid_position(index % (GRID_SIDE * GRID_SIDE), 0.0f));
        if (mover == ecs::NULL_ENTITY || !runtime->add_script(mover, moverHash, fields.root()).is_valid()) {
            context.skip("the runtime refused a Mover script");
            return;
        }
    }
    // scripting.frame_begin flushes the pending creates; the next frame runs OnStart.
    fixture.step();
    fixture.step();
    if (runtime->stats().liveInstances != count) {
        context.skip("not every Mover instance was created");
        return;
    }

    measure_frames(context, fixture, count);
}

#endif
