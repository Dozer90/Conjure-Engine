#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "scripting_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/world.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/null_script_host.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_host.h"
#include "conjure/scripting/script_runtime.h"
#include "conjure/scripting/scripting_error.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <string>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;
using namespace scripting_test;

namespace {

    constexpr std::string_view POSITION_WRITER = "Conjure.TestScripts.PositionWriter";
    constexpr std::string_view BAD_MIRROR_USER = "Conjure.TestScripts.BadMirrorUser";
    constexpr std::string_view FORGETFUL = "Conjure.TestScripts.ForgetfulQuery";
    constexpr std::string_view DEFERRED_CREATOR = "Conjure.TestScripts.DeferredCreator";
    constexpr std::string_view SCORE_USER = "Conjure.TestScripts.ScoreUser";
    constexpr std::string_view FLAGS_USER = "Conjure.TestScripts.FlagsMirrorUser";
    constexpr std::string_view TOGGLE_USER = "Conjure.TestScripts.ToggleUser";
    constexpr std::string_view WRONG_COLUMN_USER = "Conjure.TestScripts.WrongColumnUser";
    constexpr std::string_view QUERY_IN_CREATE = "Conjure.TestScripts.QueryInCreate";
    constexpr std::string_view UNDESCRIBABLE_MIRROR_USER = "Conjure.TestScripts.UndescribableMirrorUser";
    constexpr std::string_view SQUAD_USER = "Conjure.TestScripts.SquadUser";
    constexpr std::string_view DYNAMIC_SCHEMA_PROBE = "Conjure.TestScripts.DynamicSchemaProbe";
    constexpr std::string_view FACADE_EXISTS_PROBE = "Conjure.TestScripts.FacadeExistsProbe";
}

