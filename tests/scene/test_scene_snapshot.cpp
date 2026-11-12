#include "conjure/testing/doctest.h"
#include "conjure/testing/approx.h"
#include "scene_test_fixture.h"

#include "conjure/core/json/document.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_error.h"
#include "conjure/scene/scene_snapshot.h"

#include <cstring>
#include <utility>
#include <vector>

using namespace conjure;
using namespace scene_test;
using scene::scene_error;

TEST_SUITE("scene/scene_snapshot") {

    TEST_CASE("scene_snapshot - capture restore preserves guids names hierarchy transforms") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        target.set_name("play");
        core::json::document settings = core::json::document::object();
        settings.set("ambient", 0.3);
        target.set_settings(settings);
        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child", root);
        const ecs::entity leaf = fixture.create("leaf", child);
        const ecs::entity other = fixture.create("other");
        CONJURE_REQUIRE_OK(target.set_local_transform(child, at(1.0f, 2.0f, 3.0f)));
        CONJURE_REQUIRE_OK(target.set_tag(leaf, core::string_id("Leafy")));
        CONJURE_REQUIRE_OK(target.set_enabled(other, false));
        CONJURE_REQUIRE_OK(fixture.world.add<test_entity_holder>(leaf, test_entity_holder{root, 5}));
        const core::uuid rootGuid = target.guid_of(root).value();
        const core::uuid leafGuid = target.guid_of(leaf).value();
        fixture.step();

        const core::result<scene::scene_snapshot> captured = scene::capture(target);
        CONJURE_REQUIRE_OK(captured);
        const scene::scene_snapshot &snapshot = captured.value();
        CHECK(snapshot.entity_count() == 4);
        CHECK(snapshot.name == "play");
        CHECK(snapshot.rootEntries == std::vector<u32>{0, 3});
        CHECK(snapshot.rootOwners.size() == 2);
        CHECK(snapshot.structureVersion == target.structure_version());
        // TRANSIENT columns are not captured.
        CHECK(snapshot.archive.component_bytes(0, target.ids().worldTransform).empty());
        CHECK_FALSE(snapshot.archive.component_bytes(1, target.ids().parent).empty());

        CONJURE_REQUIRE_OK(scene::restore(target, snapshot));
        CHECK(fixture.world.entity_count() == 4);
        REQUIRE(target.roots().size() == 2);
        const ecs::entity newRoot = target.find_guid(rootGuid);
        const ecs::entity newLeaf = target.find_guid(leafGuid);
        REQUIRE(target.contains(newRoot));
        REQUIRE(target.contains(newLeaf));
        CHECK(target.roots()[0].entity == newRoot);
        CHECK(target.name_of(newRoot).view() == "root");
        CHECK(target.tree().depth_of(newLeaf) == 2);
        CHECK(target.tree().parent_of(target.tree().parent_of(newLeaf)) == newRoot);
        CHECK(target.tag_of(newLeaf).view() == "Leafy");
        CHECK(fixture.world.get<test_entity_holder>(newLeaf)->other == newRoot);
        CONJURE_CHECK_APPROX(target.world_transform_of(newLeaf).value().position, math::vec3(1.0f, 2.0f, 3.0f));
        CHECK_FALSE(target.is_enabled(target.find_path("other")));
        CHECK(target.name() == "play");
        CHECK(target.settings().root()["ambient"].as_f64() == doctest::Approx(0.3));
        fixture.step();
        CONJURE_CHECK_APPROX(translation_of(fixture.world_matrix(newLeaf)), math::vec3(1.0f, 2.0f, 3.0f));
    }

    TEST_CASE("scene_snapshot - restore after mutation reverts") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child", root);
        const core::uuid childGuid = target.guid_of(child).value();
        const core::result<scene::scene_snapshot> captured = scene::capture(target);
        CONJURE_REQUIRE_OK(captured);

        // "Play": move, rename, add, destroy.
        CONJURE_REQUIRE_OK(target.set_local_transform(child, at(9.0f, 9.0f, 9.0f)));
        CONJURE_REQUIRE_OK(target.set_name(child, core::string_id("renamed")));
        const ecs::entity spawned = fixture.create("spawned", child);
        CONJURE_REQUIRE_OK(target.set_parent(child, ecs::NULL_ENTITY));
        CONJURE_REQUIRE_OK(target.destroy_now(root));
        fixture.step();
        CHECK(fixture.world.entity_count() == 2);

        // "Stop".
        CONJURE_REQUIRE_OK(scene::restore(target, captured.value()));
        CHECK(fixture.world.entity_count() == 2);
        CHECK_FALSE(target.contains(spawned));
        const ecs::entity restored = target.find_guid(childGuid);
        REQUIRE(target.contains(restored));
        CHECK(target.name_of(restored).view() == "child");
        CHECK(target.local_transform_of(restored).value().position == math::vec3(0.0f));
        CHECK(target.tree().depth_of(restored) == 1);
        CHECK(target.roots().size() == 1);
        CHECK(target.name_of(target.roots()[0].entity).view() == "root");
        CHECK(target.find_path("spawned").is_null());
    }

    TEST_CASE("scene_snapshot - 10k capture plus restore round trips") {
        // The 50 ms budget of 13_scene.md 3.14 is a benchmark matter (tests/benchmarks/bench_scene.cpp);
        // here only the outcome of a 10k round trip is checked.
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        std::vector<ecs::entity> leaves(9999);
        CONJURE_REQUIRE_OK(target.create_entities(9999, ecs::NULL_ENTITY, root, leaves));
        for (u32 index = 0; index < leaves.size(); index += 7) {
            CONJURE_REQUIRE_OK(target.set_local_transform(leaves[index], at(static_cast<f32>(index), 0.0f, 0.0f)));
        }
        fixture.step();

        const core::result<scene::scene_snapshot> captured = scene::capture(target);
        CONJURE_REQUIRE_OK(captured);
        CONJURE_REQUIRE_OK(scene::restore(target, captured.value()));
        CHECK(captured.value().entity_count() == 10000);
        CHECK(fixture.world.entity_count() == 10000);
        CHECK(target.guids().size() == 10000);
        REQUIRE(target.roots().size() == 1);
        const ecs::entity newRoot = target.roots()[0].entity;
        REQUIRE(target.tree().children_of(newRoot).size() == 9999);
        CONJURE_CHECK_APPROX(target.local_transform_of(target.tree().children_of(newRoot)[7]).value().position,
          math::vec3(7.0f, 0.0f, 0.0f));
        fixture.step();
    }

    TEST_CASE("scene_snapshot - move construction and assignment keep the contents") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        target.set_name("moved");
        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child", root);
        const core::uuid childGuid = target.guid_of(child).value();
        core::result<scene::scene_snapshot> captured = scene::capture(target);
        CONJURE_REQUIRE_OK(captured);
        const u64 version = captured.value().structureVersion;

        scene::scene_snapshot moved(std::move(captured).value());
        CHECK(moved.entity_count() == 2);
        CHECK(moved.name == "moved");
        CHECK(moved.rootEntries == std::vector<u32>{0});
        CHECK(moved.structureVersion == version);

        scene::scene_snapshot assigned(fixture.registry);
        CHECK(assigned.entity_count() == 0);
        assigned = std::move(moved);
        CHECK(assigned.entity_count() == 2);
        CHECK(assigned.rootOwners == std::vector<scene::scene_id>{scene::SCENE_ID_AUTHORED});

        // The moved snapshot restores like the original.
        CONJURE_REQUIRE_OK(target.destroy_now(root));
        CONJURE_REQUIRE_OK(scene::restore(target, assigned));
        CHECK(fixture.world.entity_count() == 2);
        CHECK(target.contains(target.find_guid(childGuid)));
        CHECK(target.name() == "moved");
    }

    TEST_CASE("scene_snapshot - restore failure leaves empty scene") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const core::result<scene::scene_snapshot> captured = scene::capture(target);
        CONJURE_REQUIRE_OK(captured);
        // A snapshot whose archive holds a duplicate guid cannot be instantiated with KEEP.
        scene::scene_snapshot broken(fixture.registry);
        ecs::archive_entry first = {};
        ecs::archive_entry second = {};
        for (ecs::archive_entry *entry : {&first, &second}) {
            ecs::archive_component identity = {};
            identity.id = target.ids().guid;
            identity.bytes.resize(sizeof(scene::guid));
            const scene::guid value{test_uuid(1)};
            std::memcpy(identity.bytes.data(), &value, sizeof(value));
            entry->mask.set(identity.id);
            entry->components.push_back(std::move(identity));
        }
        broken.archive.push_entry(std::move(first));
        broken.archive.push_entry(std::move(second));
        broken.rootEntries = {0, 1};
        broken.rootOwners = {scene::SCENE_ID_AUTHORED, scene::SCENE_ID_AUTHORED};
        broken.name = "broken";
        CONJURE_CHECK_ERROR(scene::restore(target, broken), scene_error::DUPLICATE_GUID);
        CHECK(fixture.world.entity_count() == 0);
        CHECK(target.roots().empty());
        CHECK(target.guids().size() == 0);
        CHECK_FALSE(target.contains(root));
        CHECK(target.name() == "broken");
        // The good snapshot still restores afterwards.
        CONJURE_REQUIRE_OK(scene::restore(target, captured.value()));
        CHECK(fixture.world.entity_count() == 1);
    }

    TEST_CASE("scene_snapshot - restore advances structure_version past the captured value") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child", root);
        const core::result<scene::scene_snapshot> captured = scene::capture(target);
        CONJURE_REQUIRE_OK(captured);
        CHECK(captured.value().structureVersion == target.structure_version());

        // The field is informational: the counter only moves forward, restore included.
        CONJURE_REQUIRE_OK(scene::restore(target, captured.value()));
        CHECK(target.structure_version() > captured.value().structureVersion);
        CHECK(fixture.world.entity_count() == 2);
        CONJURE_UNUSED(child);
    }

    TEST_CASE("scene_snapshot - roots owners preserved") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity a = fixture.create("a");
        const ecs::entity b = fixture.create("b");
        const ecs::entity c = fixture.create("c");
        const scene::scene_id ownerB = scene::scene_id{1, 7};
        const scene::scene_id ownerC = scene::scene_id{2, 7};
        CONJURE_REQUIRE_OK(target.set_root_owner(b, ownerB));
        CONJURE_REQUIRE_OK(target.set_root_owner(c, ownerC));
        CONJURE_REQUIRE_OK(target.set_root_order(std::vector<ecs::entity>{c, a, b}));
        const core::result<scene::scene_snapshot> captured = scene::capture(target);
        CONJURE_REQUIRE_OK(captured);
        CHECK(captured.value().rootOwners == std::vector<scene::scene_id>{ownerC, scene::SCENE_ID_AUTHORED, ownerB});

        CONJURE_REQUIRE_OK(scene::restore(target, captured.value()));
        REQUIRE(target.roots().size() == 3);
        CHECK(target.name_of(target.roots()[0].entity).view() == "c");
        CHECK(target.roots()[0].owner == ownerC);
        CHECK(target.name_of(target.roots()[1].entity).view() == "a");
        CHECK(target.roots()[1].owner == scene::SCENE_ID_AUTHORED);
        CHECK(target.name_of(target.roots()[2].entity).view() == "b");
        CHECK(target.roots()[2].owner == ownerB);
        // An empty snapshot restores to an empty scene.
        target.clear();
        const core::result<scene::scene_snapshot> empty = scene::capture(target);
        CONJURE_REQUIRE_OK(empty);
        CHECK(empty.value().entity_count() == 0);
        const ecs::entity late = fixture.create("late");
        CONJURE_REQUIRE_OK(scene::restore(target, empty.value()));
        CHECK_FALSE(target.contains(late));
        CHECK(target.roots().empty());
    }
}
