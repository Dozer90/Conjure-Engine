#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "scripting_test_support.h"

#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_runtime.h"
#include "conjure/scripting/scripting_error.h"

#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;
using namespace scripting_test;

namespace {

    constexpr std::string_view RELOAD_PROBE = "Conjure.TestScripts.ReloadProbe";
    constexpr std::string_view VANISHING = "Conjure.TestScripts.Vanishing";
    constexpr std::string_view VARIANT_PROBE = "Conjure.TestScripts.VariantProbe";
    constexpr std::string_view LEAKY = "Conjure.TestScripts.LeakyTimer";
    constexpr std::string_view LIFECYCLE_PROBE = "Conjure.TestScripts.LifecycleProbe";
}

TEST_SUITE("scripting/script_hot_reload") {

    TEST_CASE(
      "hot reload - v1 to v2 preserves serialised fields and resets private state [dotnet]" * doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const script_entry_id probe = fixture.add_script(fixture.create_entity(), RELOAD_PROBE, R"({"Health": 3})");
        const script_entry_id variant = fixture.add_script(fixture.create_entity(), VARIANT_PROBE);
        fixture.run_frame();
        CHECK(fixture.field_i64(probe, "Health") == 3);
        CHECK(fixture.field_i64(probe, "SecretView") == 99);       // OnStart set the private field
        CHECK(fixture.field_i64(variant, "Variant") == 1);
        CHECK(fixture.runtime.classes().find("Conjure.TestScripts.ReloadProbe")->find_field("OnlyInV2") == nullptr);

        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly(test_scripts_assembly("v2")));
        CHECK(fixture.runtime.stats().reloadCount == 1);
        CHECK(fixture.runtime.entry(probe)->is_live());
        CHECK(fixture.field_i64(probe, "Health") == 3);              // serialised: preserved
        CHECK(fixture.field_string(probe, "Label") == "v1");         // serialised: preserved over the new default
        CHECK(fixture.field_i64(probe, "OnlyInV2") == 7);            // new field: default
        CHECK(fixture.field_i64(variant, "Variant") == 2);           // OnCreate ran on the new assembly
        fixture.run_frame();
        CHECK(fixture.field_i64(probe, "SecretView") == 42);         // private state reset to its initialiser
        CHECK(fixture.runtime.classes().find("Conjure.TestScripts.ReloadProbe")->find_field("OnlyInV2") != nullptr);
        CHECK(logs.count(core::log_level::ERR) == 0);
        CHECK(logs.count(core::log_level::WARN) == 0);
    }

    TEST_CASE("hot reload - OnCreate runs again and OnStart does not [dotnet]" * doctest::timeout(120)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        dotnet_fixture fixture = {};
        const script_entry_id probe = fixture.add_script(fixture.create_entity(), RELOAD_PROBE);
        fixture.run_frame();
        CHECK(fixture.field_i64(probe, "CreateCalls") == 1);
        CHECK(fixture.field_i64(probe, "StartCalls") == 1);

        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly(test_scripts_assembly("v2")));
        fixture.run_frame();
        CHECK(fixture.field_i64(probe, "CreateCalls") == 2);
        CHECK(fixture.field_i64(probe, "StartCalls") == 1);
        CHECK(fixture.field_i64(probe, "Updates") == 2);
        CHECK(fixture.runtime.entry(probe)->has_flag(script_entry_flags::STARTED));

        // An entry created after the first phase but never started starts after the reload.
        const script_entry_id young = fixture.add_script(fixture.create_entity(), RELOAD_PROBE);
        CHECK(fixture.field_i64(young, "StartCalls") == 0);
        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly(test_scripts_assembly("v1")));
        fixture.run_frame();
        CHECK(fixture.field_i64(young, "StartCalls") == 1);
        CHECK(fixture.field_i64(young, "CreateCalls") == 2);
    }

    TEST_CASE("hot reload - a class removed in v2 keeps its JSON and returns in v3 [dotnet]" * doctest::timeout(120)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const script_entry_id vanishing = fixture.add_script(fixture.create_entity(), VANISHING, R"({"Updates": 5})");
        fixture.run_frame();
        CHECK(fixture.field_i64(vanishing, "Updates") == 6);

        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly(test_scripts_assembly("v2")));
        const script_entry *entryValue = fixture.runtime.entry(vanishing);
        REQUIRE(entryValue != nullptr);
        CHECK_FALSE(entryValue->is_live());
        CHECK(entryValue->has_flag(script_entry_flags::CLASS_MISSING));
        CHECK(entryValue->fields.root()["Updates"].as_i64() == 6);
        CHECK(fixture.runtime.classes().find(VANISHING) == nullptr);
        CHECK(logs.count(core::log_level::WARN) == 1);
        fixture.run_frame();
        CHECK(entryValue->fields.root()["Updates"].as_i64() == 6);

        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly(test_scripts_assembly("v3")));
        CHECK(fixture.runtime.entry(vanishing)->is_live());
        CHECK_FALSE(fixture.runtime.entry(vanishing)->has_flag(script_entry_flags::CLASS_MISSING));
        CHECK(fixture.field_i64(vanishing, "Updates") == 6);
        fixture.run_frame();
        CHECK(fixture.field_i64(vanishing, "Updates") == 7);
        CHECK(fixture.runtime.stats().reloadCount == 2);
    }

    TEST_CASE("hot reload - a leaky static timer warns but the new assembly loads [dotnet]" * doctest::timeout(120)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const script_entry_id leaky = fixture.add_script(fixture.create_entity(), LEAKY, R"({"Arm": true})");
        CHECK(fixture.field_i64(leaky, "Armed") == 1);
        const script_entry_id variant = fixture.add_script(fixture.create_entity(), VARIANT_PROBE);
        CHECK(fixture.field_i64(variant, "Variant") == 1);

        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly(test_scripts_assembly("v2")));
        CHECK(fixture.runtime.stats().zombieAssemblies == 1);
        CHECK(logs.contains(core::log_level::WARN, "did not unload"));
        CHECK(fixture.field_i64(variant, "Variant") == 2);        // the reload itself succeeded
        CHECK(fixture.runtime.entry(leaky)->is_live());
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("hot reload - reloading twice collects the retired load contexts [dotnet]" * doctest::timeout(120)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const script_entry_id variant = fixture.add_script(fixture.create_entity(), VARIANT_PROBE);
        static_cast<void>(fixture.add_script(fixture.create_entity(), RELOAD_PROBE));
        fixture.run_frame();
        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly(test_scripts_assembly("v2")));
        CHECK(fixture.field_i64(variant, "Variant") == 2);
        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly(test_scripts_assembly("v3")));
        CHECK(fixture.field_i64(variant, "Variant") == 3);
        CHECK(fixture.runtime.stats().reloadCount == 2);
        CHECK(fixture.runtime.stats().zombieAssemblies == 0);
        CHECK(fixture.runtime.host().managed_api().collect_garbage() == 0);
        CHECK(logs.count(core::log_level::WARN) == 0);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "regression - review 4 a reload after a native destroy in the same frame leaves no zombie [dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const ecs::entity doomed = fixture.create_entity();
        static_cast<void>(fixture.add_script(doomed, LIFECYCLE_PROBE));
        const script_entry_id variant = fixture.add_script(fixture.create_entity(), VARIANT_PROBE);
        fixture.run_frame();

        // The editor deletes the entity, then the build completes before FRAME_BEGIN's flush.
        CONJURE_REQUIRE_OK(fixture.world.destroy_entity(doomed));
        REQUIRE(fixture.runtime.stats().pendingDestroys == 1);
        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly(test_scripts_assembly("v2")));
        CHECK(fixture.runtime.stats().zombieAssemblies == 0);
        CHECK(fixture.runtime.stats().pendingDestroys == 0);
        CHECK(fixture.runtime.stats().liveInstances == 1);
        CHECK(fixture.field_i64(variant, "Variant") == 2);
        CHECK(logs.contains(core::log_level::INFO, "LifecycleProbe.OnDestroy alive=False"));
        CHECK(logs.count(core::log_level::WARN) == 0);
        CHECK(logs.count(core::log_level::ERR) == 0);
        CHECK(fixture.runtime.host().managed_api().collect_garbage() == 0);
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F066: a reload before a young instance's first phase leaves no zombie [dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        fixture.run_frame();

        // Created by a flush outside any phase: the v1 instance still waits for its OnStart when it is destroyed.
        const script_entry_id young = fixture.add_script(fixture.create_entity(), RELOAD_PROBE);
        CHECK(fixture.field_i64(young, "StartCalls") == 0);
        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly(test_scripts_assembly("v2")));
        CHECK(fixture.runtime.stats().zombieAssemblies == 0);
        CHECK(logs.count(core::log_level::WARN) == 0);

        fixture.run_frame();
        CHECK(fixture.field_i64(young, "StartCalls") == 1);
        CHECK(fixture.field_i64(young, "CreateCalls") == 2);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("hot reload - a missing assembly file fails and keeps the running one [dotnet]" * doctest::timeout(120)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        logs.allow_errors();
        dotnet_fixture fixture = {};
        const script_entry_id variant = fixture.add_script(fixture.create_entity(), VARIANT_PROBE);
        CONJURE_CHECK_ERROR(
          fixture.runtime.reload_game_assembly(test_scripts_assembly("v1").parent_path() / "Missing.dll"),
          script_error::ASSEMBLY_LOAD_FAILED
        );
        CHECK(fixture.runtime.entry(variant)->is_live());
        CHECK(fixture.field_i64(variant, "Variant") == 1);
        CHECK(fixture.runtime.stats().reloadCount == 0);
        fixture.run_frame();
        CHECK(logs.count(core::log_level::ERR) == 1);
    }
}
