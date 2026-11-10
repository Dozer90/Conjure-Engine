#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_source.h"
#include "conjure/core/hash.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/core/vfs/memory_file_system.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/world.h"
#include "conjure/scene/components.h"
#include "conjure/scene/prefab.h"
#include "conjure/scene/register.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"
#include "conjure/scene/scene_serializer.h"
#include "conjure/scene/scene_snapshot.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/null_script_host.h"
#include "conjure/scripting/script_class_registry.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_host.h"
#include "conjure/scripting/script_runtime.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Scripts survive the scene module's round trips without the scene knowing scripting exists (R29):
// script_component registers its own copy / destruct hooks and serializer, so a scene file, a prefab
// instantiation, a subtree duplicate and a play / stop snapshot all carry the script set. The managed
// side is the null host; the scene is driven through scene_manager and scene_serializer directly.

using namespace conjure;
using namespace conjure::testing;

namespace {

    constexpr std::string_view PLAYER_CLASS = "Game.Player";
    constexpr std::string_view FIELDS_JSON = "{\"hp\": 4, \"speed\": 1.5}";

    [[nodiscard]] u64 hash_of(std::string_view name) noexcept {
        return core::hash::fnv1a64(name);
    }

    [[nodiscard]] scripting::script_field_info make_field(
      std::string_view name,
      scripting::script_field_kind kind,
      std::string_view defaultJson
    ) {
        scripting::script_field_info field = {};
        field.name = std::string(name);
        field.hash = hash_of(name);
        field.kind = kind;
        const core::result<core::json::document> parsed = core::json::document::parse(defaultJson);
        REQUIRE(parsed.has_value());
        field.defaultValue = parsed.value();
        return field;
    }

    [[nodiscard]] scripting::script_class_info player_class() {
        scripting::script_class_info info = {};
        info.fullName = std::string(PLAYER_CLASS);
        info.hash = hash_of(PLAYER_CLASS);
        info.callbackMask = scripting::callback_bit(scripting::script_callback::UPDATE);
        info.fields.push_back(make_field("hp", scripting::script_field_kind::INT32, "1"));
        info.fields.push_back(make_field("speed", scripting::script_field_kind::FLOAT, "0.5"));
        info.sort_fields();
        return info;
    }

    /// <summary>
    /// Registry with the scene components and script_component, a world, an asset manager over a memory
    /// source, a scene_manager and, declared last so it detaches first, a script_runtime over the null
    /// host with the fake Game.Player class loaded.
    /// </summary>
    class scripting_scene_fixture {
        public:
            ecs::component_registry registry = {};
            ecs::world world;                                   // no default: built on the registry
            core::jobs::job_system assetJobs = {};              // never started: loads run inline
            assets::asset_manager assets;                       // no default: built from a desc
            scene::scene_manager manager;                       // no default: bound to world and assets
            scripting::script_runtime runtime;                  // no default: built on the host

        public:
            scripting_scene_fixture() :
                world((register_types(registry), registry)),
                assets(make_asset_desc()),
                manager(world, assets),
                runtime(scripting::create_script_host(scripting::script_backend::NULL_HOST)) {
                runtime.attach_registry(&registry);
                runtime.attach_world(&world);
                scripting::script_runtime_config config = {};
                config.host.backend = scripting::script_backend::NULL_HOST;
                CONJURE_REQUIRE_OK(runtime.initialize(config));
                REQUIRE(runtime.status() == scripting::script_host_status::READY);
                null_host().add_fake_class(player_class());
                CONJURE_REQUIRE_OK(runtime.load_game_assembly({}));
            }

            ~scripting_scene_fixture() {
                CONJURE_CHECK_OK(manager.active().validate());
                CONJURE_CHECK_OK(world.validate());
                runtime.destroy_all(false);
                runtime.attach_world(nullptr);
            }

            scripting_scene_fixture(const scripting_scene_fixture &) = delete;
            scripting_scene_fixture &operator=(const scripting_scene_fixture &) = delete;

