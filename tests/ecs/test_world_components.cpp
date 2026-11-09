#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/world.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

TEST_SUITE("ecs/world_components") {

    TEST_CASE("world - add get has remove") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity();
        CONJURE_REQUIRE_OK(created);
        const entity id = created.value();

        CHECK_FALSE(world.has<position>(id));
        const core::result<position *> added = world.add<position>(id, position{1.0f, 2.0f, 3.0f});
        CONJURE_REQUIRE_OK(added);
        REQUIRE(added.value() != nullptr);
        CHECK(added.value()->y == 2.0f);
        CHECK(world.has<position>(id));

        position *mutableValue = world.get<position>(id);
        REQUIRE(mutableValue != nullptr);
        CHECK(mutableValue == added.value());
        mutableValue->z = 9.0f;
        const ecs::world &constWorld = world;
        const position *constValue = constWorld.get<position>(id);
        REQUIRE(constValue != nullptr);
        CHECK(constValue->z == 9.0f);

        CONJURE_REQUIRE_OK(world.remove<position>(id));
        CHECK_FALSE(world.has<position>(id));
        CHECK(world.get<position>(id) == nullptr);
        CHECK(world.is_alive(id));
    }

    TEST_CASE("world - add non trivial and default values") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity();
        CONJURE_REQUIRE_OK(created);
        const entity id = created.value();

        const core::result<name_component *> named = world.add<name_component>(id, name_component{"hero"});
        CONJURE_REQUIRE_OK(named);
        CHECK(named.value()->value == "hero");

        const core::result<health *> defaults = world.add<health>(id);
        CONJURE_REQUIRE_OK(defaults);
        CHECK(defaults.value()->value == 100);
        CHECK(defaults.value()->maximum == 100);

        const core::result<entity> viaMask = world.create_entity(fixture.mask<health, name_component>());
        CONJURE_REQUIRE_OK(viaMask);
        CHECK(world.get<health>(viaMask.value())->value == 100);
        CHECK(world.get<name_component>(viaMask.value())->value.empty());
    }

    TEST_CASE("world - add twice is COMPONENT_ALREADY_PRESENT") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(created);
        CONJURE_CHECK_ERROR(world.add<position>(created.value()), ecs_error::COMPONENT_ALREADY_PRESENT);
        const component_id positionId = fixture.id<position>();
        CONJURE_CHECK_ERROR(world.add_raw(created.value(), positionId), ecs_error::COMPONENT_ALREADY_PRESENT);
        CONJURE_CHECK_ERROR(world.add_raw(created.value(), component_id{700}), ecs_error::UNKNOWN_COMPONENT);
        CONJURE_CHECK_ERROR(world.add_raw(created.value(), component_id{}), ecs_error::UNKNOWN_COMPONENT);
        CHECK(world.mask_of(created.value()) == fixture.mask<position>());
    }

    TEST_CASE("world - remove missing is COMPONENT_NOT_PRESENT") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity();
        CONJURE_REQUIRE_OK(created);
        CONJURE_CHECK_ERROR(world.remove<position>(created.value()), ecs_error::COMPONENT_NOT_PRESENT);
        const component_id velocityId = fixture.id<velocity>();
        CONJURE_CHECK_ERROR(world.remove_raw(created.value(), velocityId), ecs_error::COMPONENT_NOT_PRESENT);
        CONJURE_CHECK_ERROR(world.remove_raw(created.value(), component_id{700}), ecs_error::UNKNOWN_COMPONENT);
        CONJURE_CHECK_ERROR(world.set(created.value(), position{}), ecs_error::COMPONENT_NOT_PRESENT);
    }

    TEST_CASE("world - set on existing replaces") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity(fixture.mask<position, name_component>());
        CONJURE_REQUIRE_OK(created);
        const entity id = created.value();
        CONJURE_REQUIRE_OK(world.set(id, position{4.0f, 5.0f, 6.0f}));
        CHECK(*world.get<position>(id) == position{4.0f, 5.0f, 6.0f});
        CONJURE_REQUIRE_OK(world.set(id, name_component{"renamed"}));
        CHECK(world.get<name_component>(id)->value == "renamed");
        CONJURE_REQUIRE_OK(world.set(id, name_component{"again"}));
        CHECK(world.get<name_component>(id)->value == "again");
        CHECK(world.mask_of(id) == fixture.mask<position, name_component>());
    }

    TEST_CASE("world - get on dead returns nullptr") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(created);
        CONJURE_REQUIRE_OK(world.destroy_entity(created.value()));
        CHECK(world.get<position>(created.value()) == nullptr);
        const ecs::world &constWorld = world;
        CHECK(constWorld.get<position>(created.value()) == nullptr);
        CHECK(world.get_raw(created.value(), fixture.id<position>(), false) == nullptr);
        CHECK(constWorld.get_raw(created.value(), fixture.id<position>()) == nullptr);
        CHECK_FALSE(world.has_raw(created.value(), fixture.id<position>()));
        CHECK(world.get<position>(NULL_ENTITY) == nullptr);
    }

    TEST_CASE("world - mask_of") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity(fixture.mask<position, player_tag>());
        CONJURE_REQUIRE_OK(created);
        component_mask expected = fixture.mask<position, player_tag>();
        CHECK(world.mask_of(created.value()) == expected);
        CONJURE_REQUIRE_OK(world.add<velocity>(created.value()));
        expected.set(fixture.id<velocity>());
        CHECK(world.mask_of(created.value()) == expected);
        CONJURE_REQUIRE_OK(world.remove<player_tag>(created.value()));
        expected.clear(fixture.id<player_tag>());
        CHECK(world.mask_of(created.value()) == expected);
        CHECK(world.mask_of(entity{77, 77}).none());
    }

    TEST_CASE("world - remove_from_all empties instance_count") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> withVelocity(300);
        std::vector<entity> withBoth(300);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<velocity>(), withVelocity));
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<velocity, position>(), withBoth));
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<position>()));
        CHECK(world.instance_count(fixture.id<velocity>()) == 600);

        CONJURE_REQUIRE_OK(world.remove_from_all(fixture.id<velocity>()));
        CHECK(world.instance_count(fixture.id<velocity>()) == 0);
        CHECK(world.instance_count(fixture.id<position>()) == 301);
        CHECK(world.entity_count() == 601);
        for (const entity id : withBoth) {
            CHECK(world.mask_of(id) == fixture.mask<position>());
        }
        for (const entity id : withVelocity) {
            CHECK(world.mask_of(id).none());
        }
        CONJURE_REQUIRE_OK(world.remove_from_all(fixture.id<velocity>()));
        CONJURE_CHECK_ERROR(world.remove_from_all(component_id{900}), ecs_error::UNKNOWN_COMPONENT);
    }

    TEST_CASE("world - tags occupy no bytes") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(created);
        const entity id = created.value();
        const core::result<player_tag *> tagged = world.add<player_tag>(id);
        CONJURE_REQUIRE_OK(tagged);
        CHECK(tagged.value() == nullptr);
        CHECK(world.has<player_tag>(id));
        CHECK(world.get_raw(id, fixture.id<player_tag>(), false) == nullptr);

        const archetype *owner = nullptr;
        for (const archetype *candidate : world.archetypes()) {
            if (candidate->mask() == world.mask_of(id)) {
                owner = candidate;
            }
        }
        REQUIRE(owner != nullptr);
        CHECK(owner->layout().column_count() == 1);
        CHECK(owner->layout().row_bytes() == sizeof(entity) + sizeof(position));

        const core::result<void *> rawTag = world.add_raw(id, fixture.id<frozen_tag>());
        CONJURE_REQUIRE_OK(rawTag);
        CHECK(rawTag.value() == nullptr);
        CHECK(world.has<frozen_tag>(id));
        CONJURE_REQUIRE_OK(world.set(id, player_tag{}));
    }

    TEST_CASE("world - pointer stable until structural change") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(created);
        const entity id = created.value();
        position *before = world.get<position>(id);
        REQUIRE(before != nullptr);

        // Non-structural operations keep the pointer.
        CONJURE_REQUIRE_OK(world.set(id, position{1.0f, 1.0f, 1.0f}));
        for (u32 index = 0; index < 50; ++index) {
            CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<velocity>()));
        }
        CHECK(world.get<position>(id) == before);
        CHECK(before->x == 1.0f);

        // A structural change on the entity moves it: the pointer is re-resolved.
        CONJURE_REQUIRE_OK(world.add<velocity>(id));
        position *after = world.get<position>(id);
        REQUIRE(after != nullptr);
        CHECK(after != before);
        CHECK(after->x == 1.0f);
    }

    TEST_CASE("world - raw add copies bytes and raw get reads them") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity();
        CONJURE_REQUIRE_OK(created);
        const entity id = created.value();

        const position value{7.0f, 8.0f, 9.0f};
        const core::result<void *> added = world.add_raw(
          id,
          fixture.id<position>(),
          std::span<const std::byte>(reinterpret_cast<const std::byte *>(&value), sizeof(value))
        );
        CONJURE_REQUIRE_OK(added);
        REQUIRE(added.value() != nullptr);
        CHECK(*world.get<position>(id) == value);

        const name_component named{"raw"};
        const core::result<void *> addedName = world.add_raw(
          id,
          fixture.id<name_component>(),
          std::span<const std::byte>(reinterpret_cast<const std::byte *>(&named), sizeof(named))
        );
        CONJURE_REQUIRE_OK(addedName);
        CHECK(world.get<name_component>(id)->value == "raw");

        // A payload of the wrong size asserts and keeps the default.
        const std::array<std::byte, 3> wrongSize = {};
        testing::expect_assert guard = {};
        const core::result<void *> badSize = world.add_raw(id, fixture.id<velocity>(), wrongSize);
        CONJURE_REQUIRE_OK(badSize);
        CHECK(guard.count() == 1 * ASSERTS_PER_VIOLATION);
        CHECK(*world.get<velocity>(id) == velocity{});
    }
}
