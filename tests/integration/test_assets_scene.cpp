#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "integration_test_support.h"

#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_types.h"
#include "conjure/assets/binary_asset.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/world.h"
#include "conjure/ecs/world_hash.h"
#include "conjure/scene/components.h"
#include "conjure/scene/register.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"
#include "conjure/scene/scene_serializer.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

// A .scene.json authored with an asset_ref field goes through the database (the scene importer is picked
// for the double extension, R45; the referenced uuid becomes a dependency), into the cache and through
// scene_manager::load, whose blocking load brings the dependency first and whose bind pass resolves the
// field through asset_manager::bind_slot (R25). Unloading the scene releases the asset again.

using namespace conjure;
using namespace conjure::testing;
using namespace integration_test;

namespace {

    /// <summary>
    /// A gameplay component holding a reference to a binary asset (the loot table of a chest).
    /// </summary>
    struct loot_table {
        assets::asset_ref<assets::binary_asset> table = {};
    };
}

CONJURE_REFLECT_STRUCT(loot_table, "test.loot_table", CONJURE_FIELD(table));
CONJURE_COMPONENT(loot_table, "test.loot_table");

namespace {

    constexpr std::string_view LOOT_BYTES = "gold:10;potion:2";

    void register_types(ecs::component_registry &registry) {
        CONJURE_REQUIRE_OK(scene::register_components(registry));
        CONJURE_REQUIRE_OK(registry.register_component<loot_table>());
    }
}

