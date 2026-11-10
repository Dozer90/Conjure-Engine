#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"
#include "scene_test_fixture.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/vfs/memory_file_system.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_serializer.h"

#include <random>
#include <string>
#include <vector>

// Round-trip topics of docs/design/13_scene.md 10: save -> load -> save byte-identical for a programmatic
// 500-entity scene, a hand-authored fixture, an additive load into a populated scene and the empty scene.

using namespace conjure;
using namespace scene_test;

namespace {

    /// <summary>
    /// 500 entities with every scene component in use, a nested hierarchy, asset references, tags,
    /// layers, disabled entities and an unknown component spliced in through a file.
    /// </summary>
    void build_rich_scene(scene_fixture &fixture, std::mt19937_64 &engine) {
        scene::scene &target = fixture.active();
        target.set_name("rich");
        core::json::document settings = core::json::document::object();
        settings.set("gravity", -9.81);
        settings.set("skybox", test_uuid(3).to_string());
        target.set_settings(settings);
        std::uniform_real_distribution<f32> unit(-10.0f, 10.0f);
        std::uniform_int_distribution<u32> fan(0, 3);
        std::vector<ecs::entity> entities = {};
        u32 created = 0;
        while (created < 500) {
            ecs::entity parent = ecs::NULL_ENTITY;
            if (!entities.empty() && fan(engine) != 0) {
                std::uniform_int_distribution<usize> pick(0, entities.size() - 1);
                const ecs::entity candidate = entities[pick(engine)];
                if (target.tree().depth_of(candidate) < 6) {
                    parent = candidate;
                }
            }
            const core::string_id entityName("e" + std::to_string(created));
            const core::result<ecs::entity> entity = target.create_entity(entityName, parent);
            REQUIRE(entity.has_value());
            scene::local_transform local = {};
            local.position = math::vec3(unit(engine), unit(engine), unit(engine));
            local.rotation = glm::normalize(math::quat(unit(engine), unit(engine), unit(engine), unit(engine)));
            local.scale = math::vec3(1.0f + static_cast<f32>(created % 3));
            REQUIRE(target.set_local_transform(entity.value(), local).has_value());
            if (created % 5 == 0) {
                const core::string_id tagName("tag" + std::to_string(created % 7));
                REQUIRE(target.set_tag(entity.value(), tagName).has_value());
            }
            if (created % 4 == 0) {
                REQUIRE(target.set_layer(entity.value(), static_cast<u8>(created % 32)).has_value());
            }
            if (created % 11 == 0) {
                REQUIRE(target.set_enabled(entity.value(), false).has_value());
            }
            if (created % 6 == 0) {
                test_asset_holder holder = {};
                holder.asset = assets::asset_ref<assets::binary_asset>(test_uuid(100 + created % 4));
                REQUIRE(fixture.world.add<test_asset_holder>(entity.value(), holder).has_value());
            }
            if (created % 9 == 0 && !entities.empty()) {
                const test_entity_holder holder{entities.front(), created};
                REQUIRE(fixture.world.add<test_entity_holder>(entity.value(), holder).has_value());
            }
            if (created % 13 == 0) {
                const test_speed speed{static_cast<f32>(created)};
                REQUIRE(fixture.world.add<test_speed>(entity.value(), speed).has_value());
            }
            if (created % 17 == 0) {
                const scene::prefab_link link{test_uuid(500 + created)};
                REQUIRE(fixture.world.add<scene::prefab_link>(entity.value(), link).has_value());
            }
            if (created % 23 == 0) {
                REQUIRE(fixture.world.add<scene::prefab_instance>(entity.value(),
                  scene::prefab_instance{assets::asset_ref<scene::prefab_asset>(test_uuid(700))}).has_value());
            }
            entities.push_back(entity.value());
            ++created;
        }
        REQUIRE(fixture.world.entity_count() == 500);
    }

    [[nodiscard]] std::string save_text(const scene::scene &source) {
        core::json::writer out = {};
        REQUIRE(scene::scene_serializer::save_scene(source, out).has_value());
        return out.finish();
    }

    [[nodiscard]] core::result<scene::instantiate_report> load_text(scene_fixture &fixture, const std::string &text) {
        const core::result<core::json::document> parsed = core::json::document::parse(text);
        REQUIRE(parsed.has_value());
        CONJURE_TRY_ASSIGN(
          const scene::scene_document document,
          scene::scene_serializer::parse_document(parsed.value().root(), fixture.registry)
        );
        // What a REPLACE load does before instantiating: the file's name and settings become the scene's.
        fixture.active().set_name(document.name);
        fixture.active().set_settings(document.settings);
        return scene::scene_serializer::load_scene(fixture.active(), document, scene::SCENE_ID_AUTHORED,
          scene::guid_policy::KEEP);
    }

