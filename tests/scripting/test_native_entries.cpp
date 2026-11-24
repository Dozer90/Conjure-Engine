#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"
#include "scripting_test_support.h"

#include "conjure/core/jobs/dedicated_thread.h"
#include "conjure/core/reflect/type_registry.h"
#include "conjure/core/string_id.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/world.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/null_script_host.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_runtime.h"
#include "conjure/scripting/scripting_error.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The core-owned entries of the native table (03_scripting.md 2.1, R26) driven directly through the
// function pointers over the null host, as the managed SDK drives them: no .NET needed. The query_*
// entries need a phase, which the null host's phase hook provides.

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;
using namespace scripting_test;

namespace {

    /// <summary>
    /// A tag component: no column, so get_raw has nothing to hand out.
    /// </summary>
    struct entries_tag {};
}

CONJURE_COMPONENT(entries_tag, "test.scripting.entries_tag");

namespace {

    constexpr std::string_view PLAYER = "Game.Player";

    struct entries_fixture : scripting_fixture {
        entries_fixture() {
            load_fake_classes({make_class(PLAYER, {script_callback::UPDATE})});
            runtime.set_mode(script_mode::PLAY);
        }

        [[nodiscard]] const conjure_native_api &api() noexcept {
            return runtime.native_api();
        }

        [[nodiscard]] u32 position_id() const noexcept {
            return registry.id_of<test_position>().value;
        }

        [[nodiscard]] u64 position_layout_hash() const noexcept {
            return core::reflect::type_info_of<test_position>()->layout_hash();
        }
    };

    /// <summary>
    /// Runs 'body' inside a query scope over test_position opened in an UPDATE phase, as a managed
    /// ComponentQuery would; the scope closes, and the runtime flushes its deferred buffer, right after it.
    /// </summary>
    template <typename Body>
    void run_in_scope(entries_fixture &fixture, Body body) {
        const std::array<u32, 1> ids = {fixture.position_id()};
        bool opened = false;
        fixture.null_host().set_phase_hook([&fixture, &ids, &body, &opened](script_phase, f32) {
            conjure_query_scope scope = {};
            opened = fixture.api().query_begin(ids.data(), 1, &scope) == 0;
            if (!opened) {
                return;
            }
            body();
            fixture.api().query_end(scope.scope_id);
        });
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        fixture.null_host().set_phase_hook({});
        CHECK(opened);
    }

    [[nodiscard]] std::string schema_json(std::string_view fields) {
        return "{\"fields\":[" + std::string(fields) + "]}";
    }

    [[nodiscard]] conjure_managed_component_desc managed_desc(
      const std::string &name,
      const std::string &schema,
      u32 size,
      u32 alignment
    ) {
        conjure_managed_component_desc desc = {};
        desc.name_hash = core::string_id(name).hash();
        desc.full_name = name.c_str();
        desc.name_length = static_cast<int32_t>(name.size());
        desc.size = size;
        desc.alignment = alignment;
        desc.layout_hash = 0;
        desc.schema_json = schema.c_str();
        desc.schema_length = static_cast<int32_t>(schema.size());
        return desc;
    }
}

