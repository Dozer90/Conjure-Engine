#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/testing/allocation_scope.h"

#include "conjure/core/reflect/reflect.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/world.h"
#include "conjure/ecs/world_hash.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

namespace hash_test {
    /// <summary>
    /// A trivially copyable component with three padding bytes between its fields.
    /// </summary>
    struct padded_component {
        u8 flag = 0;
        u32 value = 0;
    };
}

CONJURE_REFLECT_STRUCT(hash_test::padded_component, "test.padded", CONJURE_FIELD(flag), CONJURE_FIELD(value));
CONJURE_COMPONENT(hash_test::padded_component, "test.padded");

namespace {
    /// <summary>
    /// Builds the same content in two worlds through different archetype creation orders. Both worlds
    /// run the same create/destroy/clear sequence first so their entity ids match exactly; only the
    /// masks of the scratch entities differ, which is what orders the archetypes differently.
    /// </summary>
    void populate(ecs_fixture &fixture, bool velocityFirst) {
        world &world = fixture.world;
        const component_mask firstScratch = velocityFirst ? fixture.mask<velocity>() : fixture.mask<position>();
        const component_mask secondScratch = fixture.mask<position, velocity>();
        const core::result<entity> scratch = world.create_entity(firstScratch);
        REQUIRE(scratch.has_value());
        REQUIRE(world.destroy_entity(scratch.value()).has_value());
        const core::result<entity> scratch2 = world.create_entity(secondScratch);
        REQUIRE(scratch2.has_value());
        REQUIRE(world.destroy_entity(scratch2.value()).has_value());
        world.clear();
        for (u32 index = 0; index < 100; ++index) {
            const core::result<entity> created = world.create_entity(fixture.mask<position>());
            REQUIRE(created.has_value());
            REQUIRE(world.set(created.value(), position{static_cast<f32>(index), 0.0f, 0.0f}).has_value());
        }
        for (u32 index = 0; index < 50; ++index) {
            const core::result<entity> created = world.create_entity(fixture.mask<position, velocity>());
            REQUIRE(created.has_value());
            REQUIRE(world.set(created.value(), velocity{1.0f, static_cast<f32>(index), 0.0f}).has_value());
        }
        for (u32 index = 0; index < 20; ++index) {
            const core::result<entity> created = world.create_entity(fixture.mask<velocity, player_tag>());
            REQUIRE(created.has_value());
        }
    }
}

