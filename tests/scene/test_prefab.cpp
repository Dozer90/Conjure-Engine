#include "conjure/testing/doctest.h"
#include "conjure/testing/approx.h"
#include "conjure/testing/log_capture.h"
#include "scene_test_fixture.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/assets/cooked_blob.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/vfs/memory_file_system.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/scene/prefab.h"
#include "conjure/scene/register.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_error.h"
#include "conjure/scene/scene_serializer.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace conjure;
using namespace scene_test;
using scene::scene_error;

namespace {

    /// <summary>
    /// A loaded-looking prefab asset built from a document (no asset pipeline involved).
    /// </summary>
    [[nodiscard]] scene::prefab_asset make_asset(const scene::prefab_document &document, const assets::asset_id &id) {
        scene::prefab_asset asset = {};
        asset.archive = document.archive;
        asset.name = document.name;
        asset.id = id;
        return asset;
    }

    /// <summary>
    /// Builds a "lamp" subtree: root with a transform and tag, one child with speed and a transform.
    /// </summary>
    struct lamp_parts {
        ecs::entity root = {};
        ecs::entity bulb = {};
    };

    [[nodiscard]] lamp_parts build_lamp(scene_fixture &fixture) {
        lamp_parts parts = {};
        parts.root = fixture.create("Lamp");
        parts.bulb = fixture.create("Bulb", parts.root);
        REQUIRE(fixture.active().set_local_transform(parts.root, at(1.0f, 0.0f, 0.0f)).has_value());
        REQUIRE(fixture.active().set_local_transform(parts.bulb, at(0.0f, 2.0f, 0.0f)).has_value());
        REQUIRE(fixture.active().set_tag(parts.root, core::string_id("Light")).has_value());
        REQUIRE(fixture.world.add<test_speed>(parts.bulb, test_speed{3.0f}).has_value());
        return parts;
    }

    /// <summary>
    /// Publishes a prefab asset in the fixture's manager by cooking the document through the real
    /// importer and loading it, so prefab_instance references resolve through get().
    /// </summary>
    [[nodiscard]] assets::asset_id publish_prefab(
      scene_fixture &fixture,
      const scene::prefab_document &document,
      u64 index
    ) {
        core::json::writer out = {};
        REQUIRE(scene::scene_serializer::write_prefab(document, out).has_value());
        const std::string text = out.finish();
        const assets::asset_id id = test_uuid(index);
        assets::cooked_blob_header header = {};
        header.type = assets::asset_type_traits<scene::prefab_asset>::id();
        header.typeVersion = 1;
        header.importerVersion = 1;
        header.id = id;
        const std::byte *begin = reinterpret_cast<const std::byte *>(text.data());
        fixture.source->add(assets::make_cooked_blob(header, {}, std::vector<std::byte>(begin, begin + text.size())));
        return id;
    }

    void register_prefab_types(scene_fixture &fixture) {
        assets::importer_registry importers = {};
        REQUIRE(scene::register_asset_types(fixture.assets.types(), importers, fixture.registry).has_value());
    }
}