TEST_SUITE("scripting/native_entries") {

    TEST_CASE("native entries - entity create, name, find, is_alive and destroy") {
        entries_fixture fixture = {};
        const conjure_native_api &api = fixture.api();
        const std::string name = "hero";
        const u64 created = api.entity_create(name.data(), static_cast<int32_t>(name.size()));
        REQUIRE(created != 0);
        const ecs::entity entity = ecs::entity::from_u64(created);
        CHECK(fixture.world.is_alive(entity));
        CHECK(api.entity_is_alive(created) == 1);
        CHECK(api.entity_find_by_name(name.data(), static_cast<int32_t>(name.size())) == created);
        CHECK(api.entity_find_by_name("nobody", 6) == 0);

        std::array<char, 8> buffer = {};
        CHECK(api.entity_get_name(created, buffer.data(), static_cast<int32_t>(buffer.size())) == 4);
        CHECK(std::string_view(buffer.data(), 4) == "hero");
        CHECK(api.entity_get_name(created, nullptr, 0) == 4);           // the needed length, nothing written
        CHECK(api.entity_get_name(created, buffer.data(), 2) == 4);     // too small: unchanged, length reported
        CHECK(api.entity_get_name(999, buffer.data(), 8) == 0);

        const u64 unnamed = api.entity_create(nullptr, 0);
        CHECK(unnamed != 0);
        CHECK(api.entity_get_name(unnamed, buffer.data(), 8) == 0);

        CHECK(api.entity_destroy(created) == 1);                        // no scripts: gone at once
        CHECK_FALSE(fixture.world.is_alive(entity));
        CHECK(api.entity_destroy(created) == 0);
        CHECK(api.entity_is_alive(created) == 0);
        CHECK(api.entity_find_by_name(name.data(), static_cast<int32_t>(name.size())) == 0);
    }

    TEST_CASE("native entries - script_add queues a script and script_get_slots lists the live ones") {
        entries_fixture fixture = {};
        const conjure_native_api &api = fixture.api();
        const ecs::entity entity = fixture.create_entity();
        CHECK(api.script_add(entity.to_u64(), hash_of(PLAYER)) == 0);   // the entry index
        CHECK(api.script_add(entity.to_u64(), hash_of(PLAYER)) == 1);
        CHECK(api.script_add(999, hash_of(PLAYER)) == status_of(script_error::INSTANCE_NOT_FOUND));
        std::array<int32_t, 8> slots = {};
        CHECK(api.script_get_slots(entity.to_u64(), slots.data(), 8) == 0);     // nothing live before the flush
        fixture.runtime.flush_pending();
        CHECK(api.script_get_slots(entity.to_u64(), slots.data(), 8) == 2);
        CHECK(slots[0] != slots[1]);
        CHECK(fixture.runtime.entry_of_slot(slots[0]).entryIndex == 0);
        CHECK(fixture.runtime.entry_of_slot(slots[1]).entryIndex == 1);
        CHECK(api.script_get_slots(entity.to_u64(), slots.data(), 1) == 2);     // count reported, one written
        CHECK(api.script_get_slots(entity.to_u64(), nullptr, 0) == 2);
        CHECK(api.script_get_slots(fixture.create_entity().to_u64(), slots.data(), 8) == 0);
    }

    TEST_CASE("native entries - script_get_slots allocates nothing") {
        entries_fixture fixture = {};
        const conjure_native_api &api = fixture.api();
        const ecs::entity entity = fixture.create_entity();
        REQUIRE(api.script_add(entity.to_u64(), hash_of(PLAYER)) == 0);
        REQUIRE(api.script_add(entity.to_u64(), hash_of(PLAYER)) == 1);
        fixture.runtime.flush_pending();

        // Every managed GetScript<T>() lands here: the set is read in place.
        std::array<int32_t, 8> slots = {};
        int32_t count = 0;
        CONJURE_CHECK_NO_ALLOCATIONS(count = api.script_get_slots(entity.to_u64(), slots.data(), 8));
        CHECK(count == 2);
        CHECK(slots[0] != slots[1]);
    }

    TEST_CASE("native entries - script_register_class outside an assembly load is ignored with a warning") {
        log_capture logs = {};
        entries_fixture fixture = {};
        const conjure_native_api &api = fixture.api();
        const std::string name = "Game.Stray";
        conjure_script_class_desc desc = {};
        desc.hash = hash_of(name);
        desc.full_name = name.c_str();
        desc.name_length = static_cast<int32_t>(name.size());
        api.script_register_class(&desc);
        CHECK_FALSE(fixture.runtime.classes().is_staging_open());
        CHECK(fixture.runtime.classes().find(hash_of(name)) == nullptr);
        CHECK(fixture.runtime.classes().find_staged(hash_of(name)) == nullptr);
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK(logs.contains(core::log_level::WARN, "Game.Stray"));

        // The next load stages and commits its own classes as usual.
        CONJURE_REQUIRE_OK(fixture.runtime.load_game_assembly({}));
        CHECK(fixture.runtime.classes().find(PLAYER) != nullptr);
        CHECK(fixture.runtime.classes().find(hash_of(name)) == nullptr);
        CHECK_FALSE(fixture.runtime.classes().is_staging_open());
    }

    TEST_CASE("native entries - component lookup, has, add, get, set and remove on a native type") {
        log_capture logs = {};
        logs.allow_errors();
        entries_fixture fixture = {};
        const conjure_native_api &api = fixture.api();
        const u64 nameHash = core::string_id("test.scripting.position").hash();
        const u32 typeId = api.component_lookup_type(nameHash, sizeof(test_position), fixture.position_layout_hash());
        CHECK(typeId == fixture.position_id());
        CHECK(api.component_lookup_type(nameHash, sizeof(test_position) + 4, fixture.position_layout_hash()) == 0);
        CHECK(api.component_lookup_type(nameHash, sizeof(test_position), 1) == 0);
        CHECK(api.component_lookup_type(core::string_id("no.such").hash(), 4, 1) == 0);
        CHECK(logs.count(core::log_level::ERR) == 1);                   // the mismatch is reported once per name
        CHECK(logs.count(core::log_level::WARN) == 1);                  // the unknown name once

        const ecs::entity entity = fixture.create_entity();
        const test_position initial = {1.0f, 2.0f, 3.0f};
        CHECK(api.component_has(entity.to_u64(), typeId) == 0);
        CHECK(api.component_add(entity.to_u64(), typeId, &initial) == 1);
        CHECK(api.component_has(entity.to_u64(), typeId) == 1);
        CHECK(api.component_add(entity.to_u64(), typeId, &initial) == 0);   // already present
        test_position read = {};
        CHECK(api.component_get(entity.to_u64(), typeId, &read) == 1);
        CHECK(read == initial);
        const test_position changed = {4.0f, 5.0f, 6.0f};
        CHECK(api.component_set(entity.to_u64(), typeId, &changed) == 1);
        CHECK(*std::as_const(fixture.world).get<test_position>(entity) == changed);
        CHECK(api.component_remove(entity.to_u64(), typeId) == 1);
        CHECK(api.component_remove(entity.to_u64(), typeId) == 0);
        CHECK(api.component_get(entity.to_u64(), typeId, &read) == 0);
        CHECK(api.component_set(entity.to_u64(), typeId, &changed) == 0);
        CHECK(api.component_add(entity.to_u64(), typeId, nullptr) == 1);    // default value
        CHECK(api.component_get(entity.to_u64(), typeId, &read) == 1);
        CHECK(read == test_position{});

        // Id 0 (unresolved_components) and ids out of range are refused everywhere.
        CHECK(api.component_has(entity.to_u64(), 0) == 0);
        CHECK(api.component_add(entity.to_u64(), 0, &initial) == 0);
        CHECK(api.component_get(entity.to_u64(), 0, &read) == 0);
        CHECK(api.component_set(entity.to_u64(), 0, &changed) == 0);
        CHECK(api.component_remove(entity.to_u64(), 0) == 0);
        CHECK(api.component_has(entity.to_u64(), 60000) == 0);
        CHECK(api.component_get(entity.to_u64(), typeId, nullptr) == 0);
        CHECK(api.component_add(999, typeId, &initial) == 0);
    }

    TEST_CASE("regression - sweep-2026-11-12 F631: component_get and component_set answer for a held tag") {
        // C# SetComponent and GetComponent now throw when these entries fail, so a tag the entity holds must not
        // read as missing only because it has no column.
        log_capture logs = {};
        entries_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.registry.register_component<entries_tag>());
        const u32 tagId = fixture.registry.id_of<entries_tag>().value;
        const conjure_native_api &api = fixture.api();
        const ecs::entity entity = fixture.create_entity();
        std::array<std::byte, 4> scratch = {};

        SUBCASE("absent: both fail") {
            CHECK(api.component_get(entity.to_u64(), tagId, scratch.data()) == 0);
            CHECK(api.component_set(entity.to_u64(), tagId, scratch.data()) == 0);
        }
        SUBCASE("held: both succeed and write nothing") {
            REQUIRE(api.component_add(entity.to_u64(), tagId, nullptr) == 1);
            scratch.fill(std::byte{0x5A});
            CHECK(api.component_get(entity.to_u64(), tagId, scratch.data()) == 1);
            CHECK(scratch[0] == std::byte{0x5A});                         // zero bytes copied
            CHECK(api.component_set(entity.to_u64(), tagId, scratch.data()) == 1);
            CHECK(fixture.world.has_raw(entity, fixture.registry.id_of<entries_tag>()));
        }
        SUBCASE("removed: both fail again") {
            REQUIRE(api.component_add(entity.to_u64(), tagId, nullptr) == 1);
            REQUIRE(api.component_remove(entity.to_u64(), tagId) == 1);
            CHECK(api.component_get(entity.to_u64(), tagId, scratch.data()) == 0);
            CHECK(api.component_set(entity.to_u64(), tagId, scratch.data()) == 0);
        }
        SUBCASE("dead entity: both fail") {
            REQUIRE(api.component_add(entity.to_u64(), tagId, nullptr) == 1);
            CONJURE_REQUIRE_OK(fixture.world.destroy_entity(entity));
            CHECK(api.component_get(entity.to_u64(), tagId, scratch.data()) == 0);
            CHECK(api.component_set(entity.to_u64(), tagId, scratch.data()) == 0);
        }
        SUBCASE("null pointers are still refused") {
            REQUIRE(api.component_add(entity.to_u64(), tagId, nullptr) == 1);
            CHECK(api.component_get(entity.to_u64(), tagId, nullptr) == 0);
            CHECK(api.component_set(entity.to_u64(), tagId, nullptr) == 0);
        }
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("native entries - query scopes expose chunk columns inside a phase and defer structural requests") {
        log_capture logs = {};
        entries_fixture fixture = {};
        const conjure_native_api &api = fixture.api();
        std::vector<ecs::entity> entities = {};
        for (u32 index = 0; index < 50; ++index) {
            const ecs::entity entity = fixture.create_entity();
            const test_position initial = {static_cast<f32>(index), 0.0f, 0.0f};
            CONJURE_REQUIRE_OK(fixture.world.add<test_position>(entity, initial));
            if (index % 2 == 0) {
                CONJURE_REQUIRE_OK(fixture.world.add<test_velocity>(entity));
            }
            entities.push_back(entity);
        }
        const std::array<u32, 2> ids = {fixture.position_id(), fixture.registry.id_of<test_velocity>().value};

        // Outside a phase the scope is refused (as managed Begin would be told).
        conjure_query_scope scope = {};
        CHECK(api.query_begin(ids.data(), 2, &scope) == status_of(script_error::NOT_IN_PHASE));
        CHECK(api.query_begin(nullptr, 2, &scope) == status_of(script_error::NOT_SUPPORTED));
        CHECK(api.query_begin(ids.data(), 0, &scope) == status_of(script_error::NOT_SUPPORTED));
        const std::array<u32, 1> zero = {0};
        api.query_end(77);                                              // unknown scopes are ignored

        u64 reserved = 0;
        bool aliveInside = true;
        bool addedInside = false;
        u32 rows = 0;
        u32 chunks = 0;
        const u64 versionBefore = fixture.world.version();
        fixture.null_host().set_phase_hook([&](script_phase, f32) {
            CHECK(api.query_begin(zero.data(), 1, &scope) == status_of(script_error::NOT_SUPPORTED));
            REQUIRE(api.query_begin(ids.data(), 1, &scope) == 0);
            CHECK(fixture.world.is_locked());
            CHECK(fixture.runtime.open_query_scope_count() == 1);
            REQUIRE(scope.chunks != nullptr);
            chunks = static_cast<u32>(scope.chunk_count);
            for (int32_t chunk = 0; chunk < scope.chunk_count; ++chunk) {
                const conjure_chunk_view &view = scope.chunks[static_cast<usize>(chunk)];
                auto *positions = static_cast<test_position *>(view.columns[0]);
                REQUIRE(positions != nullptr);
                REQUIRE(view.entities != nullptr);
                for (int32_t row = 0; row < view.count; ++row) {
                    positions[row].y = positions[row].x + 1.0f;
                    CHECK(fixture.world.is_alive(ecs::entity::from_u64(view.entities[row])));
                    ++rows;
                }
            }
            // Structural requests inside the scope are deferred to its end (R5).
            reserved = api.entity_create("later", 5);
            aliveInside = api.entity_is_alive(reserved) != 0;
            addedInside = api.component_add(entities[1].to_u64(), ids[1], nullptr) != 0;
            CHECK(api.component_remove(entities[0].to_u64(), ids[1]) == 1);
            CHECK(fixture.world.has<test_velocity>(entities[0]));       // still there until query_end
            api.query_end(scope.scope_id);
            CHECK_FALSE(fixture.world.is_locked());
            CHECK(fixture.runtime.open_query_scope_count() == 0);
        });
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        fixture.null_host().set_phase_hook({});

        CHECK(rows == 50);
        CHECK(chunks == 2);                                             // two archetypes
        for (u32 index = 0; index < entities.size(); ++index) {
            CAPTURE(index);
            const test_position *position = std::as_const(fixture.world).get<test_position>(entities[index]);
            REQUIRE(position != nullptr);
            CHECK(position->y == doctest::Approx(static_cast<f32>(index) + 1.0f));
        }
        CHECK(fixture.world.version() > versionBefore);
        CHECK(reserved != 0);
        CHECK_FALSE(aliveInside);
        CHECK(addedInside);
        CHECK(fixture.world.is_alive(ecs::entity::from_u64(reserved)));
        CHECK(fixture.runtime.entity_name(ecs::entity::from_u64(reserved)) == "later");
        CHECK(fixture.world.has<test_velocity>(entities[1]));
        CHECK_FALSE(fixture.world.has<test_velocity>(entities[0]));
        CHECK(logs.count(core::log_level::WARN) == 1);                  // the id-0 request (R4)
        CHECK(logs.contains(core::log_level::WARN, "out of range"));

        // A scope left open is force-closed at phase end with a warning naming the phase.
        fixture.null_host().set_phase_hook([&](script_phase, f32) {
            REQUIRE(api.query_begin(ids.data(), 2, &scope) == 0);
        });
        fixture.runtime.run_phase(script_phase::LATE_UPDATE, 0.1f);
        fixture.null_host().set_phase_hook({});
        CHECK(fixture.runtime.open_query_scope_count() == 0);
        CHECK_FALSE(fixture.world.is_locked());
        CHECK(logs.contains(core::log_level::WARN, "LATE_UPDATE"));
        CHECK(logs.contains(core::log_level::WARN, "force-closed"));
    }

    TEST_CASE("regression - code sweep OWN-P2 requests on an id entity_create reserved in a scope apply at its end") {
        // Entity.Create inside a ComponentQuery scope reserves the id; component_add refused it (it tested
        // is_alive first), so C# could not spawn with components through the deferred buffer (17_demo_games.md P2).
        log_capture logs = {};
        entries_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.registry.register_component<entries_tag>());
        const conjure_native_api &api = fixture.api();
        const u32 positionType = fixture.position_id();
        const u32 velocityType = fixture.registry.id_of<test_velocity>().value;
        const u32 tagType = fixture.registry.id_of<entries_tag>().value;
        const test_position first = {1.0f, 2.0f, 3.0f};
        const test_position second = {4.0f, 5.0f, 6.0f};
        const test_velocity speed = {7.0f, 8.0f};
        const u32 before = fixture.world.entity_count();
        u64 reserved = 0;

        SUBCASE("two components land with their values while reads inside the scope answer as for a dead entity") {
            run_in_scope(fixture, [&]() {
                reserved = api.entity_create("spawned", 7);
                CHECK(reserved != 0);
                CHECK(api.component_add(reserved, positionType, &first) == 1);
                CHECK(api.component_add(reserved, velocityType, &speed) == 1);
                test_position read = {};
                CHECK(api.entity_is_alive(reserved) == 0);
                CHECK(api.component_has(reserved, positionType) == 0);
                CHECK(api.component_get(reserved, positionType, &read) == 0);
                CHECK(fixture.runtime.is_deferred_reserved(ecs::entity::from_u64(reserved)));
            });
            const ecs::entity created = ecs::entity::from_u64(reserved);
            REQUIRE(fixture.world.is_alive(created));
            CHECK(fixture.world.entity_count() == before + 1);
            REQUIRE(fixture.world.has<test_position>(created));
            CHECK(*std::as_const(fixture.world).get<test_position>(created) == first);
            REQUIRE(fixture.world.has<test_velocity>(created));
            CHECK(*std::as_const(fixture.world).get<test_velocity>(created) == speed);
            CHECK(fixture.runtime.entity_name(created) == "spawned");
            CHECK_FALSE(fixture.runtime.is_deferred_reserved(created));
            // An ordinary entity after the flush: requests apply at once.
            CHECK(api.component_remove(reserved, velocityType) == 1);
            CHECK_FALSE(fixture.world.has<test_velocity>(created));
        }
        SUBCASE("add then remove leaves the removed component out") {
            run_in_scope(fixture, [&]() {
                reserved = api.entity_create(nullptr, 0);
                CHECK(api.component_add(reserved, positionType, &first) == 1);
                CHECK(api.component_add(reserved, velocityType, &speed) == 1);
                CHECK(api.component_remove(reserved, velocityType) == 1);
                CHECK(api.component_remove(reserved, velocityType) == 0);       // nothing left to remove
            });
            const ecs::entity created = ecs::entity::from_u64(reserved);
            REQUIRE(fixture.world.is_alive(created));
            CHECK(fixture.world.has<test_position>(created));
            CHECK_FALSE(fixture.world.has<test_velocity>(created));
        }
        SUBCASE("a second add overwrites the first, as a set would") {
            run_in_scope(fixture, [&]() {
                reserved = api.entity_create(nullptr, 0);
                CHECK(api.component_add(reserved, positionType, &first) == 1);
                CHECK(api.component_add(reserved, positionType, &second) == 1);
            });
            const ecs::entity created = ecs::entity::from_u64(reserved);
            REQUIRE(fixture.world.has<test_position>(created));
            CHECK(*std::as_const(fixture.world).get<test_position>(created) == second);
        }
        SUBCASE("a set after the add writes its value") {
            run_in_scope(fixture, [&]() {
                reserved = api.entity_create(nullptr, 0);
                CHECK(api.component_add(reserved, positionType, &first) == 1);
                CHECK(api.component_set(reserved, positionType, &second) == 1);
            });
            const ecs::entity created = ecs::entity::from_u64(reserved);
            REQUIRE(fixture.world.has<test_position>(created));
            CHECK(*std::as_const(fixture.world).get<test_position>(created) == second);
        }
        SUBCASE("a set without an add never throws and is skipped; a remove with nothing added answers 0") {
            run_in_scope(fixture, [&]() {
                reserved = api.entity_create(nullptr, 0);
                CHECK(api.component_set(reserved, positionType, &first) == 1);
                CHECK(api.component_remove(reserved, velocityType) == 0);
            });
            const ecs::entity created = ecs::entity::from_u64(reserved);
            REQUIRE(fixture.world.is_alive(created));
            CHECK_FALSE(fixture.world.has<test_position>(created));
            CHECK_FALSE(fixture.world.has<test_velocity>(created));
        }
        SUBCASE("a tag is added and set like any component") {
            std::array<std::byte, 4> scratch = {};
            run_in_scope(fixture, [&]() {
                reserved = api.entity_create(nullptr, 0);
                CHECK(api.component_add(reserved, tagType, nullptr) == 1);
                CHECK(api.component_set(reserved, tagType, scratch.data()) == 1);
                CHECK(api.component_get(reserved, tagType, scratch.data()) == 0);
            });
            CHECK(fixture.world.has<entries_tag>(ecs::entity::from_u64(reserved)));
        }
        SUBCASE("script_add stages scripts the flush creates") {
            std::array<int32_t, 8> slots = {};
            run_in_scope(fixture, [&]() {
                reserved = api.entity_create(nullptr, 0);
                CHECK(api.script_add(reserved, hash_of(PLAYER)) == 0);
                CHECK(api.script_add(reserved, hash_of(PLAYER)) == 1);
                CHECK(api.script_get_slots(reserved, slots.data(), 8) == 0);
                CHECK(api.component_add(reserved, positionType, &first) == 1);
            });
            const ecs::entity created = ecs::entity::from_u64(reserved);
            REQUIRE(fixture.world.is_alive(created));
            CHECK(fixture.world.has<test_position>(created));
            CHECK(api.script_get_slots(reserved, slots.data(), 8) == 2);        // created by the phase's flush
            CHECK(fixture.runtime.stats().liveInstances == 2);
        }
        SUBCASE("destroy wins over every request on the id, before or after it, and the id's name goes with it") {
            const usize namesBefore = fixture.runtime.entity_name_count();
            run_in_scope(fixture, [&]() {
                reserved = api.entity_create("doomed", 6);
                CHECK(api.component_add(reserved, positionType, &first) == 1);
                CHECK(api.script_add(reserved, hash_of(PLAYER)) == 0);
                CHECK(api.entity_destroy(reserved) == 1);
                CHECK(api.entity_destroy(reserved) == 1);                       // still reserved until the flush
                CHECK(api.component_add(reserved, velocityType, &speed) == 1);
            });
            CHECK_FALSE(fixture.world.is_alive(ecs::entity::from_u64(reserved)));
            CHECK(fixture.world.entity_count() == before);
            CHECK(fixture.runtime.stats().liveInstances == 0);
            CHECK(api.entity_destroy(reserved) == 0);
            CHECK(api.entity_find_by_name("doomed", 6) == 0);
            // The flush hands the id back without a destroyed observer: the runtime forgets the name itself.
            CHECK(fixture.runtime.entity_name_count() == namesBefore);
        }
        SUBCASE("refusals keep their statuses: the script limit, a live entity without scripts, a dead id") {
            const ecs::entity live = fixture.create_entity();
            const ecs::entity dead = fixture.create_entity();
            CONJURE_REQUIRE_OK(fixture.world.destroy_entity(dead));
            run_in_scope(fixture, [&]() {
                reserved = api.entity_create(nullptr, 0);
                for (u32 index = 0; index < MAX_SCRIPTS_PER_ENTITY; ++index) {
                    CAPTURE(index);
                    CHECK(api.script_add(reserved, hash_of(PLAYER)) == static_cast<int32_t>(index));
                }
                CHECK(api.script_add(reserved, hash_of(PLAYER)) == status_of(script_error::NOT_SUPPORTED));
                CHECK(api.script_add(live.to_u64(), hash_of(PLAYER)) == status_of(script_error::NOT_SUPPORTED));
                CHECK(api.script_add(dead.to_u64(), hash_of(PLAYER)) == status_of(script_error::INSTANCE_NOT_FOUND));
                CHECK(api.component_add(dead.to_u64(), positionType, &first) == 0);
                CHECK(api.component_set(dead.to_u64(), positionType, &first) == 0);
                CHECK(api.component_remove(dead.to_u64(), positionType) == 0);
                CHECK(api.entity_destroy(dead.to_u64()) == 0);
                CHECK(api.component_add(reserved, 0, &first) == 0);
                CHECK(api.component_set(reserved, 0, &first) == 0);
                CHECK(api.component_remove(reserved, 0) == 0);
            });
            CHECK(fixture.runtime.scripts_of(ecs::entity::from_u64(reserved)).size() == MAX_SCRIPTS_PER_ENTITY);
            CHECK(fixture.runtime.scripts_of(live).empty());
            CHECK(logs.contains(core::log_level::WARN, "locked for iteration"));
        }
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("native entries - component_register_managed refuses native names and keeps equal layouts") {
        log_capture logs = {};
        entries_fixture fixture = {};
        const conjure_native_api &api = fixture.api();
        const ecs::entity entity = fixture.create_entity();
        const test_position before = {7.0f, 8.0f, 9.0f};
        CONJURE_REQUIRE_OK(fixture.world.add<test_position>(entity, before));

        // A game struct named like a native component: refused before any instance is touched (R4).
        const std::string nativeName = "test.scripting.position";
        const std::string nativeSchema = schema_json(R"({"name":"x","type":"f32","offset":0})");
        const conjure_managed_component_desc collision = managed_desc(nativeName, nativeSchema, 4, 4);
        CHECK(api.component_register_managed(&collision) == 0);
        CHECK(logs.contains(core::log_level::WARN, "name of the native component"));
        CHECK(*std::as_const(fixture.world).get<test_position>(entity) == before);

        // A dynamic type: registered, re-registered with the same layout (same id, no reset), then with a
        // changed layout while an instance exists (data reset to zero with a warning, same id).
        const std::string dynamicName = "Game.Dyn";
        const std::string schemaV1 = schema_json(R"({"name":"value","type":"i32","offset":0})");
        const conjure_managed_component_desc first = managed_desc(dynamicName, schemaV1, 4, 4);
        const u32 id = api.component_register_managed(&first);
        REQUIRE(id != 0);
        CHECK(fixture.registry.find(core::string_id(dynamicName)).value == id);
        const int32_t value = 42;
        CHECK(api.component_add(entity.to_u64(), id, &value) == 1);
        const u32 warningsBefore = logs.count(core::log_level::WARN);
        CHECK(api.component_register_managed(&first) == id);
        CHECK(logs.count(core::log_level::WARN) == warningsBefore);
        int32_t read = 0;
        CHECK(api.component_get(entity.to_u64(), id, &read) == 1);
        CHECK(read == 42);

        const std::string schemaV2 = schema_json(
          R"({"name":"value","type":"i32","offset":0},{"name":"extra","type":"f32","offset":4})"
        );
        const conjure_managed_component_desc second = managed_desc(dynamicName, schemaV2, 8, 4);
        CHECK(api.component_register_managed(&second) == id);
        CHECK(logs.contains(core::log_level::WARN, "reset to zero"));
        CHECK(fixture.registry.info(ecs::component_id{static_cast<u16>(id)}).size == 8);
        CHECK(fixture.world.has_raw(entity, ecs::component_id{static_cast<u16>(id)}));
        std::array<std::byte, 8> bytes = {};
        CHECK(api.component_get(entity.to_u64(), id, bytes.data()) == 1);
        int32_t resetValue = 0;
        std::memcpy(&resetValue, bytes.data(), sizeof(resetValue));
        CHECK(resetValue == 0);

        // Malformed descriptors are refused.
        const std::string noFields = "{}";
        const conjure_managed_component_desc bare = managed_desc(dynamicName, noFields, 4, 4);
        CHECK(api.component_register_managed(&bare) == 0);
        CHECK(api.component_register_managed(nullptr) == 0);
    }

    TEST_CASE("native entries - without a runtime every entry fails softly") {
        REQUIRE(script_runtime::active() == nullptr);
        conjure_native_api api = make_native_api(nullptr);
        fill_core_native_entries(api);
        api.log_write(2, "no runtime", 10);
        conjure_script_class_desc classDesc = {};
        api.script_register_class(&classDesc);
        api.script_mark_faulted(0, 0);
        CHECK(api.script_add(1, 1) == status_of(script_error::NOT_INITIALISED));
        CHECK(api.script_get_slots(1, nullptr, 0) == status_of(script_error::NOT_INITIALISED));
        CHECK(api.entity_create("x", 1) == 0);
        CHECK(api.entity_destroy(1) == 0);
        CHECK(api.entity_is_alive(1) == 0);
        CHECK(api.entity_get_name(1, nullptr, 0) == 0);
        CHECK(api.entity_find_by_name("x", 1) == 0);
        CHECK(api.component_lookup_type(1, 4, 1) == 0);
        CHECK(api.component_register_managed(nullptr) == 0);
        CHECK(api.component_has(1, 1) == 0);
        CHECK(api.component_add(1, 1, nullptr) == 0);
        CHECK(api.component_remove(1, 1) == 0);
        CHECK(api.component_get(1, 1, nullptr) == 0);
        CHECK(api.component_set(1, 1, nullptr) == 0);
        conjure_query_scope scope = {};
        const std::array<u32, 1> ids = {1};
        CHECK(api.query_begin(ids.data(), 1, &scope) == status_of(script_error::NOT_INITIALISED));
        api.query_end(1);
    }

    TEST_CASE("regression - review 12 native entries reject calls off the main thread") {
        entries_fixture fixture = {};
        const conjure_native_api &api = fixture.api();
        const ecs::entity entity = fixture.create_entity();
        expect_assert guard = {};
        int32_t alive = -1;
        int32_t added = -1;
        u64 created = 1;
        {
            // A Task that captured the raw function pointers (the managed guard was bypassed, D10).
            core::jobs::dedicated_thread worker("scripting-wrong-thread", [&](std::stop_token) {
                alive = api.entity_is_alive(entity.to_u64());
                added = api.script_add(entity.to_u64(), hash_of(PLAYER));
                created = api.entity_create("x", 1);
            });
            worker.join();
        }
        CHECK(alive == 0);
        CHECK(added == status_of(script_error::NOT_INITIALISED));
        CHECK(created == 0);
        CHECK(fixture.world.is_alive(entity));
        CHECK(fixture.runtime.scripts_of(entity).empty());
#if CONJURE_ASSERTS_ENABLED
        CHECK(guard.count() == 3);
#endif
    }
}