        public:
            [[nodiscard]] scene::scene &active() noexcept {
                return manager.active();
            }

            [[nodiscard]] scripting::null_script_host &null_host() noexcept {
                return static_cast<scripting::null_script_host &>(runtime.host());
            }

            /// <summary>
            /// A scene entity named 'name' carrying one Game.Player script with FIELDS_JSON.
            /// </summary>
            [[nodiscard]] ecs::entity create_scripted(std::string_view name, ecs::entity parent = ecs::NULL_ENTITY) {
                const core::result<ecs::entity> created = active().create_entity(core::string_id(name), parent);
                REQUIRE(created.has_value());
                const core::result<core::json::document> fields = core::json::document::parse(FIELDS_JSON);
                REQUIRE(fields.has_value());
                const scripting::script_entry_id id =
                  runtime.add_script(created.value(), hash_of(PLAYER_CLASS), fields.value().root());
                REQUIRE(id.is_valid());
                return created.value();
            }

            /// <summary>
            /// Checks that the entity carries exactly the Game.Player script with FIELDS_JSON and returns its
            /// set handle.
            /// </summary>
            [[nodiscard]] u32 require_player_script(ecs::entity entity) {
                const std::vector<scripting::script_entry_id> scripts = runtime.scripts_of(entity);
                REQUIRE(scripts.size() == 1);
                const scripting::script_entry *entry = runtime.entry(scripts[0]);
                REQUIRE(entry != nullptr);
                CHECK(entry->classHash == hash_of(PLAYER_CLASS));
                CHECK(entry->fields.root()["hp"].as_i64() == 4);
                CHECK(entry->fields.root()["speed"].as_f64() == doctest::Approx(1.5));
                CHECK_FALSE(entry->has_flag(scripting::script_entry_flags::CLASS_MISSING));
                return scripts[0].setHandle;
            }

        private:
            static void register_types(ecs::component_registry &target) {
                CONJURE_REQUIRE_OK(scene::register_components(target));
                CONJURE_REQUIRE_OK(scripting::register_script_component(target));
            }

            [[nodiscard]] assets::asset_manager_desc make_asset_desc() {
                assets::asset_manager_desc desc = {};
                desc.jobs = &assetJobs;
                desc.source = std::make_unique<assets::memory_asset_source>();
                return desc;
            }
    };

    [[nodiscard]] std::string save_text(const scene::scene &source) {
        core::json::writer out = {};
        CONJURE_REQUIRE_OK(scene::scene_serializer::save_scene(source, out));
        return out.finish();
    }
}