    /// <summary>
    /// Splices an unknown component into the first entity of a saved file.
    /// </summary>
    [[nodiscard]] std::string with_unknown_component(const std::string &text) {
        const std::string marker = "\"entities\":[{\"components\":{";
        const usize position = text.find(marker);
        REQUIRE(position != std::string::npos);
        return text.substr(0, position + marker.size()) + "\"com.studio.spinner\":{\"axis\":[0,1,0],\"speed\":2.5},"
          + text.substr(position + marker.size());
    }

    /// <summary>
    /// A small hand-authored scene in the exact canonical form the writer produces.
    /// </summary>
    [[nodiscard]] std::string fixture_text() {
        return std::string("{\"format\":\"conjure.scene\",\"version\":1,\"name\":\"roundtrip\",")
          + "\"settings\":{\"ambient\":[0.1,0.1,0.12],\"gravity\":[0,-9.81,0]},"
          + "\"roots\":[\"6d2a9f0e-3b7c-4d1e-9a2f-0c5b7e8d1a21\",\"b1f0c3d2-aa10-4e22-8c71-2fd3e4a5b6c7\"],"
          + "\"archive\":{\"entities\":["
          + "{\"components\":{\"conjure.scene.local_transform\":"
          + "{\"position\":[0.0,1.0,0.0],\"rotation\":[0.0,0.0,0.0,1.0],\"scale\":[1.0,1.0,1.0]},"
          + "\"conjure.scene.name\":{\"value\":\"Player\"},\"conjure.scene.tag\":{\"value\":\"Player\"},"
          + "\"conjure.scene.layer\":{\"index\":3},"
          + "\"conjure.scene.guid\":{\"value\":\"6d2a9f0e-3b7c-4d1e-9a2f-0c5b7e8d1a21\"},"
          + "\"test.speed\":{\"value\":2.5}}},"
          + "{\"components\":{\"conjure.scene.parent\":{\"value\":0,\"siblingIndex\":0},"
          + "\"conjure.scene.local_transform\":"
          + "{\"position\":[0.0,2.0,0.0],\"rotation\":[0.0,0.0,0.0,1.0],\"scale\":[1.0,1.0,1.0]},"
          + "\"conjure.scene.name\":{\"value\":\"Lamp\"},"
          + "\"conjure.scene.guid\":{\"value\":\"0e8d7c6b-5a49-4382-9170-6f5e4d3c2b1a\"}}},"
          + "{\"components\":{\"conjure.scene.local_transform\":"
          + "{\"position\":[0.0,0.0,0.0],\"rotation\":[0.0,0.0,0.0,1.0],\"scale\":[50.0,1.0,50.0]},"
          + "\"conjure.scene.name\":{\"value\":\"Ground\"},"
          + "\"conjure.scene.guid\":{\"value\":\"b1f0c3d2-aa10-4e22-8c71-2fd3e4a5b6c7\"},"
          + "\"conjure.scene.disabled\":{}}}]}}";
    }
}

