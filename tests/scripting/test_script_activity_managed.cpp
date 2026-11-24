#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "scripting_test_support.h"

#include "conjure/scripting/native_api.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_runtime.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

// OnEnable, OnDisable and inactive scripts sleeping (code sweep W3-CS-L4, owner option A) on the real CLR, with the
// probes of Conjure.TestScripts/LifecycleProbe.cs. The scripting module has no scene, so the fixture's test
// components stand in for the activity tags: test_position (C# TestPosition) plays scene.disabled and
// test_velocity plays scene.inactive. Adding or removing one outside a phase is what a native system or the
// editor does; ActivityDisabler adds TestPosition from C# inside its OnUpdate.

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;
using namespace scripting_test;

namespace {

    constexpr std::string_view ACTIVITY_PROBE = "Conjure.TestScripts.ActivityProbe";
    constexpr std::string_view ACTIVITY_WITNESS = "Conjure.TestScripts.ActivityWitness";
    constexpr std::string_view ACTIVITY_DISABLER = "Conjure.TestScripts.ActivityDisabler";
    constexpr std::string_view ENABLE_THROWER = "Conjure.TestScripts.EnableThrower";
    constexpr std::string_view COLLIDER = "Conjure.TestScripts.Collider";

    void disable(dotnet_fixture &fixture, ecs::entity entity) {
        CONJURE_REQUIRE_OK(fixture.world.add<test_position>(entity));
    }

    void enable(dotnet_fixture &fixture, ecs::entity entity) {
        CONJURE_REQUIRE_OK(fixture.world.remove<test_position>(entity));
    }
}