TEST_SUITE("integration/scripting_scene") {

    TEST_CASE("scripting_scene - scripts survive scene save and load through the script_component serializer") {
        log_capture logs = {};
        scripting_scene_fixture fixture = {};
        const ecs::entity player = fixture.create_scripted("player");
        const ecs::entity sidekick = fixture.create_scripted("sidekick", player);
        CONJURE_REQUIRE_OK(fixture.active().set_local_transform(sidekick, scene::local_transform{}));
        const u32 originalHandle = fixture.require_player_script(player);
        CHECK(fixture.runtime.store().size() == 2);

        // The file carries the script set as JSON, not the store handle.
        core::vfs::memory_file_system fs = {};
        CONJURE_REQUIRE_OK(scene::scene_serializer::save_scene_file(fixture.active(), fs, "scenes/main.scene.json"));
        const std::string text = fs.read_text_file("scenes/main.scene.json").value();
        const std::string_view expected =
          "\"conjure.scripting.script_component\":{\"scripts\":[{\"class\":\"Game.Player\"";
        CHECK(text.find(expected) != std::string::npos);
        CHECK(text.find("\"hp\":4") != std::string::npos);
        CHECK(text.find("setHandle") == std::string::npos);

        // Unloading frees the sets through ops.destruct; loading the file re-creates them through readJson.
        fixture.manager.unload_all();
        CHECK(fixture.world.entity_count() == 0);
        CHECK(fixture.runtime.store().size() == 0);
        CHECK(fixture.runtime.store().find(originalHandle) == nullptr);

        const core::result<scene::scene_id> loaded = fixture.manager.load_file(fs, "scenes/main.scene.json");
        CONJURE_REQUIRE_OK(loaded);
        CHECK(fixture.world.entity_count() == 2);
        CHECK(fixture.runtime.store().size() == 2);
        const ecs::entity loadedPlayer = fixture.active().find_path("player");
        const ecs::entity loadedSidekick = fixture.active().find_path("player/sidekick");
        REQUIRE_FALSE(loadedPlayer.is_null());
        REQUIRE_FALSE(loadedSidekick.is_null());
        const u32 playerHandle = fixture.require_player_script(loadedPlayer);
        const u32 sidekickHandle = fixture.require_player_script(loadedSidekick);
        CHECK(playerHandle != sidekickHandle);
        const scripting::script_set *set = fixture.runtime.store().find(playerHandle);
        REQUIRE(set != nullptr);
        CHECK(set->owner == loadedPlayer);

        // Save -> load -> save is byte-identical with scripts in the file.
        CHECK(save_text(fixture.active()) == text);

        // In PLAY mode the loaded entries get managed instances (one per script) at the next flush.
        fixture.runtime.set_mode(scripting::script_mode::PLAY);
        fixture.runtime.flush_pending();
        CHECK(fixture.null_host().live_slot_count() == 2);
        CHECK(fixture.runtime.stats().liveInstances == 2);
        CHECK(fixture.runtime.entry(fixture.runtime.scripts_of(loadedPlayer)[0])->is_live());
        fixture.runtime.set_mode(scripting::script_mode::EDIT);
        CHECK(fixture.null_host().live_slot_count() == 0);
    }

    TEST_CASE("scripting_scene - prefab instantiation, duplication and snapshots deep-copy the script set") {
        log_capture logs = {};
        scripting_scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity player = fixture.create_scripted("player");
        const ecs::entity weapon = fixture.create_scripted("weapon", player);
        const u32 playerHandle = fixture.require_player_script(player);
        const u32 weaponHandle = fixture.require_player_script(weapon);
        CHECK(weaponHandle != playerHandle);

        // A prefab document of the subtree carries both script sets as JSON.
        const core::result<scene::prefab_document> document = scene::prefab::build_document(target, player, "player");
        CONJURE_REQUIRE_OK(document);
        REQUIRE(document.value().archive != nullptr);
        CHECK(document.value().archive->entity_count() == 2);
        scene::prefab_asset asset = {};
        asset.archive = document.value().archive;
        asset.name = document.value().name;
        asset.id = assets::asset_id(0x5C41u, 7);

        // Instantiating produces fresh sets with the same class and fields, never aliasing the source.
        const core::result<ecs::entity> instance = scene::prefab::instantiate(target, asset);
        CONJURE_REQUIRE_OK(instance);
        const ecs::entity root = instance.value();
        CHECK(fixture.world.has<scene::prefab_instance>(root));
        REQUIRE(target.tree().children_of(root).size() == 1);
        const ecs::entity instanceChild = target.tree().children_of(root)[0];
        const u32 rootHandle = fixture.require_player_script(root);
        const u32 childHandle = fixture.require_player_script(instanceChild);
        CHECK(rootHandle != playerHandle);
        CHECK(childHandle != weaponHandle);
        CHECK(childHandle != rootHandle);
        CHECK(fixture.runtime.store().find(rootHandle)->owner == root);
        CHECK(fixture.runtime.store().find(childHandle)->owner == instanceChild);
        // Two live sets per subtree plus the two the prefab document's archive holds (ops.copy ran at
        // capture, so an archive never aliases a live set either).
        CHECK(fixture.runtime.store().size() == 6);

        // Changing the instance's script leaves the original alone.
        const std::vector<scripting::script_entry_id> rootScripts = fixture.runtime.scripts_of(root);
        const core::result<core::json::document> ninety = core::json::document::parse("90");
        REQUIRE(ninety.has_value());
        CONJURE_REQUIRE_OK(fixture.runtime.set_field(rootScripts[0], hash_of("hp"), ninety.value().root()));
        CHECK(fixture.runtime.entry(rootScripts[0])->fields.root()["hp"].as_i64() == 90);
        CHECK(fixture.runtime.entry(fixture.runtime.scripts_of(player)[0])->fields.root()["hp"].as_i64() == 4);

        // duplicate_subtree goes through ops.copy: another deep copy.
        const core::result<ecs::entity> duplicate = target.duplicate_subtree(player);
        CONJURE_REQUIRE_OK(duplicate);
        const u32 duplicateHandle = fixture.require_player_script(duplicate.value());
        CHECK(duplicateHandle != playerHandle);
        CHECK(fixture.runtime.store().size() == 8);     // the temporary archive of the copy is gone again

        // A snapshot restore (play / stop) rebuilds the sets from the captured JSON.
        const core::result<scene::scene_snapshot> snapshot = scene::capture(target);
        CONJURE_REQUIRE_OK(snapshot);
        const core::result<core::json::document> one = core::json::document::parse("1");
        REQUIRE(one.has_value());
        CONJURE_REQUIRE_OK(fixture.runtime.set_field(rootScripts[0], hash_of("hp"), one.value().root()));
        CHECK(fixture.runtime.store().size() == 14);    // six live, two in the prefab, six in the snapshot
        CONJURE_REQUIRE_OK(target.destroy_now(duplicate.value()));
        CHECK(fixture.runtime.store().size() == 12);
        CONJURE_REQUIRE_OK(scene::restore(target, snapshot.value()));
        CHECK(fixture.world.entity_count() == 6);
        CHECK(fixture.runtime.store().size() == 14);

        // Three roots are named "player" after the restore (the original, the prefab instance and the
        // duplicate), so a path lookup is ambiguous: the instance is the one root carrying
        // prefab_instance, and only its script holds the hp captured in the snapshot (90); the two
        // others still hold FIELDS_JSON.
        std::vector<ecs::entity> players = {};
        target.find_by_name(core::string_id("player"), players);
        REQUIRE(players.size() == 3);
        ecs::entity restoredRoot = ecs::NULL_ENTITY;
        for (const ecs::entity entity : players) {
            CAPTURE(entity.to_u64());
            const std::vector<scripting::script_entry_id> scripts = fixture.runtime.scripts_of(entity);
            REQUIRE(scripts.size() == 1);
            const scripting::script_set *restoredSet = fixture.runtime.store().find(scripts[0].setHandle);
            REQUIRE(restoredSet != nullptr);
            CHECK(restoredSet->owner == entity);
            if (fixture.world.has<scene::prefab_instance>(entity)) {
                CHECK(restoredRoot.is_null());
                restoredRoot = entity;
                CHECK(fixture.runtime.entry(scripts[0])->fields.root()["hp"].as_i64() == 90);
            } else {
                CHECK(fixture.runtime.entry(scripts[0])->fields.root()["hp"].as_i64() == 4);
            }
        }
        REQUIRE_FALSE(restoredRoot.is_null());
        CHECK(scene::prefab::instance_root_of(target, restoredRoot) == restoredRoot);
        REQUIRE(target.tree().children_of(restoredRoot).size() == 1);
        const ecs::entity restoredChild = target.tree().children_of(restoredRoot)[0];
        CHECK(scene::prefab::instance_root_of(target, restoredChild) == restoredRoot);
        const std::vector<scripting::script_entry_id> restoredRootScripts = fixture.runtime.scripts_of(restoredRoot);
        REQUIRE(restoredRootScripts.size() == 1);
        CHECK(fixture.require_player_script(restoredChild) != restoredRootScripts[0].setHandle);
        CHECK(scene::prefab::instance_root_of(target, player).is_null());
    }
}
