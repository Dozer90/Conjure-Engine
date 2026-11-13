#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "bindings_test_support.h"

#include "conjure/bindings/bindings_error.h"
#include "conjure/bindings/marshal.h"
#include "conjure/scene/components.h"
#include "conjure/scene/prefab.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"

#include <array>
#include <string>

// scene_* and prefab_* (docs/design/15_bindings.md 10): deferred scene loads, additive loads, the path and
// root protocols, and prefab instantiation of LOADED assets with its failure paths.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::testing;
using namespace bindings_test;

namespace {

    constexpr std::string_view SCENE_A = "assets://scenes/a.scene.json";
    constexpr std::string_view SCENE_B = "assets://scenes/b.scene.json";
    constexpr std::string_view SCENE_C = "assets://scenes/c.scene.json";

    struct scene_fixture_pair {
        bare_fixture base = {};
        assets::asset_id sceneA = {};
        assets::asset_id sceneB = {};

        scene_fixture_pair() {
            sceneA = base.publish_scene("alpha", 2, 10, SCENE_A);
            sceneB = base.publish_scene("beta", 3, 20, SCENE_B);
        }
    };

    struct prefab_fixture {
        bare_fixture base = {};
        assets::asset_id prefab = {};
        assets::asset_id unloaded = {};

        prefab_fixture() {
            const ecs::entity root = base.create("lamp");
            const ecs::entity bulb = base.create("bulb", root);
            CONJURE_REQUIRE_OK(base.active().set_local_transform(bulb, scene::local_transform::from_transform(
              math::transform::from_position(math::vec3(0.0f, 2.0f, 0.0f))
            )));
            prefab = base.publish_prefab(root, "lamp", 1, "assets://prefabs/lamp.prefab.json");
            unloaded = base.publish_prefab(root, "spare", 2);
            base.load_now(prefab);
            CONJURE_REQUIRE_OK(base.active().destroy_now(root));
        }
    };
}

