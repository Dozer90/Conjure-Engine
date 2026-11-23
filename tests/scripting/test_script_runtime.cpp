#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "scripting_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/vfs/native_file_system.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/world.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/null_script_host.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_fields.h"
#include "conjure/scripting/script_runtime.h"
#include "conjure/scripting/scripting_error.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;
using namespace scripting_test;

namespace {

    constexpr std::string_view PLAYER = "Game.Player";
    constexpr std::string_view ENEMY = "Game.Enemy";

    [[nodiscard]] script_class_info player_class() {
        const std::initializer_list<script_callback> callbacks = {
          script_callback::CREATE, script_callback::UPDATE, script_callback::COLLISION_ENTER
        };
        return make_class(PLAYER, callbacks, {
          make_field("speed", script_field_kind::FLOAT, "2.5"),
          make_field("name", script_field_kind::STRING, "\"p\""),
          make_field("target", script_field_kind::ENTITY, "0"),
        });
    }

    [[nodiscard]] script_class_info enemy_class() {
        return make_class(ENEMY, {script_callback::UPDATE, script_callback::TRIGGER_ENTER}, {
          make_field("hp", script_field_kind::INT32, "10"),
        });
    }

    /// <summary>
    /// A fixture with both classes loaded; mode chosen by the test.
    /// </summary>
    struct loaded_fixture : scripting_fixture {
        loaded_fixture() {
            load_fake_classes({player_class(), enemy_class()});
        }

        [[nodiscard]] i32 slot_of(script_entry_id id) const {
            const script_entry *entryValue = runtime.entry(id);
            return entryValue == nullptr ? INVALID_SLOT : entryValue->managedSlot;
        }
    };
}

