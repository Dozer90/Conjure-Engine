#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/assert_capture.h"

#include "conjure/core/reflect/reflect.h"
#include "conjure/ecs/archetype.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/world.h"

#include <optional>
#include <vector>

// Allocation gates of the structural paths (docs/design/08_testing.md 7.1 `ecs.spawn_destroy_10k` <= 2,
// docs/design/01_ecs.md 3.4): the flush plan, the placement undo list, the command buffer's arena and the
// archetype's parked chunk metadata all keep their capacity, so steady-state churn allocates nothing.

namespace ecs_test {

    /// <summary>
    /// Non-trivially copyable component whose default constructor calls back into its world from the
    /// second construction on: the one foreign code a structural call runs is a component constructor or
    /// destructor, and one that re-entered place_rows would clear the undo list the outer call is using.
    /// </summary>
    struct reentrant_component {
        static inline conjure::ecs::world *sWorld = nullptr;
        static inline conjure::u32 sConstructed = 0;
        static inline std::optional<conjure::core::result<conjure::ecs::entity>> sInner = {};

        conjure::u32 value = 0;

        reentrant_component() {
            ++sConstructed;
            if (sWorld == nullptr || sConstructed != 2) {
                return;
            }
            sInner = sWorld->create_entity();
        }

        reentrant_component(const reentrant_component &) = default;
        reentrant_component(reentrant_component &&) noexcept = default;
        reentrant_component &operator=(const reentrant_component &) = default;
        reentrant_component &operator=(reentrant_component &&) noexcept = default;

        ~reentrant_component() {}

        static void arm(conjure::ecs::world &target) {
            sWorld = &target;
            sConstructed = 0;
            sInner.reset();
        }

        static void disarm() {
            sWorld = nullptr;
        }
    };
}

CONJURE_REFLECT_STRUCT(ecs_test::reentrant_component, "test.reentrant", CONJURE_FIELD(value));
CONJURE_COMPONENT(ecs_test::reentrant_component, "test.reentrant");

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

namespace {
    constexpr u32 LIVE_COUNT = 10000;
    constexpr u32 CHURN_COUNT = 1000;

    /// <summary>
    /// The benchmark's iteration: the oldest CHURN_COUNT entities die and CHURN_COUNT fresh ones with two
    /// components take their slots, all through one command buffer flushed at the end. No doctest macro
    /// runs inside, so the caller can gate the whole call on allocations.
    /// </summary>
    [[nodiscard]] flush_report churn(
      world &world,
      command_buffer &commands,
      std::vector<entity> &alive,
      usize &oldest
    ) {
        for (u32 index = 0; index < CHURN_COUNT; ++index) {
            const usize slot = (oldest + index) % alive.size();
            commands.destroy_entity(alive[slot]);
            const entity spawned = commands.create_entity();
            commands.add<position>(spawned, position{1.0f, 2.0f, 3.0f});
            commands.add<velocity>(spawned, velocity{0.1f, 0.0f, 0.0f});
            alive[slot] = spawned;
        }
        oldest = (oldest + CHURN_COUNT) % alive.size();
        return world.flush(commands);
    }
}

