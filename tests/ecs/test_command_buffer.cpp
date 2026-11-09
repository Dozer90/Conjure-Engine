#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/memory_guard.h"

#include "conjure/core/log.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/world.h"

#include <cstddef>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

TEST_SUITE("ecs/command_buffer") {

    TEST_CASE("command_buffer - deferred create alive after flush not before") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        command_buffer commands(world);
        CHECK(commands.empty());
        CHECK(&commands.target() == &world);

        const entity created = commands.create_entity();
        CHECK_FALSE(created.is_null());
        CHECK_FALSE(world.is_alive(created));
        CHECK(world.entity_count() == 0);
        CHECK(commands.command_count() == 1);
        CHECK_FALSE(commands.empty());

        const flush_report report = world.flush(commands);
        CHECK(report.applied == 1);
        CHECK(report.skipped == 0);
        CHECK(world.is_alive(created));
        CHECK(world.entity_count() == 1);
        CHECK(world.mask_of(created).none());
        CHECK(commands.empty());
    }

    TEST_CASE("command_buffer - create add set on same id") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        command_buffer commands(world);
        const entity created = commands.create_entity();
        commands.add<position>(created, position{1.0f, 2.0f, 3.0f});
        commands.add<name_component>(created, name_component{"spawned"});
        commands.set<position>(created, position{4.0f, 5.0f, 6.0f});
        commands.add<player_tag>(created);
        CHECK(commands.command_count() == 5);

        const flush_report report = world.flush(commands);
        CHECK(report.applied == 5);
        CHECK(report.skipped == 0);
        REQUIRE(world.is_alive(created));
        CHECK(world.mask_of(created) == fixture.mask<position, name_component, player_tag>());
        CHECK(*world.get<position>(created) == position{4.0f, 5.0f, 6.0f});
        CHECK(world.get<name_component>(created)->value == "spawned");
        // One archetype for the final mask, none for the intermediate steps.
        CHECK(world.archetypes().size() == 2);
    }

    TEST_CASE("command_buffer - multiple add remove cause one move") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity(fixture.mask<position, health>());
        CONJURE_REQUIRE_OK(created);
        const entity id = created.value();
        CONJURE_REQUIRE_OK(world.set(id, position{9.0f, 9.0f, 9.0f}));
        const usize archetypesBefore = world.archetypes().size();

        u32 moves = 0;
        CHECK(world.observe(fixture.id<velocity>(), component_event::ADDED,
          [&](ecs::world &, entity, component_id, command_buffer &) {
              ++moves;
          }).is_valid());

        command_buffer commands(world);
        commands.add<velocity>(id, velocity{1.0f, 0.0f, 0.0f});
        commands.add<name_component>(id, name_component{"x"});
        commands.remove<health>(id);
        commands.add<player_tag>(id);
        CHECK(world.flush(commands).applied == 4);

        CHECK(world.mask_of(id) == fixture.mask<position, velocity, name_component, player_tag>());
        CHECK(*world.get<position>(id) == position{9.0f, 9.0f, 9.0f});
        CHECK(world.get<velocity>(id)->x == 1.0f);
        CHECK(world.get<name_component>(id)->value == "x");
        CHECK(world.archetypes().size() == archetypesBefore + 1);
        CHECK(moves == 1);
    }

    TEST_CASE("command_buffer - add then remove cancels") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity();
        CONJURE_REQUIRE_OK(created);
        tracked::reset_counters();
        command_buffer commands(world);
        commands.add<tracked>(created.value(), tracked(5));
        commands.remove<tracked>(created.value());
        CHECK(world.flush(commands).applied == 2);
        CHECK(world.mask_of(created.value()).none());
        CHECK_FALSE(world.has<tracked>(created.value()));
        CHECK(tracked::live() == 0);
        CHECK(world.archetypes().size() == 1);
    }

    TEST_CASE("command_buffer - remove then add keeps new payload") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity(fixture.mask<position>());
        CONJURE_REQUIRE_OK(created);
        CONJURE_REQUIRE_OK(world.set(created.value(), position{1.0f, 1.0f, 1.0f}));
        command_buffer commands(world);
        commands.remove<position>(created.value());
        commands.add<position>(created.value(), position{2.0f, 2.0f, 2.0f});
        CHECK(world.flush(commands).applied == 2);
        CHECK(world.has<position>(created.value()));
        CHECK(*world.get<position>(created.value()) == position{2.0f, 2.0f, 2.0f});
    }

    TEST_CASE("command_buffer - destroy wins over later add") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity();
        CONJURE_REQUIRE_OK(created);
        tracked::reset_counters();
        command_buffer commands(world);
        commands.destroy_entity(created.value());
        commands.add<tracked>(created.value(), tracked(1));
        commands.add<position>(created.value());
        const flush_report report = world.flush(commands);
        CHECK(report.applied == 3);
        CHECK_FALSE(world.is_alive(created.value()));
        CHECK(world.entity_count() == 0);
        CHECK(tracked::live() == 0);

        // A deferred create followed by a destroy never becomes alive but frees its id.
        const entity ghost = commands.create_entity();
        commands.add<position>(ghost);
        commands.destroy_entity(ghost);
        CHECK(world.flush(commands).applied == 3);
        CHECK_FALSE(world.is_alive(ghost));
        CHECK(world.entity_count() == 0);
        const core::result<entity> next = world.create_entity();
        CONJURE_REQUIRE_OK(next);
        CHECK(next.value().index == ghost.index);
        CHECK(next.value().generation == ghost.generation + 1);
    }

    TEST_CASE("command_buffer - stale generation skipped and counted") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> first = world.create_entity();
        CONJURE_REQUIRE_OK(first);
        CONJURE_REQUIRE_OK(world.destroy_entity(first.value()));
        const core::result<entity> second = world.create_entity();
        CONJURE_REQUIRE_OK(second);
        REQUIRE(second.value().index == first.value().index);

        tracked::reset_counters();
        testing::log_capture logs = {};
        command_buffer commands(world);
        commands.add<tracked>(first.value(), tracked(3));
        commands.destroy_entity(first.value());
        commands.set<position>(second.value(), position{});
        commands.add<velocity>(second.value());
        commands.destroy_entity(entity{5000, 9});
        const flush_report report = world.flush(commands);
        CHECK(report.applied == 1);
        CHECK(report.skipped == 4);
        CHECK(world.is_alive(second.value()));
        CHECK(world.has<velocity>(second.value()));
        CHECK_FALSE(world.has<position>(second.value()));
        CHECK(tracked::live() == 0);