TEST_SUITE("scripting/dod_query") {

    TEST_CASE("dod query - span writes through the mirror are visible to native [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        std::vector<ecs::entity> entities = {};
        for (u32 index = 0; index < 100; ++index) {
            const ecs::entity entity = fixture.create_entity();
            CONJURE_REQUIRE_OK(
              fixture.world.add<test_position>(entity, test_position{static_cast<f32>(index), 1.0f, 2.0f})
            );
            if (index % 2 == 0) {
                CONJURE_REQUIRE_OK(fixture.world.add<test_velocity>(entity));     // a second archetype
            }
            entities.push_back(entity);
        }
        const script_entry_id writer = fixture.add_script(fixture.create_entity(), POSITION_WRITER);
        const u64 versionBefore = fixture.world.version();
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(fixture.field_i64(writer, "Rows") == 100);
        CHECK(fixture.field_i64(writer, "Chunks") == 2);
        for (u32 index = 0; index < entities.size(); ++index) {
            CAPTURE(index);
            const test_position *position = std::as_const(fixture.world).get<test_position>(entities[index]);
            REQUIRE(position != nullptr);
            CHECK(position->x == doctest::Approx(static_cast<f32>(index) + 1.0f));
            CHECK(position->y == doctest::Approx(1.0f));
        }
        CHECK(fixture.world.version() > versionBefore);           // the written column was stamped
        CHECK(fixture.runtime.open_query_scope_count() == 0);
        CHECK_FALSE(fixture.world.is_locked());
        CHECK(logs.count(core::log_level::WARN) == 0);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("dod query - a mirror layout mismatch throws and is logged once [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        logs.allow_errors();
        dotnet_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.world.add<test_position>(fixture.create_entity()));
        const script_entry_id user = fixture.add_script(fixture.create_entity(), BAD_MIRROR_USER);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(fixture.field_i64(user, "Attempts") == 2);
        // Native reports the mismatch once; every attempt still throws into the guard.
        u32 mismatchLogs = 0;
        for (const std::string &message : logs.messages(core::log_level::ERR)) {
            if (message.find("mirror of 'test.scripting.position' does not match") != std::string::npos) {
                ++mismatchLogs;
            }
        }
        CHECK(mismatchLogs == 1);
        CHECK(logs.contains(core::log_level::ERR, "InvalidOperationException"));
        CHECK(fixture.runtime.open_query_scope_count() == 0);
    }

    TEST_CASE(
      "dod query - a forgotten dispose is force-closed with a warning at phase end [dotnet]" * doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.world.add<test_position>(fixture.create_entity()));
        const script_entry_id forgetful = fixture.add_script(fixture.create_entity(), FORGETFUL);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(fixture.field_i64(forgetful, "Opened") == 1);
        CHECK(logs.contains(core::log_level::WARN, "force-closed"));
        CHECK(fixture.runtime.open_query_scope_count() == 0);
        CHECK_FALSE(fixture.world.is_locked());
        // The world is usable again: structural changes succeed.
        CONJURE_CHECK_OK(fixture.world.create_entity());
    }

    TEST_CASE(
      "dod query - a structural request inside a scope is deferred to the scope end [dotnet]" * doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.world.add<test_position>(fixture.create_entity()));
        const script_entry_id creator = fixture.add_script(fixture.create_entity(), DEFERRED_CREATOR);
        const u32 before = fixture.world.entity_count();
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(fixture.world.entity_count() == before + 1);
        const ecs::entity created = ecs::entity::from_u64(fixture.fields_of(creator).root()["Created"].as_u64());
        CHECK_FALSE(created.is_null());
        CHECK(fixture.world.is_alive(created));
        CHECK(fixture.field_i64(creator, "AliveInsideScope") == 0);   // reserved, not yet alive inside the scope
        CHECK(fixture.runtime.entity_name(created) == "deferred");
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("dod query - an [EcsComponent] struct becomes a dynamic component [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const ecs::component_id scoreId = fixture.registry.find(core::string_id("Conjure.TestScripts.Score"));
        REQUIRE(scoreId.is_valid());
        const ecs::component_info &info = fixture.registry.info(scoreId);
        CHECK(info.size == 8);
        CHECK(info.alignment == 4);
        CHECK(has_flag(info.flags, ecs::component_flags::DYNAMIC));
        REQUIRE(info.reflection != nullptr);
        REQUIRE(info.reflection->fields.size() == 2);
        CHECK(info.reflection->fields[0].name == "value");
        CHECK(info.reflection->fields[1].name == "bonus");
        CHECK(info.reflection->fields[1].offset == 4);

        const ecs::entity entity = fixture.create_entity();
        const script_entry_id user = fixture.add_script(entity, SCORE_USER);
        CHECK(fixture.world.has_raw(entity, scoreId));                 // OnCreate added it
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(fixture.field_i64(user, "HasScore") == 1);
        CHECK(fixture.field_i64(user, "LastValue") == 3);
        CHECK(fixture.field_f64(user, "LastBonus") == doctest::Approx(1.5));
        const void *raw = std::as_const(fixture.world).get_raw(entity, scoreId);
        REQUIRE(raw != nullptr);
        i32 value = 0;
        f32 bonus = 0.0f;
        std::memcpy(&value, raw, sizeof(value));
        std::memcpy(&bonus, static_cast<const std::byte *>(raw) + 4, sizeof(bonus));
        CHECK(value == 3);
        CHECK(bonus == doctest::Approx(1.5f));

        // Reloading re-registers the same name under the same id.
        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly(test_scripts_assembly("v2")));
        CHECK(fixture.registry.find(core::string_id("Conjure.TestScripts.Score")) == scoreId);
        CHECK(fixture.world.has_raw(entity, scoreId));
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(fixture.field_i64(user, "LastValue") == 4);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "regression - review 3 mirrors and [EcsComponent]s with bool fields use the managed layout [dotnet]" *
      doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};

        // The mirror: Marshal.OffsetOf would put layer at 4 and the size at 12; the managed layout is 8 bytes.
        const ecs::entity flagged = fixture.create_entity();
        test_flags flags = {};
        flags.active = true;
        flags.layer = 3;
        flags.mask = 0x0F0F;
        flags.weight = 2.5f;
        CONJURE_REQUIRE_OK(fixture.world.add<test_flags>(flagged, flags));
        const script_entry_id user = fixture.add_script(flagged, FLAGS_USER);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(fixture.field_i64(user, "Resolved") == 1);
        CHECK(fixture.field_i64(user, "ActiveSeen") == 1);
        CHECK(fixture.field_i64(user, "LayerSeen") == 3);
        CHECK(fixture.field_i64(user, "MaskSeen") == 0x0F0F);
        CHECK(fixture.field_f64(user, "WeightSeen") == doctest::Approx(2.5));
        const test_flags *written = std::as_const(fixture.world).get<test_flags>(flagged);
        REQUIRE(written != nullptr);
        CHECK_FALSE(written->active);
        CHECK(written->layer == 3);
        CHECK(written->mask == 0x0F0F);
        CHECK(written->weight == doctest::Approx(3.5f));

        // The [EcsComponent]: registered with the managed size and offsets, read and written through them.
        const ecs::component_id toggleId = fixture.registry.find(core::string_id("Conjure.TestScripts.Toggle"));
        REQUIRE(toggleId.is_valid());
        const ecs::component_info &info = fixture.registry.info(toggleId);
        CHECK(info.size == 8);
        CHECK(info.alignment == 4);
        REQUIRE(info.reflection != nullptr);
        REQUIRE(info.reflection->fields.size() == 4);
        CHECK(info.reflection->fields[0].name == "on");
        CHECK(info.reflection->fields[0].offset == 0);
        CHECK(info.reflection->fields[1].name == "level");
        CHECK(info.reflection->fields[1].offset == 1);
        CHECK(info.reflection->fields[2].name == "mask");
        CHECK(info.reflection->fields[2].offset == 2);
        CHECK(info.reflection->fields[3].name == "weight");
        CHECK(info.reflection->fields[3].offset == 4);
        const ecs::entity toggled = fixture.create_entity();
        const script_entry_id toggleUser = fixture.add_script(toggled, TOGGLE_USER);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(fixture.field_i64(toggleUser, "LastOn") == 1);
        CHECK(fixture.field_i64(toggleUser, "LastLevel") == 2);
        CHECK(fixture.field_i64(toggleUser, "LastMask") == 0x0101);
        CHECK(fixture.field_f64(toggleUser, "LastWeight") == doctest::Approx(1.5));
        const auto *raw = static_cast<const std::byte *>(std::as_const(fixture.world).get_raw(toggled, toggleId));
        REQUIRE(raw != nullptr);
        CHECK(raw[0] == std::byte{0});                                  // flipped by the script
        CHECK(raw[1] == std::byte{3});                                  // incremented
        u16 mask = 0;
        f32 weight = 0.0f;
        std::memcpy(&mask, raw + 2, sizeof(mask));
        std::memcpy(&weight, raw + 4, sizeof(weight));
        CHECK(mask == 0x0101);
        CHECK(weight == doctest::Approx(1.5f));
        CHECK(logs.count(core::log_level::ERR) == 0);
        CHECK(logs.count(core::log_level::WARN) == 0);
    }

    TEST_CASE(
      "regression - review 16 mirror ids are resolved again after a new handshake [dotnet]" * doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        {
            dotnet_fixture first = {};
            CONJURE_REQUIRE_OK(first.world.add<test_position>(first.create_entity()));
            const script_entry_id writer = first.add_script(first.create_entity(), POSITION_WRITER);
            first.runtime.run_phase(script_phase::UPDATE, 0.1f);
            CHECK(first.field_i64(writer, "Rows") == 1);
        }

        // A registry where test_position has another id: a cached ComponentType<TestPosition>.Id would
        // point at the wrong column.
        ecs::component_registry registry = {};
        CONJURE_REQUIRE_OK(register_script_component(registry));
        CONJURE_REQUIRE_OK(registry.register_component<test_velocity>());
        CONJURE_REQUIRE_OK(registry.register_component<test_flags>());
        CONJURE_REQUIRE_OK(registry.register_component<test_position>());
        ecs::world world(registry);
        script_runtime runtime(create_script_host(script_backend::DOTNET));
        runtime.attach_registry(&registry);
        runtime.attach_world(&world);
        script_runtime_config config = {};
        config.host.backend = script_backend::DOTNET;
        config.host.managedDirectory = managed_directory();
        config.gameAssemblyPath = test_scripts_assembly("v1");
        config.initialMode = script_mode::PLAY;
        CONJURE_REQUIRE_OK(runtime.initialize(config));
        const core::result<ecs::entity> positioned = world.create_entity();
        CONJURE_REQUIRE_OK(positioned);
        CONJURE_REQUIRE_OK(world.add<test_position>(positioned.value(), test_position{5.0f, 0.0f, 0.0f}));
        const core::result<ecs::entity> scripted = world.create_entity();
        CONJURE_REQUIRE_OK(scripted);
        const script_entry_id writer = runtime.add_script(scripted.value(), hash_of(POSITION_WRITER), {});
        runtime.flush_pending();
        runtime.run_phase(script_phase::UPDATE, 0.1f);
        core::result<core::json::document> collected = runtime.collect_entry_fields(writer);
        CONJURE_REQUIRE_OK(collected);
        CHECK(collected.value().root()["Rows"].as_i64() == 1);
        CHECK(std::as_const(world).get<test_position>(positioned.value())->x == doctest::Approx(6.0f));
        CHECK(logs.count(core::log_level::ERR) == 0);
        runtime.destroy_all(false);
        runtime.shutdown();
        runtime.attach_world(nullptr);
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F161: ChunkView.Get with the wrong component type throws [dotnet]" *
      doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        std::vector<ecs::entity> entities = {};
        for (u32 index = 0; index < 3; ++index) {
            const ecs::entity entity = fixture.create_entity();
            const test_position position{static_cast<f32>(index) + 0.5f, 2.0f, 3.0f};
            CONJURE_REQUIRE_OK(fixture.world.add<test_position>(entity, position));
            entities.push_back(entity);
        }
        const script_entry_id user = fixture.add_script(fixture.create_entity(), WRONG_COLUMN_USER);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(fixture.field_string(user, "Caught") == "InvalidOperationException");
        for (u32 index = 0; index < entities.size(); ++index) {
            CAPTURE(index);
            const test_position *position = std::as_const(fixture.world).get<test_position>(entities[index]);
            REQUIRE(position != nullptr);
            CHECK(*position == test_position{static_cast<f32>(index) + 0.5f, 2.0f, 3.0f});
        }
        CHECK(fixture.runtime.open_query_scope_count() == 0);
        CHECK_FALSE(fixture.world.is_locked());
        CHECK(logs.count(core::log_level::WARN) == 0);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F448: ComponentQuery.Begin outside a script phase names NOT_IN_PHASE [dotnet]" *
      doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        CONJURE_REQUIRE_OK(fixture.world.add<test_position>(fixture.create_entity()));
        // add_script flushes outside any phase: OnCreate runs there and the scope is refused.
        const script_entry_id probe = fixture.add_script(fixture.create_entity(), QUERY_IN_CREATE);
        const std::string message = fixture.field_string(probe, "Message");
        CHECK(message.find("NOT_IN_PHASE") != std::string::npos);
        CHECK(message.find("OnUpdate") != std::string::npos);
        CHECK(message.find("scripting status") == std::string::npos);
        CHECK(fixture.runtime.open_query_scope_count() == 0);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F449: an undescribable mirror is reflected and logged once [dotnet]" *
      doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        logs.allow_errors();
        dotnet_fixture fixture = {};
        const script_entry_id user = fixture.add_script(fixture.create_entity(), UNDESCRIBABLE_MIRROR_USER);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(fixture.field_i64(user, "Zeros") == 6);
        u32 describeErrors = 0;
        for (const std::string &message : logs.messages(core::log_level::ERR)) {
            if (message.find("AutoLayoutPosition") != std::string::npos) {
                ++describeErrors;
            }
        }
        CHECK(describeErrors == 1);
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F662: an [EcsComponent] enum field registers as its integer [dotnet]" *
      doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const ecs::component_id squadId = fixture.registry.find(core::string_id("Conjure.TestScripts.Squad"));
        REQUIRE(squadId.is_valid());
        const ecs::component_info &info = fixture.registry.info(squadId);
        CHECK(info.size == 8);
        REQUIRE(info.reflection != nullptr);
        REQUIRE(info.reflection->fields.size() == 2);
        CHECK(info.reflection->fields[1].name == "team");
        CHECK(info.reflection->fields[1].kind == core::reflect::field_kind::I32);
        CHECK(info.reflection->fields[1].offset == 4);

        const script_entry_id user = fixture.add_script(fixture.create_entity(), SQUAD_USER);
        const script_entry_id probe = fixture.add_script(fixture.create_entity(), DYNAMIC_SCHEMA_PROBE);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(fixture.field_i64(user, "LastTeam") == 2);               // Mode.Jump
        // A field a dynamic component cannot hold is refused by name before it reaches native.
        CHECK(fixture.field_i64(probe, "SchemaIsNull") == 1);
        const std::string refusal = fixture.field_string(probe, "Refusal");
        CHECK(refusal.find("Clip") != std::string::npos);
        CHECK(refusal.find("AssetRef") != std::string::npos);
        CHECK(logs.count(core::log_level::WARN) == 0);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F663: an [EcsComponent] keeps Pad<digits> fields and their alignment [dotnet]" *
      doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const ecs::component_id seatsId = fixture.registry.find(core::string_id("Conjure.TestScripts.Seats"));
        REQUIRE(seatsId.is_valid());
        const ecs::component_info &info = fixture.registry.info(seatsId);
        CHECK(info.size == 16);
        CHECK(info.alignment == 8);
        REQUIRE(info.reflection != nullptr);
        REQUIRE(info.reflection->fields.size() == 2);
        CHECK(info.reflection->fields[0].name == "row");
        CHECK(info.reflection->fields[1].name == "pad1");
        CHECK(info.reflection->fields[1].offset == 8);
        CHECK(logs.count(core::log_level::WARN) == 0);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("dod query - reopening a scope over the same types allocates nothing after warm-up") {
        log_capture logs = {};
        scripting_fixture fixture = {};
        // More rows than one chunk holds, in two archetypes, so a scope spans several chunk views.
        constexpr u32 POSITION_COUNT = 2000;
        for (u32 index = 0; index < POSITION_COUNT; ++index) {
            const ecs::entity entity = fixture.create_entity();
            CONJURE_REQUIRE_OK(fixture.world.add<test_position>(entity));
            if (index % 4 == 0) {
                CONJURE_REQUIRE_OK(fixture.world.add<test_velocity>(entity));
            }
        }
        const std::array<u32, 1> position = {fixture.registry.id_of<test_position>().value};
        u32 failures = 0;
        u32 chunksSeen = 0;
        const auto openAndClose = [&](u32 repetitions) {
            for (u32 repetition = 0; repetition < repetitions; ++repetition) {
                conjure_query_scope scope = {};
                const core::result<u32> opened = fixture.runtime.begin_query_scope(position, scope);
                if (!opened.has_value()) {
                    ++failures;
                    continue;
                }
                chunksSeen = static_cast<u32>(scope.chunk_count);
                fixture.runtime.end_query_scope(opened.value());
            }
        };

        // Warm-up: the first scope builds the cached query and the reusable scope state.
        fixture.null_host().set_phase_hook([&](script_phase, f32) {
            openAndClose(1);
        });
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(failures == 0);
        CHECK(chunksSeen >= 2);                                         // at least one chunk per archetype
        CHECK(fixture.runtime.stats().cachedQueries == 1);

        // Steady state: 100 scopes over the same type set take no heap block (08_testing.md 7.1).
        fixture.null_host().set_phase_hook([&](script_phase, f32) {
            CONJURE_CHECK_NO_ALLOCATIONS(openAndClose(100));
        });
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        fixture.null_host().set_phase_hook({});
        CHECK(failures == 0);
        CHECK(fixture.runtime.stats().cachedQueries == 1);
        CHECK(fixture.runtime.open_query_scope_count() == 0);
        CHECK_FALSE(fixture.world.is_locked());
        CHECK(logs.count(core::log_level::WARN) == 0);
    }

    TEST_CASE("dod query - a cached query follows new archetypes and is dropped with the world") {
        log_capture logs = {};
        scripting_fixture fixture = {};
        const ecs::entity first = fixture.create_entity();
        CONJURE_REQUIRE_OK(fixture.world.add<test_position>(first));
        const std::array<u32, 1> position = {fixture.registry.id_of<test_position>().value};
        const std::array<u32, 2> both = {position[0], fixture.registry.id_of<test_velocity>().value};
        const std::array<u32, 2> swapped = {both[1], both[0]};
        const std::array<std::span<const u32>, 3> requests = {position, both, swapped};
        std::vector<i32> chunkCounts = {};
        std::array<void *, 2> bothColumns = {};
        std::array<void *, 2> swappedColumns = {};
        fixture.null_host().set_phase_hook([&](script_phase, f32) {
            for (usize request = 0; request < requests.size(); ++request) {
                conjure_query_scope scope = {};
                const core::result<u32> opened = fixture.runtime.begin_query_scope(requests[request], scope);
                REQUIRE(opened.has_value());
                chunkCounts.push_back(scope.chunk_count);
                if (scope.chunk_count > 0 && request == 1) {
                    bothColumns = {scope.chunks[0].columns[0], scope.chunks[0].columns[1]};
                }
                if (scope.chunk_count > 0 && request == 2) {
                    swappedColumns = {scope.chunks[0].columns[0], scope.chunks[0].columns[1]};
                }
                fixture.runtime.end_query_scope(opened.value());
            }
        });
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        REQUIRE(chunkCounts.size() == 3);
        CHECK(chunkCounts[0] == 1);
        CHECK(chunkCounts[1] == 0);
        CHECK(chunkCounts[2] == 0);
        CHECK(fixture.runtime.stats().cachedQueries == 2);              // {position} and {position, velocity}

        // An archetype created after the first scope is picked up by the cached query; the columns keep
        // the order of the request even though both orders share one cached query.
        const ecs::entity second = fixture.create_entity();
        CONJURE_REQUIRE_OK(fixture.world.add<test_position>(second));
        CONJURE_REQUIRE_OK(fixture.world.add<test_velocity>(second));
        chunkCounts.clear();
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        fixture.null_host().set_phase_hook({});
        REQUIRE(chunkCounts.size() == 3);
        CHECK(chunkCounts[0] == 2);
        CHECK(chunkCounts[1] == 1);
        CHECK(chunkCounts[2] == 1);
        CHECK(bothColumns[0] != nullptr);
        CHECK(bothColumns[0] == swappedColumns[1]);
        CHECK(bothColumns[1] == swappedColumns[0]);
        CHECK(fixture.runtime.stats().cachedQueries == 2);
        CHECK(logs.count(core::log_level::WARN) == 0);

        // The cache belongs to the world: detaching drops it.
        fixture.runtime.attach_world(nullptr);
        CHECK(fixture.runtime.stats().cachedQueries == 0);
        fixture.runtime.attach_world(&fixture.world);
    }

    TEST_CASE("dod query - a scope's chunk views stay put while other scopes open and close around it") {
        log_capture logs = {};
        scripting_fixture fixture = {};
        for (u32 index = 0; index < 50; ++index) {
            CONJURE_REQUIRE_OK(fixture.world.add<test_position>(fixture.create_entity()));
        }
        const std::array<u32, 1> position = {fixture.registry.id_of<test_position>().value};
        bool viewsStable = false;
        fixture.null_host().set_phase_hook([&](script_phase, f32) {
            conjure_query_scope outer = {};
            const core::result<u32> outerId = fixture.runtime.begin_query_scope(position, outer);
            REQUIRE(outerId.has_value());
            REQUIRE(outer.chunk_count == 1);
            const conjure_chunk_view *outerViews = outer.chunks;
            const void *outerColumn = outer.chunks[0].columns[0];

            // Nested scopes beyond the reserved count, then the outer one closed first (not LIFO).
            std::array<conjure_query_scope, script_runtime::QUERY_SCOPE_RESERVE + 2> inner = {};
            std::array<u32, script_runtime::QUERY_SCOPE_RESERVE + 2> innerIds = {};
            for (usize index = 0; index < inner.size(); ++index) {
                const core::result<u32> opened = fixture.runtime.begin_query_scope(position, inner[index]);
                REQUIRE(opened.has_value());
                innerIds[index] = opened.value();
            }
            fixture.runtime.end_query_scope(outerId.value());
            viewsStable = true;
            for (usize index = 0; index < inner.size(); ++index) {
                CAPTURE(index);
                CHECK(inner[index].chunk_count == 1);
                CHECK(inner[index].chunks[0].columns[0] == outerColumn);
                CHECK(inner[index].chunks[0].count == 50);
                viewsStable = viewsStable && inner[index].chunks != outerViews;
                fixture.runtime.end_query_scope(innerIds[index]);
            }
        });
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        fixture.null_host().set_phase_hook({});
        CHECK(viewsStable);
        CHECK(fixture.runtime.open_query_scope_count() == 0);
        CHECK_FALSE(fixture.world.is_locked());
        CHECK(logs.count(core::log_level::WARN) == 0);
    }

    TEST_CASE("regression - review 4 a request with a repeated id shares the cached query of its set") {
        log_capture logs = {};
        scripting_fixture fixture = {};
        for (u32 index = 0; index < 10; ++index) {
            CONJURE_REQUIRE_OK(fixture.world.add<test_position>(fixture.create_entity()));
        }
        const u32 positionId = fixture.registry.id_of<test_position>().value;
        const std::array<u32, 1> single = {positionId};
        const std::array<u32, 2> repeated = {positionId, positionId};
        std::array<i32, 2> chunkCounts = {};
        std::array<const void *, 2> repeatedColumns = {};
        const void *singleColumn = nullptr;
        fixture.null_host().set_phase_hook([&](script_phase, f32) {
            conjure_query_scope scope = {};
            const core::result<u32> singleId = fixture.runtime.begin_query_scope(single, scope);
            REQUIRE(singleId.has_value());
            REQUIRE(scope.chunk_count == 1);
            chunkCounts[0] = scope.chunk_count;
            singleColumn = scope.chunks[0].columns[0];
            fixture.runtime.end_query_scope(singleId.value());

            const core::result<u32> repeatedId = fixture.runtime.begin_query_scope(repeated, scope);
            REQUIRE(repeatedId.has_value());
            REQUIRE(scope.chunk_count == 1);
            chunkCounts[1] = scope.chunk_count;
            repeatedColumns = {scope.chunks[0].columns[0], scope.chunks[0].columns[1]};
            fixture.runtime.end_query_scope(repeatedId.value());
        });
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        fixture.null_host().set_phase_hook({});
        CHECK(chunkCounts[0] == 1);
        CHECK(chunkCounts[1] == 1);
        // The columns follow the request, so both name the one position column ...
        CHECK(singleColumn != nullptr);
        CHECK(repeatedColumns[0] == singleColumn);
        CHECK(repeatedColumns[1] == singleColumn);
        // ... and {a, a} is the component set {a}: one cached query, not two.
        CHECK(fixture.runtime.stats().cachedQueries == 1);
        CHECK(fixture.runtime.cached_query_count() == 1);
        CHECK(fixture.runtime.open_query_scope_count() == 0);
        CHECK(logs.count(core::log_level::WARN) == 0);
    }

    TEST_CASE("regression - review 4 nested scopes closed outer-first reuse the spilled state without allocating") {
        log_capture logs = {};
        scripting_fixture fixture = {};
        // More chunks than a state holds inline: 14,000 rows of {position} are 18 chunks of 819, plus the one
        // chunk of the {position, velocity} archetype, which is also the whole small scope.
        constexpr u32 POSITION_COUNT = 14000;
        constexpr u32 MOVING_COUNT = 10;
        for (u32 index = 0; index < POSITION_COUNT; ++index) {
            CONJURE_REQUIRE_OK(fixture.world.add<test_position>(fixture.create_entity()));
        }
        for (u32 index = 0; index < MOVING_COUNT; ++index) {
            const ecs::entity entity = fixture.create_entity();
            CONJURE_REQUIRE_OK(fixture.world.add<test_position>(entity));
            CONJURE_REQUIRE_OK(fixture.world.add<test_velocity>(entity));
        }
        const std::array<u32, 1> large = {fixture.registry.id_of<test_position>().value};
        const std::array<u32, 2> small = {large[0], fixture.registry.id_of<test_velocity>().value};
        std::span<const u32> outerRequest = large;
        std::span<const u32> innerRequest = small;
        SUBCASE("the large scope is the outer one") {
        }
        SUBCASE("the large scope is the inner one") {
            outerRequest = small;
            innerRequest = large;
        }

        // Closing the outer scope first puts its state on the free list below the inner one's, so a last-in
        // first-out reuse would hand the inline-only state to the large scope on the next frame.
        u32 failures = 0;
        i32 largeChunks = 0;
        i32 smallChunks = 0;
        const auto nestClosedOuterFirst = [&]() {
            conjure_query_scope outer = {};
            conjure_query_scope inner = {};
            const core::result<u32> outerId = fixture.runtime.begin_query_scope(outerRequest, outer);
            const core::result<u32> innerId = fixture.runtime.begin_query_scope(innerRequest, inner);
            if (!outerId.has_value() || !innerId.has_value()) {
                ++failures;
                return;                                                 // force-closed at phase end
            }
            largeChunks = std::max(outer.chunk_count, inner.chunk_count);
            smallChunks = std::min(outer.chunk_count, inner.chunk_count);
            fixture.runtime.end_query_scope(outerId.value());
            fixture.runtime.end_query_scope(innerId.value());
        };

        // Warm-up: both states exist and the one that carried the large scope has spilled.
        fixture.null_host().set_phase_hook([&](script_phase, f32) {
            nestClosedOuterFirst();
        });
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(failures == 0);
        CHECK(largeChunks > static_cast<i32>(script_runtime::QUERY_SCOPE_INLINE_CHUNKS));
        CHECK(smallChunks == 1);

        // Steady state: the large scope finds the spilled state again, whatever the close order was.
        fixture.null_host().set_phase_hook([&](script_phase, f32) {
            CONJURE_CHECK_NO_ALLOCATIONS(nestClosedOuterFirst());
        });
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        fixture.null_host().set_phase_hook({});
        CHECK(failures == 0);
        CHECK(fixture.runtime.stats().cachedQueries == 2);
        CHECK(fixture.runtime.open_query_scope_count() == 0);
        CHECK_FALSE(fixture.world.is_locked());
        CHECK(logs.count(core::log_level::WARN) == 0);
    }

    TEST_CASE("dod query - PositionWriter frames allocate nothing after warm-up [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        constexpr u32 POSITION_COUNT = 2000;
        for (u32 index = 0; index < POSITION_COUNT; ++index) {
            const ecs::entity entity = fixture.create_entity();
            CONJURE_REQUIRE_OK(fixture.world.add<test_position>(entity));
            if (index % 4 == 0) {
                CONJURE_REQUIRE_OK(fixture.world.add<test_velocity>(entity));
            }
        }
        const script_entry_id writer = fixture.add_script(fixture.create_entity(), POSITION_WRITER);

        // The update phase is what scripting.update_10k_dod measures: frames 3..10 are the steady state.
        constexpr u32 FRAME_COUNT = 10;
        std::array<u64, FRAME_COUNT> allocations = {};
        for (u32 frame = 0; frame < FRAME_COUNT; ++frame) {
            allocation_scope scope = {};
            fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
            allocations[frame] = scope.allocation_count();
        }
        for (u32 frame = 2; frame < FRAME_COUNT; ++frame) {
            CAPTURE(frame);
            CHECK(allocations[frame] == 0);
        }
        CHECK(fixture.field_i64(writer, "Rows") == static_cast<i64>(POSITION_COUNT) * FRAME_COUNT);
        CHECK(fixture.runtime.stats().cachedQueries == 1);
        CHECK(fixture.runtime.open_query_scope_count() == 0);
        CHECK(logs.count(core::log_level::WARN) == 0);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("dod query - begin_query_scope rejects bad requests natively [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        dotnet_fixture fixture = {};
        conjure_query_scope scope = {};
        const std::array<u32, 1> position = {fixture.registry.id_of<test_position>().value};
        CONJURE_CHECK_ERROR(fixture.runtime.begin_query_scope(position, scope), script_error::NOT_IN_PHASE);
        const std::array<u32, 9> tooMany = {};
        CONJURE_CHECK_ERROR(fixture.runtime.begin_query_scope(tooMany, scope), script_error::NOT_IN_PHASE);
        CHECK(scope.chunk_count == 0);
        CHECK(fixture.runtime.open_query_scope_count() == 0);
        fixture.runtime.end_query_scope(12345);                       // unknown scopes are ignored
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F676 a facade's Exists is false when its component type is not registered "
      "[dotnet]" *
      doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        // The fixture's registry never registers the audio or physics components, as in a build without those
        // subsystems: ComponentType<T>.Id resolves to 0 (one WARN per name), so Exists answers false instead of
        // throwing, while Entity.HasComponent<T> keeps throwing so a drifted mirror stays loud.
        dotnet_fixture fixture = {};
        const script_entry_id probe = fixture.add_script(fixture.create_entity(), FACADE_EXISTS_PROBE);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(fixture.field_i64(probe, "AudioSourceExists") == 0);
        CHECK(fixture.field_i64(probe, "RigidbodyExists") == 0);
        CHECK(fixture.field_i64(probe, "CharacterControllerExists") == 0);
        CHECK(fixture.field_string(probe, "HasComponentThrows") == "InvalidOperationException");
        CHECK(logs.contains(core::log_level::WARN, "component_lookup_type: no component named"));
        CHECK(logs.count(core::log_level::ERR) == 0);
    }
}