TEST_SUITE("scene/prefab") {

    TEST_CASE("prefab - build_document single root") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity holder = fixture.create("holder");
        const lamp_parts lamp = build_lamp(fixture);
        CONJURE_REQUIRE_OK(target.set_parent(lamp.root, holder));
        CONJURE_REQUIRE_OK(fixture.world.add<test_entity_holder>(lamp.bulb, test_entity_holder{holder, 1}));
        const core::result<scene::prefab_document> document = scene::prefab::build_document(target, lamp.root, "lamp");
        CONJURE_REQUIRE_OK(document);
        CHECK(document.value().name == "lamp");
        REQUIRE(document.value().archive != nullptr);
        CHECK(document.value().archive->entity_count() == 2);
        // Entry 0 is the only root; the link to the outside holder is dropped and external refs are null.
        CHECK(document.value().archive->component_bytes(0, target.ids().parent).empty());
        CHECK_FALSE(document.value().archive->component_bytes(1, target.ids().parent).empty());
        test_entity_holder captured = {};
        const std::span<const std::byte> bytes =
          document.value().archive->component_bytes(1, fixture.registry.id_of<test_entity_holder>());
        REQUIRE(bytes.size() == sizeof(captured));
        std::memcpy(&captured, bytes.data(), sizeof(captured));
        CHECK(captured.other.is_null());
        CONJURE_CHECK_ERROR(
          scene::prefab::build_document(target, ecs::entity{900, 1}, "x"),
          scene_error::INVALID_ENTITY
        );
    }

    TEST_CASE("prefab - multiple roots rejected") {
        scene_fixture fixture = {};
        const std::string text = "{\"format\":\"conjure.prefab\",\"version\":1,\"name\":\"two\","
          "\"archive\":{\"entities\":[{\"components\":{\"conjure.scene.name\":{\"value\":\"a\"}}},"
          "{\"components\":{\"conjure.scene.name\":{\"value\":\"b\"}}}]}}";
        const core::result<core::json::document> parsed = core::json::document::parse(text);
        REQUIRE(parsed.has_value());
        CONJURE_CHECK_ERROR(scene::scene_serializer::parse_prefab(parsed.value().root(), fixture.registry),
          scene_error::PREFAB_MULTIPLE_ROOTS);
        const std::string rootLater = "{\"format\":\"conjure.prefab\",\"version\":1,\"name\":\"two\","
          "\"archive\":{\"entities\":[{\"components\":{\"conjure.scene.parent\":{\"value\":1,\"siblingIndex\":0}}},"
          "{\"components\":{\"conjure.scene.name\":{\"value\":\"b\"}}}]}}";
        const core::result<core::json::document> parsedLater = core::json::document::parse(rootLater);
        REQUIRE(parsedLater.has_value());
        CONJURE_CHECK_ERROR(scene::scene_serializer::parse_prefab(parsedLater.value().root(), fixture.registry),
          scene_error::PREFAB_MULTIPLE_ROOTS);
        const std::string empty =
          "{\"format\":\"conjure.prefab\",\"version\":1,\"name\":\"two\",\"archive\":{\"entities\":[]}}";
        const core::result<core::json::document> parsedEmpty = core::json::document::parse(empty);
        REQUIRE(parsedEmpty.has_value());
        CONJURE_CHECK_ERROR(scene::scene_serializer::parse_prefab(parsedEmpty.value().root(), fixture.registry),
          scene_error::PREFAB_MULTIPLE_ROOTS);
        const std::string wrong = "{\"format\":\"conjure.scene\",\"version\":1,\"archive\":{\"entities\":[]}}";
        const core::result<core::json::document> parsedWrong = core::json::document::parse(wrong);
        REQUIRE(parsedWrong.has_value());
        CONJURE_CHECK_ERROR(scene::scene_serializer::parse_prefab(parsedWrong.value().root(), fixture.registry),
          scene_error::BAD_FORMAT);
        // An asset whose archive has two roots is refused at instantiation too.
        scene::prefab_asset asset = {};
        ecs::entity_archive archive(fixture.registry);
        for (u32 index = 0; index < 2; ++index) {
            ecs::archive_entry entry = {};
            ecs::archive_component nameComponent = {};
            nameComponent.id = fixture.active().ids().name;
            nameComponent.bytes.resize(sizeof(scene::name));
            entry.mask.set(nameComponent.id);
            entry.components.push_back(std::move(nameComponent));
            archive.push_entry(std::move(entry));
        }
        asset.archive = std::make_shared<const ecs::entity_archive>(std::move(archive));
        CONJURE_CHECK_ERROR(scene::prefab::instantiate(fixture.active(), asset), scene_error::PREFAB_MULTIPLE_ROOTS);
        scene::prefab_asset unloaded = {};
        CONJURE_CHECK_ERROR(scene::prefab::instantiate(fixture.active(), unloaded), scene_error::PREFAB_NOT_LOADED);
    }

    TEST_CASE("prefab - mark_instance adds links") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const lamp_parts lamp = build_lamp(fixture);
        const assets::asset_ref<scene::prefab_asset> reference(test_uuid(9));
        CONJURE_REQUIRE_OK(scene::prefab::mark_instance(target, lamp.root, reference));
        const scene::prefab_instance *instance = fixture.world.get<scene::prefab_instance>(lamp.root);
        REQUIRE(instance != nullptr);
        CHECK(instance->prefab.id() == test_uuid(9));
        for (const ecs::entity entity : {lamp.root, lamp.bulb}) {
            const scene::prefab_link *link = fixture.world.get<scene::prefab_link>(entity);
            REQUIRE(link != nullptr);
            CHECK(link->assetGuid == target.guid_of(entity).value());
        }
        CHECK(scene::prefab::instance_root_of(target, lamp.bulb) == lamp.root);
        CHECK(scene::prefab::instance_root_of(target, lamp.root) == lamp.root);
        const ecs::entity loose = fixture.create("loose");
        CHECK(scene::prefab::instance_root_of(target, loose).is_null());
        CHECK(scene::prefab::instance_root_of(target, ecs::entity{900, 1}).is_null());
        // Marking again rebinds in place.
        const assets::asset_ref<scene::prefab_asset> replacement(test_uuid(10));
        CONJURE_REQUIRE_OK(scene::prefab::mark_instance(target, lamp.root, replacement));
        CHECK(fixture.world.get<scene::prefab_instance>(lamp.root)->prefab.id() == test_uuid(10));
        CONJURE_CHECK_ERROR(
          scene::prefab::mark_instance(target, ecs::entity{900, 1}, reference),
          scene_error::INVALID_ENTITY
        );
    }

    TEST_CASE("prefab - instantiate regenerates guids keeps links") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const lamp_parts lamp = build_lamp(fixture);
        const assets::asset_ref<scene::prefab_asset> reference(test_uuid(9));
        CONJURE_REQUIRE_OK(scene::prefab::mark_instance(target, lamp.root, reference));
        const core::result<scene::prefab_document> document = scene::prefab::build_document(target, lamp.root, "lamp");
        CONJURE_REQUIRE_OK(document);
        const scene::prefab_asset asset = make_asset(document.value(), test_uuid(9));

        const core::result<ecs::entity> instance = scene::prefab::instantiate(target, asset);
        CONJURE_REQUIRE_OK(instance);
        const ecs::entity root = instance.value();
        CHECK(target.root_owner(root).has_value());
        CHECK(target.guid_of(root).value() != target.guid_of(lamp.root).value());
        REQUIRE(fixture.world.has<scene::prefab_instance>(root));
        CHECK(fixture.world.get<scene::prefab_instance>(root)->prefab.id() == test_uuid(9));
        CHECK(fixture.world.get<scene::prefab_link>(root)->assetGuid == target.guid_of(lamp.root).value());
        REQUIRE(target.tree().children_of(root).size() == 1);
        const ecs::entity bulb = target.tree().children_of(root)[0];
        CHECK(fixture.world.get<scene::prefab_link>(bulb)->assetGuid == target.guid_of(lamp.bulb).value());
        CHECK(target.guid_of(bulb).value() != target.guid_of(lamp.bulb).value());
        CHECK(fixture.world.get<test_speed>(bulb)->value == doctest::Approx(3.0f));
        CHECK(target.tag_of(root).view() == "Light");

        // A freshly authored prefab (no prefab_instance in the file) still gets its marker.
        scene_fixture fresh = {};
        const ecs::entity plain = fresh.create("Plain");
        const core::result<scene::prefab_document> plainDocument =
          scene::prefab::build_document(fresh.active(), plain, "plain");
        CONJURE_REQUIRE_OK(plainDocument);
        const core::result<ecs::entity> plainInstance =
          scene::prefab::instantiate(fresh.active(), make_asset(plainDocument.value(), test_uuid(11)));
        CONJURE_REQUIRE_OK(plainInstance);
        REQUIRE(fresh.world.has<scene::prefab_instance>(plainInstance.value()));
        CHECK(fresh.world.get<scene::prefab_instance>(plainInstance.value())->prefab.id() == test_uuid(11));
        CHECK(fresh.world.get<scene::prefab_instance>(plainInstance.value())->prefab.is_bound());
        REQUIRE(fresh.world.has<scene::prefab_link>(plainInstance.value()));
        const scene::prefab_link *plainLink = fresh.world.get<scene::prefab_link>(plainInstance.value());
        CHECK(plainLink->assetGuid == fresh.active().guid_of(plain).value());
        fixture.step();
    }

    TEST_CASE("prefab - instantiate_many 100 lands under parent in order") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const lamp_parts lamp = build_lamp(fixture);
        const core::result<scene::prefab_document> document = scene::prefab::build_document(target, lamp.root, "lamp");
        CONJURE_REQUIRE_OK(document);
        const scene::prefab_asset asset = make_asset(document.value(), test_uuid(9));
        const ecs::entity street = fixture.create("street");
        const ecs::entity existing = fixture.create("existing", street);
        std::vector<ecs::entity> roots(100);
        scene::instantiate_options options = {};
        options.parent = street;
        options.index = 0;
        CONJURE_REQUIRE_OK(scene::prefab::instantiate_many(target, asset, 100, options, roots));
        const std::span<const ecs::entity> children = target.tree().children_of(street);
        REQUIRE(children.size() == 101);
        for (u32 index = 0; index < 100; ++index) {
            CAPTURE(index);
            CHECK(children[index] == roots[index]);
            CHECK(target.tree().sibling_index(roots[index]) == index);
            CHECK(target.tree().depth_of(roots[index]) == 1);
            CHECK(fixture.world.has<scene::prefab_instance>(roots[index]));
            CHECK(target.tree().children_of(roots[index]).size() == 1);
        }
        CHECK(children[100] == existing);
        CHECK(target.tree().sibling_index(existing) == 100);
        CHECK(fixture.world.entity_count() == 2 + 2 + 200);
        CHECK(target.guids().size() == 204);
        std::vector<ecs::entity> tooFew(3);
        CONJURE_CHECK_ERROR(
          scene::prefab::instantiate_many(target, asset, 5, options, tooFew),
          core::error_code::INVALID_ARGUMENT
        );
        fixture.step();
    }

    TEST_CASE("prefab - root transform and name overrides") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const lamp_parts lamp = build_lamp(fixture);
        const core::result<scene::prefab_document> document = scene::prefab::build_document(target, lamp.root, "lamp");
        CONJURE_REQUIRE_OK(document);
        const scene::prefab_asset asset = make_asset(document.value(), test_uuid(9));
        scene::instantiate_options options = {};
        options.rootName = core::string_id("Lamp 7");
        scene::local_transform placed = at(10.0f, 20.0f, 30.0f);
        placed.scale = math::vec3(2.0f);
        options.rootTransform = placed;
        const core::result<ecs::entity> instance = scene::prefab::instantiate(target, asset, options);
        CONJURE_REQUIRE_OK(instance);
        CHECK(target.name_of(instance.value()).view() == "Lamp 7");
        CHECK(target.local_transform_of(instance.value()).value() == placed.to_transform());
        const ecs::entity bulb = target.tree().children_of(instance.value())[0];
        CHECK(target.name_of(bulb).view() == "Bulb");
        fixture.step();
        CONJURE_CHECK_APPROX(translation_of(fixture.world_matrix(bulb)), math::vec3(10.0f, 24.0f, 30.0f));
    }

    TEST_CASE("prefab - nested prefab baked and inner marker preserved") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        // Inner prefab: the lamp. Outer prefab: a post holding a lamp instance.
        const lamp_parts lamp = build_lamp(fixture);
        const core::result<scene::prefab_document> lampDocument =
          scene::prefab::build_document(target, lamp.root, "lamp");
        CONJURE_REQUIRE_OK(lampDocument);
        const scene::prefab_asset lampAsset = make_asset(lampDocument.value(), test_uuid(1));
        const ecs::entity post = fixture.create("Post");
        scene::instantiate_options under = {};
        under.parent = post;
        const core::result<ecs::entity> innerInstance = scene::prefab::instantiate(target, lampAsset, under);
        CONJURE_REQUIRE_OK(innerInstance);
        const assets::asset_ref<scene::prefab_asset> outerReference(test_uuid(2));
        CONJURE_REQUIRE_OK(scene::prefab::mark_instance(target, post, outerReference));
        // mark_instance relinks the whole subtree to the outer document's guids; the inner marker stays.
        CHECK(fixture.world.has<scene::prefab_instance>(innerInstance.value()));
        const core::result<scene::prefab_document> postDocument = scene::prefab::build_document(target, post, "post");
        CONJURE_REQUIRE_OK(postDocument);
        CHECK(postDocument.value().archive->entity_count() == 3);
        const scene::prefab_asset postAsset = make_asset(postDocument.value(), test_uuid(2));

        const core::result<ecs::entity> outer = scene::prefab::instantiate(target, postAsset);
        CONJURE_REQUIRE_OK(outer);
        REQUIRE(target.tree().children_of(outer.value()).size() == 1);
        const ecs::entity inner = target.tree().children_of(outer.value())[0];
        REQUIRE(fixture.world.has<scene::prefab_instance>(inner));
        CHECK(fixture.world.get<scene::prefab_instance>(inner)->prefab.id() == test_uuid(1));
        CHECK(fixture.world.get<scene::prefab_instance>(outer.value())->prefab.id() == test_uuid(2));
        CHECK(scene::prefab::instance_root_of(target, target.tree().children_of(inner)[0]) == inner);
        CHECK(scene::prefab::instance_root_of(target, inner) == inner);
        CHECK(target.tree().depth_of(target.tree().children_of(inner)[0]) == 2);
        fixture.step();
    }

    TEST_CASE("prefab - diff reports modified added removed") {
        scene_fixture fixture = {};
        register_prefab_types(fixture);
        scene::scene &target = fixture.active();
        const lamp_parts lamp = build_lamp(fixture);
        CONJURE_REQUIRE_OK(fixture.world.add<test_entity_holder>(lamp.bulb, test_entity_holder{lamp.root, 2}));
        const core::result<scene::prefab_document> document = scene::prefab::build_document(target, lamp.root, "lamp");
        CONJURE_REQUIRE_OK(document);
        const assets::asset_id id = publish_prefab(fixture, document.value(), 40);
        const core::result<assets::asset_handle> loaded = fixture.assets.load_blocking(id);
        CONJURE_REQUIRE_OK(loaded);
        const scene::prefab_asset *asset = loaded.value().get<scene::prefab_asset>();
        REQUIRE(asset != nullptr);
        CHECK(asset->id == id);
        CHECK(asset->name == "lamp");
        CHECK(asset->rootGuid == target.guid_of(lamp.root).value());

        const core::result<ecs::entity> instance = scene::prefab::instantiate(target, *asset);
        CONJURE_REQUIRE_OK(instance);
        const core::result<scene::prefab_diff> clean = scene::prefab::diff(target, instance.value());
        CONJURE_REQUIRE_OK(clean);
        CHECK(clean.value().empty());

        // Modify the bulb, add an entity, remove nothing yet.
        const ecs::entity bulb = target.tree().children_of(instance.value())[0];
        fixture.world.get<test_speed>(bulb)->value = 9.0f;
        CONJURE_REQUIRE_OK(target.set_local_transform(instance.value(), at(5.0f, 5.0f, 5.0f)));
        const ecs::entity added = fixture.create("Extra", instance.value());
        core::result<scene::prefab_diff> changed = scene::prefab::diff(target, instance.value());
        CONJURE_REQUIRE_OK(changed);
        REQUIRE(changed.value().entries.size() == 3);
        u32 modified = 0;
        u32 addedCount = 0;
        for (const scene::entity_diff &entry : changed.value().entries) {
            if (entry.kind == scene::entity_diff_kind::ADDED) {
                ++addedCount;
                CHECK(entry.instanceEntity == added);
                CHECK(entry.assetGuid.is_null());
            } else {
                ++modified;
                CHECK(entry.kind == scene::entity_diff_kind::MODIFIED);
                REQUIRE(entry.components.size() == 1);
                if (entry.instanceEntity == bulb) {
                    CHECK(entry.components[0] == core::string_id("test.speed"));
                    CHECK(entry.assetGuid == target.guid_of(lamp.bulb).value());
                } else {
                    CHECK(entry.instanceEntity == instance.value());
                    CHECK(entry.components[0] == core::string_id("conjure.scene.local_transform"));
                }
            }
        }
        CHECK(modified == 2);
        CHECK(addedCount == 1);

        // Removing the bulb and adding a component to the root.
        CONJURE_REQUIRE_OK(target.destroy_now(bulb));
        CONJURE_REQUIRE_OK(target.set_layer(instance.value(), 4));
        changed = scene::prefab::diff(target, instance.value());
        CONJURE_REQUIRE_OK(changed);
        u32 removed = 0;
        for (const scene::entity_diff &entry : changed.value().entries) {
            if (entry.kind == scene::entity_diff_kind::REMOVED) {
                ++removed;
                CHECK(entry.assetGuid == target.guid_of(lamp.bulb).value());
                CHECK(entry.instanceEntity.is_null());
            } else if (entry.instanceEntity == instance.value()) {
                const core::string_id layerName("conjure.scene.layer");
                CHECK(std::find(entry.components.begin(), entry.components.end(), layerName) != entry.components.end());
            }
        }
        CHECK(removed == 1);

        // Failure paths.
        CONJURE_CHECK_ERROR(scene::prefab::diff(target, lamp.root), scene_error::NOT_A_PREFAB_INSTANCE);
        const assets::asset_ref<scene::prefab_asset> unloadedReference(test_uuid(77));
        CONJURE_REQUIRE_OK(scene::prefab::mark_instance(target, lamp.root, unloadedReference));
        CONJURE_CHECK_ERROR(scene::prefab::diff(target, lamp.root), scene_error::PREFAB_NOT_LOADED);
        fixture.step();
    }

    TEST_CASE("prefab - revert restores asset state keeps root guid") {
        scene_fixture fixture = {};
        register_prefab_types(fixture);
        scene::scene &target = fixture.active();
        const lamp_parts lamp = build_lamp(fixture);
        const core::result<scene::prefab_document> document = scene::prefab::build_document(target, lamp.root, "lamp");
        CONJURE_REQUIRE_OK(document);
        const assets::asset_id id = publish_prefab(fixture, document.value(), 41);
        const core::result<assets::asset_handle> loaded = fixture.assets.load_blocking(id);
        CONJURE_REQUIRE_OK(loaded);
        const scene::prefab_asset *asset = loaded.value().get<scene::prefab_asset>();
        REQUIRE(asset != nullptr);

        const ecs::entity holder = fixture.create("holder");
        const ecs::entity before = fixture.create("before", holder);
        scene::instantiate_options options = {};
        options.parent = holder;
        options.index = 0;
        options.rootName = core::string_id("MyLamp");
        options.rootTransform = at(3.0f, 0.0f, 0.0f);
        const core::result<ecs::entity> instance = scene::prefab::instantiate(target, *asset, options);
        CONJURE_REQUIRE_OK(instance);
        const core::uuid rootGuid = target.guid_of(instance.value()).value();
        const ecs::entity bulb = target.tree().children_of(instance.value())[0];
        fixture.world.get<test_speed>(bulb)->value = 42.0f;
        CONJURE_REQUIRE_OK(target.set_local_transform(bulb, at(0.0f, -1.0f, 0.0f)));
        const ecs::entity extra = fixture.create("Extra", instance.value());
        CHECK_FALSE(scene::prefab::diff(target, instance.value()).value().empty());

        CONJURE_REQUIRE_OK(scene::prefab::revert(target, instance.value()));
        CHECK_FALSE(target.contains(instance.value()));
        CHECK_FALSE(target.contains(extra));
        const ecs::entity reverted = target.find_guid(rootGuid);
        REQUIRE(target.contains(reverted));
        CHECK(target.tree().parent_of(reverted) == holder);
        CHECK(target.tree().sibling_index(reverted) == 0);
        CHECK(target.tree().children_of(holder)[1] == before);
        CHECK(target.name_of(reverted).view() == "MyLamp");
        CONJURE_CHECK_APPROX(target.local_transform_of(reverted).value().position, math::vec3(3.0f, 0.0f, 0.0f));
        REQUIRE(target.tree().children_of(reverted).size() == 1);
        const ecs::entity newBulb = target.tree().children_of(reverted)[0];
        CHECK(fixture.world.get<test_speed>(newBulb)->value == doctest::Approx(3.0f));
        CONJURE_CHECK_APPROX(target.local_transform_of(newBulb).value().position, math::vec3(0.0f, 2.0f, 0.0f));
        // The kept root name and transform are the only remaining differences from the asset.
        const core::result<scene::prefab_diff> afterRevert = scene::prefab::diff(target, reverted);
        CONJURE_REQUIRE_OK(afterRevert);
        REQUIRE(afterRevert.value().entries.size() == 1);
        CHECK(afterRevert.value().entries[0].instanceEntity == reverted);
        CHECK(afterRevert.value().entries[0].kind == scene::entity_diff_kind::MODIFIED);
        for (const core::string_id &component : afterRevert.value().entries[0].components) {
            const bool expected = component == core::string_id("conjure.scene.name")
              || component == core::string_id("conjure.scene.local_transform");
            CHECK(expected);
        }
        CONJURE_CHECK_ERROR(scene::prefab::revert(target, holder), scene_error::NOT_A_PREFAB_INSTANCE);
        fixture.step();
    }

    TEST_CASE("prefab - apply writes links as guids") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const lamp_parts lamp = build_lamp(fixture);
        const core::result<scene::prefab_document> document = scene::prefab::build_document(target, lamp.root, "lamp");
        CONJURE_REQUIRE_OK(document);
        const scene::prefab_asset asset = make_asset(document.value(), test_uuid(9));
        const core::result<ecs::entity> instance = scene::prefab::instantiate(target, asset);
        CONJURE_REQUIRE_OK(instance);
        const ecs::entity bulb = target.tree().children_of(instance.value())[0];
        fixture.world.get<test_speed>(bulb)->value = 7.0f;
        const ecs::entity extra = fixture.create("Extra", instance.value());

        const core::result<scene::prefab_document> applied = scene::prefab::apply(target, instance.value());
        CONJURE_REQUIRE_OK(applied);
        const ecs::entity_archive &archive = *applied.value().archive;
        REQUIRE(archive.entity_count() == 3);
        const scene::scene_component_ids &ids = target.ids();
        // Entry guids are the prefab_link guids (the original lamp's guids), not the instance guids.
        scene::guid rootGuid = {};
        std::memcpy(&rootGuid, archive.component_bytes(0, ids.guid).data(), sizeof(rootGuid));
        CHECK(rootGuid.value == target.guid_of(lamp.root).value());
        scene::guid bulbGuid = {};
        std::memcpy(&bulbGuid, archive.component_bytes(1, ids.guid).data(), sizeof(bulbGuid));
        CHECK(bulbGuid.value == target.guid_of(lamp.bulb).value());
        // The new entity gets a fresh guid mirrored in its link; the root loses its instance marker.
        scene::guid extraGuid = {};
        std::memcpy(&extraGuid, archive.component_bytes(2, ids.guid).data(), sizeof(extraGuid));
        scene::prefab_link extraLink = {};
        std::memcpy(&extraLink, archive.component_bytes(2, ids.prefabLink).data(), sizeof(extraLink));
        CHECK(extraGuid.value == extraLink.assetGuid);
        CHECK(extraGuid.value != target.guid_of(extra).value());
        CHECK(archive.component_bytes(0, ids.prefabInstance).empty());
        test_speed speed = {};
        std::memcpy(&speed, archive.component_bytes(1, fixture.registry.id_of<test_speed>()).data(), sizeof(speed));
        CHECK(speed.value == doctest::Approx(7.0f));
        CONJURE_CHECK_ERROR(scene::prefab::apply(target, lamp.root), scene_error::NOT_A_PREFAB_INSTANCE);

        // The applied document round-trips through the prefab writer and parser.
        core::json::writer out = {};
        CONJURE_REQUIRE_OK(scene::scene_serializer::write_prefab(applied.value(), out));
        const core::result<core::json::document> parsed = core::json::document::parse(out.finish());
        REQUIRE(parsed.has_value());
        const core::result<scene::prefab_document> reparsed =
          scene::scene_serializer::parse_prefab(parsed.value().root(), fixture.registry);
        CONJURE_REQUIRE_OK(reparsed);
        CHECK(reparsed.value().archive->entity_count() == 3);
    }

    TEST_CASE("prefab - refresh_instances reverts only unmodified") {
        scene_fixture fixture = {};
        register_prefab_types(fixture);
        scene::scene &target = fixture.active();
        const lamp_parts lamp = build_lamp(fixture);
        const core::result<scene::prefab_document> document = scene::prefab::build_document(target, lamp.root, "lamp");
        CONJURE_REQUIRE_OK(document);
        const assets::asset_id id = publish_prefab(fixture, document.value(), 42);
        const core::result<assets::asset_handle> loaded = fixture.assets.load_blocking(id);
        CONJURE_REQUIRE_OK(loaded);
        const scene::prefab_asset *asset = loaded.value().get<scene::prefab_asset>();
        REQUIRE(asset != nullptr);
        std::vector<ecs::entity> roots(3);
        CONJURE_REQUIRE_OK(scene::prefab::instantiate_many(target, *asset, 3, {}, roots));
        const ecs::entity modifiedBulb = target.tree().children_of(roots[1])[0];
        fixture.world.get<test_speed>(modifiedBulb)->value = 99.0f;
        const std::vector<core::uuid> guids = {
            target.guid_of(roots[0]).value(),
            target.guid_of(roots[1]).value(),
            target.guid_of(roots[2]).value(),
        };

        CHECK(scene::prefab::refresh_instances(target, id) == 2);
        for (u32 index = 0; index < 3; ++index) {
            CAPTURE(index);
            const ecs::entity root = target.find_guid(guids[index]);
            CHECK(target.contains(root));
            CHECK(fixture.world.has<scene::prefab_instance>(root));
        }
        const auto bulbSpeed = [&]() {
            return fixture.world.get<test_speed>(target.tree().children_of(target.find_guid(guids[1]))[0])->value;
        };
        CHECK(bulbSpeed() == doctest::Approx(99.0f));
        CHECK(scene::prefab::refresh_instances(target, id, false) == 3);
        CHECK(bulbSpeed() == doctest::Approx(3.0f));
        CHECK(scene::prefab::refresh_instances(target, test_uuid(500)) == 0);
        CHECK(fixture.world.entity_count() == 2 + 6);
        fixture.step();
    }

    TEST_CASE("prefab - unpack removes markers stops at nested") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const lamp_parts lamp = build_lamp(fixture);
        const core::result<scene::prefab_document> lampDocument =
          scene::prefab::build_document(target, lamp.root, "lamp");
        CONJURE_REQUIRE_OK(lampDocument);
        const scene::prefab_asset lampAsset = make_asset(lampDocument.value(), test_uuid(1));
        const ecs::entity post = fixture.create("Post");
        const ecs::entity arm = fixture.create("Arm", post);
        scene::instantiate_options under = {};
        under.parent = arm;
        const core::result<ecs::entity> inner = scene::prefab::instantiate(target, lampAsset, under);
        CONJURE_REQUIRE_OK(inner);
        const assets::asset_ref<scene::prefab_asset> postReference(test_uuid(2));
        CONJURE_REQUIRE_OK(scene::prefab::mark_instance(target, post, postReference));
        const ecs::entity bulb = target.tree().children_of(inner.value())[0];

        CONJURE_REQUIRE_OK(scene::prefab::unpack(target, post, false));
        CHECK_FALSE(fixture.world.has<scene::prefab_instance>(post));
        CHECK_FALSE(fixture.world.has<scene::prefab_link>(post));
        CHECK_FALSE(fixture.world.has<scene::prefab_link>(arm));
        CHECK(fixture.world.has<scene::prefab_instance>(inner.value()));
        CHECK(fixture.world.has<scene::prefab_link>(inner.value()));
        CHECK(fixture.world.has<scene::prefab_link>(bulb));
        CONJURE_CHECK_ERROR(scene::prefab::unpack(target, post, false), scene_error::NOT_A_PREFAB_INSTANCE);

        CONJURE_REQUIRE_OK(scene::prefab::unpack(target, inner.value(), true));
        CHECK_FALSE(fixture.world.has<scene::prefab_instance>(inner.value()));
        CHECK_FALSE(fixture.world.has<scene::prefab_link>(inner.value()));
        CHECK_FALSE(fixture.world.has<scene::prefab_link>(bulb));

        // Recursive unpack from the top strips a nested instance too.
        const core::result<ecs::entity> again = scene::prefab::instantiate(target, lampAsset, under);
        CONJURE_REQUIRE_OK(again);
        CONJURE_REQUIRE_OK(scene::prefab::mark_instance(target, post, postReference));
        CONJURE_REQUIRE_OK(scene::prefab::unpack(target, post, true));
        CHECK_FALSE(fixture.world.has<scene::prefab_instance>(again.value()));
        CHECK_FALSE(fixture.world.has<scene::prefab_link>(target.tree().children_of(again.value())[0]));
        CONJURE_CHECK_ERROR(scene::prefab::unpack(target, ecs::entity{900, 1}), scene_error::NOT_A_PREFAB_INSTANCE);
    }

    TEST_CASE("prefab_builder - builds hierarchy and components by name") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        scene::prefab_builder builder(fixture.registry);
        const u32 root = builder.add_node(scene::prefab_builder::NO_PARENT, "Robot", at(0.0f, 1.0f, 0.0f));
        const u32 leftArm = builder.add_node(root, "LeftArm", at(-1.0f, 0.0f, 0.0f));
        const u32 rightArm = builder.add_node(root, "RightArm", at(1.0f, 0.0f, 0.0f));
        const u32 hand = builder.add_node(rightArm, "Hand");
        CHECK(builder.node_count() == 4);
        const core::result<core::json::document> speed = core::json::document::parse("{\"value\":12.5}");
        REQUIRE(speed.has_value());
        CONJURE_REQUIRE_OK(builder.add_component_json(hand, core::string_id("test.speed"), speed.value().root()));
        CONJURE_REQUIRE_OK(builder.add_component_json(root, core::string_id("conjure.scene.tag"),
          core::json::document::parse("{\"value\":\"Robot\"}").value().root()));
        CONJURE_REQUIRE_OK(builder.add_tag(leftArm, target.ids().disabled));
        CONJURE_REQUIRE_OK(builder.add_component_json(leftArm, core::string_id("conjure.scene.disabled"),
          core::json::document::parse("{}").value().root()));
        const test_speed rawSpeed{4.0f};
        CONJURE_REQUIRE_OK(builder.add_component_raw(leftArm, fixture.registry.id_of<test_speed>(),
          std::span<const std::byte>(reinterpret_cast<const std::byte *>(&rawSpeed), sizeof(rawSpeed))));

        // Failure paths: unknown names, wrong sizes, reserved and unknown nodes, non-tags.
        CONJURE_CHECK_ERROR(
          builder.add_component_json(root, core::string_id("com.nobody.unknown"), speed.value().root()),
          ecs::ecs_error::UNKNOWN_COMPONENT
        );
        CONJURE_CHECK_ERROR(builder.add_component_json(99, core::string_id("test.speed"), speed.value().root()),
          scene_error::BUILDER_INVALID);
        const core::result<core::json::document> parentJson =
          core::json::document::parse("{\"value\":null,\"siblingIndex\":0}");
        REQUIRE(parentJson.has_value());
        CONJURE_CHECK_ERROR(
          builder.add_component_json(root, core::string_id("conjure.scene.parent"), parentJson.value().root()),
          scene_error::BUILDER_INVALID
        );
        // Name and transform belong to add_node: a JSON copy would duplicate the entry's component.
        CONJURE_CHECK_ERROR(
          builder.add_component_json(root, core::string_id("conjure.scene.name"),
            core::json::document::parse("{\"value\":\"Other\"}").value().root()),
          scene_error::BUILDER_INVALID
        );
        CONJURE_CHECK_ERROR(
          builder.add_component_json(root, core::string_id("conjure.scene.local_transform"),
            core::json::document::parse("{\"position\":[1,2,3],\"rotation\":[0,0,0,1],\"scale\":[1,1,1]}")
              .value().root()),
          scene_error::BUILDER_INVALID
        );
        const scene::local_transform rawTransform = at(4.0f, 0.0f, 0.0f);
        CONJURE_CHECK_ERROR(
          builder.add_component_raw(root, target.ids().localTransform,
            std::span<const std::byte>(reinterpret_cast<const std::byte *>(&rawTransform), sizeof(rawTransform))),
          scene_error::BUILDER_INVALID
        );
        CONJURE_CHECK_ERROR(
          builder.add_component_raw(root, fixture.registry.id_of<test_speed>(), std::span<const std::byte>()),
          scene_error::BUILDER_INVALID
        );
        CONJURE_CHECK_ERROR(builder.add_tag(root, fixture.registry.id_of<test_speed>()), scene_error::BUILDER_INVALID);
        CONJURE_CHECK_ERROR(builder.add_tag(root, target.ids().transformDirty), scene_error::BUILDER_INVALID);
        CONJURE_CHECK_ERROR(builder.add_component_json(root, core::string_id("test.speed"),
          core::json::document::parse("{\"value\":\"fast\"}").value().root()), ecs::ecs_error::SERIALIZATION_FAILED);

        const core::result<scene::prefab_document> document = builder.build("robot");
        CONJURE_REQUIRE_OK(document);
        CHECK(document.value().name == "robot");
        REQUIRE(document.value().archive->entity_count() == 4);
        const scene::scene_component_ids &ids = target.ids();
        CHECK(document.value().archive->component_bytes(0, ids.parent).empty());
        CHECK_FALSE(document.value().archive->component_bytes(0, ids.guid).empty());
        CHECK_FALSE(document.value().archive->component_bytes(0, ids.prefabLink).empty());

        const scene::prefab_asset robotAsset = make_asset(document.value(), test_uuid(3));
        const core::result<ecs::entity> instance = scene::prefab::instantiate(target, robotAsset);
        CONJURE_REQUIRE_OK(instance);
        const ecs::entity robot = instance.value();
        CHECK(target.name_of(robot).view() == "Robot");
        CHECK(target.tag_of(robot).view() == "Robot");
        REQUIRE(target.tree().children_of(robot).size() == 2);
        const ecs::entity left = target.tree().children_of(robot)[0];
        const ecs::entity right = target.tree().children_of(robot)[1];
        CHECK(target.name_of(left).view() == "LeftArm");
        CHECK(target.name_of(right).view() == "RightArm");
        CHECK_FALSE(target.is_enabled(left));
        CHECK(fixture.world.get<test_speed>(left)->value == doctest::Approx(4.0f));
        REQUIRE(target.tree().children_of(right).size() == 1);
        const ecs::entity newHand = target.tree().children_of(right)[0];
        CHECK(target.name_of(newHand).view() == "Hand");
        CHECK(fixture.world.get<test_speed>(newHand)->value == doctest::Approx(12.5f));
        CONJURE_CHECK_APPROX(target.world_transform_of(newHand).value().position, math::vec3(1.0f, 1.0f, 0.0f));
        CHECK(fixture.world.has<scene::prefab_link>(newHand));

        // Builders without a single root or with a bad parent are invalid.
        scene::prefab_builder empty(fixture.registry);
        CONJURE_CHECK_ERROR(empty.build("empty"), scene_error::BUILDER_INVALID);
        scene::prefab_builder twoRoots(fixture.registry);
        CHECK(twoRoots.add_node(scene::prefab_builder::NO_PARENT, "a") == 0);
        CHECK(twoRoots.add_node(scene::prefab_builder::NO_PARENT, "b") == 1);
        CONJURE_CHECK_ERROR(twoRoots.build("two"), scene_error::BUILDER_INVALID);
        scene::prefab_builder badParent(fixture.registry);
        CHECK(badParent.add_node(scene::prefab_builder::NO_PARENT, "a") == 0);
        CHECK(badParent.add_node(7, "b") == 1);
        CONJURE_CHECK_ERROR(badParent.build("bad"), scene_error::BUILDER_INVALID);
        fixture.step();
    }

    TEST_CASE("prefab - third-party observer during instantiate leaves the batch intact") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const lamp_parts lamp = build_lamp(fixture);
        const core::result<scene::prefab_document> document = scene::prefab::build_document(target, lamp.root, "lamp");
        CONJURE_REQUIRE_OK(document);
        const scene::prefab_asset asset = make_asset(document.value(), test_uuid(1));
        const ecs::entity bystander = fixture.create("bystander");
        const ecs::component_id speedId = fixture.registry.id_of<test_speed>();

        // A gameplay observer reacting to the first bulb's speed by giving another entity one, through
        // the deferred buffer the ECS drains inside the batch.
        u32 reactions = 0;
        const ecs::observer_id reactor = fixture.world.observe(
          speedId,
          ecs::component_event::ADDED,
          [&](ecs::world &, ecs::entity added, ecs::component_id, ecs::command_buffer &deferred) {
              if (!(added == bystander) && reactions == 0) {
                  ++reactions;
                  deferred.add<test_speed>(bystander, test_speed{9.0f});
              }
          });
        std::vector<ecs::entity> roots(3);
        scene::instantiate_options options = {};
        options.rootName = core::string_id("Copy");
        const core::result<void> spawned = scene::prefab::instantiate_many(target, asset, 3, options, roots);
        fixture.world.unobserve(reactor);
        CONJURE_REQUIRE_OK(spawned);
        CHECK(reactions == 1);
        CHECK(fixture.world.get<test_speed>(bystander)->value == doctest::Approx(9.0f));
        CHECK(fixture.world.entity_count() == 3 + 6);
        CHECK(target.roots().size() == 2 + 3);
        for (const ecs::entity root : roots) {
            REQUIRE(target.contains(root));
            CHECK(target.name_of(root).view() == "Copy");
            REQUIRE(target.tree().children_of(root).size() == 1);
            CHECK(fixture.world.has<scene::prefab_link>(target.tree().children_of(root)[0]));
            CHECK(fixture.world.has<scene::prefab_instance>(root));
        }
        CHECK_FALSE(fixture.world.has<scene::prefab_link>(bystander));
        CONJURE_CHECK_OK(target.validate());
        fixture.step();
    }

    TEST_CASE("prefab - importer extracts asset_ref dependencies and canonicalises") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        assets::importer_registry importers = {};
        CONJURE_REQUIRE_OK(scene::register_asset_types(fixture.assets.types(), importers, fixture.registry));
        const assets::asset_importer *importer = importers.find(scene::PREFAB_IMPORTER_ID);
        REQUIRE(importer != nullptr);
        CHECK(importer->version() == 1);
        REQUIRE(importer->extensions().size() == 1);
        CHECK(importer->extensions()[0] == ".prefab.json");
        CHECK(importers.find_for_extension(".prefab.json") == importer);

        const lamp_parts lamp = build_lamp(fixture);
        CONJURE_REQUIRE_OK(fixture.world.add<test_asset_holder>(lamp.bulb,
          test_asset_holder{assets::asset_ref<assets::binary_asset>(test_uuid(200))}));
        CONJURE_REQUIRE_OK(fixture.world.add<test_asset_holder>(lamp.root,
          test_asset_holder{assets::asset_ref<assets::binary_asset>(test_uuid(100))}));
        const core::result<scene::prefab_document> document = scene::prefab::build_document(target, lamp.root, "lamp");
        CONJURE_REQUIRE_OK(document);
        core::json::writer out = {};
        CONJURE_REQUIRE_OK(scene::scene_serializer::write_prefab(document.value(), out));
        const std::string canonical = out.finish();
        // A hand-edited, pretty-printed file with an unknown component: the import canonicalises it.
        const core::result<core::json::document> parsed = core::json::document::parse(canonical);
        REQUIRE(parsed.has_value());
        std::string pretty = parsed.value().to_string(true);
        const std::string marker = "\"components\": {";
        const usize position = pretty.find(marker);
        REQUIRE(position != std::string::npos);
        pretty.insert(position + marker.size(), "\"com.studio.glow\": {\"radius\": 3},");

        core::vfs::memory_file_system fs = {};
        CONJURE_REQUIRE_OK(fs.write_text_file("assets/lamp.prefab.json", pretty));
        const std::vector<std::byte> source = fs.read_file("assets/lamp.prefab.json").value();
        assets::sub_asset_table subAssets = {};
        assets::import_context context(fs, "assets://lamp.prefab.json", "assets/lamp.prefab.json", source,
          core::json::value(), "windows", test_uuid(300), subAssets, {});
        const core::result<assets::import_output> output = importer->import(context);
        CONJURE_REQUIRE_OK(output);
        REQUIRE(output.value().assets.size() == 1);
        const assets::imported_asset &primary = output.value().assets[0];
        CHECK(primary.subPath.empty());
        CHECK(primary.type == assets::asset_type_traits<scene::prefab_asset>::id());
        CHECK(primary.dependencies == std::vector<assets::asset_id>{test_uuid(100), test_uuid(200)});
        REQUIRE(context.warnings().size() == 1);
        CHECK(context.warnings()[0].find("com.studio.glow") != std::string::npos);
        const std::string payload(reinterpret_cast<const char *>(primary.payload.data()), primary.payload.size());
        CHECK(payload.find("\n") == std::string::npos);
        CHECK(payload.find("\"com.studio.glow\":{\"radius\":3}") != std::string::npos);
        CHECK(payload.find("\"format\":\"conjure.prefab\"") != std::string::npos);
        // Importing the payload again is a fixed point.
        assets::import_context second(fs, "assets://lamp.prefab.json", "assets/lamp.prefab.json", primary.payload,
          core::json::value(), "windows", test_uuid(300), subAssets, {});
        const core::result<assets::import_output> again = importer->import(second);
        CONJURE_REQUIRE_OK(again);
        CHECK(again.value().assets[0].payload == primary.payload);

        // A scene file goes through the scene importer the same way; a broken file fails IMPORT_FAILED.
        const assets::asset_importer *sceneImporter = importers.find(scene::SCENE_IMPORTER_ID);
        REQUIRE(sceneImporter != nullptr);
        CHECK(sceneImporter->extensions()[0] == ".scene.json");
        assets::import_context broken(fs, "assets://broken.scene.json", "assets/broken.scene.json", source,
          core::json::value(), "windows", test_uuid(301), subAssets, {});
        CONJURE_CHECK_ERROR(sceneImporter->import(broken), assets::asset_error_code::IMPORT_FAILED);
        core::json::writer sceneOut = {};
        CONJURE_REQUIRE_OK(scene::scene_serializer::save_scene(target, sceneOut));
        const std::string sceneText = sceneOut.finish();
        const std::vector<std::byte> sceneBytes(reinterpret_cast<const std::byte *>(sceneText.data()),
          reinterpret_cast<const std::byte *>(sceneText.data()) + sceneText.size());
        assets::import_context sceneContext(fs, "assets://main.scene.json", "assets/main.scene.json", sceneBytes,
          core::json::value(), "windows", test_uuid(302), subAssets, {});
        const core::result<assets::import_output> sceneOutput = sceneImporter->import(sceneContext);
        CONJURE_REQUIRE_OK(sceneOutput);
        CHECK(sceneOutput.value().assets[0].type == assets::asset_type_traits<scene::scene_asset>::id());
        const std::vector<assets::asset_id> expectedDependencies = {test_uuid(100), test_uuid(200)};
        CHECK(sceneOutput.value().assets[0].dependencies == expectedDependencies);
    }

    TEST_CASE("prefab - loader parses on worker [parallel]") {
        for (const u32 workers : {0u, 4u}) {
            CAPTURE(workers);
            core::jobs::job_system jobs = {};
            core::jobs::job_system_desc desc = {};
            desc.workerCount = workers;
            REQUIRE(jobs.start(desc).has_value());
            {
                ecs::component_registry registry = {};
                register_scene_test_components(registry);
                assets::asset_manager_desc managerDesc = {};
                managerDesc.jobs = &jobs;
                std::unique_ptr<assets::memory_asset_source> owned = std::make_unique<assets::memory_asset_source>();
                assets::memory_asset_source *source = owned.get();
                managerDesc.source = std::move(owned);
                assets::asset_manager manager(std::move(managerDesc));
                assets::importer_registry importers = {};
                CONJURE_REQUIRE_OK(assets::register_binary_asset_type(manager.types()));
                CONJURE_REQUIRE_OK(scene::register_asset_types(manager.types(), importers, registry));
                CHECK(manager.types().find(scene::PREFAB_ASSET_TYPE_NAME) != nullptr);
                CHECK(manager.types().find(scene::SCENE_ASSET_TYPE_NAME) != nullptr);

                // Cook a prefab blob with a dependency and load it asynchronously.
                const std::string text = "{\"format\":\"conjure.prefab\",\"version\":1,\"name\":\"crate\","
                  "\"archive\":{\"entities\":[{\"components\":{\"conjure.scene.name\":{\"value\":\"Crate\"},"
                  "\"conjure.scene.guid\":{\"value\":\"" + test_uuid(50).to_string() + "\"},"
                  "\"test.asset_holder\":{\"asset\":\"" + test_uuid(60).to_string() + "\"}}}]}}";
                assets::cooked_blob_header header = {};
                header.type = assets::asset_type_traits<scene::prefab_asset>::id();
                header.typeVersion = 1;
                header.importerVersion = 1;
                header.id = test_uuid(70);
                const std::byte *begin = reinterpret_cast<const std::byte *>(text.data());
                std::vector<std::byte> payload(begin, begin + text.size());
                source->add(assets::make_cooked_blob(header, {test_uuid(60)}, std::move(payload)));
                assets::cooked_blob_header binaryHeader = {};
                binaryHeader.type = assets::asset_type_traits<assets::binary_asset>::id();
                binaryHeader.typeVersion = 1;
                binaryHeader.importerVersion = 1;
                binaryHeader.id = test_uuid(60);
                source->add(assets::make_cooked_blob(binaryHeader, {}, std::vector<std::byte>(4, std::byte{1})));

                assets::asset_handle handle = manager.load(test_uuid(70));
                manager.flush();
                REQUIRE(handle.state() == assets::asset_state::LOADED);
                const scene::prefab_asset *asset = handle.get<scene::prefab_asset>();
                REQUIRE(asset != nullptr);
                CHECK(asset->name == "crate");
                CHECK(asset->id == test_uuid(70));
                CHECK(asset->rootGuid == test_uuid(50));
                CHECK(asset->dependencies == std::vector<assets::asset_id>{test_uuid(60)});
                REQUIRE(asset->archive != nullptr);
                CHECK(asset->archive->entity_count() == 1);
                // The dependency was loaded before the prefab finalised.
                CHECK(manager.state_of(test_uuid(60)) == assets::asset_state::LOADED);

                // A malformed payload fails the load.
                assets::cooked_blob_header badHeader = header;
                badHeader.id = test_uuid(71);
                const std::string bad = "{\"format\":\"conjure.prefab\",\"version\":9,\"archive\":{\"entities\":[]}}";
                const std::byte *badBegin = reinterpret_cast<const std::byte *>(bad.data());
                std::vector<std::byte> badPayload(badBegin, badBegin + bad.size());
                source->add(assets::make_cooked_blob(badHeader, {}, std::move(badPayload)));
                testing::log_capture logs = {};
                logs.allow_errors();
                assets::asset_handle badHandle = manager.load(test_uuid(71));
                manager.flush();
                CHECK(badHandle.state() == assets::asset_state::FAILED);
                const core::error *failure = manager.last_error(test_uuid(71));
                REQUIRE(failure != nullptr);
                CHECK(failure->is(assets::asset_error_code::LOAD_FAILED));
                CHECK(failure->message().find("UNSUPPORTED_VERSION") != std::string::npos);
                badHandle.reset();
                handle.reset();
                manager.shutdown();
            }
            jobs.stop();
        }
    }
}