TEST_SUITE("scripting/script_runtime") {

    TEST_CASE("script_runtime - edit mode stores JSON and creates no instances") {
        loaded_fixture fixture = {};
        CHECK(fixture.runtime.mode() == script_mode::EDIT);
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id id = fixture.runtime.add_script(entity, hash_of(PLAYER), json(R"({"speed": 9})").root());
        REQUIRE(id.is_valid());
        fixture.runtime.flush_pending();
        CHECK(fixture.null_host().live_slot_count() == 0);
        CHECK(fixture.runtime.stats().liveInstances == 0);
        const script_entry *entryValue = fixture.runtime.entry(id);
        REQUIRE(entryValue != nullptr);
        CHECK_FALSE(entryValue->is_live());
        CHECK(entryValue->fields.root()["speed"].as_i64() == 9);
        CHECK(fixture.world.has<script_component>(entity));
        CHECK(fixture.runtime.scripts_of(entity).size() == 1);
        CHECK(fixture.runtime.scripts_of(entity)[0] == id);
        CHECK(fixture.runtime.frame_block().in_play_mode == 0);
    }

    TEST_CASE("script_runtime - entering play creates every entry in one batch") {
        loaded_fixture fixture = {};
        std::vector<script_entry_id> ids = {};
        for (int index = 0; index < 3; ++index) {
            const ecs::entity entity = fixture.create_entity();
            ids.push_back(fixture.runtime.add_script(entity, hash_of(PLAYER), {}));
            ids.push_back(fixture.runtime.add_script(entity, hash_of(ENEMY), {}));
        }
        fixture.null_host().reset_recorder();
        fixture.runtime.set_mode(script_mode::PLAY);
        CHECK(fixture.runtime.mode() == script_mode::PLAY);
        CHECK(fixture.runtime.frame_block().in_play_mode == 1);
        CHECK(fixture.null_host().recorder().count("create_instances") == 1);
        CHECK(fixture.null_host().recorder().creates().size() == 6);
        CHECK(fixture.null_host().live_slot_count() == 6);
        CHECK(fixture.runtime.stats().liveInstances == 6);
        for (const script_entry_id id : ids) {
            CHECK(fixture.slot_of(id) != INVALID_SLOT);
            CHECK(fixture.runtime.entry_of_slot(fixture.slot_of(id)) == id);
        }
        // The initial blob carries the defaults of the class.
        const auto &firstCreate = fixture.null_host().recorder().creates()[0];
        CHECK(firstCreate.classHash == hash_of(PLAYER));
        CHECK((firstCreate.flags & CREATE_FLAG_ALREADY_STARTED) == 0);
        const core::result<core::json::document> decoded = blob_to_json(player_class(), firstCreate.fields);
        CONJURE_REQUIRE_OK(decoded);
        CHECK(decoded.value().root()["speed"].as_f64() == doctest::Approx(2.5));
        CHECK(decoded.value().root()["name"].as_string() == "p");

        // Setting the same mode again is a no-op.
        fixture.runtime.set_mode(script_mode::PLAY);
        CHECK(fixture.null_host().recorder().count("create_instances") == 1);
    }

    TEST_CASE("script_runtime - add_script in play mode creates after the flush") {
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id id = fixture.runtime.add_script(entity, hash_of(PLAYER), {});
        REQUIRE(id.is_valid());
        CHECK(fixture.slot_of(id) == INVALID_SLOT);
        CHECK(fixture.runtime.stats().pendingCreates == 1);
        fixture.runtime.flush_pending();
        CHECK(fixture.slot_of(id) == 0);
        CHECK(fixture.runtime.stats().pendingCreates == 0);
        CHECK(fixture.null_host().slot_entity(0) == entity.to_u64());
        CHECK(fixture.null_host().slot_class(0) == hash_of(PLAYER));
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F631: add_script on a locked world refuses a first script without asserting"
    ) {
        log_capture logs = {};
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity fresh = fixture.create_entity();
        const ecs::entity holder = fixture.create_entity();
        REQUIRE(fixture.runtime.add_script(holder, hash_of(PLAYER), {}).is_valid());
        {
            const ecs::iteration_scope lock = fixture.world.lock_iteration();
            // No script_component yet: adding one is structural, so the call is refused (an assert before).
            CHECK_FALSE(fixture.runtime.add_script(fresh, hash_of(PLAYER), {}).is_valid());
            CHECK(logs.contains(core::log_level::WARN, "the world is locked for iteration"));
            CHECK_FALSE(fixture.world.has<script_component>(fresh));
            // The holder's set exists: a further script only joins it.
            CHECK(fixture.runtime.add_script(holder, hash_of(ENEMY), {}).is_valid());
        }
        CHECK(fixture.runtime.add_script(fresh, hash_of(PLAYER), {}).is_valid());
        CHECK(logs.count(core::log_level::WARN) == 1);
    }

    TEST_CASE("script_runtime - OnCreate happens before the first phase and the start flag follows the phases") {
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id id = fixture.runtime.add_script(entity, hash_of(PLAYER), {});
        fixture.null_host().reset_recorder();
        fixture.runtime.run_phase(script_phase::LATE_UPDATE, 0.1f);
        // The pending create was flushed when the phase ended, after the managed phase call.
        const std::vector<std::string> &calls = fixture.null_host().recorder().calls();
        REQUIRE(calls.size() == 2);
        CHECK(calls[0] == "run_phase");
        CHECK(calls[1] == "create_instances");
        CHECK_FALSE(fixture.runtime.entry(id)->has_flag(script_entry_flags::STARTED));

        fixture.runtime.run_phase(script_phase::LATE_UPDATE, 0.1f);
        CHECK_FALSE(fixture.runtime.entry(id)->has_flag(script_entry_flags::STARTED));
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(fixture.runtime.entry(id)->has_flag(script_entry_flags::STARTED));
        CHECK(fixture.null_host().slot_phase_count(fixture.slot_of(id), script_phase::UPDATE) == 1);
        CHECK(fixture.runtime.frame_block().current_phase == static_cast<uint8_t>(script_phase::COUNT));
        CHECK(fixture.runtime.current_phase() == script_phase::COUNT);
    }

    TEST_CASE("script_runtime - a fixed update also starts entries and run_phase ignores COUNT") {
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const script_entry_id id = fixture.runtime.add_script(fixture.create_entity(), hash_of(PLAYER), {});
        fixture.runtime.flush_pending();
        fixture.runtime.run_phase(script_phase::COUNT, 0.1f);
        CHECK(fixture.null_host().recorder().count("run_phase") == 0);
        fixture.runtime.run_phase(script_phase::FIXED_UPDATE, 0.1f);
        CHECK(fixture.runtime.entry(id)->has_flag(script_entry_flags::STARTED));
        CHECK(fixture.null_host().recorder().phases().back().deltaTime == 0.1f);
    }

    TEST_CASE("script_runtime - Entity.Destroy from a script runs OnDestroy while the entity is alive") {
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id id = fixture.runtime.add_script(entity, hash_of(PLAYER), {});
        fixture.runtime.flush_pending();
        const i32 slot = fixture.slot_of(id);
        REQUIRE(slot >= 0);

        fixture.runtime.request_entity_destroy(entity);
        CHECK(fixture.world.is_alive(entity));            // deferred to the flush
        CHECK(fixture.runtime.stats().pendingEntityDestroys == 1);
        fixture.null_host().reset_recorder();
        fixture.runtime.flush_pending();
        CHECK_FALSE(fixture.world.is_alive(entity));
        REQUIRE(fixture.null_host().recorder().destroys().size() == 1);
        CHECK(fixture.null_host().recorder().destroys()[0].slot == slot);
        CHECK(fixture.null_host().recorder().destroys()[0].callOnDestroy);
        CHECK(fixture.null_host().live_slot_count() == 0);
        CHECK(fixture.runtime.store().size() == 0);
        CHECK(fixture.runtime.stats().liveInstances == 0);

        // An entity without live scripts goes at once; a dead one is ignored.
        const ecs::entity plain = fixture.create_entity();
        fixture.runtime.request_entity_destroy(plain);
        CHECK_FALSE(fixture.world.is_alive(plain));
        fixture.runtime.request_entity_destroy(plain);
        CHECK(fixture.runtime.stats().pendingEntityDestroys == 0);
    }

    TEST_CASE("script_runtime - a native remove defers OnDestroy to the next flush") {
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id id = fixture.runtime.add_script(entity, hash_of(PLAYER), {});
        fixture.runtime.flush_pending();
        const i32 slot = fixture.slot_of(id);
        fixture.null_host().reset_recorder();

        SUBCASE("component removed") {
            CONJURE_REQUIRE_OK(fixture.world.remove<script_component>(entity));
            CHECK(fixture.world.is_alive(entity));
        }
        SUBCASE("entity destroyed natively") {
            CONJURE_REQUIRE_OK(fixture.world.destroy_entity(entity));
        }
        CHECK(fixture.null_host().recorder().destroys().empty());
        CHECK(fixture.runtime.stats().pendingDestroys == 1);
        CHECK(fixture.runtime.store().size() == 0);       // the set went with the component
        fixture.runtime.flush_pending();
        REQUIRE(fixture.null_host().recorder().destroys().size() == 1);
        CHECK(fixture.null_host().recorder().destroys()[0].slot == slot);
        CHECK(fixture.null_host().recorder().destroys()[0].callOnDestroy);
        CHECK(fixture.null_host().live_slot_count() == 0);
    }

    TEST_CASE("script_runtime - leaving play calls OnDestroy exactly once per instance") {
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id player = fixture.runtime.add_script(entity, hash_of(PLAYER), {});
        const script_entry_id enemy = fixture.runtime.add_script(entity, hash_of(ENEMY), {});
        fixture.runtime.flush_pending();
        fixture.null_host().reset_recorder();
        fixture.runtime.set_mode(script_mode::EDIT);
        CHECK(fixture.null_host().recorder().count("destroy_instances") == 1);
        CHECK(fixture.null_host().recorder().destroys().size() == 2);
        CHECK(fixture.null_host().recorder().destroys()[0].callOnDestroy);
        CHECK(fixture.null_host().live_slot_count() == 0);
        CHECK(fixture.slot_of(player) == INVALID_SLOT);
        CHECK(fixture.slot_of(enemy) == INVALID_SLOT);
        CHECK(fixture.world.is_alive(entity));
        CHECK(fixture.runtime.scripts_of(entity).size() == 2);
        fixture.runtime.set_mode(script_mode::EDIT);
        CHECK(fixture.null_host().recorder().count("destroy_instances") == 1);
        fixture.runtime.destroy_all(true);
        CHECK(fixture.null_host().recorder().count("destroy_instances") == 1);
    }

    TEST_CASE("script_runtime - collision records resolve both sides with flipped normals") {
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity a = fixture.create_entity();
        const ecs::entity b = fixture.create_entity();
        const script_entry_id playerA = fixture.runtime.add_script(a, hash_of(PLAYER), {});
        const script_entry_id playerB = fixture.runtime.add_script(b, hash_of(PLAYER), {});
        fixture.runtime.flush_pending();
        fixture.null_host().reset_recorder();

        collision_event event = {};
        event.a = a;
        event.b = b;
        event.normal = math::vec3(1.0f, 0.0f, 0.0f);
        event.impulse = 4.0f;
        event.kind = collision_kind::COLLISION_ENTER;
        const std::array<collision_event, 1> events = {event};
        fixture.runtime.dispatch_collision_events(events);

        CHECK(fixture.null_host().recorder().count("dispatch_collisions") == 1);
        const auto &records = fixture.null_host().recorder().collisions();
        REQUIRE(records.size() == 2);
        CHECK(records[0].slot == fixture.slot_of(playerA));
        CHECK(records[0].other == b.to_u64());
        CHECK(records[0].kind == collision_kind::COLLISION_ENTER);
        CHECK(records[0].impulse == 4.0f);
        CHECK(records[1].slot == fixture.slot_of(playerB));
        CHECK(records[1].other == a.to_u64());

        // An empty span and a world without scripted participants send nothing.
        fixture.runtime.dispatch_collision_events({});
        collision_event unrelated = {};
        unrelated.a = fixture.create_entity();
        unrelated.b = fixture.create_entity();
        const std::array<collision_event, 1> unrelatedEvents = {unrelated};
        fixture.runtime.dispatch_collision_events(unrelatedEvents);
        CHECK(fixture.null_host().recorder().count("dispatch_collisions") == 1);
    }

    TEST_CASE("script_runtime - collision dispatch skips classes without the callback") {
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity a = fixture.create_entity();
        const ecs::entity b = fixture.create_entity();
        const script_entry_id enemyA = fixture.runtime.add_script(a, hash_of(ENEMY), {});   // TRIGGER_ENTER only
        static_cast<void>(fixture.runtime.add_script(b, hash_of(PLAYER), {}));    // COLLISION_ENTER only
        fixture.runtime.flush_pending();
        fixture.null_host().reset_recorder();

        collision_event event = {};
        event.a = a;
        event.b = b;
        event.kind = collision_kind::TRIGGER_ENTER;
        const std::array<collision_event, 1> events = {event};
        fixture.runtime.dispatch_collision_events(events);
        const auto &records = fixture.null_host().recorder().collisions();
        REQUIRE(records.size() == 1);
        CHECK(records[0].slot == fixture.slot_of(enemyA));
        CHECK(records[0].kind == collision_kind::TRIGGER_ENTER);

        event.kind = collision_kind::COLLISION_EXIT;
        const std::array<collision_event, 1> exits = {event};
        fixture.runtime.dispatch_collision_events(exits);
        CHECK(records.size() == 1);
    }

    TEST_CASE("script_runtime - reload preserves serialised fields and recreates instances") {
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id id = fixture.runtime.add_script(
          entity,
          hash_of(PLAYER),
          json(R"({"speed": 7.5})").root()
        );
        fixture.runtime.flush_pending();
        const i32 oldSlot = fixture.slot_of(id);

        // The live instance changed its speed: the null host echoes the blob the test overrides.
        const core::json::document liveFields = json(R"({"speed": 11.0, "name": "live"})");
        const core::result<usize> needed = json_to_blob(player_class(), liveFields.root(), {});
        CONJURE_REQUIRE_OK(needed);
        std::vector<u8> changed(needed.value());
        const core::result<usize> written = json_to_blob(player_class(), liveFields.root(), changed);
        CONJURE_REQUIRE_OK(written);
        REQUIRE(written.value() == changed.size());
        fixture.null_host().set_collect_override(oldSlot, changed);
        fixture.null_host().reset_recorder();

        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly({}));
        const std::vector<std::string> &calls = fixture.null_host().recorder().calls();
        const std::vector<std::string> expected = {
          "load_game_assembly", "collect_fields", "destroy_instances", "unload_game_assembly", "create_instances"
        };
        CHECK(calls == expected);
        REQUIRE(fixture.null_host().recorder().destroys().size() == 1);
        CHECK_FALSE(fixture.null_host().recorder().destroys()[0].callOnDestroy);
        const script_entry *entryValue = fixture.runtime.entry(id);
        REQUIRE(entryValue != nullptr);
        CHECK(entryValue->is_live());
        CHECK(entryValue->fields.root()["speed"].as_f64() == doctest::Approx(11.0));
        CHECK(entryValue->fields.root()["name"].as_string() == "live");
        CHECK(fixture.runtime.stats().reloadCount == 1);
        CHECK(fixture.runtime.stats().liveInstances == 1);
        const core::result<core::json::document> recreated = blob_to_json(
          player_class(),
          fixture.null_host().recorder().creates()[0].fields
        );
        CONJURE_REQUIRE_OK(recreated);
        CHECK(recreated.value().root()["speed"].as_f64() == doctest::Approx(11.0));
    }

    TEST_CASE("script_runtime - reload keeps the started flag so OnStart does not run again") {
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const script_entry_id started = fixture.runtime.add_script(fixture.create_entity(), hash_of(PLAYER), {});
        fixture.runtime.flush_pending();
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        const script_entry_id fresh = fixture.runtime.add_script(fixture.create_entity(), hash_of(PLAYER), {});
        fixture.runtime.flush_pending();
        REQUIRE(fixture.runtime.entry(started)->has_flag(script_entry_flags::STARTED));
        REQUIRE_FALSE(fixture.runtime.entry(fresh)->has_flag(script_entry_flags::STARTED));

        fixture.null_host().reset_recorder();
        bool reloadedFired = false;
        fixture.runtime.on_reloaded = core::delegate<void()>::bind([&reloadedFired]() {
            reloadedFired = true;
        });
        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly({}));
        CHECK(reloadedFired);
        const auto &creates = fixture.null_host().recorder().creates();
        REQUIRE(creates.size() == 2);
        u32 alreadyStarted = 0;
        for (const auto &create : creates) {
            if ((create.flags & CREATE_FLAG_ALREADY_STARTED) != 0) {
                ++alreadyStarted;
            }
        }
        CHECK(alreadyStarted == 1);
        CHECK(fixture.runtime.entry(started)->has_flag(script_entry_flags::STARTED));
        CHECK_FALSE(fixture.runtime.entry(fresh)->has_flag(script_entry_flags::STARTED));
    }

    TEST_CASE("script_runtime - a failed reload keeps the old assembly running") {
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const script_entry_id id = fixture.runtime.add_script(fixture.create_entity(), hash_of(PLAYER), {});
        fixture.runtime.flush_pending();
        const i32 slot = fixture.slot_of(id);
        fixture.null_host().set_load_result(status_of(script_error::ASSEMBLY_LOAD_FAILED));
        fixture.null_host().reset_recorder();
        CONJURE_CHECK_ERROR(fixture.runtime.reload_game_assembly({}), script_error::ASSEMBLY_LOAD_FAILED);
        CHECK(fixture.null_host().recorder().calls() == std::vector<std::string>{"load_game_assembly"});
        CHECK(fixture.slot_of(id) == slot);
        CHECK(fixture.null_host().is_slot_live(slot));
        CHECK(fixture.runtime.classes().find(PLAYER) != nullptr);
        CHECK_FALSE(fixture.runtime.classes().is_staging_open());
        CHECK(fixture.runtime.stats().reloadCount == 0);
    }

    TEST_CASE("script_runtime - a class missing after reload keeps its JSON and returns later") {
        log_capture logs = {};
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const script_entry_id id = fixture.runtime.add_script(
          fixture.create_entity(),
          hash_of(ENEMY),
          json(R"({"hp": 3})").root()
        );
        fixture.runtime.flush_pending();

        REQUIRE(fixture.null_host().remove_fake_class(hash_of(ENEMY)));
        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly({}));
        const script_entry *entryValue = fixture.runtime.entry(id);
        REQUIRE(entryValue != nullptr);
        CHECK_FALSE(entryValue->is_live());
        CHECK(entryValue->has_flag(script_entry_flags::CLASS_MISSING));
        CHECK(entryValue->fields.root()["hp"].as_i64() == 3);
        CHECK(logs.count(core::log_level::WARN) == 1);
        // A second flush does not warn again for the same class.
        static_cast<void>(fixture.runtime.add_script(fixture.create_entity(), hash_of(ENEMY), {}));
        fixture.runtime.flush_pending();
        CHECK(logs.count(core::log_level::WARN) == 1);

        fixture.null_host().add_fake_class(enemy_class());
        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly({}));
        CHECK(fixture.runtime.entry(id)->is_live());
        CHECK_FALSE(fixture.runtime.entry(id)->has_flag(script_entry_flags::CLASS_MISSING));
        CHECK(fixture.runtime.entry(id)->fields.root()["hp"].as_i64() == 3);
    }

    TEST_CASE("script_runtime - a disabled runtime keeps entries with an invalid slot") {
        log_capture logs = {};
        ecs::component_registry registry = {};
        register_test_components(registry);
        ecs::world world(registry);
        {
            script_runtime runtime(create_script_host(script_backend::DOTNET));
            runtime.attach_world(&world);
            script_runtime_config config = {};
            config.host.backend = script_backend::DOTNET;
            config.host.managedDirectory = "definitely/not/a/managed/directory";
            config.initialMode = script_mode::PLAY;
            CONJURE_CHECK_ERROR(runtime.initialize(config), script_error::MANAGED_SDK_MISSING);
            CHECK(runtime.status() == script_host_status::DISABLED);
            CHECK(logs.count(core::log_level::WARN) == 1);

            const core::result<ecs::entity> entity = world.create_entity();
            CONJURE_REQUIRE_OK(entity);
            const script_entry_id id = runtime.add_script(entity.value(), 123, json(R"({"x": 1})").root());
            REQUIRE(id.is_valid());
            runtime.flush_pending();
            runtime.run_phase(script_phase::UPDATE, 0.1f);
            CHECK(runtime.entry(id)->managedSlot == INVALID_SLOT);
            CHECK(runtime.entry(id)->fields.root()["x"].as_i64() == 1);
            CHECK(runtime.stats().liveInstances == 0);
            CONJURE_CHECK_ERROR(runtime.load_game_assembly("Game.dll"), script_error::NOT_INITIALIZED);
            conjure_query_scope scope = {};
            const std::array<u32, 1> ids = {1};
            CONJURE_CHECK_ERROR(runtime.begin_query_scope(ids, scope), script_error::NOT_INITIALIZED);
            runtime.shutdown();
            runtime.attach_world(nullptr);
        }
        CHECK(script_runtime::active() == nullptr);
    }

    TEST_CASE("script_runtime - serialize and deserialize an entity's scripts round trip") {
        loaded_fixture fixture = {};
        const ecs::entity entity = fixture.create_entity();
        const ecs::entity other = fixture.create_entity();
        core::json::document fields = core::json::document::object();
        fields.set("speed", 3.5f);
        fields.set("target", other.to_u64());
        fields.set("custom", "kept");
        static_cast<void>(fixture.runtime.add_script(entity, hash_of(PLAYER), fields.root()));
        static_cast<void>(fixture.runtime.add_script(entity, hash_of(ENEMY), json(R"({"hp": 2})").root()));

        core::result<core::json::document> serialized = fixture.runtime.serialize_entity_scripts(entity);
        CONJURE_REQUIRE_OK(serialized);
        const core::json::value scripts = serialized.value().root()["scripts"];
        REQUIRE(scripts.size() == 2);
        CHECK(scripts[0]["class"].as_string() == PLAYER);
        CHECK(scripts[0]["hash"].as_u64() == hash_of(PLAYER));
        CHECK(scripts[0]["fields"]["speed"].as_f64() == doctest::Approx(3.5));
        CHECK(scripts[0]["fields"]["custom"].as_string() == "kept");
        CHECK_FALSE(scripts[0].contains("started"));
        CHECK(scripts[1]["fields"]["hp"].as_i64() == 2);

        const ecs::entity restored = fixture.create_entity();
        CONJURE_REQUIRE_OK(fixture.runtime.deserialize_entity_scripts(restored, serialized.value().root()));
        const std::vector<script_entry_id> ids = fixture.runtime.scripts_of(restored);
        REQUIRE(ids.size() == 2);
        CHECK(fixture.runtime.entry(ids[0])->classHash == hash_of(PLAYER));
        CHECK(fixture.runtime.entry(ids[0])->fields.root()["custom"].as_string() == "kept");
        CHECK(fixture.runtime.entry(ids[1])->fields.root()["hp"].as_i64() == 2);
        core::result<core::json::document> again = fixture.runtime.serialize_entity_scripts(restored);
        CONJURE_REQUIRE_OK(again);
        CHECK(again.value() == serialized.value());

        // Deserialising replaces, and an entity without scripts serialises to an empty list.
        CONJURE_REQUIRE_OK(fixture.runtime.deserialize_entity_scripts(restored, json(R"({"scripts":[]})").root()));
        CHECK(fixture.runtime.scripts_of(restored).empty());
        const ecs::entity plain = fixture.create_entity();
        core::result<core::json::document> none = fixture.runtime.serialize_entity_scripts(plain);
        CONJURE_REQUIRE_OK(none);
        CHECK(none.value().root()["scripts"].size() == 0);
        CONJURE_CHECK_ERROR(
          fixture.runtime.deserialize_entity_scripts(plain, json(R"({"scripts": 5})").root()),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          fixture.runtime.deserialize_entity_scripts(plain, json(R"([1])").root()),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_REQUIRE_OK(fixture.world.destroy_entity(plain));
        CONJURE_CHECK_ERROR(fixture.runtime.serialize_entity_scripts(plain), ecs::ecs_error::ENTITY_NOT_ALIVE);
        CONJURE_CHECK_ERROR(
          fixture.runtime.deserialize_entity_scripts(plain, serialized.value().root()),
          ecs::ecs_error::ENTITY_NOT_ALIVE
        );
    }

    TEST_CASE("script_runtime - deserialising in play mode destroys the old instances and creates the new ones") {
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        static_cast<void>(fixture.runtime.add_script(entity, hash_of(PLAYER), {}));
        fixture.runtime.flush_pending();
        fixture.null_host().reset_recorder();
        const core::json::document replacement = json(
          R"({"scripts":[{"class":"Game.Enemy","fields":{"hp":1},"started":true}]})"
        );
        CONJURE_REQUIRE_OK(fixture.runtime.deserialize_entity_scripts(entity, replacement.root()));
        CHECK(fixture.null_host().recorder().destroys().size() == 1);
        fixture.runtime.flush_pending();
        REQUIRE(fixture.null_host().recorder().creates().size() == 1);
        CHECK(fixture.null_host().recorder().creates()[0].classHash == hash_of(ENEMY));
        CHECK((fixture.null_host().recorder().creates()[0].flags & CREATE_FLAG_ALREADY_STARTED) != 0);
        CHECK(fixture.runtime.stats().liveInstances == 1);
    }

    TEST_CASE("script_runtime - the frame block is 64-byte aligned and reachable through the table") {
        loaded_fixture fixture = {};
        const conjure_frame_block &block = fixture.runtime.frame_block();
        CHECK(reinterpret_cast<std::uintptr_t>(&block) % 64 == 0);
        CHECK(fixture.runtime.native_api().frame == &block);
        CHECK(block.time_scale == 1.0f);
        CHECK(fixture.runtime.native_api().size == sizeof(conjure_native_api));
        CHECK(fixture.runtime.native_api().layout_hash == CONJURE_NATIVE_API_HASH);
        CHECK(fixture.runtime.native_api().log_write != nullptr);
        CHECK(fixture.runtime.native_api().transform_get_local == nullptr);
    }

    TEST_CASE("script_runtime - flush handles creates requested during a flush and bounds the loop") {
        log_capture logs = {};
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        static_cast<void>(fixture.runtime.add_script(entity, hash_of(PLAYER), {}));

        fixture.runtime.flush_pending();
        CHECK(fixture.runtime.stats().liveInstances == 1);

        // Creates queued while a flush runs (as OnCreate would queue them) are picked up by the same loop.
        for (u32 index = 0; index < 20; ++index) {
            static_cast<void>(fixture.runtime.add_script(fixture.create_entity(), hash_of(ENEMY), {}));
        }
        fixture.runtime.flush_pending();
        CHECK(fixture.runtime.stats().liveInstances == 21);
        CHECK(fixture.runtime.stats().pendingCreates == 0);
        CHECK(logs.count(core::log_level::WARN) == 0);
    }

    TEST_CASE("script_runtime - remove_script destroys one entry and keeps the others") {
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id player = fixture.runtime.add_script(entity, hash_of(PLAYER), {});
        const script_entry_id enemy = fixture.runtime.add_script(entity, hash_of(ENEMY), {});
        fixture.runtime.flush_pending();
        const i32 playerSlot = fixture.slot_of(player);
        const i32 enemySlot = fixture.slot_of(enemy);
        fixture.null_host().reset_recorder();

        fixture.runtime.remove_script(entity, player);
        REQUIRE(fixture.null_host().recorder().destroys().size() == 1);
        CHECK(fixture.null_host().recorder().destroys()[0].slot == playerSlot);
        CHECK(fixture.null_host().recorder().destroys()[0].callOnDestroy);
        const std::vector<script_entry_id> remaining = fixture.runtime.scripts_of(entity);
        REQUIRE(remaining.size() == 1);
        CHECK(fixture.runtime.entry(remaining[0])->classHash == hash_of(ENEMY));
        CHECK(fixture.runtime.entry(remaining[0])->managedSlot == enemySlot);
        CHECK(fixture.runtime.entry_of_slot(enemySlot) == remaining[0]);
        fixture.runtime.remove_script(entity, script_entry_id{});       // unknown ids are ignored
        CHECK(fixture.runtime.scripts_of(entity).size() == 1);

        // Removing a queued-but-not-created entry drops its pending create.
        const script_entry_id queued = fixture.runtime.add_script(entity, hash_of(PLAYER), {});
        CHECK(fixture.runtime.stats().pendingCreates == 1);
        fixture.runtime.remove_script(entity, queued);
        CHECK(fixture.runtime.stats().pendingCreates == 0);
        fixture.runtime.flush_pending();
        CHECK(fixture.runtime.stats().liveInstances == 1);
    }

    TEST_CASE("script_runtime - set_field validates and applies a single-field blob when live") {
        loaded_fixture fixture = {};
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id id = fixture.runtime.add_script(entity, hash_of(PLAYER), {});
        CONJURE_CHECK_OK(fixture.runtime.set_field(id, hash_of("speed"), json("4.0").root()));
        CHECK(fixture.runtime.entry(id)->fields.root()["speed"].as_f64() == doctest::Approx(4.0));
        CONJURE_CHECK_ERROR(
          fixture.runtime.set_field(id, hash_of("nope"), json("1").root()),
          script_error::FIELD_NOT_FOUND
        );
        CONJURE_CHECK_ERROR(
          fixture.runtime.set_field(id, hash_of("speed"), json("\"text\"").root()),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          fixture.runtime.set_field(script_entry_id{}, hash_of("speed"), json("1").root()),
          script_error::INSTANCE_NOT_FOUND
        );
        const script_entry_id unknownClass = fixture.runtime.add_script(entity, 555, {});
        CONJURE_CHECK_ERROR(
          fixture.runtime.set_field(unknownClass, hash_of("speed"), json("1").root()),
          script_error::CLASS_NOT_FOUND
        );

        fixture.runtime.set_mode(script_mode::PLAY);
        fixture.null_host().reset_recorder();
        CONJURE_CHECK_OK(fixture.runtime.set_field(id, hash_of("name"), json("\"live\"").root()));
        REQUIRE(fixture.null_host().recorder().applies().size() == 1);
        CHECK(fixture.null_host().recorder().applies()[0].slot == fixture.slot_of(id));
        script_class_info single = player_class();
        const core::result<core::json::document> applied = blob_to_json(
          single,
          fixture.null_host().recorder().applies()[0].blob
        );
        CONJURE_REQUIRE_OK(applied);
        CHECK(applied.value().root().size() == 1);
        CHECK(applied.value().root()["name"].as_string() == "live");
    }

    TEST_CASE("script_runtime - collect_entry_fields refreshes live JSON and preserves unknown keys") {
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const script_entry_id id = fixture.runtime.add_script(
          fixture.create_entity(),
          hash_of(PLAYER),
          json(R"({"speed": 1.0, "custom": true})").root()
        );
        fixture.runtime.flush_pending();
        std::vector<u8> live(64);
        const core::result<usize> written = json_to_blob(player_class(), json(R"({"speed": 6.0})").root(), live);
        CONJURE_REQUIRE_OK(written);
        live.resize(written.value());
        fixture.null_host().set_collect_override(fixture.slot_of(id), live);

        const core::result<core::json::document> collected = fixture.runtime.collect_entry_fields(id);
        CONJURE_REQUIRE_OK(collected);
        CHECK(collected.value().root()["speed"].as_f64() == doctest::Approx(6.0));
        CHECK(collected.value().root()["custom"].as_bool() == true);
        CONJURE_CHECK_ERROR(fixture.runtime.collect_entry_fields(script_entry_id{}), script_error::INSTANCE_NOT_FOUND);
    }

    TEST_CASE("script_runtime - remap_entity_fields rewrites entity references through the class schema") {
        loaded_fixture fixture = {};
        const ecs::entity entity = fixture.create_entity();
        const ecs::entity before = {40, 1};
        const ecs::entity after = {50, 1};
        core::json::document fields = core::json::document::object();
        fields.set("target", before.to_u64());
        const script_entry_id id = fixture.runtime.add_script(entity, hash_of(PLAYER), fields.root());
        fixture.runtime.remap_entity_fields(entity, [before, after](ecs::entity value) {
            return value == before ? after : value;
        });
        CHECK(entity_from_json(fixture.runtime.entry(id)->fields.root()["target"]) == after);
        fixture.runtime.remap_entity_fields(fixture.create_entity(), [](ecs::entity value) {
            return value;
        });
    }

    TEST_CASE("script_runtime - add_script fails for dead entities, detached worlds and full sets") {
        loaded_fixture fixture = {};
        const ecs::entity entity = fixture.create_entity();
        CONJURE_REQUIRE_OK(fixture.world.destroy_entity(entity));
        CHECK_FALSE(fixture.runtime.add_script(entity, hash_of(PLAYER), {}).is_valid());

        const ecs::entity full = fixture.create_entity();
        for (u32 index = 0; index < MAX_SCRIPTS_PER_ENTITY; ++index) {
            CAPTURE(index);
            CHECK(fixture.runtime.add_script(full, hash_of(PLAYER), {}).is_valid());
        }
        {
            log_capture logs = {};
            CHECK_FALSE(fixture.runtime.add_script(full, hash_of(PLAYER), {}).is_valid());
            CHECK(logs.count(core::log_level::WARN) == 1);
        }
        CHECK(fixture.runtime.scripts_of(full).size() == MAX_SCRIPTS_PER_ENTITY);

        fixture.runtime.attach_world(nullptr);
        CHECK_FALSE(fixture.runtime.add_script(full, hash_of(PLAYER), {}).is_valid());
        CHECK(fixture.runtime.scripts_of(full).empty());
        fixture.runtime.attach_world(&fixture.world);
    }

    TEST_CASE("script_runtime - mark_faulted flags the entry of a slot") {
        log_capture logs = {};
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const script_entry_id id = fixture.runtime.add_script(fixture.create_entity(), hash_of(PLAYER), {});
        fixture.runtime.flush_pending();
        fixture.runtime.mark_faulted(fixture.slot_of(id), script_callback::UPDATE);
        CHECK(fixture.runtime.entry(id)->has_flag(script_entry_flags::FAULTED));
        CHECK(logs.contains(core::log_level::WARN, "UPDATE"));
        CHECK(logs.contains(core::log_level::WARN, "Game.Player"));
        fixture.runtime.mark_faulted(99, script_callback::UPDATE);     // unknown slots are ignored
        CHECK(logs.count(core::log_level::WARN) == 1);
    }

    TEST_CASE("script_runtime - initialize twice is refused and shutdown allows a restart") {
        loaded_fixture fixture = {};
        script_runtime_config config = {};
        config.host.backend = script_backend::NULL_HOST;
        CONJURE_CHECK_ERROR(fixture.runtime.initialize(config), core::error_code::INVALID_STATE);
        fixture.runtime.set_mode(script_mode::PLAY);
        static_cast<void>(fixture.runtime.add_script(fixture.create_entity(), hash_of(PLAYER), {}));
        fixture.runtime.flush_pending();
        fixture.runtime.shutdown();
        CHECK(fixture.runtime.status() == script_host_status::UNINITIALIZED);
        CHECK(fixture.runtime.stats().liveInstances == 0);
        CHECK(fixture.runtime.classes().size() == 0);
        CHECK_FALSE(fixture.runtime.is_assembly_loaded());
        CHECK(fixture.null_host().recorder().shutdown_count() == 1);
        CONJURE_REQUIRE_OK(fixture.runtime.initialize(config));
        CHECK(fixture.runtime.status() == script_host_status::READY);
        CHECK(fixture.runtime.mode() == script_mode::EDIT);
    }

    TEST_CASE("script_runtime - entity names are remembered for live entities only") {
        loaded_fixture fixture = {};
        const ecs::entity entity = fixture.create_entity();
        CHECK(fixture.runtime.entity_name(entity).empty());
        fixture.runtime.set_entity_name(entity, "hero");
        CHECK(fixture.runtime.entity_name(entity) == "hero");
        CHECK(fixture.runtime.find_entity_by_name("hero") == entity);
        CHECK(fixture.runtime.find_entity_by_name("villain") == ecs::NULL_ENTITY);
        fixture.runtime.set_entity_name(entity, "");
        CHECK(fixture.runtime.entity_name(entity).empty());
        fixture.runtime.set_entity_name(entity, "hero");
        CONJURE_REQUIRE_OK(fixture.world.destroy_entity(entity));
        CHECK(fixture.runtime.entity_name(entity).empty());
        CHECK(fixture.runtime.find_entity_by_name("hero") == ecs::NULL_ENTITY);
        CHECK(fixture.runtime.note_mirror_mismatch(5));
        CHECK_FALSE(fixture.runtime.note_mirror_mismatch(5));
    }

    TEST_CASE("script_runtime - a world created without the component registration warns on attach") {
        log_capture logs = {};
        ecs::component_registry bare = {};
        ecs::world world(bare);
        script_runtime runtime(create_script_host(script_backend::NULL_HOST));
        runtime.attach_world(&world);
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK_FALSE(runtime.script_component_id().is_valid());
        runtime.attach_world(nullptr);
    }

    TEST_CASE("script_runtime - a game assembly in a non-ASCII directory reaches the host as UTF-8") {
        scripting_fixture fixture = {};
        temp_directory directory = {};
        // "Jörg" spelt as UTF-8 bytes: the classes fixture beside the assembly is found only through them.
        const std::string_view folderUtf8 = "J\xC3\xB6rg";
        core::vfs::native_file_system files(directory.path());
        CONJURE_REQUIRE_OK(files.create_directories(folderUtf8));
        CONJURE_REQUIRE_OK(files.write_text_file(
          std::string(folderUtf8) + "/Game.classes.json",
          R"({"classes":[{"name":"Game.Umlaut"}]})"
        ));
        const std::u8string folder(folderUtf8.begin(), folderUtf8.end());
        const std::filesystem::path assembly = directory.path() / std::filesystem::path(folder) / "Game.dll";

        CONJURE_REQUIRE_OK(fixture.runtime.load_game_assembly(assembly));
        const std::u8string assemblyUtf8 = assembly.u8string();
        const std::string expected(assemblyUtf8.begin(), assemblyUtf8.end());
        REQUIRE_FALSE(fixture.null_host().recorder().loaded_assemblies().empty());
        CHECK(fixture.null_host().recorder().loaded_assemblies().back() == expected);
        CHECK(fixture.null_host().loaded_assembly() == expected);
        CHECK(fixture.runtime.classes().find("Game.Umlaut") != nullptr);
        CHECK(fixture.runtime.assembly_path() == assembly);
    }

    TEST_CASE("script_runtime - initialize with a missing game assembly fails but stays READY") {
        temp_directory directory = {};
        ecs::component_registry registry = {};
        register_test_components(registry);
        ecs::world world(registry);
        {
            script_runtime runtime(create_script_host(script_backend::NULL_HOST));
            runtime.attach_world(&world);
            script_runtime_config config = {};
            config.host.backend = script_backend::NULL_HOST;
            config.gameAssemblyPath = directory.file("Missing.dll");
            CONJURE_CHECK_ERROR(runtime.initialize(config), script_error::ASSEMBLY_LOAD_FAILED);
            CHECK(runtime.status() == script_host_status::READY);
            CHECK_FALSE(runtime.is_assembly_loaded());
            CHECK_FALSE(runtime.classes().is_staging_open());

            // Scripting keeps working without an assembly: the next build loads normally.
            CONJURE_REQUIRE_OK(directory.write_text("Game.classes.json", R"({"classes":[{"name":"Game.Late"}]})"));
            CONJURE_REQUIRE_OK(runtime.load_game_assembly(directory.file("Game.dll")));
            CHECK(runtime.is_assembly_loaded());
            CHECK(runtime.classes().find("Game.Late") != nullptr);
            runtime.shutdown();
            runtime.attach_world(nullptr);
        }
        CHECK(script_runtime::active() == nullptr);
    }

    TEST_CASE("script_runtime - an entity name survives script removal and is forgotten on every destroy path") {
        loaded_fixture fixture = {};
        const ecs::entity scripted = fixture.create_entity();
        REQUIRE(fixture.runtime.add_script(scripted, hash_of(PLAYER), {}).is_valid());
        fixture.runtime.set_entity_name(scripted, "scripted");
        CONJURE_REQUIRE_OK(fixture.world.remove<script_component>(scripted));
        CHECK(fixture.runtime.entity_name(scripted) == "scripted");
        CHECK(fixture.runtime.find_entity_by_name("scripted") == scripted);

        const ecs::entity native = fixture.create_entity();
        const ecs::entity requested = fixture.create_entity();
        const ecs::entity buffered = fixture.create_entity();
        fixture.runtime.set_entity_name(native, "native");
        fixture.runtime.set_entity_name(requested, "requested");
        fixture.runtime.set_entity_name(buffered, "buffered");
        REQUIRE(fixture.runtime.entity_name_count() == 4);

        CONJURE_REQUIRE_OK(fixture.world.destroy_entity(scripted));
        CONJURE_REQUIRE_OK(fixture.world.destroy_entity(native));
        CHECK(fixture.runtime.entity_name_count() == 2);

        // Entity.Destroy inside a query scope goes through the runtime's deferred command buffer.
        {
            const ecs::iteration_scope lock = fixture.world.lock_iteration();
            fixture.runtime.request_entity_destroy(requested);
        }
        CHECK(fixture.runtime.entity_name(requested) == "requested");
        static_cast<void>(fixture.world.flush(fixture.runtime.deferred_commands()));
        CHECK_FALSE(fixture.world.is_alive(requested));
        CHECK(fixture.runtime.entity_name_count() == 1);

        ecs::command_buffer commands(fixture.world);
        commands.destroy_entity(buffered);
        static_cast<void>(fixture.world.flush(commands));
        CHECK_FALSE(fixture.world.is_alive(buffered));
        CHECK(fixture.runtime.entity_name_count() == 0);
        CHECK(fixture.runtime.find_entity_by_name("buffered") == ecs::NULL_ENTITY);
    }

    TEST_CASE("script_runtime - remove_script ignores an id that belongs to another entity") {
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity first = fixture.create_entity();
        const ecs::entity second = fixture.create_entity();
        const script_entry_id firstId = fixture.runtime.add_script(first, hash_of(PLAYER), {});
        const script_entry_id secondId = fixture.runtime.add_script(second, hash_of(ENEMY), {});
        fixture.runtime.flush_pending();
        const i32 firstSlot = fixture.slot_of(firstId);
        REQUIRE(firstSlot != INVALID_SLOT);
        fixture.null_host().reset_recorder();

        fixture.runtime.remove_script(second, firstId);
        CHECK(fixture.null_host().recorder().destroys().empty());
        CHECK(fixture.slot_of(firstId) == firstSlot);
        CHECK(fixture.null_host().is_slot_live(firstSlot));
        CHECK(fixture.runtime.scripts_of(first).size() == 1);
        REQUIRE(fixture.runtime.scripts_of(second).size() == 1);
        CHECK(fixture.runtime.entry(secondId)->classHash == hash_of(ENEMY));
        fixture.runtime.remove_script(fixture.create_entity(), firstId);      // an entity without scripts
        CHECK(fixture.runtime.scripts_of(first).size() == 1);

        fixture.runtime.remove_script(first, firstId);
        CHECK(fixture.runtime.scripts_of(first).empty());
        REQUIRE(fixture.null_host().recorder().destroys().size() == 1);
        CHECK(fixture.null_host().recorder().destroys()[0].slot == firstSlot);
    }

    TEST_CASE("script_runtime - an OnCreate fault is flagged and logged with its own message") {
        log_capture logs = {};
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const script_entry_id id = fixture.runtime.add_script(fixture.create_entity(), hash_of(PLAYER), {});
        conjure_native_api &api = fixture.runtime.native_api();
        fixture.null_host().set_create_hook([&api](const conjure_create_request &, i32 slot) {
            // What the managed guard does when OnCreate throws; the slot is not mapped yet.
            api.script_mark_faulted(slot, static_cast<uint8_t>(script_callback::CREATE));
        });
        fixture.runtime.flush_pending();
        fixture.null_host().set_create_hook({});

        REQUIRE(fixture.runtime.entry(id) != nullptr);
        CHECK(fixture.runtime.entry(id)->is_live());
        CHECK(fixture.runtime.entry(id)->has_flag(script_entry_flags::FAULTED));
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK(logs.contains(core::log_level::WARN, "OnCreate"));
        CHECK(logs.contains(core::log_level::WARN, PLAYER));
        CHECK_FALSE(logs.contains(core::log_level::WARN, "three consecutive"));
    }

    TEST_CASE("script_runtime - a reload, a shutdown and a world attach re-arm the mirror mismatch warnings") {
        loaded_fixture fixture = {};
        CHECK(fixture.runtime.note_mirror_mismatch(5));
        CHECK_FALSE(fixture.runtime.note_mirror_mismatch(5));

        // A failed load changes nothing, a successful one re-arms.
        fixture.null_host().set_load_result(status_of(script_error::ASSEMBLY_LOAD_FAILED));
        CONJURE_CHECK_ERROR(fixture.runtime.load_game_assembly({}), script_error::ASSEMBLY_LOAD_FAILED);
        CHECK_FALSE(fixture.runtime.note_mirror_mismatch(5));
        fixture.null_host().set_load_result(0);
        CONJURE_REQUIRE_OK(fixture.runtime.load_game_assembly({}));
        CHECK(fixture.runtime.note_mirror_mismatch(5));
        CHECK_FALSE(fixture.runtime.note_mirror_mismatch(5));

        fixture.runtime.shutdown();
        script_runtime_config config = {};
        config.host.backend = script_backend::NULL_HOST;
        CONJURE_REQUIRE_OK(fixture.runtime.initialize(config));
        CHECK(fixture.runtime.note_mirror_mismatch(5));
        CHECK_FALSE(fixture.runtime.note_mirror_mismatch(5));

        fixture.runtime.attach_world(&fixture.world);
        CHECK(fixture.runtime.note_mirror_mismatch(5));
        CHECK_FALSE(fixture.runtime.note_mirror_mismatch(5));
    }

    TEST_CASE("regression - review 1 OnDestroy may add scripts while the set being replaced is destroyed") {
        log_capture logs = {};
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        static_cast<void>(fixture.runtime.add_script(entity, hash_of(PLAYER), {}));
        fixture.runtime.flush_pending();

        // "Spawn loot on death": every OnDestroy creates scripted entities, growing the store under the
        // deserialisation that triggered it.
        constexpr u32 LOOT = 64;
        fixture.null_host().set_destroy_hook([&fixture](i32) {
            for (u32 index = 0; index < LOOT; ++index) {
                REQUIRE(fixture.runtime.add_script(fixture.create_entity(), hash_of(ENEMY), {}).is_valid());
            }
        });
        const core::json::document replacement = json(R"({"scripts":[{"class":"Game.Enemy","fields":{"hp":5}}]})");
        CONJURE_REQUIRE_OK(fixture.runtime.deserialize_entity_scripts(entity, replacement.root()));
        fixture.null_host().set_destroy_hook({});

        const std::vector<script_entry_id> scripts = fixture.runtime.scripts_of(entity);
        REQUIRE(scripts.size() == 1);
        CHECK(fixture.runtime.entry(scripts[0])->classHash == hash_of(ENEMY));
        CHECK(fixture.runtime.entry(scripts[0])->fields.root()["hp"].as_i64() == 5);
        CHECK(fixture.runtime.store().size() == LOOT + 1);
        fixture.runtime.flush_pending();
        CHECK(fixture.runtime.stats().liveInstances == LOOT + 1);
        CHECK(fixture.runtime.entry(scripts[0])->is_live());
        CHECK(logs.count(core::log_level::WARN) == 0);

        // The same OnDestroy freeing the set itself (the entity went natively) is reported, not written to.
        fixture.null_host().set_destroy_hook([&fixture, entity](i32) {
            CONJURE_REQUIRE_OK(fixture.world.destroy_entity(entity));
        });
        CONJURE_CHECK_ERROR(
          fixture.runtime.deserialize_entity_scripts(entity, replacement.root()),
          script_error::INSTANCE_NOT_FOUND
        );
        fixture.null_host().set_destroy_hook({});
        CHECK_FALSE(fixture.world.is_alive(entity));
        fixture.runtime.flush_pending();
        CHECK(fixture.runtime.stats().liveInstances == LOOT);
    }

    TEST_CASE("regression - review 2 an OnCreate that destroys its entity leaves no orphan instance") {
        log_capture logs = {};
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        const ecs::entity other = fixture.create_entity();
        static_cast<void>(fixture.runtime.add_script(entity, hash_of(PLAYER), {}));
        static_cast<void>(fixture.runtime.add_script(other, hash_of(ENEMY), {}));
        fixture.runtime.flush_pending();
        REQUIRE(fixture.runtime.stats().liveInstances == 2);

        // A third entity's only script destroys its own entity from OnCreate: the entity goes at once (no
        // live slot yet), so the instance the batch just created must not outlive it.
        fixture.null_host().reset_recorder();
        const ecs::entity self = fixture.create_entity();
        static_cast<void>(fixture.runtime.add_script(self, hash_of(ENEMY), {}));
        fixture.null_host().set_create_hook([&fixture, self](const conjure_create_request &request, i32) {
            if (ecs::entity::from_u64(request.entity) == self) {
                fixture.runtime.request_entity_destroy(self);
            }
        });
        fixture.runtime.flush_pending();
        fixture.null_host().set_create_hook({});

        CHECK_FALSE(fixture.world.is_alive(self));
        CHECK(fixture.world.is_alive(entity));
        CHECK(fixture.world.is_alive(other));
        CHECK(fixture.null_host().live_slot_count() == 2);
        CHECK(fixture.runtime.stats().liveInstances == 2);
        CHECK(fixture.runtime.stats().pendingDestroys == 0);
        REQUIRE(fixture.null_host().recorder().destroys().size() == 1);
        CHECK(fixture.null_host().recorder().destroys()[0].callOnDestroy);
        CHECK(fixture.null_host().recorder().destroys()[0].slot == fixture.null_host().recorder().creates()[0].slot);
        CHECK(logs.count(core::log_level::WARN) == 0);
    }

    TEST_CASE("regression - review 4 a reload drains the pending destroys before unloading") {
        log_capture logs = {};
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        const script_entry_id id = fixture.runtime.add_script(entity, hash_of(PLAYER), {});
        const ecs::entity requested = fixture.create_entity();
        static_cast<void>(fixture.runtime.add_script(requested, hash_of(ENEMY), {}));
        fixture.runtime.flush_pending();
        const i32 slot = fixture.slot_of(id);

        // A native destroy and an Entity.Destroy earlier in the frame, then the build completes.
        CONJURE_REQUIRE_OK(fixture.world.destroy_entity(entity));
        fixture.runtime.request_entity_destroy(requested);
        REQUIRE(fixture.runtime.stats().pendingDestroys == 1);
        REQUIRE(fixture.runtime.stats().pendingEntityDestroys == 1);
        fixture.null_host().reset_recorder();
        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly({}));

        const std::vector<std::string> expected = {
          "destroy_instances", "destroy_instances", "load_game_assembly", "unload_game_assembly"
        };
        CHECK(fixture.null_host().recorder().calls() == expected);
        REQUIRE(fixture.null_host().recorder().destroys().size() == 2);
        CHECK(fixture.null_host().recorder().destroys()[0].slot == slot);
        CHECK(fixture.null_host().recorder().destroys()[0].callOnDestroy);
        CHECK(fixture.null_host().recorder().destroys()[1].callOnDestroy);
        CHECK_FALSE(fixture.world.is_alive(requested));
        CHECK(fixture.null_host().live_slot_count() == 0);
        CHECK(fixture.runtime.stats().liveInstances == 0);
        CHECK(fixture.runtime.stats().pendingDestroys == 0);
        CHECK(fixture.runtime.stats().pendingEntityDestroys == 0);
        CHECK(logs.count(core::log_level::WARN) == 0);
    }

    TEST_CASE("regression - review 5 leaving play clears the started and faulted flags, reload the faulted one") {
        log_capture logs = {};
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const script_entry_id id = fixture.runtime.add_script(fixture.create_entity(), hash_of(PLAYER), {});
        fixture.runtime.flush_pending();
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        fixture.runtime.mark_faulted(fixture.slot_of(id), script_callback::UPDATE);
        REQUIRE(fixture.runtime.entry(id)->has_flag(script_entry_flags::STARTED));
        REQUIRE(fixture.runtime.entry(id)->has_flag(script_entry_flags::FAULTED));

        SUBCASE("PLAY -> EDIT -> PLAY starts again") {
            fixture.runtime.set_mode(script_mode::EDIT);
            CHECK_FALSE(fixture.runtime.entry(id)->has_flag(script_entry_flags::STARTED));
            CHECK_FALSE(fixture.runtime.entry(id)->has_flag(script_entry_flags::FAULTED));
            fixture.null_host().reset_recorder();
            fixture.runtime.set_mode(script_mode::PLAY);
            REQUIRE(fixture.null_host().recorder().creates().size() == 1);
            CHECK((fixture.null_host().recorder().creates()[0].flags & CREATE_FLAG_ALREADY_STARTED) == 0);
            CHECK_FALSE(fixture.null_host().is_slot_started(fixture.slot_of(id)));
        }
        SUBCASE("a reload keeps started and clears faulted") {
            CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly({}));
            CHECK(fixture.runtime.entry(id)->is_live());
            CHECK(fixture.runtime.entry(id)->has_flag(script_entry_flags::STARTED));
            CHECK_FALSE(fixture.runtime.entry(id)->has_flag(script_entry_flags::FAULTED));
        }
    }

    TEST_CASE("regression - review 8 Entity.Destroy inside a query scope still runs OnDestroy with the entity alive") {
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity scripted = fixture.create_entity();
        static_cast<void>(fixture.runtime.add_script(scripted, hash_of(PLAYER), {}));
        fixture.runtime.flush_pending();
        const ecs::entity plain = fixture.create_entity();
        fixture.null_host().reset_recorder();
        {
            const ecs::iteration_scope lock = fixture.world.lock_iteration();
            REQUIRE(fixture.world.is_locked());
            fixture.runtime.request_entity_destroy(scripted);
            fixture.runtime.request_entity_destroy(plain);
            CHECK(fixture.runtime.stats().pendingEntityDestroys == 1);
            CHECK_FALSE(fixture.runtime.deferred_commands().empty());
            CHECK(fixture.world.is_alive(scripted));
            CHECK(fixture.world.is_alive(plain));
        }
        bool aliveInDestroy = false;
        fixture.null_host().set_destroy_hook([&fixture, scripted, &aliveInDestroy](i32) {
            aliveInDestroy = fixture.world.is_alive(scripted);
        });
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);     // flushes the deferred commands and the pending
        fixture.null_host().set_destroy_hook({});
        CHECK(aliveInDestroy);
        CHECK_FALSE(fixture.world.is_alive(scripted));
        CHECK_FALSE(fixture.world.is_alive(plain));
        REQUIRE(fixture.null_host().recorder().destroys().size() == 1);
        CHECK(fixture.null_host().recorder().destroys()[0].callOnDestroy);
    }

    TEST_CASE("regression - review 10 collision callbacks run inside FIXED_UPDATE and flushes inside their phase") {
        log_capture logs = {};
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity a = fixture.create_entity();
        const ecs::entity b = fixture.create_entity();
        static_cast<void>(fixture.runtime.add_script(a, hash_of(PLAYER), {}));
        static_cast<void>(fixture.runtime.add_script(b, hash_of(PLAYER), {}));
        fixture.runtime.flush_pending();
        CONJURE_REQUIRE_OK(fixture.world.add<test_position>(a));
        const std::array<u32, 1> position = {fixture.registry.id_of<test_position>().value};

        script_phase phaseInCollision = script_phase::COUNT;
        bool openedInCollision = false;
        fixture.null_host().set_collision_hook([&](const conjure_collision_record &) {
            phaseInCollision = fixture.runtime.current_phase();
            conjure_query_scope scope = {};
            openedInCollision = fixture.runtime.begin_query_scope(position, scope).has_value();
            // Deliberately left open: the dispatch force-closes it like a phase end would.
        });
        collision_event event = {};
        event.a = a;
        event.b = b;
        event.kind = collision_kind::COLLISION_ENTER;
        const std::array<collision_event, 1> events = {event};
        fixture.runtime.dispatch_collision_events(events);
        fixture.null_host().set_collision_hook({});
        CHECK(phaseInCollision == script_phase::FIXED_UPDATE);
        CHECK(openedInCollision);
        CHECK(fixture.runtime.current_phase() == script_phase::COUNT);
        CHECK(fixture.runtime.frame_block().current_phase == static_cast<uint8_t>(script_phase::COUNT));
        CHECK(fixture.runtime.open_query_scope_count() == 0);
        CHECK_FALSE(fixture.world.is_locked());
        CHECK(logs.count(core::log_level::WARN) == 2);                // one per record, both force-closed

        // OnCreate of a script created by the flush at the end of a phase runs inside that phase.
        script_phase phaseInCreate = script_phase::COUNT;
        core::result<u32> openedInCreate = core::make_error(script_error::NOT_IN_PHASE, "unset");
        fixture.null_host().set_create_hook([&](const conjure_create_request &, i32) {
            phaseInCreate = fixture.runtime.current_phase();
            conjure_query_scope scope = {};
            openedInCreate = fixture.runtime.begin_query_scope(position, scope);
            if (openedInCreate.has_value()) {
                fixture.runtime.end_query_scope(openedInCreate.value());
            }
        });
        static_cast<void>(fixture.runtime.add_script(fixture.create_entity(), hash_of(ENEMY), {}));
        fixture.runtime.run_phase(script_phase::UPDATE, 0.1f);
        CHECK(phaseInCreate == script_phase::UPDATE);
        CHECK(openedInCreate.has_value());
        CHECK(fixture.runtime.current_phase() == script_phase::COUNT);

        // Outside a phase (frame_begin's flush) a query scope is refused.
        static_cast<void>(fixture.runtime.add_script(fixture.create_entity(), hash_of(ENEMY), {}));
        fixture.runtime.flush_pending();
        fixture.null_host().set_create_hook({});
        CHECK(phaseInCreate == script_phase::COUNT);
        CONJURE_CHECK_ERROR(openedInCreate, script_error::NOT_IN_PHASE);
        CHECK(logs.count(core::log_level::WARN) == 2);
    }

    TEST_CASE("regression - review 20 the serializer refuses an object without a scripts array") {
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity entity = fixture.create_entity();
        static_cast<void>(fixture.runtime.add_script(entity, hash_of(PLAYER), {}));
        fixture.runtime.flush_pending();
        const script_component *component = std::as_const(fixture.world).get<script_component>(entity);
        REQUIRE(component != nullptr);
        fixture.null_host().reset_recorder();
        CONJURE_CHECK_ERROR(
          fixture.runtime.read_set_json(component->setHandle, json(R"({})").root()),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          fixture.runtime.read_set_json(component->setHandle, json(R"({"foo":1})").root()),
          core::error_code::INVALID_ARGUMENT
        );
        CHECK(fixture.runtime.scripts_of(entity).size() == 1);
        CHECK(fixture.runtime.stats().liveInstances == 1);
        CHECK(fixture.null_host().recorder().destroys().empty());
        // A stale handle gets a fresh set (the ADDED observer adopts it later); a malformed document does not.
        const usize setsBefore = fixture.runtime.store().size();
        CONJURE_CHECK_ERROR(
          fixture.runtime.read_set_json(script_store::make_handle(4000, 7), json(R"({"other":[]})").root()),
          core::error_code::INVALID_ARGUMENT
        );
        CHECK(fixture.runtime.store().size() == setsBefore);
        const core::result<u32> fresh = fixture.runtime.read_set_json(
          script_store::make_handle(4000, 7),
          json(R"({"scripts":[]})").root()
        );
        CONJURE_REQUIRE_OK(fresh);
        CHECK(fresh.value() != script_store::make_handle(4000, 7));
        CHECK(fixture.runtime.store().size() == setsBefore + 1);
    }

    TEST_CASE("regression - review 21 swapping the attached world binds instances to that world only") {
        log_capture logs = {};
        loaded_fixture fixture = {};
        fixture.runtime.set_mode(script_mode::PLAY);
        const ecs::entity original = fixture.create_entity();
        static_cast<void>(fixture.runtime.add_script(original, hash_of(PLAYER), {}));
        fixture.runtime.flush_pending();
        REQUIRE(fixture.runtime.stats().liveInstances == 1);

        ecs::world other(fixture.registry);
        fixture.null_host().reset_recorder();
        fixture.runtime.attach_world(&other);
        CHECK(fixture.runtime.world() == &other);
        CHECK(logs.count(core::log_level::WARN) == 1);               // live instances dropped without OnDestroy
        REQUIRE(fixture.null_host().recorder().destroys().size() == 1);
        CHECK_FALSE(fixture.null_host().recorder().destroys()[0].callOnDestroy);
        CHECK(fixture.runtime.stats().liveInstances == 0);
        CHECK(fixture.runtime.store().size() == 1);                   // the original's set keeps its JSON

        // Only the attached world's entities get instances, in set_mode and in reload.
        const core::result<ecs::entity> created = other.create_entity();
        CONJURE_REQUIRE_OK(created);
        const ecs::entity swapped = created.value();
        static_cast<void>(fixture.runtime.add_script(swapped, hash_of(ENEMY), {}));
        fixture.runtime.flush_pending();
        CHECK(fixture.runtime.stats().liveInstances == 1);
        fixture.runtime.set_mode(script_mode::EDIT);
        fixture.null_host().reset_recorder();
        fixture.runtime.set_mode(script_mode::PLAY);
        REQUIRE(fixture.null_host().recorder().creates().size() == 1);
        CHECK(fixture.null_host().recorder().creates()[0].entity == swapped.to_u64());
        fixture.null_host().reset_recorder();
        CONJURE_REQUIRE_OK(fixture.runtime.reload_game_assembly({}));
        REQUIRE(fixture.null_host().recorder().creates().size() == 1);
        CHECK(fixture.null_host().recorder().creates()[0].entity == swapped.to_u64());

        // Swapping back adopts the original world's sets again (its component arrived while unobserved).
        fixture.runtime.set_mode(script_mode::EDIT);
        fixture.runtime.attach_world(&fixture.world);
        fixture.runtime.set_mode(script_mode::PLAY);
        REQUIRE(fixture.runtime.stats().liveInstances == 1);
        const i32 lastSlot = fixture.null_host().recorder().creates().back().slot;
        CHECK(fixture.null_host().slot_entity(lastSlot) == original.to_u64());
        CHECK(logs.count(core::log_level::WARN) == 1);
        fixture.runtime.set_mode(script_mode::EDIT);
    }
}