TEST_SUITE("scripting/script_activity_managed") {

    TEST_CASE("script_activity - the callbacks follow the entity's activity in order [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        use_test_activity_tags(fixture.registry, fixture.runtime);
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id probe = fixture.add_script(entity, ACTIVITY_PROBE);
        const script_entry_id witness = fixture.add_script(fixture.create_entity(), ACTIVITY_WITNESS);
        CHECK(fixture.field_string(probe, "Log") == "Create,Enable,");
        // IsActive: true from OnCreate on for an instance created active, false inside OnDisable and OnDestroy.
        CHECK(fixture.field_i64(probe, "ActiveInCreate") == 1);
        CHECK(fixture.field_i64(probe, "ActiveInEnable") == 1);

        fixture.run_frame();
        fixture.run_frame();
        CHECK(fixture.field_string(probe, "Log") == "Create,Enable,Start,Update,Update,");

        disable(fixture, entity);
        fixture.run_frame();
        fixture.run_frame();
        CHECK(fixture.field_string(probe, "Log") == "Create,Enable,Start,Update,Update,Disable,");
        CHECK(fixture.field_i64(probe, "ActiveInDisable") == 0);

        enable(fixture, entity);
        fixture.run_frame();
        CHECK(fixture.field_string(probe, "Log") == "Create,Enable,Start,Update,Update,Disable,Enable,Update,");
        CHECK(fixture.field_i64(probe, "ActiveDuringUpdate") == 1);

        disable(fixture, entity);
        fixture.run_frame();
        fixture.runtime.request_entity_destroy(entity);
        fixture.runtime.flush_pending();
        fixture.run_frame();
        CHECK(
          fixture.field_string(witness, "LastDestroyedLog")
          == "Create,Enable,Start,Update,Update,Disable,Enable,Update,Disable,Destroy,"
        );
        CHECK(fixture.field_i64(witness, "LastActiveInDestroy") == 0);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("script_activity - an instance created inactive waits for its first activation [dotnet]"
              * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        dotnet_fixture fixture = {};
        use_test_activity_tags(fixture.registry, fixture.runtime);

        SUBCASE("the entity disabled itself") {
            const ecs::entity entity = fixture.create_entity();
            disable(fixture, entity);
            const script_entry_id probe = fixture.add_script(entity, ACTIVITY_PROBE);
            fixture.run_frame();
            CHECK(fixture.field_string(probe, "Log") == "Create,");
            CHECK(fixture.field_i64(probe, "ActiveInCreate") == 0);
            CHECK_FALSE(fixture.runtime.entry(probe)->has_flag(script_entry_flags::STARTED));

            enable(fixture, entity);
            fixture.run_frame();
            CHECK(fixture.field_string(probe, "Log") == "Create,Enable,Start,Update,");
            CHECK(fixture.runtime.entry(probe)->has_flag(script_entry_flags::STARTED));
        }
        SUBCASE("under a disabled ancestor, entered Play inactive") {
            fixture.runtime.set_mode(script_mode::EDIT);
            const ecs::entity entity = fixture.create_entity();
            CONJURE_REQUIRE_OK(fixture.world.add<test_velocity>(entity));
            const script_entry_id probe = fixture.runtime.add_script(entity, hash_of(ACTIVITY_PROBE), {});
            REQUIRE(probe.is_valid());
            fixture.runtime.set_mode(script_mode::PLAY);
            fixture.run_frame();
            CHECK(fixture.field_string(probe, "Log") == "Create,");
            CHECK(fixture.field_i64(probe, "ActiveInCreate") == 0);

            CONJURE_REQUIRE_OK(fixture.world.remove<test_velocity>(entity));
            fixture.run_frame();
            CHECK(fixture.field_string(probe, "Log") == "Create,Enable,Start,Update,");
            CHECK(fixture.field_i64(probe, "ActiveInEnable") == 1);
        }
    }

    TEST_CASE("script_activity - destroying an active script runs OnDisable then OnDestroy [dotnet]"
              * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        dotnet_fixture fixture = {};
        use_test_activity_tags(fixture.registry, fixture.runtime);
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id witness = fixture.add_script(fixture.create_entity(), ACTIVITY_WITNESS);

        SUBCASE("Entity.Destroy") {
            static_cast<void>(fixture.add_script(entity, ACTIVITY_PROBE));
            fixture.run_frame();
            fixture.runtime.request_entity_destroy(entity);
            fixture.runtime.flush_pending();
        }
        SUBCASE("the script removed") {
            const script_entry_id probe = fixture.add_script(entity, ACTIVITY_PROBE);
            fixture.run_frame();
            fixture.runtime.remove_script(entity, probe);
        }
        SUBCASE("Play stopped") {
            static_cast<void>(fixture.add_script(entity, ACTIVITY_PROBE));
            fixture.run_frame();
            fixture.runtime.set_mode(script_mode::EDIT);
            fixture.runtime.set_mode(script_mode::PLAY);
        }
        fixture.run_frame();
        CHECK(fixture.field_string(witness, "LastDestroyedLog") == "Create,Enable,Start,Update,Disable,Destroy,");
        // Cleared before the callbacks: IsActive reads false inside OnDisable and OnDestroy of an active instance.
        CHECK(fixture.field_i64(witness, "LastActiveInDestroy") == 0);
    }

    TEST_CASE("script_activity - a hot reload runs OnReload only [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        dotnet_fixture fixture = {};
        use_test_activity_tags(fixture.registry, fixture.runtime);
        const ecs::entity awake = fixture.create_entity();
        const ecs::entity asleep = fixture.create_entity();
        const script_entry_id awakeProbe = fixture.add_script(awake, ACTIVITY_PROBE);
        const script_entry_id asleepProbe = fixture.add_script(asleep, ACTIVITY_PROBE);
        fixture.run_frame();
        disable(fixture, asleep);
        fixture.run_frame();
        REQUIRE(fixture.field_string(awakeProbe, "Log") == "Create,Enable,Start,Update,Update,");
        REQUIRE(fixture.field_string(asleepProbe, "Log") == "Create,Enable,Start,Update,Disable,");

        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly(test_scripts_assembly("v2")));
        CHECK(fixture.field_string(awakeProbe, "Log") == "Create,Enable,Start,Update,Update,Reload,");
        CHECK(fixture.field_string(asleepProbe, "Log") == "Create,Enable,Start,Update,Disable,Reload,");
        // The recreated instance learns its state before OnReload (CREATE_FLAG_INACTIVE).
        CHECK(fixture.field_i64(awakeProbe, "ActiveInReload") == 1);
        CHECK(fixture.field_i64(asleepProbe, "ActiveInReload") == 0);

        // The recreated instances keep their state: the sleeping one still gets no update.
        fixture.run_frame();
        CHECK(fixture.field_string(awakeProbe, "Log") == "Create,Enable,Start,Update,Update,Reload,Update,");
        CHECK(fixture.field_string(asleepProbe, "Log") == "Create,Enable,Start,Update,Disable,Reload,");
    }

    TEST_CASE("script_activity - an OnEnable that throws three times is disabled alone [dotnet]"
              * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        logs.allow_errors();                    // OnEnable throws on purpose
        dotnet_fixture fixture = {};
        use_test_activity_tags(fixture.registry, fixture.runtime);
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id thrower = fixture.add_script(entity, ENABLE_THROWER);
        constexpr i64 TOGGLES = 4;
        for (i64 toggle = 0; toggle < TOGGLES; ++toggle) {
            CAPTURE(toggle);
            fixture.run_frame();
            disable(fixture, entity);
            fixture.run_frame();
            enable(fixture, entity);
        }
        fixture.run_frame();

        // One OnEnable at creation plus one per toggle until the third consecutive fault disabled it.
        CHECK(fixture.field_i64(thrower, "EnableCalls") == 3);
        CHECK(fixture.field_i64(thrower, "DisableCalls") == TOGGLES);
        CHECK(fixture.field_i64(thrower, "Updates") == TOGGLES + 1);
        CHECK(fixture.field_i64(thrower, "DisabledSeen") == callback_bit(script_callback::ENABLE));
        CHECK(fixture.runtime.entry(thrower)->has_flag(script_entry_flags::FAULTED));
        CHECK(logs.count(core::log_level::ERR) >= 3);
    }

    TEST_CASE("script_activity - a script that disables its entity lets the rest of the phase run [dotnet]"
              * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        dotnet_fixture fixture = {};
        use_test_activity_tags(fixture.registry, fixture.runtime);
        const ecs::entity entity = fixture.create_entity();
        // ActivityDisabler sorts before ActivityProbe, so it updates first; SawDisabledTag checks that order.
        static_cast<void>(fixture.add_script(entity, ACTIVITY_DISABLER, R"({"DisableOnUpdate":1})"));
        const script_entry_id probe = fixture.add_script(entity, ACTIVITY_PROBE);

        fixture.runtime.run_phase(script_phase::UPDATE, 1.0f / 60.0f);
        CHECK(fixture.world.has<test_position>(entity));
        // The probe still had its Update in that phase; OnDisable ran at the phase end, before the next phase.
        CHECK(fixture.field_string(probe, "Log") == "Create,Enable,Start,Update,Disable,");
        CHECK(fixture.field_i64(probe, "ActiveDuringUpdate") == 1);
        CHECK(fixture.field_i64(probe, "SawDisabledTag") == 1);

        fixture.run_frame();
        CHECK(fixture.field_string(probe, "Log") == "Create,Enable,Start,Update,Disable,");
    }

    TEST_CASE("regression - code sweep W3-CS-L4 an inactive entity's scripts sleep [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        dotnet_fixture fixture = {};
        use_test_activity_tags(fixture.registry, fixture.runtime);
        const ecs::entity a = fixture.create_entity();
        const ecs::entity b = fixture.create_entity();
        const script_entry_id probe = fixture.add_script(a, ACTIVITY_PROBE);
        const script_entry_id collider = fixture.add_script(a, COLLIDER);
        const script_entry_id other = fixture.add_script(b, COLLIDER);
        fixture.run_frame();
        disable(fixture, a);
        fixture.runtime.flush_pending();
        // run_frame runs FIXED_UPDATE, UPDATE and LATE_UPDATE: none of them reaches the sleeping probe.
        const i64 fixedUpdates = fixture.field_i64(probe, "FixedUpdates");
        const i64 lateUpdates = fixture.field_i64(probe, "LateUpdates");
        REQUIRE(fixedUpdates == 1);
        REQUIRE(lateUpdates == 1);

        constexpr i64 FRAMES = 3;
        const std::array<collision_event, 1> events = {collision_between(a, b, collision_kind::COLLISION_STAY)};
        for (i64 frame = 0; frame < FRAMES; ++frame) {
            CAPTURE(frame);
            fixture.runtime.dispatch_collision_events(events);
            fixture.run_frame();
        }
        CHECK(fixture.field_string(probe, "Log") == "Create,Enable,Start,Update,Disable,");
        CHECK(fixture.field_i64(probe, "FixedUpdates") == fixedUpdates);
        CHECK(fixture.field_i64(probe, "LateUpdates") == lateUpdates);
        CHECK(fixture.field_i64(collider, "Stays") == 0);
        CHECK(fixture.field_i64(other, "Stays") == FRAMES);

        enable(fixture, a);
        fixture.run_frame();
        CHECK(fixture.field_i64(probe, "FixedUpdates") == fixedUpdates + 1);
        CHECK(fixture.field_i64(probe, "LateUpdates") == lateUpdates + 1);
    }

    TEST_CASE("regression - code sweep W3-CS-L4 OnStart runs once, and only once the instance is active [dotnet]"
              * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        dotnet_fixture fixture = {};
        use_test_activity_tags(fixture.registry, fixture.runtime);
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id probe = fixture.add_script(entity, ACTIVITY_PROBE);
        disable(fixture, entity);
        fixture.runtime.flush_pending();
        REQUIRE(fixture.field_string(probe, "Log") == "Create,Enable,Disable,");

        SUBCASE("enabled again before its first phase: listed twice for OnStart, started once") {
            enable(fixture, entity);
            fixture.runtime.flush_pending();
            fixture.run_frame();
            CHECK(fixture.field_string(probe, "Log") == "Create,Enable,Disable,Enable,Start,Update,");
            CHECK(fixture.runtime.entry(probe)->has_flag(script_entry_flags::STARTED));
        }
        SUBCASE("still inactive at its first phase: OnStart waits for the activation") {
            fixture.run_frame();
            CHECK(fixture.field_string(probe, "Log") == "Create,Enable,Disable,");
            CHECK_FALSE(fixture.runtime.entry(probe)->has_flag(script_entry_flags::STARTED));

            enable(fixture, entity);
            fixture.run_frame();
            CHECK(fixture.field_string(probe, "Log") == "Create,Enable,Disable,Enable,Start,Update,");
            CHECK(fixture.runtime.entry(probe)->has_flag(script_entry_flags::STARTED));
        }
    }

    TEST_CASE("script_activity - a snapshot restore of an active script runs OnReload only [dotnet]"
              * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        use_test_activity_tags(fixture.registry, fixture.runtime);
        const ecs::entity entity = fixture.create_entity();
        static_cast<void>(fixture.add_script(entity, ACTIVITY_PROBE));
        const script_entry_id witness = fixture.add_script(fixture.create_entity(), ACTIVITY_WITNESS);
        fixture.run_frame();
        const core::result<core::json::document> snapshot = fixture.runtime.serialize_entity_scripts(entity);
        CONJURE_REQUIRE_OK(snapshot);

        CONJURE_REQUIRE_OK(fixture.runtime.deserialize_entity_scripts(entity, snapshot.value().root()));
        fixture.runtime.flush_pending();
        fixture.run_frame();
        const std::vector<script_entry_id> scripts = fixture.runtime.scripts_of(entity);
        REQUIRE(scripts.size() == 1);
        // The fields came back from the snapshot; the restored instance adds Reload, no Enable and no Start.
        CHECK(fixture.field_string(scripts[0], "Log") == "Create,Enable,Start,Update,Reload,Update,");
        // No probe was destroyed with callbacks: no OnDisable, no OnDestroy.
        CHECK(fixture.field_string(witness, "LastDestroyedLog").empty());
        CHECK(logs.count(core::log_level::ERR) == 0);
    }
}
