#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/headless_engine.h"
#include "conjure/testing/test_environment.h"
#include "conjure/testing/world_builder.h"
#include "integration_engine_support.h"

#include "conjure/core/math.h"
#include "conjure/core/types.h"
#include "conjure/ecs/world.h"
#include "conjure/scene/components.h"
#include "conjure/scene/scene.h"

#include <algorithm>
#include <optional>
#include <vector>

// docs/design/08_testing.md 6.1 test_hierarchy_propagation: a depth-8 chain whose root moves is propagated to
// the leaf in one frame, exactly; a 10,000-entity hierarchy runs its frame allocation-free once warm.

using namespace conjure;
using namespace conjure::testing;

namespace {
    constexpr u32 CHAIN_DEPTH = 8;
    constexpr u32 BASELINE_FRAMES = 5;

    // The frame loop's own per-frame cost with inline jobs, measured and recorded in
    // docs/design/09_cross_area_resolutions.md 4 item 15a. The engine's frame is allocation-free in Development
    // (the engine.full_frame_10k_* benchmarks gate 0); the one block left in this Debug executable is the
    // container proxy MSVC's debug iterators give the local std::vector of
    // assets::detail::completion_queue::drain (asset_manager::update, engine/src/assets), which does not exist
    // without _ITERATOR_DEBUG_LEVEL. 08_testing.md 6.1 asks for zero; this budget may only go down, never up.
    // Run the case the way CTest does (one case per process, no doctest -s): the success reporter's own
    // bookkeeping adds blocks to a frame.
    constexpr u64 EMPTY_FRAME_ALLOCATION_BUDGET = 1;
}

TEST_SUITE("integration/hierarchy_propagation") {

    TEST_CASE("hierarchy_propagation - moving the root of a depth-8 chain offsets the leaf exactly in one frame") {
        headless_engine engine(integration_test::quiet_options());
        scene::scene &draft = engine.active_scene();

        std::vector<ecs::entity> chain = {};
        chain.push_back(integration_test::create_at(draft, "Link_0", math::vec3(0.0f)));
        for (u32 depth = 1; depth < CHAIN_DEPTH; ++depth) {
            chain.push_back(integration_test::create_at(draft, "Link", math::vec3(1.0f, 0.0f, 0.0f), chain.back()));
        }
        engine.step_frames(1);
        const ecs::entity leaf = chain.back();
        CHECK(draft.tree().depth_of(leaf) == CHAIN_DEPTH - 1);
        const math::vec3 before = integration_test::propagated_position(draft, leaf);
        CHECK(before.x == doctest::Approx(static_cast<f32>(CHAIN_DEPTH - 1)));
        CHECK(before.y == doctest::Approx(0.0f));

        scene::local_transform moved = {};
        moved.position = math::vec3(0.0f, 5.0f, -2.0f);
        CONJURE_REQUIRE_OK(draft.set_local_transform(chain.front(), moved));
        engine.step_frames(1);

        const math::vec3 after = integration_test::propagated_position(draft, leaf);
        CHECK(after.x == before.x);
        CHECK(after.y == 5.0f);
        CHECK(after.z == -2.0f);
        CHECK(integration_test::rendered_position(draft, leaf) == after);

        // The fresh ancestor walk agrees with what the propagation pass wrote.
        const std::optional<math::transform> walked = draft.world_transform_of(leaf);
        REQUIRE(walked.has_value());
        CHECK(walked->position.x == doctest::Approx(after.x));
        CHECK(walked->position.y == doctest::Approx(after.y));
        CHECK(walked->position.z == doctest::Approx(after.z));
    }

    TEST_CASE("hierarchy_propagation - a 10,000-entity hierarchy adds nothing to the frame's allocations once warm") {
        // 08_testing.md 6.1 asks for an allocation-free frame. The frame loop itself is allocation-free in
        // Development; this Debug executable sees one block of debug-iterator bookkeeping per frame
        // (EMPTY_FRAME_ALLOCATION_BUDGET, 09_cross_area_resolutions.md 4 item 15a), so two gates hold here: the
        // empty frame stays within that recorded budget, and a 10,000-entity hierarchy with 10 % of its entities
        // moved every frame costs no more than the empty frame, frame after frame, from frame 2 on.
        u64 emptyFrame = 0;
        u64 worstFrame = 0;
        {
            headless_engine_options options = integration_test::quiet_options();
            options.jobThreadCount = 0;   // the inline baseline: a job system's scheduling counters are not the scene's
            headless_engine engine(options);
            scene::scene &draft = engine.active_scene();
            engine.step_frames(2);
            for (u32 frame = 0; frame < BASELINE_FRAMES; ++frame) {
                allocation_scope scope = {};
                engine.step_frames(1);
                emptyFrame = std::max(emptyFrame, scope.allocation_count());
            }
            CHECK_MESSAGE(
              emptyFrame <= EMPTY_FRAME_ALLOCATION_BUDGET,
              "an empty frame allocated " << emptyFrame << " block(s); the recorded frame-loop budget is "
              << EMPTY_FRAME_ALLOCATION_BUDGET << " (09_cross_area_resolutions.md 4 item 15a)"
            );

            const hierarchy_shape shape = {};
            const std::vector<ecs::entity> tree = spawn_hierarchy(draft, shape);
            REQUIRE(tree.size() == hierarchy_entity_count(shape));
            CHECK(engine.world().entity_count() == shape.totalEntityCount);
            const usize movedPerFrame = tree.size() / 10;

            const auto moveTenth = [&](u32 frame) {
                for (usize index = 0; index < movedPerFrame; ++index) {
                    const ecs::entity target = tree[(index * 10 + frame) % tree.size()];
                    const std::optional<math::transform> local = draft.local_transform_of(target);
                    REQUIRE(local.has_value());
                    scene::local_transform moved = scene::local_transform::from_transform(*local);
                    moved.position.x += 0.01f;
                    CONJURE_REQUIRE_OK(draft.set_local_transform(target, moved));
                }
            };

            // Frames 0 and 1 warm the propagation state (chunk lists, per-level scratch); frame 2 onwards is steady.
            for (u32 frame = 0; frame < 2; ++frame) {
                moveTenth(frame);
                engine.step_frames(1);
            }
            for (u32 frame = 2; frame < 12; ++frame) {
                CAPTURE(frame);
                moveTenth(frame);
                allocation_scope scope = {};
                engine.step_frames(1);
                worstFrame = std::max(worstFrame, scope.allocation_count());
            }
            CHECK_MESSAGE(
              worstFrame <= emptyFrame,
              "a frame over the 10,000-entity hierarchy allocated " << worstFrame
                << " block(s); an empty frame allocates " << emptyFrame
            );
            CHECK_MESSAGE(
              worstFrame <= EMPTY_FRAME_ALLOCATION_BUDGET,
              "a frame over the 10,000-entity hierarchy allocated " << worstFrame
                << " block(s); the recorded budget is " << EMPTY_FRAME_ALLOCATION_BUDGET
            );
            // The moved tenth was propagated: the pass and a fresh ancestor walk agree on the last entity.
            const std::optional<math::transform> walked = draft.world_transform_of(tree.back());
            REQUIRE(walked.has_value());
            CHECK(integration_test::propagated_position(draft, tree.back()).x == doctest::Approx(walked->position.x));
        }
        // After the fixture's leak check: a MESSAGE record stays allocated until the case ends.
        MESSAGE("an empty frame allocates " << emptyFrame << " block(s), a frame over the 10,000-entity hierarchy "
          << worstFrame);
    }
}