TEST_SUITE("integration/assets_scene") {

    TEST_CASE("assets_scene - a scene imported through the database binds its asset_ref fields on load") {
        log_capture logs = {};
        ecs::component_registry registry = {};
        register_types(registry);

        asset_pipeline pipeline = {};
        CONJURE_REQUIRE_OK(assets::register_binary_asset_type(pipeline.manager->types(), &pipeline.importers));
        CONJURE_REQUIRE_OK(scene::register_asset_types(pipeline.manager->types(), pipeline.importers, registry));
        pipeline.open();

        // The referenced asset first: its uuid is minted by the scan.
        pipeline.write_source("data/loot.bin", LOOT_BYTES);
        pipeline.scan_and_import();
        const assets::asset_id lootId = pipeline.id_of("data/loot.bin");

        // Author the scene in a scratch world and save it into the assets mount.
        {
            ecs::world authoring(registry);
            scene::scene draft(authoring, nullptr);
            draft.set_name("vault");
            const core::result<ecs::entity> chest = draft.create_entity(core::string_id("chest"));
            CONJURE_REQUIRE_OK(chest);
            const loot_table filled{assets::asset_ref<assets::binary_asset>(lootId)};
            CONJURE_REQUIRE_OK(authoring.add<loot_table>(chest.value(), filled));
            const core::result<ecs::entity> crate = draft.create_entity(core::string_id("crate"), chest.value());
            CONJURE_REQUIRE_OK(crate);
            CONJURE_REQUIRE_OK(authoring.add<loot_table>(crate.value()));
            CONJURE_REQUIRE_OK(
              scene::scene_serializer::save_scene_file(draft, *pipeline.fileSystem, "assets/scenes/vault.scene.json")
            );
            const std::string text = pipeline.fileSystem->read_text_file("assets/scenes/vault.scene.json").value();
            CHECK(text.find("\"test.loot_table\":{\"table\":\"" + lootId.to_string() + "\"}") != std::string::npos);
            CHECK(text.find("\"test.loot_table\":{\"table\":null}") != std::string::npos);
            draft.clear();
        }

        // The database picks the scene importer for the double extension and records the dependency.
        pipeline.scan_and_import();
        const assets::asset_id sceneId = pipeline.id_of("scenes/vault.scene.json");
        const std::optional<assets::asset_record> record = pipeline.database->find(sceneId);
        REQUIRE(record.has_value());
        CHECK(record->importerId == scene::SCENE_IMPORTER_ID);
        CHECK(record->status == assets::asset_record_status::OK);
        CHECK(record->type == assets::asset_type_traits<scene::scene_asset>::id());
        const std::vector<assets::asset_id> dependencies = pipeline.database->dependencies_of(sceneId);
        CHECK(std::find(dependencies.begin(), dependencies.end(), lootId) != dependencies.end());
        const std::optional<assets::asset_record> lootRecord = pipeline.database->find(lootId);
        REQUIRE(lootRecord.has_value());
        CHECK(lootRecord->importerId == assets::binary_importer::ID);

        // The runtime side: scene_manager::load brings the loot table first, then binds the field.
        ecs::world world(registry);
        scene::scene_manager manager(world, *pipeline.manager);
        const core::result<scene::scene_id> loaded = manager.load(sceneId, scene::load_mode::REPLACE);
        CONJURE_REQUIRE_OK(loaded);
        CHECK(manager.active().name() == "vault");
        CHECK(world.entity_count() == 2);
        const ecs::entity chest = manager.active().find_path("chest");
        const ecs::entity crate = manager.active().find_path("chest/crate");
        REQUIRE_FALSE(chest.is_null());
        REQUIRE_FALSE(crate.is_null());

        const loot_table *bound = world.get<loot_table>(chest);
        REQUIRE(bound != nullptr);
        CHECK(bound->table.id() == lootId);
        CHECK(bound->table.is_bound());
        REQUIRE(bound->table.slot() != nullptr);
        CHECK(bound->table.slot() == pipeline.manager->bind_slot(lootId));
        CHECK(bound->table.state() == assets::asset_state::LOADED);
        const assets::binary_asset *loot = bound->table.get();
        REQUIRE(loot != nullptr);
        CHECK(std::string(reinterpret_cast<const char *>(loot->bytes.data()), loot->bytes.size()) == LOOT_BYTES);

        // A null reference stays unbound: nothing to resolve.
        const loot_table *empty = world.get<loot_table>(crate);
        REQUIRE(empty != nullptr);
        CHECK(empty->table.id().is_null());
        CHECK_FALSE(empty->table.is_bound());
        CHECK(empty->table.get() == nullptr);

        // The record keeps the handles: the loot table stays resident until the scene is unloaded, and
        // collect() releases it then.
        const std::optional<scene::loaded_scene_info> info = manager.info(loaded.value());
        REQUIRE(info.has_value());
        CHECK(info->asset == sceneId);
        REQUIRE(info->roots.size() == 1);
        CHECK(info->roots[0] == chest);
        CHECK(pipeline.manager->bind_slot(lootId)->state.load() == assets::asset_state::LOADED);
        CONJURE_REQUIRE_OK(manager.unload(loaded.value()));
        CHECK(world.entity_count() == 0);
        CHECK(pipeline.manager->bind_slot(lootId)->state.load() == assets::asset_state::UNLOADED);
        CHECK(pipeline.manager->bind_slot(sceneId)->state.load() == assets::asset_state::UNLOADED);
        CONJURE_CHECK_OK(manager.active().validate());
        CONJURE_CHECK_OK(world.validate());
    }

    TEST_CASE("regression - demo-review P1 a scene-bound asset_ref hashes like the unbound reference") {
        log_capture logs = {};
        ecs::component_registry registry = {};
        register_types(registry);

        asset_pipeline pipeline = {};
        CONJURE_REQUIRE_OK(assets::register_binary_asset_type(pipeline.manager->types(), &pipeline.importers));
        CONJURE_REQUIRE_OK(scene::register_asset_types(pipeline.manager->types(), pipeline.importers, registry));
        pipeline.open();
        pipeline.write_source("data/loot.bin", LOOT_BYTES);
        pipeline.write_source("data/other.bin", "arrow:30");
        pipeline.scan_and_import();
        const assets::asset_id lootId = pipeline.id_of("data/loot.bin");
        const assets::asset_id otherId = pipeline.id_of("data/other.bin");
        REQUIRE(lootId != otherId);

        // The scene on disk carries the uuid alone.
        {
            ecs::world authoring(registry);
            scene::scene draft(authoring, nullptr);
            draft.set_name("vault");
            const core::result<ecs::entity> chest = draft.create_entity(core::string_id("chest"));
            CONJURE_REQUIRE_OK(chest);
            const loot_table filled{assets::asset_ref<assets::binary_asset>(lootId)};
            CONJURE_REQUIRE_OK(authoring.add<loot_table>(chest.value(), filled));
            CONJURE_REQUIRE_OK(
              scene::scene_serializer::save_scene_file(draft, *pipeline.fileSystem, "assets/scenes/vault.scene.json")
            );
            draft.clear();
        }
        pipeline.scan_and_import();
        const assets::asset_id sceneId = pipeline.id_of("scenes/vault.scene.json");

        // scene_manager::load binds the field to the live slot of this process (R25): the reference now
        // holds a heap address that no other process and no unbound copy of the world shares.
        ecs::world world(registry);
        scene::scene_manager manager(world, *pipeline.manager);
        CONJURE_REQUIRE_OK(manager.load(sceneId, scene::load_mode::REPLACE));
        const ecs::entity chest = manager.active().find_path("chest");
        REQUIRE_FALSE(chest.is_null());
        const loot_table *loaded = world.get<loot_table>(chest);
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->table.is_bound());
        REQUIRE(loaded->table.slot() == pipeline.manager->bind_slot(lootId));
        CHECK(loaded->table.state() == assets::asset_state::LOADED);
        const u64 sceneBound = ecs::world_hash(world);

        // The same uuid, unbound: what a process that has not run its bind pass yet holds.
        CONJURE_REQUIRE_OK(world.set(chest, loot_table{assets::asset_ref<assets::binary_asset>(lootId)}));
        REQUIRE_FALSE(world.get<loot_table>(chest)->table.is_bound());
        const u64 unbound = ecs::world_hash(world);
        CHECK(unbound == sceneBound);

        // Bound again, this time by asset_manager::bind (what gameplay code and scripts do).
        const assets::asset_ref<assets::binary_asset> rebound = pipeline.manager->bind<assets::binary_asset>(lootId);
        REQUIRE(rebound.is_bound());
        CONJURE_REQUIRE_OK(world.set(chest, loot_table{rebound}));
        CHECK(ecs::world_hash(world) == unbound);

        // The identity still counts: another asset changes the hash, bound or not.
        const assets::asset_ref<assets::binary_asset> other = pipeline.manager->bind<assets::binary_asset>(otherId);
        REQUIRE(other.is_bound());
        CONJURE_REQUIRE_OK(world.set(chest, loot_table{other}));
        const u64 otherBound = ecs::world_hash(world);
        CHECK(otherBound != unbound);
        CONJURE_REQUIRE_OK(world.set(chest, loot_table{assets::asset_ref<assets::binary_asset>(otherId)}));
        CHECK(ecs::world_hash(world) == otherBound);
    }
}