TEST_SUITE("ecs/world_hash") {

    TEST_CASE("world_hash - empty world hashes to a constant and content changes it") {
        ecs_fixture fixture = {};
        const u64 empty = world_hash(fixture.world);
        ecs_fixture other = {};
        CHECK(world_hash(other.world) == empty);
        CONJURE_REQUIRE_OK(fixture.world.create_entity());
        CHECK(world_hash(fixture.world) != empty);
    }

    TEST_CASE("world_hash - independent of archetype creation order") {
        ecs_fixture a = {};
        ecs_fixture b = {};
        populate(a, false);
        populate(b, true);
        const bool layoutsDiffer = a.world.archetypes().size() != b.world.archetypes().size() ||
          a.world.archetypes()[1]->mask() != b.world.archetypes()[1]->mask();
        CHECK(layoutsDiffer);
        CHECK(world_hash(a.world) == world_hash(b.world));
    }

    TEST_CASE("world_hash - one float bit flips it") {
        ecs_fixture fixture = {};
        populate(fixture, false);
        const u64 before = world_hash(fixture.world);
        CHECK(world_hash(fixture.world) == before);

        position *target = nullptr;
        fixture.world.view<position>().for_each([&](entity, position &value) {
            if (target == nullptr) {
                target = &value;
            }
        });
        REQUIRE(target != nullptr);
        const u32 bits = std::bit_cast<u32>(target->z);
        target->z = std::bit_cast<f32>(bits ^ 1u);
        CHECK(world_hash(fixture.world) != before);
        target->z = std::bit_cast<f32>(bits);
        CHECK(world_hash(fixture.world) == before);

        // Negative zero differs from zero in the bits, so it differs in the hash.
        target->z = -0.0f;
        CHECK(world_hash(fixture.world) != before);
    }

    TEST_CASE("world_hash - entity ids and tags take part") {
        ecs_fixture a = {};
        ecs_fixture b = {};
        CONJURE_REQUIRE_OK(a.world.create_entity(a.mask<position>()));
        CONJURE_REQUIRE_OK(b.world.create_entity(b.mask<position>()));
        CHECK(world_hash(a.world) == world_hash(b.world));

        // Same data, different id (generation 2 instead of 1).
        const core::result<entity> extra = b.world.create_entity(b.mask<position>());
        CONJURE_REQUIRE_OK(extra);
        CONJURE_REQUIRE_OK(b.world.destroy_entity(extra.value()));
        CONJURE_REQUIRE_OK(b.world.create_entity(b.mask<position>()));
        const core::result<entity> extraA = a.world.create_entity(a.mask<position>());
        CONJURE_REQUIRE_OK(extraA);
        CHECK(a.world.entity_count() == b.world.entity_count());
        CHECK(world_hash(a.world) != world_hash(b.world));

        // A tag changes the archetype and therefore the hash.
        ecs_fixture c = {};
        ecs_fixture d = {};
        CONJURE_REQUIRE_OK(c.world.create_entity(c.mask<position>()));
        CONJURE_REQUIRE_OK(d.world.create_entity(d.mask<position, player_tag>()));
        CHECK(world_hash(c.world) != world_hash(d.world));
    }

    TEST_CASE("world_hash - non trivial components hash their content") {
        ecs_fixture a = {};
        ecs_fixture b = {};
        const core::result<entity> first = a.world.create_entity(a.mask<name_component>());
        const core::result<entity> second = b.world.create_entity(b.mask<name_component>());
        CONJURE_REQUIRE_OK(first);
        CONJURE_REQUIRE_OK(second);
        CONJURE_REQUIRE_OK(a.world.set(first.value(), name_component{"same"}));
        CONJURE_REQUIRE_OK(b.world.set(second.value(), name_component{"same"}));
        CHECK(world_hash(a.world) == world_hash(b.world));
        CONJURE_REQUIRE_OK(b.world.set(second.value(), name_component{"other"}));
        CHECK(world_hash(a.world) != world_hash(b.world));
    }

    TEST_CASE("regression - review 13 padding bytes do not affect the hash") {
        ecs_fixture a = {};
        ecs_fixture b = {};
        CONJURE_REQUIRE_OK(a.registry.register_component<hash_test::padded_component>());
        CONJURE_REQUIRE_OK(b.registry.register_component<hash_test::padded_component>());
        const component_id paddedA = a.id<hash_test::padded_component>();
        const component_id paddedB = b.id<hash_test::padded_component>();
        const core::result<entity> first = a.world.create_entity();
        const core::result<entity> second = b.world.create_entity();
        CONJURE_REQUIRE_OK(first);
        CONJURE_REQUIRE_OK(second);

        // Same fields, different padding bytes: the raw copy brings the padding into the cell.
        std::array<std::byte, sizeof(hash_test::padded_component)> dirty = {};
        dirty.fill(std::byte{0xFF});
        std::array<std::byte, sizeof(hash_test::padded_component)> clean = {};
        const u8 flag = 1;
        const u32 value = 7;
        for (std::array<std::byte, sizeof(hash_test::padded_component)> *image : {&dirty, &clean}) {
            std::memcpy(image->data() + offsetof(hash_test::padded_component, flag), &flag, sizeof(flag));
            std::memcpy(image->data() + offsetof(hash_test::padded_component, value), &value, sizeof(value));
        }
        CONJURE_REQUIRE_OK(a.world.add_raw(first.value(), paddedA, dirty));
        CONJURE_REQUIRE_OK(b.world.add_raw(second.value(), paddedB, clean));
        CHECK(std::memcmp(a.world.get_raw(first.value(), paddedA), b.world.get_raw(second.value(), paddedB), 8) != 0);
        CHECK(world_hash(a.world) == world_hash(b.world));

        // The fields themselves still count.
        hash_test::padded_component changed = {};
        changed.flag = 1;
        changed.value = 8;
        CONJURE_REQUIRE_OK(b.world.set(second.value(), changed));
        CHECK(world_hash(a.world) != world_hash(b.world));
    }

    TEST_CASE("world_hash - allocation free for trivial worlds") {
        ecs_fixture fixture = {};
        populate(fixture, false);
        static_cast<void>(world_hash(fixture.world));
        testing::allocation_scope allocations = {};
        const u64 hash = world_hash(fixture.world);
        const u64 allocated = allocations.allocation_count();
        CHECK(allocated == 0);
        CHECK(hash != 0);
    }
}