TEST_SUITE("ecs/world_structural") {

    TEST_CASE("world - a warmed-up spawn destroy flush allocates at most two blocks") {
        // The 08_testing.md 7.1 gate of ecs.spawn_destroy_10k, measured here on the world alone: after the
        // warm-up the plan vector, the arena block and the chunk metadata are all reused.
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> alive(LIVE_COUNT);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position, velocity>(), alive));
        command_buffer commands(world);
        usize oldest = 0;
        for (u32 warmUp = 0; warmUp < 4; ++warmUp) {
            const flush_report report = churn(world, commands, alive, oldest);
            CHECK(report.applied == 4 * CHURN_COUNT);
            CHECK(report.skipped == 0);
        }
        CHECK(world.entity_count() == LIVE_COUNT);

        for (u32 iteration = 0; iteration < 5; ++iteration) {
            CAPTURE(iteration);
            testing::allocation_scope scope = {};
            const flush_report report = churn(world, commands, alive, oldest);
            const u64 allocated = scope.allocation_count();
            CHECK(allocated <= 2);
            CHECK(report.applied == 4 * CHURN_COUNT);
            CHECK(report.skipped == 0);
        }
        CHECK(world.entity_count() == LIVE_COUNT);
        for (const entity id : alive) {
            CHECK(world.is_alive(id));
        }
        CONJURE_CHECK_OK(world.validate());
    }

    TEST_CASE("world - destroying and recreating a thousand entities allocates nothing after the first round") {
        // Direct API counterpart of the flush gate: the placement undo list is world scratch kept at
        // capacity (not a spilling local), the released ids come back from the allocator's free list and
        // the drained chunks' metadata is re-attached to pool blocks.
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> ids(1000);
        const component_mask mask = fixture.mask<position, velocity, health>();
        const auto round = [&world, &ids, &mask]() -> bool {
            bool succeeded = true;
            for (const entity id : ids) {
                succeeded = world.destroy_entity(id).has_value() && succeeded;
            }
            return world.create_entities(mask, ids).has_value() && succeeded;
        };
        CONJURE_REQUIRE_OK(world.create_entities(mask, ids));
        CHECK(round());
        CHECK(world.entity_count() == 1000);

        bool succeeded = false;
        CONJURE_CHECK_NO_ALLOCATIONS(succeeded = round());
        CHECK(succeeded);
        CHECK(world.entity_count() == 1000);
        for (const entity id : ids) {
            CHECK(world.is_alive(id));
            CHECK(world.has<health>(id));
        }
        CONJURE_CHECK_OK(world.validate());
    }

    TEST_CASE("world - a failed bulk placement undoes every row and keeps the scratch usable") {
        // place_rows undoes through the borrowed scratch list on failure; the list must be handed back so
        // the next placement works and the world is unchanged.
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const zero_capacity_archetype zero = make_zero_capacity_archetype(fixture);
        std::vector<entity> ids(10);
        CONJURE_CHECK_ERROR(world.create_entities(zero.mask, ids), ecs_error::ARCHETYPE_TOO_LARGE);
        CHECK(world.entity_count() == 0);
        CONJURE_CHECK_OK(world.validate());

        std::vector<entity> placed(300);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), placed));
        CHECK(world.entity_count() == 300);
        CONJURE_CHECK_ERROR(world.create_entities(zero.mask, ids), ecs_error::ARCHETYPE_TOO_LARGE);
        CHECK(world.entity_count() == 300);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), placed));
        CHECK(world.entity_count() == 600);
        CONJURE_CHECK_OK(world.validate());
    }

    TEST_CASE("regression - review perf 1 a component constructor that calls back into the world is refused") {
        // place_rows constructs each row's columns through ops.constructDefault with the member undo list in
        // use; a constructor that made a structural call would re-enter it. check_structural refuses the
        // call (STRUCTURE_LOCKED, one assertion) and the outer placement completes untouched.
        ecs_fixture fixture = {};
        world &world = fixture.world;
        CONJURE_REQUIRE_OK(fixture.registry.register_component<reentrant_component>());
        std::vector<entity> ids(3);
        reentrant_component::arm(world);
        {
            testing::expect_assert guard = {};
            CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<reentrant_component>(), ids));
            CHECK(guard.count() == ASSERTS_PER_VIOLATION);
        }
        reentrant_component::disarm();
        REQUIRE(reentrant_component::sInner.has_value());
        CONJURE_CHECK_ERROR(*reentrant_component::sInner, ecs_error::STRUCTURE_LOCKED);
        CHECK(world.entity_count() == 3);
        for (const entity id : ids) {
            CHECK(world.is_alive(id));
            CHECK(world.has<reentrant_component>(id));
        }
        CONJURE_CHECK_OK(world.validate());
    }

    TEST_CASE("regression - review perf 2 clear returns the structural scratch and the parked chunk metadata") {
        // The flush plan, the placement list, the parked chunk metadata and the deferred buffers' room are
        // sized by their peak and kept for the world's lifetime; clear() is the one point that gives them
        // back, next to the chunk pool shrink it already did.
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const component_mask mask = fixture.mask<position, velocity>();
        std::vector<entity> alive(LIVE_COUNT);
        CONJURE_REQUIRE_OK(world.create_entities(mask, alive));
        command_buffer commands(world);
        usize oldest = 0;
        for (u32 warmUp = 0; warmUp < 4; ++warmUp) {
            CHECK(churn(world, commands, alive, oldest).applied == 4 * CHURN_COUNT);
        }
        {
            testing::allocation_scope steady = {};
            CHECK(churn(world, commands, alive, oldest).applied == 4 * CHURN_COUNT);
            CHECK(steady.allocation_count() <= 2);
        }
        bool parkedChunks = false;
        for (const archetype *owner : world.archetypes()) {
            parkedChunks = parkedChunks || owner->spare_chunk_count() > 0;
        }
        CHECK(parkedChunks);

        world.clear();
        CHECK(world.entity_count() == 0);
        for (const archetype *owner : world.archetypes()) {
            CHECK(owner->chunk_count() == 0);
            CHECK(owner->spare_chunk_count() == 0);
        }

        // The same churn has to grow the plan again (the capacity was given back, not kept), after which
        // the world is allocation-free in steady state once more.
        CONJURE_REQUIRE_OK(world.create_entities(mask, alive));
        oldest = 0;
        {
            testing::allocation_scope regrown = {};
            CHECK(churn(world, commands, alive, oldest).applied == 4 * CHURN_COUNT);
            CHECK(regrown.allocation_count() > 0);
        }
        for (u32 warmUp = 0; warmUp < 3; ++warmUp) {
            CHECK(churn(world, commands, alive, oldest).applied == 4 * CHURN_COUNT);
        }
        {
            testing::allocation_scope steady = {};
            CHECK(churn(world, commands, alive, oldest).applied == 4 * CHURN_COUNT);
            CHECK(steady.allocation_count() <= 2);
        }
        CHECK(world.entity_count() == LIVE_COUNT);
        CONJURE_CHECK_OK(world.validate());
    }
}
