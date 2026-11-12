#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/observer.h"
#include "conjure/ecs/world.h"

#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

TEST_SUITE("ecs/observer") {

    TEST_CASE("observer_id - default is invalid") {
        const observer_id none = {};
        CHECK_FALSE(none.is_valid());
        CHECK(observer_id{0, 1}.is_valid());
        CHECK(observer_id{3, 2} == observer_id{3, 2});
        CHECK_FALSE(observer_id{3, 2} == observer_id{3, 3});
    }

    TEST_CASE("observer - ADDED fires after data exists") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<f32> seen = {};
        const observer_id id = world.observe(
          fixture.id<position>(),
          component_event::ADDED,
          [&](ecs::world &owner, entity target, component_id component, command_buffer &) {
              CHECK(component == fixture.id<position>());
              CHECK(owner.has<position>(target));
              const position *value = owner.get<position>(target);
              REQUIRE(value != nullptr);
              seen.push_back(value->x);
          }
        );
        CHECK(id.is_valid());

        const core::result<entity> created = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(created);
        const core::result<entity> plain = world.create_entity();
        CONJURE_REQUIRE_OK(plain);
        CONJURE_REQUIRE_OK(world.add<position>(plain.value(), position{5.0f, 0.0f, 0.0f}));
        const core::result<entity> other = world.create_entity();
        CONJURE_REQUIRE_OK(other);
        const position raw{6.0f, 0.0f, 0.0f};
        CONJURE_REQUIRE_OK(world.add_raw(
          other.value(),
          fixture.id<position>(),
          std::span<const std::byte>(reinterpret_cast<const std::byte *>(&raw), sizeof(raw))
        ));
        CHECK(seen == std::vector<f32>{0.0f, 5.0f, 6.0f});

        // Adding an unrelated component does not fire.
        CONJURE_REQUIRE_OK(world.add<velocity>(plain.value()));
        CHECK(seen.size() == 3);
    }

    TEST_CASE("observer - REMOVED fires before data is destroyed") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<std::string> seen = {};
        const observer_id id = world.observe(
          fixture.id<name_component>(),
          component_event::REMOVED,
          [&](ecs::world &owner, entity target, component_id, command_buffer &) {
              CHECK(owner.is_alive(target));
              const name_component *value = owner.get<name_component>(target);
              REQUIRE(value != nullptr);
              seen.push_back(value->value);
          }
        );
        CHECK(id.is_valid());

        const core::result<entity> a = world.create_entity();
        const core::result<entity> b = world.create_entity();
        CONJURE_REQUIRE_OK(a);
        CONJURE_REQUIRE_OK(b);
        CONJURE_REQUIRE_OK(world.add<name_component>(a.value(), name_component{"removed"}));
        CONJURE_REQUIRE_OK(world.add<name_component>(b.value(), name_component{"destroyed"}));
        CONJURE_REQUIRE_OK(world.remove<name_component>(a.value()));
        CONJURE_REQUIRE_OK(world.destroy_entity(b.value()));
        CHECK(seen == std::vector<std::string>{"removed", "destroyed"});
    }

    TEST_CASE("observer - observers fire for direct api move and flush") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        u32 added = 0;
        u32 removed = 0;
        CHECK(world.observe(fixture.id<velocity>(), component_event::ADDED,
          [&](ecs::world &, entity, component_id, command_buffer &) {
              ++added;
          }).is_valid());
        CHECK(world.observe(fixture.id<velocity>(), component_event::REMOVED,
          [&](ecs::world &, entity, component_id, command_buffer &) {
              ++removed;
          }).is_valid());

        const core::result<entity> direct = world.create_entity(fixture.mask<velocity>());
        CONJURE_REQUIRE_OK(direct);
        CHECK(added == 1);
        CONJURE_REQUIRE_OK(world.remove<velocity>(direct.value()));
        CHECK(removed == 1);

        command_buffer commands(world);
        const entity deferred = commands.create_entity();
        commands.add<velocity>(deferred);
        commands.add<velocity>(direct.value());
        CHECK(world.flush(commands).applied == 3);
        CHECK(added == 3);
        commands.remove<velocity>(deferred);
        commands.destroy_entity(direct.value());
        CHECK(world.flush(commands).applied == 2);
        CHECK(removed == 3);

        // Only the mask delta fires: a set does not.
        commands.set(deferred, velocity{});
        CHECK(world.flush(commands).skipped == 1);
        CHECK(added == 3);
        CHECK(removed == 3);
    }

    TEST_CASE("observer - observer enqueues deferred destroy chain") {
        ecs_fixture fixture = {};
        world &world = fixture.world;

        // A chain of links: destroying an entity destroys the entity its link points at.
        std::vector<entity> chain(6);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<link>(), chain));
        for (u32 index = 0; index + 1 < chain.size(); ++index) {
            CONJURE_REQUIRE_OK(world.set(chain[index], link{chain[index + 1], NULL_ENTITY}));
        }
        std::vector<entity> order = {};
        const observer_id id = world.observe_destroyed([&](ecs::world &owner, entity target, command_buffer &deferred) {
            order.push_back(target);
            const link *current = owner.get<link>(target);
            if (current != nullptr && !current->target.is_null()) {
                deferred.destroy_entity(current->target);
            }
        });
        CHECK(id.is_valid());

        CONJURE_REQUIRE_OK(world.destroy_entity(chain[0]));
        CHECK(order == chain);
        CHECK(world.entity_count() == 0);
        for (const entity link : chain) {
            CHECK_FALSE(world.is_alive(link));
        }
    }

    TEST_CASE("observer - deferred creations from an ADDED observer are applied") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        CHECK(world.observe(fixture.id<player_tag>(), component_event::ADDED,
          [&](ecs::world &, entity target, component_id, command_buffer &deferred) {
              const entity shadow = deferred.create_entity();
              deferred.add<link>(shadow, link{target, NULL_ENTITY});
          }).is_valid());

        const core::result<entity> player = world.create_entity(fixture.mask<player_tag>());
        CONJURE_REQUIRE_OK(player);
        CHECK(world.entity_count() == 2);
        u32 shadows = 0;
        world.view<const link>().for_each([&](entity, const link &link) {
            CHECK(link.target == player.value());
            ++shadows;
        });
        CHECK(shadows == 1);
    }

    TEST_CASE("observer - observer calling world directly asserts") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        core::result<void> insideDestroy = core::result<void>::success();
        core::result<entity> insideCreate = NULL_ENTITY;
        CHECK(world.observe(fixture.id<position>(), component_event::ADDED,
          [&](ecs::world &owner, entity target, component_id, command_buffer &) {
              insideDestroy = owner.destroy_entity(target);
              insideCreate = owner.create_entity();
          }).is_valid());

        testing::expect_assert guard = {};
        const core::result<entity> created = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(created);
        CHECK(guard.count() == 2 * ASSERTS_PER_VIOLATION);
        CONJURE_CHECK_ERROR(insideDestroy, ecs_error::STRUCTURE_LOCKED);
        CONJURE_CHECK_ERROR(insideCreate, ecs_error::STRUCTURE_LOCKED);
        CHECK(world.is_alive(created.value()));
        CHECK(world.entity_count() == 1);
    }

    TEST_CASE("observer - unobserve stops delivery") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        u32 count = 0;
        const observer_id id = world.observe(fixture.id<position>(), component_event::ADDED,
          [&](ecs::world &, entity, component_id, command_buffer &) {
              ++count;
          });
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<position>()));
        CHECK(count == 1);
        world.unobserve(id);
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<position>()));
        CHECK(count == 1);
        // Stale and bogus ids are ignored.
        world.unobserve(id);
        world.unobserve(observer_id{});
        world.unobserve(observer_id{99, 99});

        // A new observer reuses the slot with a fresh generation and the old id cannot remove it.
        const observer_id fresh = world.observe(fixture.id<position>(), component_event::ADDED,
          [&](ecs::world &, entity, component_id, command_buffer &) {
              count += 10;
          });
        CHECK(fresh.index == id.index);
        CHECK(fresh.generation != id.generation);
        world.unobserve(id);
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<position>()));
        CHECK(count == 11);
    }

    TEST_CASE("observer - an observer may unobserve itself during delivery") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        u32 count = 0;
        observer_id self = {};
        self = world.observe(fixture.id<position>(), component_event::ADDED,
          [&](ecs::world &owner, entity, component_id, command_buffer &) {
              ++count;
              owner.unobserve(self);
          });
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<position>()));
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<position>()));
        CHECK(count == 1);
    }

    TEST_CASE("regression - review 3 registering and removing observers during delivery is safe") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        constexpr u32 LATE_OBSERVERS = 64;
        u32 lateFired = 0;
        u32 otherFired = 0;
        bool registered = false;
        observer_id other = {};
        const observer_id registrar = world.observe(fixture.id<position>(), component_event::ADDED,
          [&](ecs::world &owner, entity, component_id, command_buffer &) {
              if (registered) {
                  return;
              }
              registered = true;
              // Growing the table while this handler runs must not move it from under us.
              for (u32 index = 0; index < LATE_OBSERVERS; ++index) {
                  CHECK(owner.observe(fixture.id<position>(), component_event::ADDED,
                    [&](ecs::world &, entity, component_id, command_buffer &) {
                        ++lateFired;
                    }).is_valid());
              }
              CHECK(owner.observe_destroyed([&](ecs::world &, entity, command_buffer &) {
                  ++lateFired;
              }).is_valid());
              owner.unobserve(other);
          });
        CHECK(registrar.is_valid());
        other = world.observe(fixture.id<position>(), component_event::ADDED,
          [&](ecs::world &, entity, component_id, command_buffer &) {
              ++otherFired;
          });
        CHECK(other.is_valid());

        // The first delivery registers the late observers; they are first invoked by the next one, and
        // the observer removed during delivery never runs.
        const core::result<entity> first = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(first);
        CHECK(registered);
        CHECK(lateFired == 0);
        CHECK(otherFired == 0);
        const core::result<entity> second = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(second);
        CHECK(lateFired == LATE_OBSERVERS);
        CONJURE_REQUIRE_OK(world.destroy_entity(second.value()));
        CHECK(lateFired == LATE_OBSERVERS + 1);
        CHECK(otherFired == 0);
    }

    TEST_CASE("regression - review 9 a move the pool cannot serve fires no REMOVED observer") {
        // One slab of one chunk: the first archetype takes the only block the pool will ever hand out.
        component_registry registry = {};
        register_test_components(registry);
        world_desc desc = {};
        desc.chunksPerSlab = 1;
        desc.maxChunkSlabs = 1;
        world world(registry, desc);
        component_mask mask = {};
        mask.set(registry.id_of<position>());
        mask.set(registry.id_of<velocity>());
        const core::result<entity> created = world.create_entity(mask);
        CONJURE_REQUIRE_OK(created);

        u32 removed = 0;
        CHECK(world.observe(registry.id_of<velocity>(), component_event::REMOVED,
          [&](ecs::world &, entity, component_id, command_buffer &) {
              ++removed;
          }).is_valid());
        CONJURE_CHECK_ERROR(world.remove<velocity>(created.value()), ecs_error::OUT_OF_MEMORY);
        CHECK(removed == 0);
        CHECK(world.has<velocity>(created.value()));
        CHECK(world.mask_of(created.value()) == mask);
        CHECK(world.stats().chunks == 1);
        CONJURE_CHECK_OK(world.validate());
    }

    TEST_CASE("observer - interest tracking follows observe and unobserve") {
        // An event for a component nobody observes skips the slot scan; the interest the table keeps per
        // (event, component) must follow every observe and unobserve.
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const auto counter = [](u32 &count) {
            return [&count](ecs::world &, entity, component_id, command_buffer &) {
                ++count;
            };
        };
        u32 first = 0;
        u32 second = 0;
        u32 removedFired = 0;
        const observer_id firstId = world.observe(fixture.id<health>(), component_event::ADDED, counter(first));
        const observer_id secondId = world.observe(fixture.id<health>(), component_event::ADDED, counter(second));
        CHECK(world.observe(fixture.id<health>(), component_event::REMOVED, counter(removedFired)).is_valid());

        // Unobserved components and the other event fire nothing.
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<position>()));
        CHECK(first == 0);
        CHECK(second == 0);
        const core::result<entity> healthy = world.create_entity(fixture.mask<health>());
        CONJURE_REQUIRE_OK(healthy);
        CHECK(first == 1);
        CHECK(second == 1);
        CHECK(removedFired == 0);

        // One of two observers gone: the interest stays and the other still fires.
        world.unobserve(firstId);
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<health>()));
        CHECK(first == 1);
        CHECK(second == 2);

        // The last ADDED observer gone: ADDED fires nothing, REMOVED still does.
        world.unobserve(secondId);
        const core::result<entity> both = world.create_entity(fixture.mask<position, health>());
        CONJURE_REQUIRE_OK(both);
        CHECK(second == 2);
        CONJURE_REQUIRE_OK(world.remove<health>(both.value()));
        CHECK(removedFired == 1);

        // Observing again restores the interest.
        u32 third = 0;
        CHECK(world.observe(fixture.id<health>(), component_event::ADDED, counter(third)).is_valid());
        CONJURE_REQUIRE_OK(world.add<health>(both.value()));
        CHECK(third == 1);
        CHECK(first == 1);
        CHECK(second == 2);

        // An observer registered by a handler for an earlier bit of one mask fires for a later bit.
        u32 lateVelocity = 0;
        bool registeredLate = false;
        CHECK(world.observe(fixture.id<position>(), component_event::ADDED,
          [&](ecs::world &owner, entity, component_id, command_buffer &) {
              if (registeredLate) {
                  return;
              }
              registeredLate = true;
              CHECK(owner.observe(fixture.id<velocity>(), component_event::ADDED, counter(lateVelocity)).is_valid());
          }).is_valid());
        REQUIRE(fixture.id<position>().value < fixture.id<velocity>().value);
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<position, velocity>()));
        CHECK(registeredLate);
        CHECK(lateVelocity == 1);
    }

    TEST_CASE("observer - add returns null when an ADDED observer destroys the target") {
        // add<T> and add_raw drain the deferred buffer before reading the component back, so an ADDED
        // observer that destroys the entity leaves a completed add with a null result.
        ecs_fixture fixture = {};
        world &world = fixture.world;
        CHECK(world.observe(fixture.id<health>(), component_event::ADDED,
          [](ecs::world &, entity target, component_id, command_buffer &deferred) {
              deferred.destroy_entity(target);
          }).is_valid());

        const core::result<entity> typed = world.create_entity();
        CONJURE_REQUIRE_OK(typed);
        const core::result<health *> added = world.add<health>(typed.value());
        REQUIRE(added.has_value());
        CHECK(added.value() == nullptr);
        CHECK_FALSE(world.is_alive(typed.value()));

        const core::result<entity> untyped = world.create_entity();
        CONJURE_REQUIRE_OK(untyped);
        const core::result<void *> raw = world.add_raw(untyped.value(), fixture.id<health>());
        REQUIRE(raw.has_value());
        CHECK(raw.value() == nullptr);
        CHECK_FALSE(world.is_alive(untyped.value()));
        CHECK(world.entity_count() == 0);
    }

    TEST_CASE("observer - unregistered component, invalid event or empty callable is refused") {
        ecs_fixture fixture = {};
        testing::expect_assert guard = {};
        CHECK_FALSE(fixture.world.observe(component_id{800}, component_event::ADDED,
          [](ecs::world &, entity, component_id, command_buffer &) {}).is_valid());
        const component_id positionId = fixture.id<position>();
        CHECK_FALSE(fixture.world.observe(positionId, component_event::ADDED, component_observer_fn()).is_valid());
        CHECK_FALSE(fixture.world.observe_destroyed(entity_observer_fn()).is_valid());

        // An event past REMOVED (an integer cast) is refused before the observer table indexes by it.
        u32 fired = 0;
        const u8 pastRemoved = static_cast<u8>(static_cast<u8>(component_event::REMOVED) + 1);
        const component_event invalidEvent = static_cast<component_event>(pastRemoved);
        CHECK_FALSE(fixture.world.observe(positionId, invalidEvent,
          [&fired](ecs::world &, entity, component_id, command_buffer &) {
              ++fired;
          }).is_valid());
        CHECK(guard.count() == 4 * ASSERTS_PER_VIOLATION);
        CONJURE_REQUIRE_OK(fixture.world.create_entity(fixture.mask<position>()));
        CHECK(fired == 0);
    }
}
