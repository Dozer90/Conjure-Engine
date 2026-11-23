#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/entity_handle.h"
#include "conjure/ecs/world.h"

#include <string>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

TEST_SUITE("ecs/entity_handle") {

    TEST_CASE("entity_handle - handle mirrors world") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity();
        CONJURE_REQUIRE_OK(created);
        entity_handle handle = world.handle(created.value());
        CHECK(handle.id() == created.value());
        CHECK(handle.is_alive());
        CHECK_FALSE(handle.has<position>());

        const core::result<component_ref<position>> added = handle.add<position>(position{1.0f, 2.0f, 3.0f});
        CONJURE_REQUIRE_OK(added);
        CHECK(added.value().is_valid());
        CHECK(added.value().read()->y == 2.0f);
        CHECK(handle.has<position>());
        CHECK(world.has<position>(created.value()));
        CONJURE_CHECK_ERROR(handle.add<position>(), ecs_error::COMPONENT_ALREADY_PRESENT);

        const core::result<component_ref<name_component>> named = handle.add<name_component>(name_component{"h"});
        CONJURE_REQUIRE_OK(named);
        CHECK(named.value().read()->value == "h");
        CONJURE_REQUIRE_OK(handle.remove<name_component>());
        CHECK_FALSE(handle.has<name_component>());
        CONJURE_CHECK_ERROR(handle.remove<name_component>(), ecs_error::COMPONENT_NOT_PRESENT);

        CONJURE_REQUIRE_OK(handle.destroy());
        CHECK_FALSE(handle.is_alive());
        CHECK_FALSE(world.is_alive(created.value()));
        CONJURE_CHECK_ERROR(handle.destroy(), ecs_error::ENTITY_NOT_ALIVE);
        CHECK(handle == world.handle(created.value()));
    }

    TEST_CASE("entity_handle - dead handle safe for get has is_alive") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(created);
        entity_handle handle = world.handle(created.value());
        component_ref<position> reference = handle.component<position>();
        CHECK(reference.is_valid());
        CONJURE_REQUIRE_OK(world.destroy_entity(created.value()));

        CHECK_FALSE(handle.is_alive());
        CHECK_FALSE(handle.has<position>());
        CHECK_FALSE(reference.is_valid());
        CHECK(reference.read() == nullptr);
        CHECK(reference.write() == nullptr);
        CONJURE_CHECK_ERROR(reference.set(position{}), ecs_error::ENTITY_NOT_ALIVE);
        CONJURE_CHECK_ERROR(handle.add<velocity>(), ecs_error::ENTITY_NOT_ALIVE);
        CONJURE_CHECK_ERROR(handle.remove<position>(), ecs_error::ENTITY_NOT_ALIVE);
        CHECK(reference.owner() == created.value());

        // Handles to nothing behave the same way.
        const entity_handle empty = {};
        CHECK_FALSE(empty.is_alive());
        CHECK_FALSE(empty.has<position>());
        CHECK(empty.owner() == nullptr);
        CHECK(empty.id() == NULL_ENTITY);
        entity_handle emptyMutable = {};
        CONJURE_CHECK_ERROR(emptyMutable.destroy(), ecs_error::ENTITY_NOT_ALIVE);
        CONJURE_CHECK_ERROR(emptyMutable.add<position>(), ecs_error::ENTITY_NOT_ALIVE);
        CONJURE_CHECK_ERROR(emptyMutable.remove<position>(), ecs_error::ENTITY_NOT_ALIVE);
        component_ref<position> noRef = empty.component<position>();
        CHECK_FALSE(noRef.is_valid());
        CHECK(noRef.read() == nullptr);
        CHECK(noRef.write() == nullptr);
        CONJURE_CHECK_ERROR(noRef.set(position{}), ecs_error::ENTITY_NOT_ALIVE);
    }

    TEST_CASE("component_ref - re-resolves after move") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(created);
        component_ref<position> reference = world.handle(created.value()).component<position>();
        reference.write()->x = 5.0f;
        const position *before = reference.read();

        // Moving the entity to another archetype relocates the component; the reference follows.
        CONJURE_REQUIRE_OK(world.add<velocity>(created.value()));
        CONJURE_REQUIRE_OK(world.add<health>(created.value()));
        CHECK(reference.is_valid());
        CHECK(reference.read() != before);
        CHECK(reference.read()->x == 5.0f);

        // Removing the component invalidates it; adding it back revalidates it.
        CONJURE_REQUIRE_OK(world.remove<position>(created.value()));
        CHECK_FALSE(reference.is_valid());
        CHECK(reference.read() == nullptr);
        CONJURE_CHECK_ERROR(reference.set(position{}), ecs_error::COMPONENT_NOT_PRESENT);
        CONJURE_REQUIRE_OK(world.add<position>(created.value(), position{9.0f, 0.0f, 0.0f}));
        CHECK(reference.is_valid());
        CHECK(reference.read()->x == 9.0f);
        CONJURE_REQUIRE_OK(reference.set(position{1.0f, 1.0f, 1.0f}));
        CHECK(reference.read()->z == 1.0f);
    }

    TEST_CASE("component_ref - the former name valid() answers as is_valid() for one wave") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(created);
        component_ref<position> reference = {};
        bool expected = false;

        SUBCASE("a reference to nothing") {
            expected = false;
        }
        SUBCASE("a reference to a present component") {
            reference = world.handle(created.value()).component<position>();
            expected = true;
        }
        SUBCASE("a reference to a removed component") {
            reference = world.handle(created.value()).component<position>();
            CONJURE_REQUIRE_OK(world.remove<position>(created.value()));
            expected = false;
        }

        CONJURE_TEST_DEPRECATED_BEGIN
        const bool former = reference.valid();
        CONJURE_TEST_DEPRECATED_END
        CHECK(reference.is_valid() == expected);
        CHECK(former == expected);
    }

    TEST_CASE("component_ref - write stamps") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(created);
        world.bump_version();
        const u64 since = world.version();
        world.bump_version();
        component_ref<position> reference = world.handle(created.value()).component<position>();
        const auto changed = [&]() {
            return world.view<const position>().changed<position>(since).entity_count();
        };
        CHECK(reference.read() != nullptr);
        CHECK(changed() == 0);
        CHECK(reference.write() != nullptr);
        CHECK(changed() == 1);
    }
}
