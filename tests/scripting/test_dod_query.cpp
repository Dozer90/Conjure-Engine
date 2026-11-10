#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "scripting_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/string_id.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/world.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_host.h"
#include "conjure/scripting/script_runtime.h"
#include "conjure/scripting/scripting_error.h"

#include <array>
#include <cstddef>
#include <cstring>
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
}
