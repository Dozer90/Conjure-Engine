#include "conjure/testing/doctest.h"

#include "conjure/testing/headless_engine.h"
#include "conjure/testing/test_assets.h"
#include "conjure/testing/world_builder.h"
#include "integration_engine_support.h"

#include "conjure/core/math/glm_types.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/chunk_view.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/ecs/view.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/engine_context.h"
#include "conjure/scene/scene.h"

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #include "conjure/physics/components.h"
#endif

#include <span>
#include <vector>

// docs/design/08_testing.md 6.1 test_determinism (R41): the per-frame world_hash sequence of 300 frames over
// tests/assets/scenes/determinism.scene.json plus two test systems is identical (a) between two runs with the
// same options, (b) across 0, 2 and 8 job workers with physics off and (c) between two runs with physics on at
// one worker count. The spawner is EXCLUSIVE and creates entities through its command buffer; the integrator
// is PARALLEL over chunks and destroys through the grain buffers, which are flushed in grain order, so entity
// id values never depend on how the work was split (01_ecs.md 3.5).

using namespace conjure;
using namespace conjure::testing;

namespace integration_determinism {

    struct mover {
        math::vec3 position = math::vec3(0.0f);
        math::vec3 velocity = math::vec3(0.0f);
        u64 bornFrame = 0;
    };
}

CONJURE_REFLECT_STRUCT(integration_determinism::mover, "integration.determinism.mover",
    CONJURE_FIELD(position),
    CONJURE_FIELD(velocity),
    CONJURE_FIELD(bornFrame));
CONJURE_COMPONENT(integration_determinism::mover, "integration.determinism.mover");

namespace {
    using integration_determinism::mover;

    constexpr u32 FRAME_COUNT = 300;
    constexpr u32 SPAWN_PER_FRAME = 7;
    constexpr u64 LIFETIME_FRAMES = 90;
    constexpr core::string_id SPAWN_SYSTEM("integration.determinism.spawn");
    constexpr core::string_id INTEGRATE_SYSTEM("integration.determinism.integrate");

    /// <summary>
    /// EXCLUSIVE (UPDATE): spawns SPAWN_PER_FRAME movers per frame through the system's command buffer with
    /// values derived from the frame index only.
    /// </summary>
    void spawn_movers(ecs::system_context &context) {
        const u64 frame = context.frame().frameIndex;
        for (u32 index = 0; index < SPAWN_PER_FRAME; ++index) {
            const ecs::entity spawned = context.commands().create_entity();
            mover value = {};
            value.position = math::vec3(static_cast<f32>(index), static_cast<f32>(frame % 17), 0.0f);
            value.velocity = math::vec3(0.5f, -0.25f * static_cast<f32>(index + 1), 1.0f);
            value.bornFrame = frame;
            context.commands().add<mover>(spawned, value);
        }
    }

    /// <summary>
    /// PARALLEL (UPDATE, after the spawner): integrates every mover over the chunks in grains and destroys the
    /// ones past their lifetime through the grain command buffer.
    /// </summary>
    void integrate_movers(ecs::system_context &context) {
        const f32 deltaTime = context.delta_time();
        const u64 frame = context.frame().frameIndex;
        ecs::view<mover> movers = context.view<mover>();
        auto integrate = [deltaTime, frame](ecs::chunk_view &chunk, ecs::command_buffer &commands) {
            const std::span<mover> values = chunk.get<mover>();
            const std::span<const ecs::entity> entities = chunk.entities();
            for (u32 index = 0; index < chunk.count(); ++index) {
                mover &value = values[index];
                value.position += value.velocity * deltaTime;
                if (frame >= value.bornFrame + LIFETIME_FRAMES) {
                    commands.destroy_entity(entities[index]);
                }
            }
        };
        movers.parallel_for_each_chunk(context, integrate);
    }

