#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "scripting_test_support.h"

#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_runtime.h"

#include <string>
#include <string_view>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;
using namespace scripting_test;

namespace {

    constexpr std::string_view THROWER = "Conjure.TestScripts.Thrower";
    constexpr std::string_view CREATE_THROWER = "Conjure.TestScripts.CreateThrower";
    constexpr std::string_view RELOAD_THROWER = "Conjure.TestScripts.ReloadThrower";
    constexpr std::string_view FIXED_CREATE_THROWER = "Conjure.TestScripts.FixedCreateThrower";
    constexpr std::string_view ADD_SCRIPT_STATUS_PROBE = "Conjure.TestScripts.AddScriptStatusProbe";
    constexpr std::string_view WRONG_THREAD = "Conjure.TestScripts.WrongThreadProbe";
    constexpr std::string_view NULL_ENTRY = "Conjure.TestScripts.NullEntryProbe";
}

TEST_SUITE("scripting/script_exceptions") {

    TEST_CASE("script exceptions - a throwing update is disabled after three faults [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        logs.allow_errors();
        dotnet_fixture fixture = {};
        const script_entry_id thrower = fixture.add_script(fixture.create_entity(), THROWER, R"({"Pattern": 7})");
        for (int frame = 0; frame < 6; ++frame) {
            fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        }
        CHECK(fixture.field_i64(thrower, "Updates") == 3);          // disabled after the third fault
        CHECK(logs.count(core::log_level::ERR) == 3);
        CHECK(logs.contains(core::log_level::ERR, "Thrower faults on update 2"));
        CHECK(logs.contains(core::log_level::WARN, "disabled after"));
        CHECK(fixture.runtime.entry(thrower)->has_flag(script_entry_flags::FAULTED));
        CHECK(fixture.runtime.entry(thrower)->is_live());            // the instance survives
    }

    TEST_CASE("script exceptions - a success resets the consecutive fault counter [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        logs.allow_errors();
        dotnet_fixture fixture = {};
        // Faults on updates 0, 1, 3, 4; success on 2 and from 5 on: never three in a row.
        const script_entry_id thrower = fixture.add_script(fixture.create_entity(), THROWER, R"({"Pattern": 27})");
        for (int frame = 0; frame < 8; ++frame) {
            fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        }
        CHECK(fixture.field_i64(thrower, "Updates") == 8);
        CHECK(logs.count(core::log_level::ERR) == 4);
        CHECK_FALSE(fixture.runtime.entry(thrower)->has_flag(script_entry_flags::FAULTED));
    }

    TEST_CASE("script exceptions - a throwing OnCreate disables every callback [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        logs.allow_errors();
        dotnet_fixture fixture = {};
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id id = fixture.add_script(entity, CREATE_THROWER);
        CHECK(fixture.runtime.entry(id)->is_live());
        CHECK(fixture.runtime.entry(id)->has_flag(script_entry_flags::FAULTED));
        CHECK(logs.count(core::log_level::ERR) == 1);
        // The managed guard reports the CREATE fault to native, which logs its own WARN for it.
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK(logs.contains(core::log_level::WARN, "OnCreate of 'Conjure.TestScripts.CreateThrower'"));
        fixture.run_frame();
        fixture.run_frame();
        CHECK(fixture.field_i64(id, "Updates") == 0);
        fixture.runtime.destroy_all(true);
        CHECK(fixture.runtime.stats().liveInstances == 0);
        CHECK(logs.count(core::log_level::ERR) == 1);                 // OnDestroy was disabled too
    }

    TEST_CASE(
      "script exceptions - a throwing OnReload disables every callback of the reloaded instance [dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        logs.allow_errors();
        dotnet_fixture fixture = {};
        const script_entry_id id = fixture.add_script(fixture.create_entity(), RELOAD_THROWER);
        fixture.run_frame();
        CHECK(fixture.field_i64(id, "Updates") == 1);
        CHECK(logs.count(core::log_level::ERR) == 0);

        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly(test_scripts_assembly("v2")));
        CHECK(fixture.runtime.entry(id)->is_live());
        CHECK(fixture.runtime.entry(id)->has_flag(script_entry_flags::FAULTED));
        CHECK(logs.count(core::log_level::ERR) == 1);
        CHECK(logs.contains(core::log_level::ERR, "ReloadThrower faults in OnReload"));
        // The managed guard reports the RELOAD fault to native, which logs its own WARN for it.
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK(logs.contains(core::log_level::WARN, "OnReload of 'Conjure.TestScripts.ReloadThrower'"));
        CHECK(logs.contains(core::log_level::WARN, "every callback of the instance is disabled"));
        fixture.run_frame();
        fixture.run_frame();
        CHECK(fixture.field_i64(id, "Updates") == 1);
        fixture.runtime.destroy_all(true);
        CHECK(fixture.runtime.stats().liveInstances == 0);
        CHECK_FALSE(logs.contains(core::log_level::INFO, "ReloadThrower.OnDestroy ran"));    // disabled too
        CHECK(logs.count(core::log_level::ERR) == 1);
    }

    TEST_CASE(
      "regression - code sweep OWN-RELOAD an OnCreate that threw runs again after the reload that fixed it [dotnet]" *
      doctest::timeout(120)
    ) {
        // Owner ruling 2026-11-24: an OnCreate that threw did not complete, so the reload to the fixed v2 runs it
        // once more (then OnStart, which the disabled instance never ran), and OnReload only after it returned.
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        logs.allow_errors();
        dotnet_fixture fixture = {};
        const script_entry_id id = fixture.add_script(fixture.create_entity(), FIXED_CREATE_THROWER);
        fixture.run_frame();
        CHECK(fixture.runtime.entry(id)->has_flag(script_entry_flags::FAULTED));
        CHECK_FALSE(fixture.runtime.entry(id)->has_flag(script_entry_flags::CREATED));
        CHECK(fixture.field_i64(id, "CreateCalls") == 1);
        CHECK(fixture.field_i64(id, "StartCalls") == 0);
        CHECK(fixture.field_i64(id, "Updates") == 0);
        CHECK(logs.count(core::log_level::ERR) == 1);
        CHECK(logs.contains(core::log_level::ERR, "FixedCreateThrower faults in OnCreate"));

        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly(test_scripts_assembly("v2")));
        CHECK(fixture.field_i64(id, "CreateCalls") == 2);
        CHECK(fixture.field_i64(id, "ReloadCalls") == 0);
        CHECK_FALSE(fixture.runtime.entry(id)->has_flag(script_entry_flags::FAULTED));
        CHECK(fixture.runtime.entry(id)->has_flag(script_entry_flags::CREATED));
        fixture.run_frame();
        CHECK(fixture.field_i64(id, "StartCalls") == 1);
        CHECK(fixture.field_i64(id, "Updates") == 1);

        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly(test_scripts_assembly("v3")));
        fixture.run_frame();
        CHECK(fixture.field_i64(id, "CreateCalls") == 2);
        CHECK(fixture.field_i64(id, "ReloadCalls") == 1);
        CHECK(fixture.field_i64(id, "StartCalls") == 1);
        CHECK(fixture.field_i64(id, "Updates") == 2);
        CHECK(logs.count(core::log_level::ERR) == 1);
    }

    TEST_CASE(
      "regression - code sweep OWN-P2 AddScript says the runtime is not running when script_add has no world "
      "[dotnet]" *
      doctest::timeout(60)
    ) {
        // A runtime detached from its world keeps its live instances (only a swap to another world destroys them),
        // so their callbacks still run and script_add answers NOT_INITIALISED: the message names that, not the
        // entity (IsAlive would answer false for every entity then).
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const script_entry_id probe = fixture.add_script(fixture.create_entity(), ADD_SCRIPT_STATUS_PROBE);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(fixture.field_string(probe, "Caught") == "none");
        fixture.runtime.attach_world(nullptr);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(fixture.field_string(probe, "Caught") == "InvalidOperationException");
        CHECK(fixture.field_string(probe, "Message").find("the script runtime is not running") != std::string::npos);
        fixture.runtime.attach_world(&fixture.world);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("script exceptions - a call off the main thread throws [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const script_entry_id probe = fixture.add_script(fixture.create_entity(), WRONG_THREAD);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(fixture.field_string(probe, "Caught") == "InvalidOperationException");
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("script exceptions - an unprovided entry throws NotSupportedException [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const script_entry_id probe = fixture.add_script(fixture.create_entity(), NULL_ENTRY);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(fixture.field_string(probe, "Caught") == "NotSupportedException");
        CHECK(logs.count(core::log_level::ERR) == 0);
    }
}
