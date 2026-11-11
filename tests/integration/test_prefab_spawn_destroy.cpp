#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/headless_engine.h"
#include "conjure/testing/test_assets.h"
#include "integration_engine_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/memory/allocation_counters.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/engine_context.h"
#include "conjure/scene/prefab.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_serializer.h"

#include <algorithm>
#include <array>
#include <limits>
#include <span>
#include <vector>

// docs/design/08_testing.md 6.1 test_prefab_spawn_destroy: tests/assets/prefabs/crate.prefab.json is instantiated
// 100 times and the previous frame's 100 instances are destroyed every frame for 300 frames: the entity count is
// stable and every handle is invalid after its destroy. The design asks for zero per-frame allocations after frame
// 10; the archive instantiation path allocates per batch today (measured and recorded in
// 09_cross_area_resolutions.md 4 item 15a), so three gates hold here: from frame 10 on a frame stays within the
// recorded budget, the number of live allocations stops growing (every block a frame allocates is released by a
// later one) and the per-frame count stays within a narrow band.

using namespace conjure;
using namespace conjure::testing;

namespace {
    constexpr u32 BATCH = 100;
    constexpr u32 FRAMES = 300;
    constexpr u32 WARM_FRAMES = 10;
    constexpr u32 CRATE_ENTITIES = 2;   // the crate and its lid
    constexpr u64 STEADY_BAND = 64;     // per-frame variation the chunk and guid-index churn may show

    // What spawning 100 crates and destroying 100 costs per steady-state frame with inline jobs, measured and
    // recorded in docs/design/09_cross_area_resolutions.md 4 item 15a (526-542 blocks: archive instantiation and
    // the guid index). 08_testing.md 6.1 asks for zero; this budget may only go down as prefab::instantiate_many
    // is made allocation-free, never up. Run the case the way CTest does (one case per process, no doctest -s):
    // the success reporter's own bookkeeping adds a block to a frame.
    constexpr u64 STEADY_FRAME_ALLOCATION_BUDGET = 542;

    [[nodiscard]] scene::prefab_asset load_crate(const ecs::component_registry &registry) {
        const std::string text = integration_test::read_file(repo_asset("prefabs/crate.prefab.json"));
        const core::result<core::json::document> parsed = core::json::document::parse(text);
        CONJURE_REQUIRE_OK(parsed);
        const core::result<scene::prefab_document> document =
          scene::scene_serializer::parse_prefab(parsed.value().root(), registry);
        CONJURE_REQUIRE_OK(document);
        scene::prefab_asset asset = {};
        asset.archive = document.value().archive;
        asset.name = document.value().name;
        return asset;
    }

    void spawn_batch(scene::scene &draft, const scene::prefab_asset &crate, std::array<ecs::entity, BATCH> &out) {
        const scene::instantiate_options options = {};
        CONJURE_REQUIRE_OK(scene::prefab::instantiate_many(draft, crate, BATCH, options, out));
    }

    [[nodiscard]] bool all_alive(const ecs::world &world, std::span<const ecs::entity> entities) {
        for (const ecs::entity entity : entities) {
            if (!world.is_alive(entity)) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool none_alive(const ecs::world &world, std::span<const ecs::entity> entities) {
        for (const ecs::entity entity : entities) {
            if (world.is_alive(entity)) {
                return false;
            }
        }
        return true;
    }
}

TEST_SUITE("integration/prefab_spawn_destroy") {

    TEST_CASE("prefab_spawn_destroy - 100 crates spawned and 100 destroyed per frame keep the count stable") {
        u64 worstSteadyAllocations = 0;
        u64 leastSteadyAllocations = std::numeric_limits<u64>::max();
        {
            headless_engine_options options = integration_test::quiet_options();
            options.jobThreadCount = 0;
            headless_engine engine(options);
            scene::scene &draft = engine.active_scene();
            const scene::prefab_asset crate = load_crate(*engine.context().components);
            REQUIRE(crate.archive != nullptr);

            std::array<ecs::entity, BATCH> live = {};
            spawn_batch(draft, crate, live);
            engine.step_frames(1);
            CHECK(engine.world().entity_count() == BATCH * CRATE_ENTITIES);

            i64 liveAtWarm = 0;
            for (u32 frame = 1; frame < FRAMES; ++frame) {
                CAPTURE(frame);
                std::array<ecs::entity, BATCH> previous = live;
                allocation_scope scope = {};
                for (const ecs::entity root : previous) {
                    draft.destroy(root);           // queued; scene.destroy_pending runs at FRAME_END
                }
                spawn_batch(draft, crate, live);
                engine.step_frames(1);
                const u64 allocations = scope.allocation_count();
                if (frame == WARM_FRAMES) {
                    liveAtWarm = core::memory::snapshot().live_count();
                }
                if (frame >= WARM_FRAMES) {
                    worstSteadyAllocations = std::max(worstSteadyAllocations, allocations);
                    leastSteadyAllocations = std::min(leastSteadyAllocations, allocations);
                }

                // The previous batch is gone, every one of its 100 handles is stale, and exactly one batch is alive.
                CHECK(engine.world().entity_count() == BATCH * CRATE_ENTITIES);
                CHECK(none_alive(engine.world(), previous));
                CHECK(all_alive(engine.world(), live));
            }
            CHECK(draft.roots().size() == BATCH);
            const i64 liveAtEnd = core::memory::snapshot().live_count();
            CHECK_MESSAGE(
              liveAtEnd <= liveAtWarm,
              "live allocations grew from " << liveAtWarm << " at frame " << WARM_FRAMES << " to " << liveAtEnd
                << " at frame " << FRAMES - 1 << ": the spawn / destroy churn accumulates memory"
            );
            CHECK_MESSAGE(
              worstSteadyAllocations - leastSteadyAllocations <= STEADY_BAND,
              "steady-state frames (spawn 100 + destroy 100 + step) allocated between " << leastSteadyAllocations
                << " and " << worstSteadyAllocations << " block(s)"
            );
            CHECK_MESSAGE(
              worstSteadyAllocations <= STEADY_FRAME_ALLOCATION_BUDGET,
              "a steady-state frame (spawn 100 + destroy 100 + step) allocated " << worstSteadyAllocations
                << " block(s); the recorded budget is " << STEADY_FRAME_ALLOCATION_BUDGET
                << " (09_cross_area_resolutions.md 4 item 15a)"
            );
            // Stale handles stay invalid and the frame after the last destroy leaves nothing pending.
            for (const ecs::entity root : live) {
                draft.destroy(root);
            }
            engine.step_frames(1);
            CHECK(engine.world().entity_count() == 0);
            CHECK(none_alive(engine.world(), live));
            CHECK(draft.pending_destroy_count() == 0);
        }
        // After the fixture's leak check: a MESSAGE record stays allocated until the case ends.
        MESSAGE("a steady-state frame (spawn 100 + destroy 100 + step) allocates " << leastSteadyAllocations << " to "
          << worstSteadyAllocations << " block(s)");
    }
}
