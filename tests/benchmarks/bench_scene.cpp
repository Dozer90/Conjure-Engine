// Scene scenarios (docs/design/08_testing.md 7.1, 13_scene.md 10): the transform propagation of 10,000
// entities through the TRANSFORM phase with every root moved (and with 10 % of them), a 10,000-entity
// play/stop snapshot round trip and a 10,000-entity scene file load plus unload.

#include "bench_support.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/bench/benchmark.h"
#include "conjure/assets/asset_source.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/math.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/vfs/memory_file_system.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/ecs/world.h"
#include "conjure/scene/components.h"
#include "conjure/scene/register.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"
#include "conjure/scene/scene_serializer.h"
#include "conjure/scene/scene_snapshot.h"

#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::bench;
using namespace conjure::bench::support;

namespace {

    constexpr f32 STEP_SECONDS = 1.0f / 60.0f;
    constexpr std::string_view SCENE_FILE = "bench.scene.json";

    /// <summary>
    /// Registry, world, an asset manager over a memory source (the scene manager binds through it), the
    /// scene manager and a scheduler with the four scene systems. Stages run inline so the allocation gate
    /// sees the propagation pass alone; parallel_for_chunks fans it out over the benchmark job system.
    /// </summary>
    class scene_fixture {
        public:
            ecs::component_registry registry = {};
            ecs::world world;                                   // no default: built on the registry
            core::jobs::job_system assetJobs = {};              // never started: nothing loads asynchronously here
            assets::asset_manager assets;                       // no default: built from a desc
            scene::scene_manager manager;                       // no default: bound to world and assets
            ecs::scheduler scheduler;                           // no default: bound to the world
            ecs::frame_info frame = {};

        public:
            explicit scene_fixture(core::jobs::job_system &jobs) :
                world((CONJURE_UNUSED(scene::register_components(registry)), registry)),
                assets(make_asset_desc()),
                manager(world, assets),
                scheduler(world, &jobs, make_scheduler_desc()) {
                CONJURE_UNUSED(scene::register_systems(scheduler, manager));
                CONJURE_UNUSED(scheduler.finalise());
                frame.deltaTime = STEP_SECONDS;
                frame.fixedDeltaTime = STEP_SECONDS;
            }

            ~scene_fixture() {}

            scene_fixture(const scene_fixture &) = delete;
            scene_fixture &operator=(const scene_fixture &) = delete;

        public:
            [[nodiscard]] scene::scene &active() noexcept {
                return manager.active();
            }

            /// <summary>
            /// Runs FRAME_BEGIN, TRANSFORM and FRAME_END once.
            /// </summary>
            void step() {
                ++frame.frameIndex;
                scheduler.run_phase(ecs::system_phase::FRAME_BEGIN, frame);
                scheduler.run_phase(ecs::system_phase::TRANSFORM, frame);
                scheduler.run_phase(ecs::system_phase::FRAME_END, frame);
            }

            /// <summary>
            /// Runs only the TRANSFORM phase once.
            /// </summary>
            void step_transform() {
                ++frame.frameIndex;
                scheduler.run_phase(ecs::system_phase::TRANSFORM, frame);
            }

        private:
            [[nodiscard]] assets::asset_manager_desc make_asset_desc() {
                assets::asset_manager_desc desc = {};
                desc.jobs = &assetJobs;
                desc.source = std::make_unique<assets::memory_asset_source>();
                return desc;
            }

            [[nodiscard]] static ecs::scheduler_desc make_scheduler_desc() noexcept {
                ecs::scheduler_desc desc = {};
                desc.parallelSystems = false;
                desc.parallelChunks = true;
                return desc;
            }
    };

    [[nodiscard]] hierarchy_shape shape_for(const bench_context &context) noexcept {
        hierarchy_shape shape = {};
        shape.totalEntityCount = context.entity_count();
        return shape;
    }

    /// <summary>
    /// Moves the first 'count' roots to a new position so every entity below them is recomputed.
    /// </summary>
    void move_roots(scene::scene &target, std::span<const ecs::entity> roots, usize count, u64 frame) {
        const f32 offset = static_cast<f32>(frame % 100) * 0.01f;
        for (usize index = 0; index < count && index < roots.size(); ++index) {
            scene::local_transform moved = {};
            moved.position = math::vec3(offset, static_cast<f32>(index), 0.0f);
            CONJURE_UNUSED(target.set_local_transform(roots[index], moved));
        }
    }

    void propagation_scenario(bench_context &context, f32 dirtyFraction) {
        scene_fixture fixture(context.jobs());
        const bench_hierarchy built = spawn_hierarchy(fixture.active(), shape_for(context), context.seed());
        if (built.all.empty()) {
            context.skip("the scene refused the hierarchy");
            return;
        }
        // Two frames settle the structural dirty tags (13_scene.md: allocation-free from the third frame).
        fixture.step();
        fixture.step();

        const usize dirtyRoots = static_cast<usize>(static_cast<f32>(built.roots.size()) * dirtyFraction + 0.5f);
        context.set_items(built.all.size());
        context.measure([&]() {
            move_roots(fixture.active(), built.roots, dirtyRoots, fixture.frame.frameIndex);
            fixture.step_transform();
        });
    }
}

CONJURE_BENCHMARK(scene.transform_propagation_10k) {
    propagation_scenario(context, 1.0f);
}

CONJURE_BENCHMARK(scene.transform_propagation_10k_sparse) {
    propagation_scenario(context, 0.1f);
}

CONJURE_BENCHMARK(scene.snapshot_capture_restore_10k) {
    scene_fixture fixture(context.jobs());
    const bench_hierarchy built = spawn_hierarchy(fixture.active(), shape_for(context), context.seed());
    if (built.all.empty()) {
        context.skip("the scene refused the hierarchy");
        return;
    }
    fixture.step();

    context.set_items(built.all.size());
    context.measure([&]() {
        core::result<scene::scene_snapshot> captured = scene::capture(fixture.active());
        if (captured.has_value()) {
            CONJURE_UNUSED(scene::restore(fixture.active(), captured.value()));
        }
    });
}

CONJURE_BENCHMARK(scene.load_10k) {
    scene_fixture fixture(context.jobs());
    const bench_hierarchy built = spawn_hierarchy(fixture.active(), shape_for(context), context.seed());
    if (built.all.empty()) {
        context.skip("the scene refused the hierarchy");
        return;
    }
    fixture.step();

    core::vfs::memory_file_system files = {};
    const core::result<void> saved = scene::scene_serializer::save_scene_file(fixture.active(), files, SCENE_FILE);
    if (!saved.has_value()) {
        context.skip("the scene file could not be written to the memory file system");
        return;
    }
    fixture.active().clear();

    context.set_items(built.all.size());
    context.measure([&]() {
        const core::result<scene::scene_id> loaded =
          fixture.manager.load_file(files, SCENE_FILE, scene::load_mode::REPLACE);
        if (loaded.has_value()) {
            CONJURE_UNUSED(fixture.manager.unload(loaded.value()));
        }
    });
}
