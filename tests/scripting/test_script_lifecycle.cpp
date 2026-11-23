#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "scripting_test_support.h"

#include "conjure/core/clock.h"
#include "conjure/core/json/document.h"
#include "conjure/ecs/entity.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_runtime.h"

#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;
using namespace scripting_test;

namespace {

    constexpr std::string_view PROBE = "Conjure.TestScripts.LifecycleProbe";
    constexpr std::string_view MOVER = "Conjure.TestScripts.Mover";
    constexpr std::string_view SPAWNER = "Conjure.TestScripts.Spawner";
    constexpr std::string_view DESTROY_PROBE = "Conjure.TestScripts.DestroyProbe";
    constexpr std::string_view FRAME_PROBE = "Conjure.TestScripts.FrameProbe";
    constexpr std::string_view DESTROY_IN_CREATE = "Conjure.TestScripts.DestroyInCreate";
    constexpr std::string_view LOOT_DROPPER = "Conjure.TestScripts.LootDropper";
}

TEST_SUITE("scripting/script_lifecycle") {

    TEST_CASE(
      "script lifecycle - create, start and update counters match the phases run [dotnet]" * doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id id = fixture.add_script(entity, PROBE);
        CHECK(fixture.field_i64(id, "Created") == 1);
        CHECK(fixture.field_i64(id, "Started") == 0);
        CHECK(fixture.field_i64(id, "Updates") == 0);

        for (int frame = 0; frame < 10; ++frame) {
            fixture.run_frame();
        }
        CHECK(fixture.field_i64(id, "Created") == 1);
        CHECK(fixture.field_i64(id, "Started") == 1);
        CHECK(fixture.field_i64(id, "Updates") == 10);
        CHECK(fixture.field_i64(id, "FixedUpdates") == 10);
        CHECK(fixture.field_i64(id, "LateUpdates") == 10);
        CHECK(fixture.field_i64(id, "StartedBeforeUpdate") == 1);
        CHECK(fixture.field_i64(id, "UpdateBeforeLate") == 1);
        CHECK(fixture.runtime.entry(id)->has_flag(script_entry_flags::STARTED));
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "script lifecycle - fixed update runs once per fixed step and late after update [dotnet]" * doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        dotnet_fixture fixture = {};
        const script_entry_id id = fixture.add_script(fixture.create_entity(), PROBE);
        fixture.runtime.run_phase(script_phase::FIXED_UPDATE, 0.01f);
        fixture.runtime.run_phase(script_phase::FIXED_UPDATE, 0.01f);
        fixture.runtime.run_phase(script_phase::FIXED_UPDATE, 0.01f);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.03f);
        fixture.runtime.run_phase(script_phase::LATE_UPDATE, 0.03f);
        CHECK(fixture.field_i64(id, "FixedUpdates") == 3);
        CHECK(fixture.field_i64(id, "Updates") == 1);
        CHECK(fixture.field_i64(id, "LateUpdates") == 1);
        CHECK(fixture.field_i64(id, "Started") == 1);
    }

    TEST_CASE(
      "script lifecycle - Destroy from a script sees a live entity in OnDestroy [dotnet]" * doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const ecs::entity entity = fixture.create_entity();
        static_cast<void>(fixture.add_script(entity, DESTROY_PROBE));
        fixture.run_frame();
        CHECK_FALSE(fixture.world.is_alive(entity));
        CHECK(logs.contains(core::log_level::INFO, "DestroyProbe.OnDestroy alive=True"));
        CHECK(fixture.runtime.stats().liveInstances == 0);

        // A native destroy of a scripted entity runs OnDestroy at the next flush with a dead entity.
        const ecs::entity other = fixture.create_entity();
        static_cast<void>(fixture.add_script(other, PROBE));
        CONJURE_REQUIRE_OK(fixture.world.destroy_entity(other));
        fixture.runtime.flush_pending();
        CHECK(logs.contains(core::log_level::INFO, "LifecycleProbe.OnDestroy alive=False"));
    }

    TEST_CASE(
      "script lifecycle - scripts added from a script are created in the same frame [dotnet]" * doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        dotnet_fixture fixture = {};
        const script_entry_id spawner = fixture.add_script(fixture.create_entity(), SPAWNER);
        const u32 before = fixture.world.entity_count();
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(fixture.world.entity_count() == before + 1);
        CHECK(fixture.field_i64(spawner, "SpawnedAlive") == 1);
        const ecs::entity spawned = ecs::entity::from_u64(fixture.fields_of(spawner).root()["Spawned"].as_u64());
        CHECK(fixture.world.is_alive(spawned));
        CHECK(fixture.runtime.entity_name(spawned) == "spawned");
        const std::vector<script_entry_id> scripts = fixture.runtime.scripts_of(spawned);
        REQUIRE(scripts.size() == 1);
        CHECK(fixture.runtime.entry(scripts[0])->is_live());
        CHECK(fixture.field_i64(scripts[0], "Created") == 1);     // OnCreate ran in the flush of the same phase
        CHECK(fixture.field_i64(scripts[0], "Started") == 0);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(fixture.field_i64(scripts[0], "Started") == 1);     // OnStart before the next update phase
        CHECK(fixture.field_i64(scripts[0], "Updates") == 1);
        CHECK(fixture.world.entity_count() == before + 1);         // the spawner spawns once
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F068: a script created in UPDATE gets no OnLateUpdate before OnStart [dotnet]" *
      doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const script_entry_id spawner = fixture.add_script(fixture.create_entity(), SPAWNER);
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        fixture.runtime.run_phase(script_phase::LATE_UPDATE, 0.1f);
        const ecs::entity spawned = ecs::entity::from_u64(fixture.fields_of(spawner).root()["Spawned"].as_u64());
        const std::vector<script_entry_id> scripts = fixture.runtime.scripts_of(spawned);
        REQUIRE(scripts.size() == 1);
        CHECK(fixture.field_i64(scripts[0], "Created") == 1);
        CHECK(fixture.field_i64(scripts[0], "Started") == 0);
        CHECK(fixture.field_i64(scripts[0], "LateUpdates") == 0);     // skipped: OnStart has not run yet

        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        fixture.runtime.run_phase(script_phase::LATE_UPDATE, 0.1f);
        CHECK(fixture.field_i64(scripts[0], "Started") == 1);
        CHECK(fixture.field_i64(scripts[0], "Updates") == 1);
        CHECK(fixture.field_i64(scripts[0], "LateUpdates") == 1);
        CHECK(fixture.field_i64(scripts[0], "StartedBeforeUpdate") == 1);
        CHECK(fixture.field_i64(scripts[0], "UpdateBeforeLate") == 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "script lifecycle - 10k instances update in one call well under the frame budget [dotnet]" * doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        dotnet_fixture fixture = {};
        constexpr u32 COUNT = 10000;
        const u64 moverHash = hash_of(MOVER);
        for (u32 index = 0; index < COUNT; ++index) {
            REQUIRE(fixture.runtime.add_script(fixture.create_entity(), moverHash, {}).is_valid());
        }
        fixture.runtime.flush_pending();
        CHECK(fixture.runtime.stats().liveInstances == COUNT);

        fixture.runtime.run_phase(script_phase::UPDATE, 0.016f);   // warm up (JIT, OnStart)
        core::stopwatch watch = {};
        watch.start();
        fixture.runtime.run_phase(script_phase::UPDATE, 0.016f);
        const f64 seconds = watch.elapsed_seconds();
        MESSAGE("10k Mover updates took ", seconds * 1000.0, " ms");
        // The design target is 2 ms on the reference desktop; this is the loose CI gate.
        CHECK(seconds < 0.100);

        const std::vector<script_entry_id> scripts = fixture.runtime.scripts_of(ecs::entity{1, 1});
        REQUIRE(scripts.size() == 1);
        CHECK(fixture.field_i64(scripts[0], "Updates") == 2);
    }

    TEST_CASE("script lifecycle - shutdown calls OnDestroy and leaves nothing live [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        static_cast<void>(fixture.add_script(fixture.create_entity(), PROBE));
        static_cast<void>(fixture.add_script(fixture.create_entity(), PROBE));
        fixture.runtime.shutdown();
        CHECK(fixture.runtime.status() == script_host_status::UNINITIALISED);
        CHECK(fixture.runtime.stats().liveInstances == 0);
        CHECK(logs.count(core::log_level::INFO) >= 2);
        CHECK(logs.contains(core::log_level::INFO, "LifecycleProbe.OnDestroy alive=True"));
    }

    TEST_CASE(
      "regression - review 2 Destroy from OnCreate runs OnDestroy once and leaves no instance [dotnet]" *
      doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const ecs::entity entity = fixture.create_entity();
        REQUIRE(fixture.runtime.add_script(entity, hash_of(DESTROY_IN_CREATE), {}).is_valid());
        fixture.runtime.flush_pending();
        CHECK_FALSE(fixture.world.is_alive(entity));
        CHECK(fixture.runtime.stats().liveInstances == 0);
        CHECK(fixture.runtime.stats().pendingDestroys == 0);
        CHECK(logs.contains(core::log_level::INFO, "DestroyInCreate.OnDestroy alive=False"));
        // No orphan keeps running: a frame produces no managed activity and no errors.
        fixture.run_frame();
        CHECK(fixture.runtime.host().managed_api().run_phase(static_cast<uint8_t>(script_phase::UPDATE), 0.1f) == 0);
        CHECK(logs.count(core::log_level::ERR) == 0);
        CHECK(logs.count(core::log_level::WARN) == 0);
    }

    TEST_CASE(
      "regression - review 1 OnDestroy that spawns scripted entities during a script replacement [dotnet]" *
      doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const ecs::entity entity = fixture.create_entity();
        static_cast<void>(fixture.add_script(entity, LOOT_DROPPER, R"({"LootCount": 64})"));
        const u32 before = fixture.world.entity_count();
        const core::json::document replacement = json(
          R"({"scripts":[{"class":"Conjure.TestScripts.LifecycleProbe"}]})"
        );
        CONJURE_REQUIRE_OK(fixture.runtime.deserialize_entity_scripts(entity, replacement.root()));
        CHECK(fixture.world.entity_count() == before + 64);
        fixture.runtime.flush_pending();
        CHECK(fixture.runtime.stats().liveInstances == 65);
        const std::vector<script_entry_id> scripts = fixture.runtime.scripts_of(entity);
        REQUIRE(scripts.size() == 1);
        CHECK(fixture.runtime.entry(scripts[0])->classHash == hash_of(PROBE));
        CHECK(fixture.field_i64(scripts[0], "Created") == 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("regression - review 5 stopping and starting play runs OnStart again [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        dotnet_fixture fixture = {};
        const script_entry_id id = fixture.add_script(fixture.create_entity(), PROBE);
        fixture.run_frame();
        CHECK(fixture.field_i64(id, "Started") == 1);
        fixture.runtime.set_mode(script_mode::EDIT);
        CHECK_FALSE(fixture.runtime.entry(id)->is_live());
        CHECK_FALSE(fixture.runtime.entry(id)->has_flag(script_entry_flags::STARTED));
        // The counters are public fields: the JSON snapshot (Created 1, Started 1, Updates 1) seeds the new
        // instance, whose own OnCreate and OnStart then run on top of it.
        fixture.runtime.set_mode(script_mode::PLAY);
        CHECK(fixture.runtime.entry(id)->is_live());
        CHECK(fixture.field_i64(id, "Created") == 2);
        CHECK(fixture.field_i64(id, "Started") == 1);
        fixture.run_frame();
        CHECK(fixture.field_i64(id, "Started") == 2);
        CHECK(fixture.field_i64(id, "Updates") == 2);
        CHECK(fixture.runtime.entry(id)->has_flag(script_entry_flags::STARTED));
    }

    TEST_CASE("script lifecycle - Time and Input read the shared frame block [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        dotnet_fixture fixture = {};
        const script_entry_id id = fixture.add_script(fixture.create_entity(), FRAME_PROBE);
        conjure_frame_block &frame = fixture.runtime.frame_block();
        frame.delta_time = 0.25f;
        frame.frame_index = 77;
        frame.input.keys_down[0] = u64{1} << 32;      // key code 32 = SPACE
        fixture.runtime.run_phase(script_phase::UPDATE, 0.25f);
        CHECK(fixture.field_f64(id, "LastDelta") == doctest::Approx(0.25));
        CHECK(fixture.field_i64(id, "LastFrame") == 77);
        CHECK(fixture.field_i64(id, "SpaceDown") == 1);
        CHECK(fixture.field_i64(id, "CurrentPhase") == static_cast<i64>(script_phase::UPDATE));
        frame.input.keys_down[0] = 0;
        fixture.runtime.run_phase(script_phase::UPDATE, 0.25f);
        CHECK(fixture.field_i64(id, "SpaceDown") == 0);
    }
}
