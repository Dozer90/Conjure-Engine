#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/testing/allocation_scope.h"

#include "conjure/ecs/archetype.h"
#include "conjure/ecs/world.h"

#include <algorithm>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

namespace {
    const archetype *find_archetype(const world &world, const component_mask &mask) {
        for (const archetype *owner : world.archetypes()) {
            if (owner->mask() == mask) {
                return owner;
            }
        }
        return nullptr;
    }
}

TEST_SUITE("ecs/archetype") {

    TEST_CASE("archetype - push and swap remove keep rows packed") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const component_mask mask = fixture.mask<position>();

        std::vector<entity> ids(600);
        CONJURE_REQUIRE_OK(world.create_entities(mask, ids));
        const archetype *owner = find_archetype(world, mask);
        REQUIRE(owner != nullptr);
        const u32 capacity = owner->layout().capacity;
        CHECK(owner->entity_count() == 600);
        CHECK(owner->chunk_count() == (600 + capacity - 1) / capacity);

        // Remove from the middle of the first chunk: the archetype's last row fills the hole.
        for (u32 index = 10; index < 20; ++index) {
            CONJURE_REQUIRE_OK(world.destroy_entity(ids[index]));
        }
        CHECK(owner->entity_count() == 590);
        u32 total = 0;
        for (u32 chunkIndex = 0; chunkIndex < owner->chunk_count(); ++chunkIndex) {
            const u32 count = owner->chunk_entity_count(chunkIndex);
            total += count;
            if (chunkIndex + 1 < owner->chunk_count() && owner->chunk_entity_count(chunkIndex + 1) > 0) {
                CHECK(count == capacity);
            }
        }
        CHECK(total == 590);
        CONJURE_CHECK_OK(world.validate());

        // Every remaining entity is still exactly once in the rows.
        std::vector<entity> rows = {};
        for (u32 chunkIndex = 0; chunkIndex < owner->chunk_count(); ++chunkIndex) {
            const std::span<const entity> chunkRows = owner->chunk_entities(chunkIndex);
            rows.insert(rows.end(), chunkRows.begin(), chunkRows.end());
        }
        CHECK(rows.size() == 590);
        for (u32 index = 0; index < ids.size(); ++index) {
            const bool destroyed = index >= 10 && index < 20;
            const bool present = std::find(rows.begin(), rows.end(), ids[index]) != rows.end();
            CHECK(present != destroyed);
        }
    }

    TEST_CASE("archetype - moved entity record updated") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const component_mask mask = fixture.mask<position>();
        std::vector<entity> ids(5);
        CONJURE_REQUIRE_OK(world.create_entities(mask, ids));
        for (u32 index = 0; index < ids.size(); ++index) {
            CONJURE_REQUIRE_OK(world.set(ids[index], position{static_cast<f32>(index), 0.0f, 0.0f}));
        }

        // Destroying row 1 moves the last entity (ids[4]) into row 1; its data must follow.
        CONJURE_REQUIRE_OK(world.destroy_entity(ids[1]));
        const position *moved = world.get<position>(ids[4]);
        REQUIRE(moved != nullptr);
        CHECK(moved->x == 4.0f);
        const archetype *owner = find_archetype(world, mask);
        REQUIRE(owner != nullptr);
        CHECK(owner->chunk_entities(0)[1] == ids[4]);
        CHECK(world.is_alive(ids[4]));
        CHECK_FALSE(world.is_alive(ids[1]));
        CONJURE_CHECK_OK(world.validate());
    }

    TEST_CASE("archetype - one spare chunk retained") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const component_mask mask = fixture.mask<velocity>();
        std::vector<entity> ids(3000);
        CONJURE_REQUIRE_OK(world.create_entities(mask, ids));
        const archetype *owner = find_archetype(world, mask);
        REQUIRE(owner != nullptr);
        CHECK(owner->chunk_count() > 2);

        for (const entity id : ids) {
            CONJURE_REQUIRE_OK(world.destroy_entity(id));
        }
        CHECK(owner->entity_count() == 0);
        CHECK(owner->chunk_count() == 1);
        CHECK(owner->chunk_entity_count(0) == 0);
        CHECK(world.stats().chunks >= 1);

        // The spare is reused by the next creation without a new allocation.
        const u32 chunksBefore = world.stats().chunks;
        const core::result<entity> reused = world.create_entity(mask);
        CONJURE_REQUIRE_OK(reused);
        CHECK(world.stats().chunks == chunksBefore);
        CHECK(owner->chunk_entity_count(0) == 1);
        CONJURE_CHECK_OK(world.validate());
    }

    TEST_CASE("archetype - drained chunk metadata is parked and reused without allocating") {
        // Only one drained chunk stays in the archetype with its block; the metadata of every further
        // drained chunk is parked without a block and re-attached before a new chunk object is made, so
        // destroy/create churn over several chunks allocates nothing once it has happened once.
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const component_mask mask = fixture.mask<velocity>();
        const archetype *owner = nullptr;
        std::vector<entity> ids(3000);
        CONJURE_REQUIRE_OK(world.create_entities(mask, ids));
        owner = find_archetype(world, mask);
        REQUIRE(owner != nullptr);
        const u32 peakChunks = owner->chunk_count();
        CHECK(peakChunks > 2);
        CHECK(owner->spare_chunk_count() == 0);

        for (const entity id : ids) {
            CONJURE_REQUIRE_OK(world.destroy_entity(id));
        }
        CHECK(owner->chunk_count() == 1);
        CHECK(owner->spare_chunk_count() == peakChunks - 1);
        CONJURE_CHECK_OK(world.validate());

        // Growing back re-attaches the parked metadata to pool blocks: no chunk object is allocated.
        CONJURE_REQUIRE_OK(world.create_entities(mask, ids));
        CHECK(owner->chunk_count() == peakChunks);
        CHECK(owner->spare_chunk_count() == 0);
        CONJURE_CHECK_OK(world.validate());
        for (const entity id : ids) {
            CONJURE_REQUIRE_OK(world.destroy_entity(id));
        }
        CHECK(owner->spare_chunk_count() == peakChunks - 1);
        bool regrown = false;
        CONJURE_CHECK_NO_ALLOCATIONS(regrown = world.create_entities(mask, ids).has_value());
        CHECK(regrown);
        CHECK(owner->entity_count() == 3000);
        CHECK(owner->spare_chunk_count() == 0);
        CONJURE_CHECK_OK(world.validate());
    }

    TEST_CASE("regression - review 1 spare chunk is filled after a partial predecessor") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const component_mask mask = fixture.mask<position>();
        const core::result<entity> probe = world.create_entity(mask);
        CONJURE_REQUIRE_OK(probe);
        const archetype *owner = find_archetype(world, mask);
        REQUIRE(owner != nullptr);
        const u32 capacity = owner->layout().capacity;
        CONJURE_REQUIRE_OK(world.destroy_entity(probe.value()));

        // capacity + 1 rows: a full chunk and one row in a second chunk.
        std::vector<entity> ids(capacity + 1);
        CONJURE_REQUIRE_OK(world.create_entities(mask, ids));
        REQUIRE(owner->chunk_count() == 2);

        // Draining the second chunk keeps it as the spare; removing from the first makes that partial.
        CONJURE_REQUIRE_OK(world.destroy_entity(ids[capacity]));
        CONJURE_REQUIRE_OK(world.destroy_entity(ids[0]));
        CHECK(owner->chunk_count() == 2);
        CHECK(owner->chunk_entity_count(0) == capacity - 1);
        CHECK(owner->chunk_entity_count(1) == 0);
        CONJURE_CHECK_OK(world.validate());

        // The next row fills the partial chunk, not the spare: only the last used chunk may be partial.
        const core::result<entity> created = world.create_entity(mask);
        CONJURE_REQUIRE_OK(created);
        CHECK(owner->chunk_entity_count(0) == capacity);
        CHECK(owner->chunk_entity_count(1) == 0);
        CHECK(owner->chunk_count() == 2);
        CONJURE_CHECK_OK(world.validate());

        // Then the spare is used, and the archetype stays packed through further churn.
        const core::result<entity> overflow = world.create_entity(mask);
        CONJURE_REQUIRE_OK(overflow);
        CHECK(owner->chunk_entity_count(1) == 1);
        CHECK(owner->chunk_count() == 2);
        CONJURE_REQUIRE_OK(world.destroy_entity(ids[5]));
        CHECK(owner->chunk_entity_count(0) == capacity);
        CHECK(owner->chunk_entity_count(1) == 0);
        CONJURE_CHECK_OK(world.validate());
    }

    TEST_CASE("archetype - edge cache resolves same archetype") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> first = world.create_entity(fixture.mask<position>());
        const core::result<entity> second = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(first);
        CONJURE_REQUIRE_OK(second);
        const usize before = world.archetypes().size();

        CONJURE_REQUIRE_OK(world.add<velocity>(first.value()));
        CHECK(world.archetypes().size() == before + 1);
        CONJURE_REQUIRE_OK(world.add<velocity>(second.value()));
        CHECK(world.archetypes().size() == before + 1);

        // Removing goes back to the existing archetype, through the cached edge.
        CONJURE_REQUIRE_OK(world.remove<velocity>(first.value()));
        CHECK(world.archetypes().size() == before + 1);
        CHECK(world.mask_of(first.value()) == fixture.mask<position>());
        CHECK(world.mask_of(second.value()) == fixture.mask<position, velocity>());

        // The same mask built on another path lands in the same archetype object.
        const core::result<entity> third = world.create_entity(fixture.mask<velocity, position>());
        CONJURE_REQUIRE_OK(third);
        CHECK(world.archetypes().size() == before + 1);
        CHECK(find_archetype(world, fixture.mask<position, velocity>())->entity_count() == 2);
    }

    TEST_CASE("archetype - non trivial lifecycle counts balance across moves") {
        tracked::reset_counters();
        {
            ecs_fixture fixture = {};
            world &world = fixture.world;
            std::vector<entity> ids(300);
            CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<tracked>(), ids));
            for (u32 index = 0; index < ids.size(); ++index) {
                CONJURE_REQUIRE_OK(world.set(ids[index], tracked(static_cast<i32>(index))));
            }
            // Move every other entity through two archetypes and back.
            for (u32 index = 0; index < ids.size(); index += 2) {
                CONJURE_REQUIRE_OK(world.add<position>(ids[index]));
                CONJURE_REQUIRE_OK(world.add<name_component>(ids[index], name_component{"n"}));
                CONJURE_REQUIRE_OK(world.remove<position>(ids[index]));
            }
            for (u32 index = 0; index < ids.size(); ++index) {
                const tracked *value = world.get<tracked>(ids[index]);
                REQUIRE(value != nullptr);
                CHECK(value->value == static_cast<i32>(index));
            }
            CHECK(tracked::live() == 300);
            for (u32 index = 0; index < ids.size(); index += 3) {
                CONJURE_REQUIRE_OK(world.destroy_entity(ids[index]));
            }
            CHECK(tracked::live() == 200);
            CONJURE_CHECK_OK(world.validate());
        }
        CHECK(tracked::live() == 0);
        CHECK(tracked::sConstructed > 0);
    }

    TEST_CASE("archetype - read accessors tolerate out of range chunks") {
        ecs_fixture fixture = {};
        const core::result<entity> created = fixture.world.create_entity(fixture.mask<position, player_tag>());
        CONJURE_REQUIRE_OK(created);
        const archetype *owner = find_archetype(fixture.world, fixture.mask<position, player_tag>());
        REQUIRE(owner != nullptr);
        CHECK(owner->has(fixture.id<player_tag>()));
        CHECK_FALSE(owner->has(fixture.id<velocity>()));
        CHECK(owner->chunk_entity_count(7) == 0);
        CHECK(owner->chunk_entities(7).empty());
        CHECK(owner->chunk_column(7, fixture.id<position>()) == nullptr);
        CHECK(owner->chunk_column(0, fixture.id<player_tag>()) == nullptr);
        CHECK(owner->chunk_column(0, fixture.id<position>()) != nullptr);
        CHECK(owner->chunk_column_version(0, fixture.id<position>()) > 0);
        CHECK(owner->chunk_column_version(3, fixture.id<position>()) == 0);
        CHECK(owner->index() < fixture.world.archetypes().size());
        CHECK(fixture.world.archetypes()[owner->index()] == owner);
    }
}