#if !CONJURE_DISTRIBUTION
        CHECK(logs.count(core::log_level::DEBUG) >= 2);
#endif
    }

    TEST_CASE("command_buffer - payload destructors run on clear") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity();
        CONJURE_REQUIRE_OK(created);
        tracked::reset_counters();
        {
            command_buffer commands(world);
            commands.add<tracked>(created.value(), tracked(1));
            commands.set<tracked>(created.value(), tracked(2));
            commands.add<name_component>(created.value(), name_component{"text"});
            CHECK(tracked::live() == 2);
            commands.clear();
            CHECK(tracked::live() == 0);
            CHECK(commands.empty());
            CHECK(commands.command_count() == 0);

            // The destructor discards and destroys as well, with a warning.
            commands.add<tracked>(created.value(), tracked(3));
            CHECK(tracked::live() == 1);
            testing::log_capture logs = {};
            {
                command_buffer moved = std::move(commands);
                CHECK(moved.command_count() == 1);
                CHECK(commands.empty());
            }
            CHECK(logs.count(core::log_level::WARN) == 1);
        }
        CHECK(tracked::live() == 0);
        CHECK_FALSE(world.has<tracked>(created.value()));
    }

    TEST_CASE("command_buffer - append preserves order") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> target = world.create_entity();
        CONJURE_REQUIRE_OK(target);
        tracked::reset_counters();

        command_buffer first(world);
        command_buffer second(world);
        first.add<tracked>(target.value(), tracked(1));
        second.set<tracked>(target.value(), tracked(2));
        const entity spawned = second.create_entity();
        second.add<name_component>(spawned, name_component{"second"});

        first.append(std::move(second));
        CHECK(first.command_count() == 4);
        CHECK(second.empty());
        CHECK(tracked::live() == 2);

        const flush_report report = world.flush(first);
        CHECK(report.applied == 4);
        CHECK(report.skipped == 0);
        REQUIRE(world.has<tracked>(target.value()));
        CHECK(world.get<tracked>(target.value())->value == 2);
        CHECK(world.is_alive(spawned));
        CHECK(world.get<name_component>(spawned)->value == "second");
        CHECK(tracked::live() == 1);

        // Appending an empty buffer or itself changes nothing.
        first.append(std::move(second));
        CHECK(first.empty());
    }

    TEST_CASE("command_buffer - reservation across 200 creates") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        command_buffer commands(world);
        std::unordered_set<u64> unique = {};
        std::vector<entity> created = {};
        for (u32 index = 0; index < 200; ++index) {
            const entity id = commands.create_entity();
            CHECK(unique.insert(id.to_u64()).second);
            created.push_back(id);
        }
        CHECK(commands.reserved_count() == 4 * command_buffer::RESERVE_BLOCK - 200);
        CHECK(world.flush(commands).applied == 200);
        CHECK(world.entity_count() == 200);
        for (const entity id : created) {
            CHECK(world.is_alive(id));
        }

        // Ids a direct create hands out never collide with ones still reserved by the buffer.
        const core::result<entity> direct = world.create_entity();
        CONJURE_REQUIRE_OK(direct);
        CHECK(unique.insert(direct.value().to_u64()).second);
        const entity late = commands.create_entity();
        CHECK(unique.insert(late.to_u64()).second);
        CHECK(world.flush(commands).applied == 1);
        CHECK(world.entity_count() == 202);
    }

    TEST_CASE("command_buffer - unused reserved ids returned on destruction") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        entity reservedButUnused = {};
        {
            command_buffer commands(world);
            const entity used = commands.create_entity();
            CHECK(world.flush(commands).applied == 1);
            CHECK(commands.reserved_count() == command_buffer::RESERVE_BLOCK - 1);
            // Peek at the next id the buffer would hand out and give it back unused.
            reservedButUnused = commands.create_entity();
            commands.clear();
            CHECK(commands.reserved_count() == command_buffer::RESERVE_BLOCK - 2);
            CHECK(world.is_alive(used));
        }
        // The 62 ids still reserved come back: 70 creations reuse them and take 8 fresh indices
        // (64..71). Without the return they would start at index 64 and reach 133.
        std::vector<entity> fresh(70);
        CONJURE_REQUIRE_OK(world.create_entities(component_mask{}, fresh));
        u32 highest = 0;
        for (const entity id : fresh) {
            highest = std::max(highest, id.index);
        }
        CHECK(highest == 71);
        CHECK(world.entity_count() == 71);
        CHECK_FALSE(world.is_alive(reservedButUnused));
    }

    TEST_CASE("command_buffer - raw commands and tag payloads") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity();
        CONJURE_REQUIRE_OK(created);
        const entity id = created.value();
        command_buffer commands(world);

        const position value{3.0f, 2.0f, 1.0f};
        const std::span<const std::byte> bytes(reinterpret_cast<const std::byte *>(&value), sizeof(value));
        commands.add_raw(id, fixture.id<position>(), bytes);
        commands.add_raw(id, fixture.id<player_tag>());
        commands.add_raw(id, fixture.id<health>());
        const name_component named{"raw"};
        commands.add_raw(id, fixture.id<name_component>(),
          std::span<const std::byte>(reinterpret_cast<const std::byte *>(&named), sizeof(named)));
        CHECK(world.flush(commands).applied == 4);
        CHECK(*world.get<position>(id) == value);
        CHECK(world.has<player_tag>(id));
        CHECK(world.get<health>(id)->value == 100);
        CHECK(world.get<name_component>(id)->value == "raw");

        const position replaced{7.0f, 7.0f, 7.0f};
        commands.set_raw(id, fixture.id<position>(),
          std::span<const std::byte>(reinterpret_cast<const std::byte *>(&replaced), sizeof(replaced)));
        commands.remove_raw(id, fixture.id<health>());
        commands.set_raw(id, fixture.id<player_tag>(), std::span<const std::byte>());
        CHECK(world.flush(commands).applied == 2);
        CHECK(*world.get<position>(id) == replaced);
        CHECK_FALSE(world.has<health>(id));

        testing::expect_assert guard = {};
        commands.add_raw(id, component_id{900});
        commands.set_raw(id, component_id{900}, bytes);
        commands.remove_raw(id, component_id{900});
        commands.add_raw(id, fixture.id<velocity>(), std::span<const std::byte>(bytes.data(), 3));
        commands.set_raw(id, fixture.id<velocity>(), std::span<const std::byte>(bytes.data(), 3));
        CHECK(guard.count() == 5 * ASSERTS_PER_VIOLATION);
        CHECK(commands.empty());
    }

    TEST_CASE("regression - review 5 unregistered typed add asserts and records nothing") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity();
        CONJURE_REQUIRE_OK(created);
        REQUIRE_FALSE(fixture.registry.id_of<unreflected_component>().is_valid());

        command_buffer commands(world);
        testing::expect_assert guard = {};
        commands.add<unreflected_component>(created.value());
        commands.set<unreflected_component>(created.value(), unreflected_component{7});
        commands.remove<unreflected_component>(created.value());
        CHECK(guard.count() == 3 * ASSERTS_PER_VIOLATION);
        CHECK(commands.empty());

        // Nothing to apply and nothing to destroy: the flush and the destructor see an empty buffer.
        const flush_report report = world.flush(commands);
        CHECK(report.applied == 0);
        CHECK(report.skipped == 0);
        CHECK(world.mask_of(created.value()).none());
    }

    TEST_CASE("regression - review 14 flush releases a reserved id it could not place") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const zero_capacity_archetype zero = make_zero_capacity_archetype(fixture);

        command_buffer commands(world);
        const entity reserved = commands.create_entity();
        for (const component_id id : zero.ids) {
            commands.add_raw(reserved, id);
        }
        const flush_report report = world.flush(commands);
        CHECK(report.applied == 0);
        CHECK(report.skipped == 5);
        CHECK_FALSE(world.is_alive(reserved));
        CHECK(world.entity_count() == 0);
        CONJURE_CHECK_OK(world.validate());

        // The index is free again with a bumped generation: the stale handle can never alias it.
        const core::result<entity> next = world.create_entity();
        CONJURE_REQUIRE_OK(next);
        CHECK(next.value().index == reserved.index);
        CHECK(next.value().generation == reserved.generation + 1);
        CHECK_FALSE(world.is_alive(reserved));
        commands.destroy_entity(reserved);
        CHECK(world.flush(commands).skipped == 1);
        CHECK(world.is_alive(next.value()));
        CONJURE_CHECK_OK(world.validate());
    }

    TEST_CASE("command_buffer - recording within capacity allocates nothing") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        const core::result<entity> created = world.create_entity();
        CONJURE_REQUIRE_OK(created);
        command_buffer commands(world);
        // Warm the arena, the reserved block and the per-type id caches.
        commands.add<position>(created.value());
        commands.set<velocity>(created.value(), velocity{});
        static_cast<void>(commands.create_entity());
        commands.clear();

        testing::allocation_scope allocations = {};
        for (u32 index = 0; index < 20; ++index) {
            commands.add<position>(created.value(), position{1.0f, 0.0f, 0.0f});
            commands.set<velocity>(created.value(), velocity{});
            static_cast<void>(commands.create_entity());
        }
        const u64 allocated = allocations.allocation_count();
        CHECK(allocated == 0);
        commands.clear();
    }

    TEST_CASE("command_buffer - flush into another world asserts") {
        ecs_fixture fixture = {};
        world other(fixture.registry);
        command_buffer commands(fixture.world);
        commands.destroy_entity(NULL_ENTITY);
        testing::expect_assert guard = {};
        const flush_report report = other.flush(commands);
        CHECK(report.applied == 0);
        CHECK(guard.count() == 1 * ASSERTS_PER_VIOLATION);
        CHECK(commands.command_count() == 1);
        commands.clear();
    }

    TEST_CASE("command_buffer - no leaks across moves and flushes") {
        // Reflection descriptions and interned names are registered process-wide on first use.
        {
            const ecs_fixture warmUp = {};
        }
        testing::memory_guard guard = {};
        {
            ecs_fixture fixture = {};
            command_buffer a(fixture.world);
            for (u32 index = 0; index < 100; ++index) {
                const entity id = a.create_entity();
                a.add<name_component>(id, name_component{std::to_string(index)});
            }
            command_buffer b(std::move(a));
            command_buffer c(fixture.world);
            c = std::move(b);
            CHECK(fixture.world.flush(c).applied == 200);
            CHECK(fixture.world.entity_count() == 100);
        }
        guard.check();
    }
}
