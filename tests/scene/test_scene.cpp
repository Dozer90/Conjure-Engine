#include "conjure/testing/doctest.h"
#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/approx.h"
#include "conjure/testing/log_capture.h"
#include "scene_test_fixture.h"

#include "conjure/core/json/document.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/entity_archive.h"
#include "conjure/scene/register.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_error.h"
#include "conjure/scene/scene_serializer.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

using namespace conjure;
using namespace scene_test;
using scene::scene_error;

TEST_SUITE("scene/scene") {

    TEST_CASE("scene - create_entity has guid name transform companions and dirty") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const core::string_id playerName("Player");
        const core::result<ecs::entity> created = target.create_entity(playerName);
        CONJURE_REQUIRE_OK(created);
        const ecs::entity player = created.value();
        CHECK(target.contains(player));
        CHECK(fixture.world.has<scene::guid>(player));
        CHECK(fixture.world.has<scene::name>(player));
        CHECK(fixture.world.has<scene::local_transform>(player));
        CHECK(fixture.world.has<scene::world_transform>(player));
        CHECK(fixture.world.has<scene::render_world_transform>(player));
        CHECK(fixture.world.has<scene::transform_state>(player));
        CHECK(fixture.world.has<scene::transform_dirty>(player));
        CHECK_FALSE(fixture.world.has<scene::parent>(player));
        CHECK_FALSE(fixture.world.has<scene::children>(player));
        CHECK(target.name_of(player) == playerName);
        CHECK(target.name_of(player).view() == "Player");
        const std::optional<core::uuid> id = target.guid_of(player);
        REQUIRE(id.has_value());
        CHECK_FALSE(id->is_null());
        CHECK(target.find_guid(*id) == player);
        REQUIRE(target.roots().size() == 1);
        CHECK(target.roots()[0].entity == player);
        CHECK(target.roots()[0].owner == scene::SCENE_ID_AUTHORED);
        CHECK(target.local_transform_of(player).value() == math::transform{});
        CHECK(target.structure_version() == 2);

        // The dirty tag is gone once the pass has run; guids stay unique.
        fixture.step();
        CHECK_FALSE(fixture.world.has<scene::transform_dirty>(player));
        const ecs::entity second = fixture.create("Second");
        CHECK(target.guid_of(second).value() != *id);
        CONJURE_CHECK_ERROR(
          target.create_entity(core::string_id("x"), ecs::entity{77, 77}),
          scene_error::INVALID_ENTITY
        );
    }

    TEST_CASE("scene - create under parent gets depth tag") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child", root);
        const ecs::entity grandChild = fixture.create("grand", child);
        CHECK(fixture.world.has<scene::parent>(child));
        CHECK(fixture.world.has<scene::depth_tag<1>>(child));
        CHECK(fixture.world.has<scene::depth_tag<2>>(grandChild));
        CHECK_FALSE(fixture.world.has<scene::depth_tag<1>>(grandChild));
        CHECK(target.tree().depth_of(grandChild) == 2);
        CHECK(target.tree().parent_of(child) == root);
        CHECK(target.tree().sibling_index(grandChild) == 0);
        CHECK(target.roots().size() == 1);
        CHECK(target.max_depth_in_use() == 2);
        REQUIRE(target.tree().children_of(root).size() == 1);
        CHECK(target.tree().children_of(root)[0] == child);
    }

    TEST_CASE("scene - destroy is deferred until destroy_pending") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child", root);
        target.destroy(root);
        target.destroy(root);
        target.destroy(child);
        // Queuing is O(1): requests are listed as made, the subtree still dies exactly once.
        CHECK(target.pending_destroy_count() == 3);
        CHECK(target.contains(root));
        CHECK(target.contains(child));
        u32 destroyedEvents = 0;
        const ecs::observer_id destroyed = fixture.world.observe_destroyed(
          [&destroyedEvents](ecs::world &, ecs::entity, ecs::command_buffer &) {
              ++destroyedEvents;
          });
        fixture.step();
        fixture.world.unobserve(destroyed);
        CHECK(destroyedEvents == 2);
        CHECK(target.pending_destroy_count() == 0);
        CHECK_FALSE(target.contains(root));
        CHECK_FALSE(target.contains(child));
        CHECK(target.roots().empty());
        CHECK(target.guids().size() == 0);
        // A dead entity is never queued.
        target.destroy(root);
        CHECK(target.pending_destroy_count() == 0);
    }

    TEST_CASE("scene - destroy_now cascades 10k subtree without recursion") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        // A deep chain plus a wide fan: 31 levels, the rest spread under the root's first child.
        std::vector<ecs::entity> chain = {root};
        for (u32 level = 1; level < scene::MAX_HIERARCHY_DEPTH; ++level) {
            chain.push_back(fixture.create("link", chain.back()));
        }
        std::vector<ecs::entity> leaves(10000 - chain.size());
        CONJURE_REQUIRE_OK(target.create_entities(static_cast<u32>(leaves.size()), ecs::NULL_ENTITY, chain[1], leaves));
        CHECK(fixture.world.entity_count() == 10000);
        CHECK(target.guids().size() == 10000);

        const ecs::entity survivor = fixture.create("survivor");
        CONJURE_REQUIRE_OK(target.destroy_now(root));
        CHECK(fixture.world.entity_count() == 1);
        CHECK(target.contains(survivor));
        CHECK(target.guids().size() == 1);
        REQUIRE(target.roots().size() == 1);
        CHECK(target.roots()[0].entity == survivor);
        for (const ecs::entity link : chain) {
            CHECK_FALSE(target.contains(link));
        }
        CONJURE_CHECK_ERROR(target.destroy_now(root), scene_error::INVALID_ENTITY);
    }

    TEST_CASE("scene - destroy unlinks from alive parent") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity a = fixture.create("a", root);
        const ecs::entity b = fixture.create("b", root);
        const ecs::entity c = fixture.create("c", root);
        CONJURE_REQUIRE_OK(target.destroy_now(b));
        REQUIRE(target.tree().children_of(root).size() == 2);
        CHECK(target.tree().children_of(root)[0] == a);
        CHECK(target.tree().children_of(root)[1] == c);
        CHECK(target.tree().sibling_index(c) == 1);

        // A raw world destroy is tolerated: observers unlink it too, without a repair WARN.
        testing::log_capture logs = {};
        CONJURE_REQUIRE_OK(fixture.world.destroy_entity(a));
        REQUIRE(target.tree().children_of(root).size() == 1);
        CHECK(target.tree().children_of(root)[0] == c);
        CHECK(target.tree().sibling_index(c) == 0);
        CHECK(target.pending_repair_count() == 0);
        fixture.step();
        CHECK(logs.count(core::log_level::WARN) == 0);

        // A raw destroy of a root forgets the root.
        CONJURE_REQUIRE_OK(fixture.world.destroy_entity(root));
        CHECK(target.roots().empty());
        CHECK_FALSE(target.contains(c));
    }

    TEST_CASE("scene - raw remove parent repaired same frame with one WARN") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child", root);
        const ecs::entity grand = fixture.create("grand", child);
        testing::log_capture logs = {};
        CONJURE_REQUIRE_OK(fixture.world.remove<scene::parent>(child));
        CHECK(target.tree().children_of(root).empty());
        CHECK(target.pending_repair_count() == 1);
        // Until the TRANSFORM phase the tree is inconsistent; the frame repairs it.
        fixture.step();
        CHECK(target.pending_repair_count() == 0);
        REQUIRE(target.roots().size() == 2);
        CHECK(target.roots()[1].entity == child);
        CHECK(target.tree().depth_of(child) == 0);
        CHECK(target.tree().depth_of(grand) == 1);
        CHECK(fixture.world.has<scene::depth_tag<1>>(grand));
        CHECK(logs.count(core::log_level::WARN) == 1);
        CONJURE_CHECK_OK(target.validate());
        // The next frame is quiet.
        fixture.step();
        CHECK(logs.count(core::log_level::WARN) == 1);
    }

    TEST_CASE("scene - raw add parent repaired") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity other = fixture.create("other");
        const ecs::entity grand = fixture.create("grand", other);
        testing::log_capture logs = {};
        CONJURE_REQUIRE_OK(fixture.world.add<scene::parent>(other, scene::parent{root, 0}));
        CHECK(target.pending_repair_count() == 1);
        fixture.step();
        REQUIRE(target.roots().size() == 1);
        CHECK(target.roots()[0].entity == root);
        REQUIRE(target.tree().children_of(root).size() == 1);
        CHECK(target.tree().children_of(root)[0] == other);
        CHECK(target.tree().depth_of(other) == 1);
        CHECK(target.tree().depth_of(grand) == 2);
        CHECK(logs.count(core::log_level::WARN) == 1);
        CONJURE_CHECK_OK(target.validate());

        // A raw parent pointing at a dead entity makes the child a root again, with a WARN.
        const ecs::entity doomed = fixture.create("doomed");
        const ecs::entity orphan = fixture.create("orphan");
        CONJURE_REQUIRE_OK(fixture.world.add<scene::parent>(orphan, scene::parent{doomed, 0}));
        CONJURE_REQUIRE_OK(target.destroy_now(doomed));
        logs.clear();
        fixture.step();
        CHECK(target.tree().parent_of(orphan).is_null());
        CHECK(target.root_owner(orphan).has_value());
        CHECK(logs.count(core::log_level::WARN) >= 1);
        CONJURE_CHECK_OK(target.validate());
    }

    TEST_CASE("scene - deferred_set_parent repairs silently") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity loose = fixture.create("loose");
        const ecs::entity child = fixture.create("child", root);
        CONJURE_REQUIRE_OK(target.set_local_transform(root, at(5.0f, 0.0f, 0.0f)));
        CONJURE_REQUIRE_OK(target.set_local_transform(loose, at(1.0f, 1.0f, 1.0f)));
        testing::log_capture logs = {};
        ecs::command_buffer commands(fixture.world);
        {
            // Recorded while the world is locked, like a script inside a DOD query scope.
            const ecs::iteration_scope lock = fixture.world.lock_iteration();
            CONJURE_REQUIRE_OK(target.deferred_set_parent(commands, loose, root, true, 0));
            CONJURE_REQUIRE_OK(target.deferred_set_parent(commands, child, ecs::NULL_ENTITY, false));
            CONJURE_CHECK_ERROR(target.deferred_set_parent(commands, root, root), scene_error::SELF_PARENT);
            CONJURE_CHECK_ERROR(target.deferred_set_parent(commands, root, child), scene_error::HIERARCHY_CYCLE);
        }
        CHECK(target.pending_repair_count() == 2);
        CHECK(fixture.world.flush(commands).applied >= 3);
        fixture.step();
        CHECK(target.pending_repair_count() == 0);
        CHECK(target.tree().parent_of(loose) == root);
        CHECK(target.tree().sibling_index(loose) == 0);
        CHECK(target.tree().depth_of(loose) == 1);
        CHECK(target.tree().parent_of(child).is_null());
        CHECK(target.root_owner(child).has_value());
        CONJURE_CHECK_APPROX(target.world_transform_of(loose).value().position, math::vec3(1.0f, 1.0f, 1.0f));
        CHECK(logs.count(core::log_level::WARN) == 0);
        CONJURE_CHECK_OK(target.validate());
    }

    TEST_CASE("regression - deferred_set_parent repair waits for the buffer flush") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity other = fixture.create("other");
        const ecs::entity loose = fixture.create("loose");
        const ecs::entity child = fixture.create("child", root);
        testing::log_capture logs = {};
        ecs::command_buffer commands(fixture.world);
        CONJURE_REQUIRE_OK(target.deferred_set_parent(commands, loose, root, false, 0));
        CONJURE_REQUIRE_OK(target.deferred_set_parent(commands, child, other, false));
        CHECK(target.pending_repair_count() == 2);

        // The buffer was recorded after TRANSFORM: the repair pass runs before the flush and must not
        // repair against the old links (which left the child in two lists and the root with a parent).
        fixture.step();
        CHECK(target.pending_repair_count() == 2);
        CHECK(target.tree().parent_of(loose).is_null());
        CHECK(target.root_owner(loose).has_value());
        CHECK(target.tree().parent_of(child) == root);
        REQUIRE(target.tree().children_of(root).size() == 1);
        CONJURE_CHECK_OK(target.validate());
        CHECK(logs.count(core::log_level::WARN) == 0);

        // Once the buffer lands, the next pass repairs silently.
        CHECK(fixture.world.flush(commands).applied >= 2);
        fixture.step();
        CHECK(target.pending_repair_count() == 0);
        CHECK(target.tree().parent_of(loose) == root);
        CHECK(target.tree().depth_of(loose) == 1);
        CHECK_FALSE(target.root_owner(loose).has_value());
        CHECK(target.tree().parent_of(child) == other);
        CHECK(target.tree().children_of(root).size() == 1);
        CHECK(target.tree().children_of(other).size() == 1);
        CHECK(target.roots().size() == 2);
        CHECK(logs.count(core::log_level::WARN) == 0);
        CONJURE_CHECK_OK(target.validate());

        SUBCASE("a later call for the same child replaces the intent") {
            ecs::command_buffer again(fixture.world);
            CONJURE_REQUIRE_OK(target.deferred_set_parent(again, loose, ecs::NULL_ENTITY, false));
            CONJURE_REQUIRE_OK(target.deferred_set_parent(again, loose, other, false, 0));
            CHECK(target.pending_repair_count() == 1);
            CHECK(fixture.world.flush(again).applied >= 1);
            fixture.step();
            CHECK(target.pending_repair_count() == 0);
            CHECK(target.tree().parent_of(loose) == other);
            CHECK(target.tree().sibling_index(loose) == 0);
            CHECK(target.tree().children_of(root).empty());
            CHECK(target.tree().children_of(other).size() == 2);
            CHECK(logs.count(core::log_level::WARN) == 0);
            // The silent-observer count followed the net change: a raw edit afterwards still warns.
            CONJURE_REQUIRE_OK(fixture.world.remove<scene::parent>(child));
            fixture.step();
            CHECK(logs.count(core::log_level::WARN) == 1);
            CONJURE_CHECK_OK(target.validate());
        }
    }

    TEST_CASE("regression - batched hierarchy repairs retag from live depth tags") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity other = fixture.create("other");
        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child");
        const ecs::entity grand = fixture.create("grand", child);
        testing::log_capture logs = {};

        SUBCASE("two raw parent edits on one chain in one frame") {
            CONJURE_REQUIRE_OK(fixture.world.add<scene::parent>(child, scene::parent{root, 0}));
            CONJURE_REQUIRE_OK(fixture.world.add<scene::parent>(root, scene::parent{other, 0}));
            CHECK(target.pending_repair_count() == 2);
            fixture.step();
            CHECK(logs.count(core::log_level::WARN) == 1);
        }
        SUBCASE("two deferred_set_parent calls on one chain in one buffer") {
            ecs::command_buffer commands(fixture.world);
            CONJURE_REQUIRE_OK(target.deferred_set_parent(commands, child, root, false, 0));
            CONJURE_REQUIRE_OK(target.deferred_set_parent(commands, root, other, false, 0));
            CHECK(fixture.world.flush(commands).applied >= 2);
            fixture.step();
            CHECK(logs.count(core::log_level::WARN) == 0);
        }

        // The second repair retags root's subtree after the first one landed: one depth tag each.
        CHECK(target.pending_repair_count() == 0);
        CHECK(target.tree().parent_of(root) == other);
        CHECK(target.tree().parent_of(child) == root);
        CHECK(target.tree().depth_of(root) == 1);
        CHECK(target.tree().depth_of(child) == 2);
        CHECK(target.tree().depth_of(grand) == 3);
        CHECK(fixture.world.has<scene::depth_tag<2>>(child));
        CHECK_FALSE(fixture.world.has<scene::depth_tag<1>>(child));
        CHECK(fixture.world.has<scene::depth_tag<3>>(grand));
        CHECK_FALSE(fixture.world.has<scene::depth_tag<2>>(grand));
        REQUIRE(target.roots().size() == 1);
        CHECK(target.roots()[0].entity == other);
        CONJURE_CHECK_OK(target.validate());
        // Propagation visits each level once (the dirty tags are gone after two frames).
        fixture.step(2);
        CONJURE_REQUIRE_OK(target.set_local_transform(other, at(1.0f, 0.0f, 0.0f)));
        fixture.step();
        CHECK(fixture.manager.transforms().last_stats().worldRecomputed == 4);
        CONJURE_CHECK_APPROX(translation_of(fixture.world_matrix(grand)), math::vec3(1.0f, 0.0f, 0.0f));
    }

    TEST_CASE("scene - raw add parent cycle is repaired with a WARN") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child", root);
        const ecs::entity grand = fixture.create("grand", child);
        testing::log_capture logs = {};
        CONJURE_REQUIRE_OK(fixture.world.add<scene::parent>(root, scene::parent{grand, 0}));
        CHECK(target.pending_repair_count() == 1);
        fixture.step();
        // The loop is cut at the root, which stays a root of the scene it belonged to.
        CHECK(target.pending_repair_count() == 0);
        CHECK_FALSE(fixture.world.has<scene::parent>(root));
        CHECK(target.root_owner(root) == std::optional<scene::scene_id>(scene::SCENE_ID_AUTHORED));
        CHECK(target.tree().parent_of(child) == root);
        CHECK(target.tree().parent_of(grand) == child);
        CHECK(target.tree().depth_of(grand) == 2);
        CHECK(target.tree().children_of(grand).empty());
        CHECK(logs.count(core::log_level::WARN) >= 1);
        CHECK(logs.contains(core::log_level::WARN, "unusable parent"));
        CONJURE_CHECK_OK(target.validate());
    }

    TEST_CASE("scene - validate ignores raw entities outside the scene") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        // Gameplay-only entities (no scene component) are not expected among the roots.
        const core::result<ecs::entity> bare = fixture.world.create_entity();
        CONJURE_REQUIRE_OK(bare);
        const core::result<ecs::entity> gameplay = fixture.world.create_entity();
        CONJURE_REQUIRE_OK(gameplay);
        CONJURE_REQUIRE_OK(fixture.world.add<test_speed>(gameplay.value(), test_speed{2.0f}));
        CONJURE_CHECK_OK(target.validate());
        CHECK(target.roots().size() == 1);
        // An entity that carries a scene component without being linked in is a violation.
        CONJURE_REQUIRE_OK(fixture.world.add<scene::tag>(gameplay.value(), scene::tag{core::string_id("Loose")}));
        CONJURE_CHECK_ERROR(target.validate(), core::error_code::INVALID_STATE);
        CONJURE_REQUIRE_OK(fixture.world.remove<scene::tag>(gameplay.value()));
        CONJURE_CHECK_OK(target.validate());
        CHECK(target.contains(root));
        fixture.step();
    }

    TEST_CASE("scene - set_sibling_index on a raw entity is INVALID_SIBLING_INDEX") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const core::result<ecs::entity> raw = fixture.world.create_entity();
        CONJURE_REQUIRE_OK(raw);
        // Alive, parentless, but in no sibling list: neither a root nor a child.
        CONJURE_CHECK_ERROR(target.set_sibling_index(raw.value(), 0), scene_error::INVALID_SIBLING_INDEX);
        CONJURE_CHECK_ERROR(target.set_sibling_index(ecs::entity{900, 1}, 0), scene_error::INVALID_ENTITY);
        CONJURE_CHECK_OK(target.set_sibling_index(root, 5));
        CHECK(target.roots().size() == 1);
    }

    TEST_CASE("scene - set_enabled propagates inactive") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child", root);
        const ecs::entity grand = fixture.create("grand", child);
        CHECK(target.is_enabled(root));
        CHECK(target.is_active(grand));
        CONJURE_REQUIRE_OK(target.set_enabled(root, false));
        CHECK_FALSE(target.is_enabled(root));
        CHECK_FALSE(target.is_active(root));
        CHECK(target.is_enabled(child));
        CHECK_FALSE(target.is_active(child));
        CHECK_FALSE(target.is_active(grand));
        CHECK(fixture.world.has<scene::disabled>(root));
        CHECK(fixture.world.has<scene::inactive>(child));
        CHECK(fixture.world.has<scene::inactive>(grand));
        CHECK_FALSE(fixture.world.has<scene::inactive>(root));

        // Disabling a child too keeps it inactive once the root is enabled again.
        CONJURE_REQUIRE_OK(target.set_enabled(child, false));
        CONJURE_REQUIRE_OK(target.set_enabled(root, true));
        CHECK(target.is_active(root));
        CHECK_FALSE(target.is_enabled(child));
        CHECK_FALSE(fixture.world.has<scene::inactive>(child));
        CHECK(fixture.world.has<scene::inactive>(grand));
        CONJURE_REQUIRE_OK(target.set_enabled(child, true));
        CHECK(target.is_active(grand));
        CONJURE_CHECK_OK(target.set_enabled(child, true));
        CONJURE_CHECK_ERROR(target.set_enabled(ecs::entity{900, 1}, false), scene_error::INVALID_ENTITY);
        CHECK_FALSE(target.is_enabled(ecs::entity{900, 1}));
        CHECK_FALSE(target.is_active(ecs::entity{900, 1}));
        // New children of a disabled parent start inactive.
        CONJURE_REQUIRE_OK(target.set_enabled(root, false));
        const ecs::entity late = fixture.create("late", root);
        CHECK(fixture.world.has<scene::inactive>(late));
    }

    TEST_CASE("scene - reparent under disabled parent sets inactive") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity off = fixture.create("off");
        const ecs::entity on = fixture.create("on");
        const ecs::entity mover = fixture.create("mover");
        const ecs::entity leaf = fixture.create("leaf", mover);
        CONJURE_REQUIRE_OK(target.set_enabled(off, false));
        CONJURE_REQUIRE_OK(target.set_parent(mover, off));
        CHECK(fixture.world.has<scene::inactive>(mover));
        CHECK(fixture.world.has<scene::inactive>(leaf));
        CHECK_FALSE(target.is_active(leaf));
        CONJURE_REQUIRE_OK(target.set_parent(mover, on));
        CHECK_FALSE(fixture.world.has<scene::inactive>(mover));
        CHECK_FALSE(fixture.world.has<scene::inactive>(leaf));
        CONJURE_REQUIRE_OK(target.set_parent(mover, off));
        CONJURE_REQUIRE_OK(target.set_parent(mover, ecs::NULL_ENTITY));
        CHECK(target.is_active(leaf));
    }

    TEST_CASE("scene - find_path") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("Root");
        const ecs::entity child = fixture.create("Child", root);
        const ecs::entity leaf = fixture.create("Leaf", child);
        const ecs::entity twin = fixture.create("Leaf", child);
        const ecs::entity other = fixture.create("Other");
        CHECK(target.find_path("Root") == root);
        CHECK(target.find_path("Root/Child") == child);
        CHECK(target.find_path("Root/Child/Leaf") == leaf);
        CHECK(target.find_path("/Root/Child/") == child);
        CHECK(target.find_path("Other") == other);
        CHECK(target.find_path("Child").is_null());
        CHECK(target.find_path("Root/Missing").is_null());
        CHECK(target.find_path("").is_null());
        CHECK(target.find_path("Root/Child/Leaf/Deeper").is_null());
        CONJURE_UNUSED(twin);
    }

    TEST_CASE("scene - find_by_name tag layer") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const core::string_id enemy("Enemy");
        const core::string_id hostile("Hostile");
        const ecs::entity first = fixture.create("Enemy");
        const ecs::entity second = fixture.create("Enemy");
        const ecs::entity third = fixture.create("Friend");
        CONJURE_REQUIRE_OK(target.set_tag(first, hostile));
        CONJURE_REQUIRE_OK(target.set_tag(third, hostile));
        CONJURE_REQUIRE_OK(target.set_layer(second, 7));
        CONJURE_REQUIRE_OK(target.set_layer(third, 7));

        std::vector<ecs::entity> found = {};
        target.find_by_name(enemy, found);
        std::sort(found.begin(), found.end(), [](ecs::entity a, ecs::entity b) { return a.index < b.index; });
        CHECK(found == std::vector<ecs::entity>{first, second});
        found.clear();
        target.find_by_tag(hostile, found);
        CHECK(found.size() == 2);
        CHECK(std::find(found.begin(), found.end(), first) != found.end());
        CHECK(std::find(found.begin(), found.end(), third) != found.end());
        found.clear();
        target.find_by_layer(7, found);
        CHECK(found.size() == 2);
        found.clear();
        target.find_by_layer(3, found);
        CHECK(found.empty());
        found.clear();
        target.find_by_tag(core::string_id("Nobody"), found);
        CHECK(found.empty());
        CHECK(target.tag_of(first) == hostile);
        CHECK(target.tag_of(second).is_null());
        CHECK(target.layer_of(second) == 7);
        CHECK(target.layer_of(first) == 0);
    }

    TEST_CASE("scene - world_bounds and subtree_bounds") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child", root);
        const ecs::entity bare = fixture.create("bare", root);
        CHECK_FALSE(target.world_bounds(root).has_value());
        CHECK_FALSE(target.subtree_bounds(root).has_value());

        CONJURE_REQUIRE_OK(target.set_local_transform(root, at(10.0f, 0.0f, 0.0f)));
        CONJURE_REQUIRE_OK(target.set_local_transform(child, at(0.0f, 5.0f, 0.0f)));
        scene::local_bounds unit = {};
        unit.box = math::aabb::from_center_extents(math::vec3(0.0f), math::vec3(1.0f));
        CONJURE_REQUIRE_OK(fixture.world.add<scene::local_bounds>(root, unit));
        CONJURE_REQUIRE_OK(fixture.world.add<scene::local_bounds>(child, unit));

        const std::optional<math::aabb> rootBounds = target.world_bounds(root);
        REQUIRE(rootBounds.has_value());
        CONJURE_CHECK_APPROX(rootBounds->min, math::vec3(9.0f, -1.0f, -1.0f));
        CONJURE_CHECK_APPROX(rootBounds->max, math::vec3(11.0f, 1.0f, 1.0f));
        const std::optional<math::aabb> childBounds = target.world_bounds(child);
        REQUIRE(childBounds.has_value());
        CONJURE_CHECK_APPROX(childBounds->center(), math::vec3(10.0f, 5.0f, 0.0f));
        const std::optional<math::aabb> whole = target.subtree_bounds(root);
        REQUIRE(whole.has_value());
        CONJURE_CHECK_APPROX(whole->min, math::vec3(9.0f, -1.0f, -1.0f));
        CONJURE_CHECK_APPROX(whole->max, math::vec3(11.0f, 6.0f, 1.0f));
        CHECK_FALSE(target.world_bounds(bare).has_value());
        CHECK_FALSE(target.world_bounds(ecs::entity{900, 1}).has_value());
        CHECK_FALSE(target.subtree_bounds(ecs::entity{900, 1}).has_value());

        // A rotated parent gives a conservative box.
        scene::local_transform rotated = at(10.0f, 0.0f, 0.0f);
        rotated.rotation = glm::angleAxis(math::QUARTER_PI, math::vec3(0.0f, 1.0f, 0.0f));
        CONJURE_REQUIRE_OK(target.set_local_transform(root, rotated));
        const std::optional<math::aabb> turned = target.world_bounds(root);
        REQUIRE(turned.has_value());
        CHECK(turned->extents().x > 1.0f);
    }

    TEST_CASE("scene - structure_version increments once per operation") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        u32 emitted = 0;
        const core::event_subscription subscription = target.on_structure_changed().subscribe([&emitted]() {
            ++emitted;
        });
        const u64 start = target.structure_version();
        const ecs::entity root = fixture.create("root");
        CHECK(target.structure_version() == start + 1);
        const ecs::entity child = fixture.create("child", root);
        CHECK(target.structure_version() == start + 2);
        CONJURE_REQUIRE_OK(target.set_parent(child, ecs::NULL_ENTITY));
        CHECK(target.structure_version() == start + 3);
        CONJURE_REQUIRE_OK(target.set_sibling_index(child, 0));
        CHECK(target.structure_version() == start + 4);
        CONJURE_REQUIRE_OK(target.destroy_now(child));
        CHECK(target.structure_version() == start + 5);
        CHECK(emitted == 5);
        // Failures and value writes do not count.
        CONJURE_CHECK_ERROR(target.set_parent(root, root), scene_error::SELF_PARENT);
        CONJURE_REQUIRE_OK(target.set_local_transform(root, at(1.0f, 0.0f, 0.0f)));
        CONJURE_REQUIRE_OK(target.set_name(root, core::string_id("renamed")));
        CHECK(target.structure_version() == start + 5);
        CHECK(emitted == 5);
        // Frames without hierarchy changes leave the version alone.
        fixture.step(3);
        CHECK(target.structure_version() == start + 5);
    }

    TEST_CASE("scene - clear empties roots and index") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child", root);
        const ecs::entity other = fixture.create("other");
        target.destroy(other);
        target.set_name("level");
        core::json::document settings = core::json::document::object();
        settings.set("gravity", -9.81);
        target.set_settings(settings);
        CHECK(target.max_depth_in_use() == 1);
        // A raw entity outside the tree disappears too.
        const core::result<ecs::entity> raw = fixture.world.create_entity();
        CONJURE_REQUIRE_OK(raw);

        target.clear();
        CHECK(target.roots().empty());
        CHECK(target.guids().size() == 0);
        CHECK(target.pending_destroy_count() == 0);
        CHECK(target.max_depth_in_use() == 0);
        CHECK(fixture.world.entity_count() == 0);
        CHECK_FALSE(target.contains(root));
        CHECK_FALSE(target.contains(child));
        CHECK_FALSE(target.contains(raw.value()));
        CHECK(target.name() == "level");
        CHECK(target.settings().root()["gravity"].as_f64() == doctest::Approx(-9.81));

        // The scene keeps working afterwards.
        const ecs::entity fresh = fixture.create("fresh");
        CHECK(target.roots().size() == 1);
        CHECK(target.find_guid(target.guid_of(fresh).value()) == fresh);
        fixture.step();
    }

    TEST_CASE("scene - create_entities 100 from template") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity holder = fixture.create("holder");
        const ecs::entity templateRoot = fixture.create("template");
        const ecs::entity templateChild = fixture.create("part", templateRoot);
        CONJURE_REQUIRE_OK(target.set_local_transform(templateRoot, at(1.0f, 2.0f, 3.0f)));
        CONJURE_REQUIRE_OK(target.set_tag(templateChild, core::string_id("part")));
        CONJURE_REQUIRE_OK(fixture.world.add<test_entity_holder>(templateChild, test_entity_holder{templateRoot, 4}));

        std::vector<ecs::entity> roots(100);
        const core::result<scene::instantiate_report> report =
          target.create_entities(100, templateRoot, holder, roots);
        CONJURE_REQUIRE_OK(report);
        CHECK(report.value().entityCount == 200);
        CHECK(report.value().entities.size() == 200);
        CHECK(target.tree().children_of(holder).size() == 100);
        CHECK(fixture.world.entity_count() == 203);
        CHECK(target.guids().size() == 203);
        for (u32 index = 0; index < 100; ++index) {
            CAPTURE(index);
            const ecs::entity copy = roots[index];
            REQUIRE(target.contains(copy));
            CHECK(target.tree().parent_of(copy) == holder);
            CHECK(target.tree().sibling_index(copy) == index);
            CHECK(target.name_of(copy) == target.name_of(templateRoot));
            CHECK(target.guid_of(copy).value() != target.guid_of(templateRoot).value());
            REQUIRE(target.tree().children_of(copy).size() == 1);
            const ecs::entity part = target.tree().children_of(copy)[0];
            CHECK(target.tree().depth_of(part) == 2);
            // The internal reference was remapped to the copy's own root.
            const test_entity_holder *link = fixture.world.get<test_entity_holder>(part);
            REQUIRE(link != nullptr);
            CHECK(link->other == copy);
            CHECK(link->weight == 4);
            CONJURE_CHECK_APPROX(target.local_transform_of(copy).value().position, math::vec3(1.0f, 2.0f, 3.0f));
        }

        // A null template creates plain entities; a short output span is refused.
        std::vector<ecs::entity> plain(5);
        CONJURE_REQUIRE_OK(target.create_entities(5, ecs::NULL_ENTITY, ecs::NULL_ENTITY, plain));
        for (const ecs::entity entity : plain) {
            CHECK(target.contains(entity));
            CHECK(target.root_owner(entity).has_value());
            CHECK(fixture.world.has<scene::local_transform>(entity));
        }
        std::vector<ecs::entity> tooFew(2);
        CONJURE_CHECK_ERROR(
          target.create_entities(5, templateRoot, holder, tooFew),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(target.create_entities(1, ecs::entity{900, 1}, holder, plain), scene_error::INVALID_ENTITY);
        fixture.step();
    }

    TEST_CASE("scene - duplicate_subtree lands next to the source") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity source = fixture.create("source", root);
        const ecs::entity leaf = fixture.create("leaf", source);
        const ecs::entity sibling = fixture.create("sibling", root);
        CONJURE_REQUIRE_OK(target.set_local_transform(leaf, at(0.0f, 1.0f, 0.0f)));
        const core::result<ecs::entity> copy = target.duplicate_subtree(source, ecs::NULL_ENTITY, 1);
        CONJURE_REQUIRE_OK(copy);
        CHECK(target.tree().parent_of(copy.value()) == root);
        REQUIRE(target.tree().children_of(root).size() == 3);
        CHECK(target.tree().children_of(root)[0] == source);
        CHECK(target.tree().children_of(root)[1] == copy.value());
        CHECK(target.tree().children_of(root)[2] == sibling);
        REQUIRE(target.tree().children_of(copy.value()).size() == 1);
        const ecs::entity copiedLeaf = target.tree().children_of(copy.value())[0];
        CHECK(target.name_of(copiedLeaf) == target.name_of(leaf));
        CHECK(target.guid_of(copiedLeaf).value() != target.guid_of(leaf).value());
        CONJURE_CHECK_APPROX(target.local_transform_of(copiedLeaf).value().position, math::vec3(0.0f, 1.0f, 0.0f));

        // Duplicating a root keeps its owner and becomes a root.
        const core::result<ecs::entity> rootCopy = target.duplicate_subtree(root);
        CONJURE_REQUIRE_OK(rootCopy);
        CHECK(target.root_owner(rootCopy.value()) == scene::SCENE_ID_AUTHORED);
        CHECK(target.tree().subtree_size(rootCopy.value()) == target.tree().subtree_size(root));
        CONJURE_CHECK_ERROR(target.duplicate_subtree(ecs::entity{900, 1}), scene_error::INVALID_ENTITY);
        fixture.step();
    }

    TEST_CASE("scene - settings and name are stored") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        u32 changes = 0;
        const core::event_subscription subscription = target.on_settings_changed().subscribe([&changes]() {
            ++changes;
        });
        CHECK(target.name().empty());
        CHECK(target.settings().root().is_object());
        target.set_name("main");
        CHECK(target.name() == "main");
        core::json::document settings = core::json::document::object();
        settings.set("ambient", 0.25);
        target.set_settings(settings);
        CHECK(target.settings().root()["ambient"].as_f64() == doctest::Approx(0.25));
        CHECK(changes == 1);
        // A non-object root is replaced by an empty object.
        target.set_settings(core::json::document::array());
        CHECK(target.settings().root().is_object());
        CHECK(target.settings().root().size() == 0);
        CHECK(changes == 2);
    }

    TEST_CASE("scene - guid helpers") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity entity = fixture.create("entity");
        const core::uuid original = target.guid_of(entity).value();
        CHECK(target.ensure_guid(entity).value() == original);
        CHECK(target.resolve_guid(original).value() == entity);
        CONJURE_REQUIRE_OK(target.set_guid(entity, test_uuid(1)));
        CHECK(target.guid_of(entity).value() == test_uuid(1));
        CHECK(target.find_guid(original).is_null());
        CHECK(target.find_guid(test_uuid(1)) == entity);
        CONJURE_CHECK_ERROR(target.resolve_guid(original), scene_error::GUID_NOT_FOUND);
        CONJURE_CHECK_ERROR(target.resolve_guid(core::uuid()), scene_error::GUID_NOT_FOUND);

        const core::result<ecs::entity> raw = fixture.world.create_entity();
        CONJURE_REQUIRE_OK(raw);
        CHECK_FALSE(target.guid_of(raw.value()).has_value());
        CONJURE_CHECK_ERROR(target.set_guid(raw.value(), test_uuid(1)), scene_error::DUPLICATE_GUID);
        const core::result<core::uuid> ensured = target.ensure_guid(raw.value());
        CONJURE_REQUIRE_OK(ensured);
        CHECK(target.find_guid(ensured.value()) == raw.value());
        CONJURE_REQUIRE_OK(target.set_guid(raw.value(), test_uuid(2)));
        CHECK(target.find_guid(ensured.value()).is_null());
        CHECK(target.find_guid(test_uuid(2)) == raw.value());
        CONJURE_CHECK_ERROR(target.set_guid(raw.value(), core::uuid()), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(target.ensure_guid(ecs::entity{900, 1}), scene_error::INVALID_ENTITY);
        CONJURE_REQUIRE_OK(fixture.world.destroy_entity(raw.value()));
        CHECK(target.find_guid(test_uuid(2)).is_null());
        CONJURE_CHECK_ERROR(target.resolve_guid(test_uuid(2)), scene_error::GUID_NOT_FOUND);
    }

    TEST_CASE("scene - create_entities without a template is one structural operation") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        u32 emitted = 0;
        const core::event_subscription subscription = target.on_structure_changed().subscribe([&emitted]() {
            ++emitted;
        });
        const u64 start = target.structure_version();
        std::vector<ecs::entity> out(100);
        CONJURE_REQUIRE_OK(target.create_entities(100, ecs::NULL_ENTITY, ecs::NULL_ENTITY, out));
        CHECK(target.structure_version() == start + 1);
        CHECK(emitted == 1);
        CHECK(target.roots().size() == 100);
        // A count of zero creates nothing and announces nothing.
        CONJURE_REQUIRE_OK(target.create_entities(0, ecs::NULL_ENTITY, ecs::NULL_ENTITY, out));
        CHECK(target.structure_version() == start + 1);
        CHECK(emitted == 1);
    }

    TEST_CASE("scene - create_entities without a template leaves nothing behind on failure") {
        // One slab of one chunk: the chunk fills up long before the thousandth entity.
        ecs::component_registry registry = {};
        CONJURE_REQUIRE_OK(scene::register_components(registry));
        ecs::world_desc desc = {};
        desc.chunksPerSlab = 1;
        desc.maxChunkSlabs = 1;
        ecs::world world(registry, desc);
        scene::scene target(world, nullptr);
        u32 emitted = 0;
        const core::event_subscription subscription = target.on_structure_changed().subscribe([&emitted]() {
            ++emitted;
        });
        const u64 start = target.structure_version();
        std::vector<ecs::entity> out(1000);
        CONJURE_CHECK_ERROR(
          target.create_entities(1000, ecs::NULL_ENTITY, ecs::NULL_ENTITY, out),
          ecs::ecs_error::OUT_OF_MEMORY
        );
        // The call created and destroyed again with no net change, so it announces nothing.
        CHECK(target.structure_version() == start);
        CHECK(emitted == 0);
        CHECK(world.entity_count() == 0);
        CHECK(target.roots().empty());
        CHECK(target.guids().size() == 0);
        CHECK(std::all_of(out.begin(), out.end(), [](ecs::entity entity) { return entity.is_null(); }));
        CONJURE_CHECK_OK(target.validate());
    }

    TEST_CASE("regression - transformless entities never keep transform_dirty") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");

        // set_parent of a raw entity.
        const core::result<ecs::entity> raw = fixture.world.create_entity();
        CONJURE_REQUIRE_OK(raw);
        CONJURE_REQUIRE_OK(target.set_parent(raw.value(), root));
        CHECK_FALSE(fixture.world.has<scene::transform_dirty>(raw.value()));

        // deferred_set_parent through a flushed buffer.
        const core::result<ecs::entity> deferred = fixture.world.create_entity();
        CONJURE_REQUIRE_OK(deferred);
        ecs::command_buffer commands(fixture.world);
        CONJURE_REQUIRE_OK(target.deferred_set_parent(commands, deferred.value(), root, false));
        CHECK(fixture.world.flush(commands).applied >= 1);

        // A loaded entry without components.
        const std::string text = "{\"format\":\"conjure.scene\",\"version\":1,\"name\":\"plain\",\"settings\":{},"
          "\"roots\":[],\"archive\":{\"entities\":[{\"components\":{}}]}}";
        const core::result<core::json::document> parsed = core::json::document::parse(text);
        REQUIRE(parsed.has_value());
        const core::result<scene::scene_document> document =
          scene::scene_serializer::parse_document(parsed.value().root(), fixture.registry);
        CONJURE_REQUIRE_OK(document);
        const core::result<scene::instantiate_report> loaded = scene::scene_serializer::load_scene(
          target,
          document.value(),
          scene::SCENE_ID_AUTHORED,
          scene::guid_policy::KEEP
        );
        CONJURE_REQUIRE_OK(loaded);
        REQUIRE(loaded.value().entities.size() == 1);
        const ecs::entity plain = loaded.value().entities[0];
        CHECK_FALSE(fixture.world.has<scene::transform_dirty>(plain));

        // The pass never visits them, so nothing would ever take a tag away again.
        fixture.step();
        CHECK(target.tree().parent_of(deferred.value()) == root);
        for (const ecs::entity entity : {raw.value(), deferred.value(), plain}) {
            CAPTURE(entity.index);
            CHECK_FALSE(fixture.world.has<scene::transform_dirty>(entity));
        }

        // Positive control: a transform brings the tag and the next pass removes it.
        CONJURE_REQUIRE_OK(target.add_transform(raw.value()));
        CHECK(fixture.world.has<scene::transform_dirty>(raw.value()));
        fixture.step();
        CHECK_FALSE(fixture.world.has<scene::transform_dirty>(raw.value()));
        CONJURE_CHECK_OK(target.validate());
    }

    TEST_CASE("regression - a waiting deferred repair keeps the repair queue's storage") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity loose = fixture.create("loose");
        ecs::command_buffer commands(fixture.world);
        CONJURE_REQUIRE_OK(target.deferred_set_parent(commands, loose, root, false, 0));

        // The buffer stays unflushed, so every pass puts the waiting entry back; after two warm-up passes
        // that costs no allocation.
        target.apply_hierarchy_changes();
        target.apply_hierarchy_changes();
        CONJURE_CHECK_NO_ALLOCATIONS(target.apply_hierarchy_changes());
        CHECK(target.pending_repair_count() == 1);

        CHECK(fixture.world.flush(commands).applied >= 1);
        fixture.step();
        CHECK(target.pending_repair_count() == 0);
        CHECK(target.tree().parent_of(loose) == root);
        CONJURE_CHECK_OK(target.validate());
    }

    TEST_CASE("scene - instantiate_archive attaches a kept external parent") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity outer = fixture.create("P");
        const ecs::entity first = fixture.create("A", outer);
        const ecs::entity branch = fixture.create("B", outer);
        const ecs::entity leaf = fixture.create("C", branch);
        CONJURE_REQUIRE_OK(target.set_enabled(outer, false));
        CONJURE_UNUSED(leaf);

        // An undo payload: the branch captured with its link to the outside parent.
        ecs::archive_options options = {};
        options.keepExternalReferences = true;
        core::result<ecs::entity_archive> captured = scene::scene_serializer::capture_subtree(target, branch, options);
        CONJURE_REQUIRE_OK(captured);
        CHECK_FALSE(captured.value().component_bytes(0, target.ids().parent).empty());
        CONJURE_REQUIRE_OK(target.destroy_now(branch));

        // A null options.parent: the archive root goes back under its live external parent.
        ecs::entity restored = {};
        const core::result<scene::instantiate_report> report = target.instantiate_archive(
          std::make_shared<const ecs::entity_archive>(std::move(captured).value()),
          1,
          scene::instantiate_options{},
          std::span<ecs::entity>(&restored, 1)
        );
        CONJURE_REQUIRE_OK(report);
        REQUIRE(report.value().entities.size() == 2);
        const ecs::entity restoredLeaf = report.value().entities[1];
        CHECK(target.tree().parent_of(restored) == outer);
        CHECK(target.tree().sibling_index(restored) == 1);
        REQUIRE(target.tree().children_of(outer).size() == 2);
        CHECK(target.tree().children_of(outer)[0] == first);
        CHECK(target.tree().children_of(outer)[1] == restored);
        CHECK_FALSE(target.root_owner(restored).has_value());
        CHECK(fixture.world.has_raw(restored, target.ids().depth[1]));
        CHECK(fixture.world.has_raw(restoredLeaf, target.ids().depth[2]));
        CHECK(fixture.world.has<scene::inactive>(restored));
        CHECK(fixture.world.has<scene::inactive>(restoredLeaf));
        CHECK(target.tree().parent_of(restoredLeaf) == restored);
        CONJURE_CHECK_OK(target.validate());
    }

    TEST_CASE("scene - set_local_transform takes the math::transform local_transform_of returns") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child", root);
        CONJURE_REQUIRE_OK(target.set_local_transform(root, at(10.0f, 0.0f, 0.0f)));
        fixture.step();

        // The getter's result goes back into the setter, edited or as it is, without from_transform.
        math::transform moved = target.local_transform_of(child).value();
        moved.position = math::vec3(1.0f, 2.0f, 3.0f);
        moved.scale = math::vec3(2.0f);
        CONJURE_REQUIRE_OK(target.set_local_transform(child, moved));
        REQUIRE(fixture.world.has<scene::local_transform>(child));
        CHECK(*fixture.world.get<scene::local_transform>(child) == scene::local_transform::from_transform(moved));
        CONJURE_REQUIRE_OK(target.set_local_transform(child, target.local_transform_of(child).value()));
        CHECK(*fixture.world.get<scene::local_transform>(child) == scene::local_transform::from_transform(moved));

        // The write stamps the chunk like the local_transform overload, so the next pass picks it up.
        fixture.step();
        CONJURE_CHECK_APPROX(translation_of(fixture.world_matrix(child)), math::vec3(11.0f, 2.0f, 3.0f));

        CONJURE_CHECK_ERROR(target.set_local_transform(ecs::entity{900, 1}, moved), scene_error::INVALID_ENTITY);
        CONJURE_CHECK_OK(target.validate());
    }
}