TEST_SUITE("bindings/prefab_scene_bindings") {

    TEST_CASE("scene - request_load applies at FRAME_BEGIN, not immediately") {
        scene_fixture_pair fixture = {};
        conjure_native_api &api = fixture.base.api();
        const utf8_argument path = utf8(SCENE_A);
        const u32 before = fixture.base.world.entity_count();
        CHECK(api.scene_is_loaded(path.data, path.length) == 0);
        CHECK(api.scene_request_load(path.data, path.length, 0) == 0);
        CHECK(api.scene_is_load_pending(path.data, path.length) == 1);
        CHECK(api.scene_is_loaded(path.data, path.length) == 0);
        CHECK(fixture.base.world.entity_count() == before);
        fixture.base.assets.flush();
        CHECK(fixture.base.world.entity_count() == before);          // resident, still waiting for FRAME_BEGIN
        fixture.base.step();
        CHECK(api.scene_is_load_pending(path.data, path.length) == 0);
        CHECK(api.scene_is_loaded(path.data, path.length) == 1);
        CHECK(api.scene_loaded_count() == 1);
        CHECK(api.scene_entity_count() == static_cast<int32_t>(before + 2));
        CHECK(api.scene_structure_version() > 1);
    }

    TEST_CASE("scene - additive load keeps the existing entities") {
        scene_fixture_pair fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity authored = fixture.base.create("authored");
        const utf8_argument pathA = utf8(SCENE_A);
        const utf8_argument pathB = utf8(SCENE_B);
        REQUIRE(api.scene_request_load(pathA.data, pathA.length, 1) == 0);
        fixture.base.assets.flush();
        fixture.base.step();
        REQUIRE(api.scene_request_load(pathB.data, pathB.length, 1) == 0);
        fixture.base.assets.flush();
        fixture.base.step();
        CHECK(api.scene_loaded_count() == 2);
        CHECK(fixture.base.world.is_alive(authored));
        CHECK(api.scene_entity_count() == 1 + 2 + 3);
        // A REPLACE load retires everything.
        const conjure_uuid asset = uuid_of(fixture.sceneA);
        REQUIRE(api.scene_request_load_asset(&asset, 0) == 0);
        fixture.base.assets.flush();
        fixture.base.step();
        CHECK(api.scene_loaded_count() == 1);
        CHECK_FALSE(fixture.base.world.is_alive(authored));
        CHECK(api.scene_entity_count() == 2);
        CHECK(api.scene_request_load_asset(nullptr, 0) == to_status(bindings_error::INVALID_ARGUMENT));
        const conjure_uuid null = {};
        CHECK(api.scene_request_load_asset(&null, 0) == to_status(bindings_error::INVALID_ARGUMENT));
    }

    TEST_CASE("scene - is_load_pending until the batch completes") {
        scene_fixture_pair fixture = {};
        conjure_native_api &api = fixture.base.api();
        const utf8_argument path = utf8(SCENE_B);
        CHECK(api.scene_is_load_pending(path.data, path.length) == 0);
        REQUIRE(api.scene_request_load(path.data, path.length, 1) == 0);
        CHECK(api.scene_is_load_pending(path.data, path.length) == 1);
        fixture.base.step();                                          // the asset is not resident yet: still pending
        CHECK(api.scene_is_load_pending(path.data, path.length) == 1);
        fixture.base.assets.flush();
        fixture.base.step();
        CHECK(api.scene_is_load_pending(path.data, path.length) == 0);
        CHECK(api.scene_is_loaded(path.data, path.length) == 1);
        const utf8_argument unknown = utf8("assets://scenes/missing.scene.json");
        CHECK(api.scene_is_load_pending(unknown.data, unknown.length) == 0);
        CHECK(api.scene_is_load_pending(nullptr, 3) == to_status(bindings_error::INVALID_ARGUMENT));
    }

    TEST_CASE("scene - an unknown path is SCENE_NOT_FOUND and unloading an unloaded scene is a no-op") {
        scene_fixture_pair fixture = {};
        conjure_native_api &api = fixture.base.api();
        const utf8_argument unknown = utf8("assets://scenes/missing.scene.json");
        CHECK(api.scene_request_load(unknown.data, unknown.length, 0) == to_status(bindings_error::SCENE_NOT_FOUND));
        CHECK(last_error_message() == "scene_request_load: assets://scenes/missing.scene.json");
        CHECK(api.scene_request_unload(unknown.data, unknown.length) == to_status(bindings_error::SCENE_NOT_FOUND));
        CHECK(api.scene_is_loaded(unknown.data, unknown.length) == 0);
        const utf8_argument known = utf8(SCENE_A);
        CHECK(api.scene_request_unload(known.data, known.length) == 0);      // known asset, not loaded: no-op
        CHECK(api.scene_request_load(nullptr, 5, 0) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.scene_is_loaded(nullptr, 5) == to_status(bindings_error::INVALID_ARGUMENT));

        REQUIRE(api.scene_request_load(known.data, known.length, 0) == 0);
        fixture.base.assets.flush();
        fixture.base.step();
        REQUIRE(api.scene_is_loaded(known.data, known.length) == 1);
        CHECK(api.scene_request_unload(known.data, known.length) == 0);
        CHECK(api.scene_is_loaded(known.data, known.length) == 1);           // applied at FRAME_BEGIN
        fixture.base.step();
        CHECK(api.scene_is_loaded(known.data, known.length) == 0);
        CHECK(api.scene_loaded_count() == 0);
    }

    TEST_CASE("scene - loaded paths and roots follow the string and count protocols") {
        scene_fixture_pair fixture = {};
        conjure_native_api &api = fixture.base.api();
        const utf8_argument pathA = utf8(SCENE_A);
        const utf8_argument pathB = utf8(SCENE_B);
        REQUIRE(api.scene_request_load(pathA.data, pathA.length, 1) == 0);
        REQUIRE(api.scene_request_load(pathB.data, pathB.length, 1) == 0);
        fixture.base.assets.flush();
        fixture.base.step();
        REQUIRE(api.scene_loaded_count() == 2);
        // Without an asset database the uuid is the only name an asset-loaded scene has (cooked projects).
        const std::string first = read_string([&](char *buffer, int32_t capacity) {
            return api.scene_loaded_path(0, buffer, capacity);
        });
        CHECK(first == fixture.sceneA.to_string());
        const std::string active = read_string([&](char *buffer, int32_t capacity) {
            return api.scene_active_path(buffer, capacity);
        });
        CHECK(active == fixture.sceneA.to_string());                      // the first loaded scene
        CHECK(api.scene_loaded_path(0, nullptr, 0) == static_cast<int32_t>(first.size()));
        CHECK(api.scene_loaded_path(2, nullptr, 0) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.scene_loaded_path(-1, nullptr, 0) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.scene_loaded_path(0, nullptr, -1) == to_status(bindings_error::INVALID_ARGUMENT));
        std::array<uint64_t, 2> some = {};
        CHECK(api.scene_roots(some.data(), 2) == 5);
        CHECK(api.scene_roots(nullptr, 0) == 5);
        CHECK(some[0] == fixture.base.active().roots()[0].entity.to_u64());
        CHECK(api.scene_roots(some.data(), -1) == to_status(bindings_error::INVALID_ARGUMENT));
    }

    TEST_CASE("regression - sweep-2026-11-12 F438: active_path is the first loaded scene, not the latest additive") {
        scene_fixture_pair fixture = {};
        conjure_native_api &api = fixture.base.api();
        const assets::asset_id sceneC = fixture.base.publish_scene("gamma", 1, 30, SCENE_C);
        const utf8_argument pathA = utf8(SCENE_A);
        const utf8_argument pathB = utf8(SCENE_B);
        const utf8_argument pathC = utf8(SCENE_C);
        const auto activePath = [&]() {
            return read_string([&](char *buffer, int32_t capacity) {
                return api.scene_active_path(buffer, capacity);
            });
        };
        const auto apply = [&]() {
            fixture.base.assets.flush();
            fixture.base.step();
        };

        REQUIRE(api.scene_request_load(pathB.data, pathB.length, 0) == 0);
        apply();
        CHECK(activePath() == fixture.sceneB.to_string());
        // Additive loads never change the active scene.
        REQUIRE(api.scene_request_load(pathA.data, pathA.length, 1) == 0);
        apply();
        REQUIRE(api.scene_loaded_count() == 2);
        CHECK(activePath() == fixture.sceneB.to_string());
        REQUIRE(api.scene_request_load(pathC.data, pathC.length, 1) == 0);
        apply();
        REQUIRE(api.scene_loaded_count() == 3);
        CHECK(activePath() == fixture.sceneB.to_string());
        // Unloading the active scene hands the role to the oldest remaining one (A, loaded before C).
        REQUIRE(api.scene_request_unload(pathB.data, pathB.length) == 0);
        apply();
        REQUIRE(api.scene_loaded_count() == 2);
        CHECK(activePath() == fixture.sceneA.to_string());
        const std::string second = read_string([&](char *buffer, int32_t capacity) {
            return api.scene_loaded_path(1, buffer, capacity);
        });
        CHECK(second == sceneC.to_string());
        // A REPLACE load unloads everything before it and becomes the active scene.
        REQUIRE(api.scene_request_load(pathB.data, pathB.length, 0) == 0);
        apply();
        REQUIRE(api.scene_loaded_count() == 1);
        CHECK(activePath() == fixture.sceneB.to_string());
    }

    TEST_CASE("scene - active_path is empty without a loaded scene") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        std::array<char, 16> buffer = {};
        CHECK(api.scene_active_path(buffer.data(), 16) == 0);
        CHECK(api.scene_active_path(buffer.data(), -1) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.scene_loaded_count() == 0);
        CHECK(api.scene_entity_count() == 0);
        CHECK(api.scene_structure_version() >= 1);
    }

    TEST_CASE("prefab - instantiate requires a LOADED prefab") {
        prefab_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const conjure_uuid spare = uuid_of(fixture.unloaded);
        uint64_t root = 0;
        CHECK(api.prefab_instantiate(&spare, 0, nullptr, &root) == to_status(bindings_error::ASSET_NOT_LOADED));
        CHECK(root == 0);
        CHECK(api.prefab_instantiate(nullptr, 0, nullptr, &root) == to_status(bindings_error::INVALID_ARGUMENT));
        const conjure_uuid loaded = uuid_of(fixture.prefab);
        CHECK(api.prefab_instantiate(&loaded, 0, nullptr, nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        const conjure_uuid null = {};
        CHECK(api.prefab_instantiate(&null, 0, nullptr, &root) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.prefab_instantiate(&loaded, 999, nullptr, &root) == to_status(bindings_error::ENTITY_NOT_ALIVE));
        // A uuid no source knows is refused without binding a slot for it (05: slots are never freed).
        const u32 slotsBefore = fixture.base.assets.stats().slots;
        const conjure_uuid unknown = uuid_of(test_uuid(4242));
        CHECK(api.prefab_instantiate(&unknown, 0, nullptr, &root) == to_status(bindings_error::ASSET_NOT_LOADED));
        std::array<uint64_t, 2> roots = {};
        CHECK(api.prefab_instantiate_many(&unknown, 0, 2, roots.data()) == to_status(bindings_error::ASSET_NOT_LOADED));
        CHECK(fixture.base.assets.stats().slots == slotsBefore);
    }

    TEST_CASE("prefab - instantiate with a local transform and a parent") {
        prefab_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity holder = fixture.base.create("holder");
        const conjure_uuid prefab = uuid_of(fixture.prefab);
        conjure_transform local = {};
        local.position = {5.0f, 0.0f, 0.0f};
        local.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        local.scale = {1.0f, 1.0f, 1.0f};
        uint64_t root = 0;
        CHECK(api.prefab_instantiate(&prefab, holder.to_u64(), &local, &root) == 0);
        REQUIRE(root != 0);
        const ecs::entity instance = marshal::to_entity(root);
        CHECK(fixture.base.active().tree().parent_of(instance) == holder);
        REQUIRE(fixture.base.active().tree().children_of(instance).size() == 1);
        const ecs::entity bulb = fixture.base.active().tree().children_of(instance)[0];
        conjure_transform read = {};
        CHECK(api.transform_get_local(root, &read) == 0);
        CHECK(read.position.x == 5.0f);
        CHECK(api.transform_get_world(bulb.to_u64(), &read) == 0);
        CHECK(read.position.x == 5.0f);
        CHECK(read.position.y == 2.0f);
        CHECK(api.prefab_is_instance_root(root) == 1);
        CHECK(api.prefab_is_instance_root(bulb.to_u64()) == 0);
        CONJURE_CHECK_OK(fixture.base.active().validate());
    }

    TEST_CASE("prefab - instantiate_many yields distinct roots") {
        prefab_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity holder = fixture.base.create("holder");
        const conjure_uuid prefab = uuid_of(fixture.prefab);
        std::array<uint64_t, 5> roots = {};
        CHECK(api.prefab_instantiate_many(&prefab, holder.to_u64(), 5, roots.data()) == 5);
        for (usize index = 0; index < roots.size(); ++index) {
            CAPTURE(index);
            REQUIRE(roots[index] != 0);
            for (usize other = index + 1; other < roots.size(); ++other) {
                CHECK(roots[index] != roots[other]);
            }
            CHECK(fixture.base.active().tree().parent_of(marshal::to_entity(roots[index])) == holder);
            CHECK(api.prefab_is_instance_root(roots[index]) == 1);
        }
        CHECK(fixture.base.active().tree().children_of(holder).size() == 5);
        CHECK(api.prefab_instantiate_many(&prefab, 0, 0, roots.data()) == 0);
        CHECK(api.prefab_instantiate_many(&prefab, 0, -1, roots.data()) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.prefab_instantiate_many(&prefab, 0, 2, nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        const conjure_uuid spare = uuid_of(fixture.unloaded);
        CHECK(api.prefab_instantiate_many(&spare, 0, 2, roots.data()) == to_status(bindings_error::ASSET_NOT_LOADED));
    }

    TEST_CASE("prefab - source_of and is_instance_root") {
        prefab_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const conjure_uuid prefab = uuid_of(fixture.prefab);
        uint64_t root = 0;
        REQUIRE(api.prefab_instantiate(&prefab, 0, nullptr, &root) == 0);
        conjure_uuid source = {};
        CHECK(api.prefab_source_of(root, &source) == 0);
        CHECK(marshal::to_uuid(source) == fixture.prefab);
        const ecs::entity plain = fixture.base.create("plain");
        CHECK(api.prefab_source_of(plain.to_u64(), &source) == to_status(bindings_error::COMPONENT_MISSING));
        CHECK(last_error_message() == "prefab_source_of: prefab_instance");
        CHECK(api.prefab_is_instance_root(plain.to_u64()) == 0);
        CHECK(api.prefab_source_of(root, nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.prefab_source_of(999, &source) == to_status(bindings_error::ENTITY_NOT_ALIVE));
        CHECK(api.prefab_is_instance_root(999) == to_status(bindings_error::ENTITY_NOT_ALIVE));
    }

    TEST_CASE("prefab - a loaded asset of another type is ASSET_TYPE_MISMATCH") {
        prefab_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const assets::asset_id scene = fixture.base.publish_scene("notaprefab", 1, 30, "assets://scenes/n.scene.json");
        fixture.base.load_now(scene);
        const conjure_uuid wrong = uuid_of(scene);
        uint64_t root = 0;
        CHECK(api.prefab_instantiate(&wrong, 0, nullptr, &root) == to_status(bindings_error::ASSET_TYPE_MISMATCH));
        CHECK(root == 0);
    }

    TEST_CASE("prefab - a payload that failed to load is ASSET_NOT_LOADED") {
        log_capture logs = {};
        logs.allow_errors();
        prefab_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const assets::asset_id broken = test_uuid(40);
        const std::string garbage = "not json at all";
        fixture.base.add_blob(
          broken,
          assets::asset_type_traits<scene::prefab_asset>::id(),
          std::as_bytes(std::span<const char>(garbage.data(), garbage.size()))
        );
        const core::result<assets::asset_handle> loaded = fixture.base.assets.load_blocking(broken);
        CHECK_FALSE(loaded.has_value());
        CHECK(fixture.base.assets.state_of(broken) == assets::asset_state::FAILED);
        const conjure_uuid id = uuid_of(broken);
        uint64_t root = 0;
        CHECK(api.prefab_instantiate(&id, 0, nullptr, &root) == to_status(bindings_error::ASSET_NOT_LOADED));
        CHECK(root == 0);
    }
}
