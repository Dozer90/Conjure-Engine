#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "scripting_test_support.h"

#include "conjure/core/string_id.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/world.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_runtime.h"
#include "conjure/scripting/scripting_error.h"

#include <string>
#include <string_view>
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
    constexpr std::string_view COIN_PROBE = "Conjure.TestScripts.CoinProbe";
    constexpr std::string_view COIN_CHIME = "Conjure.TestScripts.CoinChime";
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
        CHECK(fixture.field_i64(variant, "Variant") == 2);           // OnReload ran on the new assembly
        fixture.run_frame();
        CHECK(fixture.field_i64(probe, "SecretView") == 42);         // private state reset to its initialiser
        CHECK(fixture.runtime.classes().find("Conjure.TestScripts.ReloadProbe")->find_field("OnlyInV2") != nullptr);
        CHECK(logs.count(core::log_level::ERR) == 0);
        CHECK(logs.count(core::log_level::WARN) == 0);
    }

    TEST_CASE("hot reload - reload does not run OnCreate again [dotnet]" * doctest::timeout(120)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const script_entry_id probe = fixture.add_script(fixture.create_entity(), RELOAD_PROBE);
        fixture.run_frame();
        CHECK(fixture.field_i64(probe, "CreateCalls") == 1);
        CHECK(fixture.field_i64(probe, "StartCalls") == 1);

        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly(test_scripts_assembly("v2")));
        fixture.run_frame();
        CHECK(fixture.field_i64(probe, "CreateCalls") == 1);
        CHECK(fixture.field_i64(probe, "StartCalls") == 1);
        CHECK(fixture.field_i64(probe, "Updates") == 2);
        CHECK(fixture.runtime.entry(probe)->has_flag(script_entry_flags::STARTED));
        CHECK(fixture.runtime.entry(probe)->has_flag(script_entry_flags::CREATED));

        SUBCASE("an instance created but not started yet starts after the reload, without a second OnCreate") {
            const script_entry_id young = fixture.add_script(fixture.create_entity(), RELOAD_PROBE);
            CHECK(fixture.field_i64(young, "CreateCalls") == 1);
            CHECK(fixture.field_i64(young, "StartCalls") == 0);
            CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly(test_scripts_assembly("v1")));
            fixture.run_frame();
            CHECK(fixture.field_i64(young, "StartCalls") == 1);
            CHECK(fixture.field_i64(young, "CreateCalls") == 1);
            CHECK(fixture.field_i64(young, "ReloadCalls") == 1);
        }
        SUBCASE("a create still queued at the reload runs OnCreate on the new assembly") {
            const ecs::entity entity = fixture.create_entity();
            const script_entry_id queued = fixture.runtime.add_script(entity, hash_of(RELOAD_PROBE), {});
            REQUIRE(queued.is_valid());
            REQUIRE_FALSE(fixture.runtime.entry(queued)->is_live());
            CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly(test_scripts_assembly("v1")));
            CHECK(fixture.field_i64(queued, "CreateCalls") == 1);
            CHECK(fixture.field_i64(queued, "ReloadCalls") == 0);
            CHECK(fixture.runtime.entry(queued)->has_flag(script_entry_flags::CREATED));
        }
        SUBCASE("leaving play and entering it again runs OnCreate on the new session's instance") {
            fixture.runtime.set_mode(script_mode::EDIT);
            fixture.runtime.set_mode(script_mode::PLAY);
            CHECK(fixture.field_i64(probe, "CreateCalls") == 2);        // the count is a serialised field
            CHECK(fixture.field_i64(probe, "ReloadCalls") == 1);
        }
        CHECK(logs.count(core::log_level::ERR) == 0);
        CHECK(logs.count(core::log_level::WARN) == 0);
    }

    TEST_CASE("hot reload - reload runs OnReload once per instance [dotnet]" * doctest::timeout(120)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        std::vector<script_entry_id> probes = {};
        for (int index = 0; index < 3; ++index) {
            probes.push_back(fixture.add_script(fixture.create_entity(), RELOAD_PROBE));
        }
        fixture.run_frame();
        for (usize index = 0; index < probes.size(); ++index) {
            CAPTURE(index);
            CHECK(fixture.field_i64(probes[index], "ReloadCalls") == 0);
            CHECK(fixture.field_i64(probes[index], "SecretView") == 99);        // OnStart set the private field
        }

        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly(test_scripts_assembly("v2")));
        for (usize index = 0; index < probes.size(); ++index) {
            CAPTURE(index);
            CHECK(fixture.field_i64(probes[index], "ReloadCalls") == 1);
            // OnReload saw the private field at its initialiser: the place to rebuild what OnStart had set.
            CHECK(fixture.field_i64(probes[index], "SecretAtReload") == 42);
            CHECK(fixture.field_i64(probes[index], "CreateCalls") == 1);
        }

        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly(test_scripts_assembly("v3")));
        fixture.run_frame();
        for (usize index = 0; index < probes.size(); ++index) {
            CAPTURE(index);
            CHECK(fixture.field_i64(probes[index], "ReloadCalls") == 2);
            CHECK(fixture.field_i64(probes[index], "CreateCalls") == 1);
            CHECK(fixture.field_i64(probes[index], "StartCalls") == 1);
            CHECK(fixture.field_i64(probes[index], "Updates") == 2);
        }
        CHECK(fixture.runtime.stats().reloadCount == 2);
        CHECK(logs.count(core::log_level::ERR) == 0);
        CHECK(logs.count(core::log_level::WARN) == 0);
    }

    TEST_CASE(
      "regression - code sweep OWN-RELOAD the blue coin's one-shot component does not come back after a hot reload "
      "[dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const ecs::component_id chimeId = fixture.registry.find(core::string_id(COIN_CHIME));
        REQUIRE(chimeId.is_valid());
        const ecs::entity coin = fixture.create_entity();
        const script_entry_id probe = fixture.add_script(coin, COIN_PROBE);
        CHECK(fixture.world.has_raw(coin, chimeId));                    // OnCreate added the chime
        fixture.run_frame();
        CHECK_FALSE(fixture.world.has_raw(coin, chimeId));              // it played and removed itself
        CHECK(fixture.field_i64(probe, "ChimesPlayed") == 1);

        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly(test_scripts_assembly("v2")));
        CHECK_FALSE(fixture.world.has_raw(coin, chimeId));
        fixture.run_frame();
        CHECK_FALSE(fixture.world.has_raw(coin, chimeId));
        CHECK(fixture.field_i64(probe, "ChimesPlayed") == 1);
        CHECK(fixture.field_i64(probe, "CreateCalls") == 1);
        CHECK(fixture.field_i64(probe, "ReloadCalls") == 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
        CHECK(logs.count(core::log_level::WARN) == 0);
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
        CHECK(fixture.field_i64(young, "CreateCalls") == 1);        // OnCreate runs once per entity lifetime
        CHECK(fixture.field_i64(young, "ReloadCalls") == 1);
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
          scripting_error::ASSEMBLY_LOAD_FAILED
        );
        CHECK(fixture.runtime.entry(variant)->is_live());
        CHECK(fixture.field_i64(variant, "Variant") == 1);
        CHECK(fixture.runtime.stats().reloadCount == 0);
        fixture.run_frame();
        CHECK(logs.count(core::log_level::ERR) == 1);
    }
}