TEST_SUITE("scene/scene_serialisation") {

    TEST_CASE("serialisation - save load save byte identical") {
        scene_fixture first = {};
        std::mt19937_64 engine(testing::environment().seed);
        build_rich_scene(first, engine);
        const std::string original = save_text(first.active());
        const std::string spliced = with_unknown_component(original);

        scene_fixture second = {};
        {
            testing::log_capture logs = {};
            const core::result<scene::instantiate_report> report = load_text(second, spliced);
            CONJURE_REQUIRE_OK(report);
            CHECK(report.value().entityCount == 500);
            CHECK(report.value().unknownComponents == 1);
            CHECK(logs.count(core::log_level::WARN) == 1);
        }
        CHECK(second.world.entity_count() == 500);
        CHECK(second.active().guids().size() == 500);
        CHECK(second.active().name() == "rich");
        CHECK(second.active().roots().size() == first.active().roots().size());
        const std::string resaved = save_text(second.active());
        CHECK(resaved.size() == spliced.size());
        CHECK(resaved == spliced);

        // And once more through a third scene: the fixed point holds.
        scene_fixture third = {};
        CONJURE_REQUIRE_OK(load_text(third, resaved));
        CHECK(save_text(third.active()) == resaved);
        second.step();
        third.step();
    }

    TEST_CASE("serialisation - roundtrip.scene.json fixture byte identical") {
        scene_fixture fixture = {};
        const std::string text = fixture_text();
        CONJURE_REQUIRE_OK(load_text(fixture, text));
        scene::scene &target = fixture.active();
        REQUIRE(target.roots().size() == 2);
        const ecs::entity player = target.find_path("Player");
        const ecs::entity lamp = target.find_path("Player/Lamp");
        const ecs::entity ground = target.find_path("Ground");
        REQUIRE(target.contains(player));
        REQUIRE(target.contains(lamp));
        REQUIRE(target.contains(ground));
        CHECK(target.roots()[0].entity == player);
        CHECK(target.roots()[1].entity == ground);
        CHECK(target.layer_of(player) == 3);
        CHECK(target.tag_of(player).view() == "Player");
        CHECK(target.tree().depth_of(lamp) == 1);
        CHECK_FALSE(target.is_enabled(ground));
        CHECK(target.local_transform_of(ground).value().scale == math::vec3(50.0f, 1.0f, 50.0f));
        CHECK(fixture.world.get<test_speed>(player)->value == doctest::Approx(2.5f));
        CHECK(target.settings().root()["gravity"][1].as_f64() == doctest::Approx(-9.81));
        CHECK(save_text(target) == text);
        fixture.step();
    }

    TEST_CASE("serialisation - load into additive scene keeps existing entities") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity keep = fixture.create("keep");
        const ecs::entity keepChild = fixture.create("child", keep);
        target.set_name("existing");
        const scene::scene_id owner = scene::scene_id{4, 9};
        const core::result<core::json::document> parsed = core::json::document::parse(fixture_text());
        REQUIRE(parsed.has_value());
        const core::result<scene::scene_document> document =
          scene::scene_serializer::parse_document(parsed.value().root(), fixture.registry);
        CONJURE_REQUIRE_OK(document);
        CONJURE_REQUIRE_OK(scene::scene_serializer::load_scene(target, document.value(), owner,
          scene::guid_policy::REGENERATE_ON_CONFLICT));
        CHECK(target.contains(keep));
        CHECK(target.contains(keepChild));
        CHECK(target.name() == "existing");
        REQUIRE(target.roots().size() == 3);
        CHECK(target.roots()[0].entity == keep);
        CHECK(target.roots()[0].owner == scene::SCENE_ID_AUTHORED);
        CHECK(target.roots()[1].owner == owner);
        CHECK(target.roots()[2].owner == owner);
        CHECK(target.name_of(target.roots()[1].entity).view() == "Player");
        CHECK(fixture.world.entity_count() == 5);

        // Loading the same file again regenerates the colliding guids with one WARN.
        testing::log_capture logs = {};
        CONJURE_REQUIRE_OK(scene::scene_serializer::load_scene(target, document.value(), scene::scene_id{5, 9},
          scene::guid_policy::REGENERATE_ON_CONFLICT));
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK(logs.contains(core::log_level::WARN, "3 guid collision(s)"));
        CHECK(fixture.world.entity_count() == 8);
        CHECK(target.guids().size() == 8);
        CHECK(target.roots().size() == 5);
        fixture.step();
    }

    TEST_CASE("serialisation - empty scene round trip") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const std::string text = save_text(target);
        CHECK(text == "{\"format\":\"conjure.scene\",\"version\":1,\"name\":\"\",\"settings\":{},\"roots\":[],"
          "\"archive\":{\"entities\":[]}}");
        scene_fixture other = {};
        const core::result<scene::instantiate_report> report = load_text(other, text);
        CONJURE_REQUIRE_OK(report);
        CHECK(report.value().entityCount == 0);
        CHECK(report.value().entities.empty());
        CHECK(other.world.entity_count() == 0);
        CHECK(save_text(other.active()) == text);
    }

    TEST_CASE("serialisation - save_scene_file and load_scene_file through the memory vfs") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child", root);
        CONJURE_REQUIRE_OK(target.set_local_transform(child, at(1.0f, 1.0f, 1.0f)));
        core::vfs::memory_file_system fs = {};
        CONJURE_REQUIRE_OK(scene::scene_serializer::save_scene_file(target, fs, "scenes/main.scene.json"));
        CHECK(fs.exists("scenes/main.scene.json"));
        const std::string onDisk = fs.read_text_file("scenes/main.scene.json").value();
        CHECK(onDisk == save_text(target));

        scene_fixture other = {};
        const core::result<scene::instantiate_report> loaded = scene::scene_serializer::load_scene_file(
          other.active(),
          fs,
          "scenes/main.scene.json",
          scene::SCENE_ID_AUTHORED,
          scene::guid_policy::KEEP
        );
        CONJURE_REQUIRE_OK(loaded);
        CHECK(loaded.value().entityCount == 2);
        const ecs::entity loadedRoot = other.active().roots()[0].entity;
        CHECK(other.active().find_path("root/child") == other.active().tree().children_of(loadedRoot)[0]);
        const scene::scene_id authored = scene::SCENE_ID_AUTHORED;
        const scene::guid_policy keep = scene::guid_policy::KEEP;
        CONJURE_CHECK_ERROR(
          scene::scene_serializer::load_scene_file(other.active(), fs, "scenes/missing.scene.json", authored, keep),
          core::error_code::NOT_FOUND
        );
        CONJURE_REQUIRE_OK(fs.write_text_file("scenes/broken.scene.json", "{not json"));
        CONJURE_CHECK_ERROR(
          scene::scene_serializer::load_scene_file(other.active(), fs, "scenes/broken.scene.json", authored, keep),
          core::error_code::PARSE_FAILURE
        );
        CHECK(other.world.entity_count() == 2);
    }
}