    void register_determinism_systems(ecs::scheduler &scheduler, engine::engine_context &context) {
        CONJURE_REQUIRE_OK(context.components->register_component<mover>());

        ecs::system_desc spawner = ecs::make_system<mover>(SPAWN_SYSTEM, ecs::system_phase::UPDATE, &spawn_movers);
        spawner.flags = ecs::system_flags::EXCLUSIVE;
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(spawner)));

        ecs::system_desc integrator =
          ecs::make_system<mover>(INTEGRATE_SYSTEM, ecs::system_phase::UPDATE, &integrate_movers);
        integrator.after.push_back(SPAWN_SYSTEM);
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(integrator)));
    }

    struct run_options {
        u32 workers = 0;
        bool physics = false;
    };

    /// <summary>
    /// One 300-frame run over the determinism scene: the world_hash after every frame. The result vector is
    /// reserved before the fixture takes its memory baseline (it outlives the fixture), and the reflection
    /// description of the test component, which lives in the process-wide registry for good, is created then too.
    /// </summary>
    [[nodiscard]] std::vector<u64> run_sequence(const run_options &run) {
        std::vector<u64> hashes = {};
        hashes.reserve(FRAME_COUNT);
        CONJURE_UNUSED(core::reflect::type_info_of<mover>());
        headless_engine_options options = integration_test::quiet_options();
        options.jobThreadCount = run.workers;
        options.enablePhysics = run.physics;
        options.registerSystems = &register_determinism_systems;
        headless_engine engine(options);
        CONJURE_REQUIRE_OK(engine.load_scene(repo_asset("scenes/determinism.scene.json").generic_string()));
        REQUIRE(engine.world().entity_count() > 0);
        CHECK(engine.context().jobs->worker_count() == run.workers);

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
        if (run.physics) {
            REQUIRE(engine.context().physics != nullptr);
            for (u32 index = 0; index < 20; ++index) {
                const math::vec3 position(static_cast<f32>(index % 5) * 1.5f, 5.0f + static_cast<f32>(index), 0.0f);
                const core::result<ecs::entity> box =
                  spawn_box_body(engine.active_scene(), position, physics::motion_type::DYNAMIC);
                CONJURE_REQUIRE_OK(box);
                // Scene entities get a random guid; the hash covers it, so the boxes receive fixed ones.
                CONJURE_REQUIRE_OK(engine.active_scene().set_guid(box.value(), core::uuid(0x0de7e2ull, index + 1)));
            }
        }
#endif

        for (u32 frame = 0; frame < FRAME_COUNT; ++frame) {
            engine.step_frames(1);
            hashes.push_back(engine.world_hash());
        }
        // The spawner and integrator did run: the population grew and then held at the lifetime window.
        CHECK(engine.world().entity_count() > SPAWN_PER_FRAME * (LIFETIME_FRAMES - 2));
        return hashes;
    }

    [[nodiscard]] usize first_difference(const std::vector<u64> &left, const std::vector<u64> &right) {
        for (usize index = 0; index < left.size() && index < right.size(); ++index) {
            if (left[index] != right[index]) {
                return index;
            }
        }
        return left.size() == right.size() ? left.size() : std::min(left.size(), right.size());
    }
}

TEST_SUITE("integration/determinism") {

    TEST_CASE("determinism - two runs with the same options produce identical per-frame hash sequences") {
        const run_options options = {};
        const std::vector<u64> first = run_sequence(options);
        const std::vector<u64> second = run_sequence(options);
        REQUIRE(first.size() == FRAME_COUNT);
        CHECK_MESSAGE(first == second, "sequences diverge at frame " << first_difference(first, second));
        // The world changes every frame: a constant sequence would prove nothing.
        CHECK(first.front() != first.back());
    }

    TEST_CASE("determinism - 0, 2 and 8 job workers produce identical sequences with physics off [parallel]") {
        run_options inlineRun = {};
        inlineRun.workers = 0;
        run_options twoWorkers = {};
        twoWorkers.workers = 2;
        run_options eightWorkers = {};
        eightWorkers.workers = 8;
        const std::vector<u64> baseline = run_sequence(inlineRun);
        const std::vector<u64> two = run_sequence(twoWorkers);
        const std::vector<u64> eight = run_sequence(eightWorkers);
        REQUIRE(baseline.size() == FRAME_COUNT);
        CHECK_MESSAGE(baseline == two, "2 workers diverge at frame " << first_difference(baseline, two));
        CHECK_MESSAGE(baseline == eight, "8 workers diverge at frame " << first_difference(baseline, eight));
    }

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    TEST_CASE("determinism - two runs with physics enabled and the same worker count are identical") {
        run_options options = {};
        options.workers = 2;
        options.physics = true;
        const std::vector<u64> first = run_sequence(options);
        const std::vector<u64> second = run_sequence(options);
        REQUIRE(first.size() == FRAME_COUNT);
        CHECK_MESSAGE(first == second, "physics runs diverge at frame " << first_difference(first, second));
    }
#endif
}
