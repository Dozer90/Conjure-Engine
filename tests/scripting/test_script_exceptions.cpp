#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "scripting_test_support.h"

#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_runtime.h"

#include <string>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;
using namespace scripting_test;

namespace {

    constexpr std::string_view THROWER = "Conjure.TestScripts.Thrower";
    constexpr std::string_view CREATE_THROWER = "Conjure.TestScripts.CreateThrower";
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
