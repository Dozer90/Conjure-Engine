#include "conjure/testing/doctest.h"
#include "conjure/testing/approx.h"
#include "scene_test_fixture.h"

#include "conjure/scene/hierarchy.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_error.h"
#include "conjure/scene/transform_system.h"

#include <vector>

using namespace conjure;
using namespace scene_test;
using scene::scene_error;

namespace {

    /// <summary>
    /// Builds a chain root -> c1 -> ... -> c(length) and returns every entity, the root first.
    /// </summary>
    std::vector<ecs::entity> make_chain(scene_fixture &fixture, u32 length) {
        std::vector<ecs::entity> chain = {};
        chain.push_back(fixture.create("root"));
        for (u32 level = 1; level <= length; ++level) {
            chain.push_back(fixture.create("link", chain.back()));
        }
        return chain;
    }
}

TEST_SUITE("scene/hierarchy") {

    TEST_CASE("hierarchy - set_parent links and unlinks") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const scene::hierarchy &tree = target.tree();
        const ecs::entity parentA = fixture.create("a");
        const ecs::entity parentB = fixture.create("b");
        const ecs::entity child = fixture.create("child");
        CHECK(target.roots().size() == 3);
        CHECK(tree.parent_of(child).is_null());
        CHECK(tree.depth_of(child) == 0);

        CONJURE_REQUIRE_OK(target.set_parent(child, parentA));
        CHECK(tree.parent_of(child) == parentA);
        CHECK(tree.depth_of(child) == 1);
        REQUIRE(tree.children_of(parentA).size() == 1);
        CHECK(tree.children_of(parentA)[0] == child);
        CHECK(tree.sibling_index(child) == 0);
        CHECK(target.roots().size() == 2);
        CHECK(fixture.world.has<scene::transform_dirty>(child));
        CHECK(target.max_depth_in_use() == 1);

        CONJURE_REQUIRE_OK(target.set_parent(child, parentB));
        CHECK(tree.children_of(parentA).empty());
        REQUIRE(tree.children_of(parentB).size() == 1);
        CHECK(tree.parent_of(child) == parentB);
        CHECK(tree.root_of(child) == parentB);
        CHECK(tree.is_ancestor_of(parentB, child));
        CHECK_FALSE(tree.is_ancestor_of(parentA, child));
        CHECK_FALSE(tree.is_ancestor_of(child, child));

        CONJURE_REQUIRE_OK(target.set_parent(child, ecs::NULL_ENTITY));
        CHECK(tree.parent_of(child).is_null());
        CHECK(tree.children_of(parentB).empty());
        CHECK(target.roots().size() == 3);
        CHECK(target.roots().back().entity == child);
        CHECK(tree.depth_of(child) == 0);
        CHECK_FALSE(fixture.world.has<scene::parent>(child));

        // Dead entities answer NULL / empty / 0 and mutators refuse them.
        CONJURE_REQUIRE_OK(target.destroy_now(child));
        CHECK(tree.parent_of(child).is_null());
        CHECK(tree.children_of(child).empty());
        CHECK(tree.depth_of(child) == 0);
        CHECK(tree.subtree_height(child) == 0);
        CHECK(tree.root_of(child).is_null());
        CONJURE_CHECK_ERROR(target.set_parent(child, parentA), scene_error::INVALID_ENTITY);
        CONJURE_CHECK_ERROR(target.set_parent(parentA, child), scene_error::INVALID_ENTITY);
    }

    TEST_CASE("hierarchy - cycle rejected and nothing changes") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const std::vector<ecs::entity> chain = make_chain(fixture, 3);
        const u64 version = target.structure_version();
        CONJURE_CHECK_ERROR(target.set_parent(chain[0], chain[3]), scene_error::HIERARCHY_CYCLE);
        CONJURE_CHECK_ERROR(target.set_parent(chain[1], chain[2]), scene_error::HIERARCHY_CYCLE);
        CHECK(target.structure_version() == version);
        CHECK(target.tree().parent_of(chain[0]).is_null());
        CHECK(target.tree().parent_of(chain[1]) == chain[0]);
        CHECK(target.tree().parent_of(chain[3]) == chain[2]);
        CHECK(target.roots().size() == 1);
        // A sibling move inside the chain is still fine.
        CONJURE_CHECK_OK(target.set_parent(chain[3], chain[1]));
        CHECK(target.tree().children_of(chain[1]).size() == 2);
    }

    TEST_CASE("hierarchy - self parent rejected") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity entity = fixture.create("solo");
        const u64 version = target.structure_version();
        CONJURE_CHECK_ERROR(target.set_parent(entity, entity), scene_error::SELF_PARENT);
        CHECK(target.structure_version() == version);
        CHECK(target.roots().size() == 1);
    }

    TEST_CASE("hierarchy - depth 31 accepted depth 32 is HIERARCHY_TOO_DEEP") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const std::vector<ecs::entity> chain = make_chain(fixture, 31);
        CHECK(chain.size() == 32);
        CHECK(target.tree().depth_of(chain[31]) == 31);
        CHECK(target.max_depth_in_use() == 31);
        CHECK(fixture.world.has<scene::depth_tag<31>>(chain[31]));

        // One more level is refused by both creation and reparenting.
        CONJURE_CHECK_ERROR(target.create_entity(core::string_id("deep"), chain[31]), scene_error::HIERARCHY_TOO_DEEP);
        const ecs::entity loose = fixture.create("loose");
        CONJURE_CHECK_ERROR(target.set_parent(loose, chain[31]), scene_error::HIERARCHY_TOO_DEEP);
        CHECK(target.tree().parent_of(loose).is_null());

        // Moving the whole chain under another root pushes the leaf to depth 32: refused, nothing moves.
        const ecs::entity other = fixture.create("other");
        CONJURE_CHECK_ERROR(target.set_parent(chain[0], other), scene_error::HIERARCHY_TOO_DEEP);
        CHECK(target.tree().parent_of(chain[0]).is_null());
        CHECK(target.tree().subtree_height(chain[0]) == 31);
        // The chain minus its root fits under the other root.
        CONJURE_CHECK_OK(target.set_parent(chain[1], other));
        CHECK(target.tree().depth_of(chain[31]) == 31);
        CHECK(target.tree().subtree_height(chain[0]) == 0);
    }

    TEST_CASE("hierarchy - keepWorld preserves world transform within epsilon") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity parentA = fixture.create("a");
        const ecs::entity parentB = fixture.create("b");
        const ecs::entity child = fixture.create("child");
        scene::local_transform transformA = at(10.0f, 0.0f, 0.0f);
        transformA.rotation = glm::angleAxis(math::HALF_PI, math::vec3(0.0f, 1.0f, 0.0f));
        transformA.scale = math::vec3(2.0f);
        scene::local_transform transformB = at(-3.0f, 4.0f, 1.0f);
        transformB.rotation = glm::angleAxis(0.7f, math::vec3(0.0f, 0.0f, 1.0f));
        CONJURE_REQUIRE_OK(target.set_local_transform(parentA, transformA));
        CONJURE_REQUIRE_OK(target.set_local_transform(parentB, transformB));
        scene::local_transform childLocal = at(1.0f, 2.0f, 3.0f);
        childLocal.rotation = glm::angleAxis(0.3f, math::vec3(1.0f, 0.0f, 0.0f));
        CONJURE_REQUIRE_OK(target.set_local_transform(child, childLocal));

        const math::transform before = target.world_transform_of(child).value();
        CONJURE_REQUIRE_OK(target.set_parent(child, parentA, true));
        CONJURE_CHECK_APPROX(target.world_transform_of(child).value(), before);
        CONJURE_REQUIRE_OK(target.set_parent(child, parentB, true));
        CONJURE_CHECK_APPROX(target.world_transform_of(child).value(), before);
        CONJURE_REQUIRE_OK(target.set_parent(child, ecs::NULL_ENTITY, true));
        CONJURE_CHECK_APPROX(target.world_transform_of(child).value(), before);
        CONJURE_CHECK_APPROX(target.local_transform_of(child).value(), before);

        // The propagated matrix agrees with the fresh walk.
        CONJURE_REQUIRE_OK(target.set_parent(child, parentA, true));
        fixture.step();
        CONJURE_CHECK_APPROX(fixture.world_matrix(child), before.to_matrix());
    }

    TEST_CASE("hierarchy - keepWorld false keeps local") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity parentA = fixture.create("a");
        const ecs::entity child = fixture.create("child");
        CONJURE_REQUIRE_OK(target.set_local_transform(parentA, at(10.0f, 0.0f, 0.0f)));
        CONJURE_REQUIRE_OK(target.set_local_transform(child, at(1.0f, 2.0f, 3.0f)));
        CONJURE_REQUIRE_OK(target.set_parent(child, parentA, false));
        CONJURE_CHECK_APPROX(target.local_transform_of(child).value().position, math::vec3(1.0f, 2.0f, 3.0f));
        CONJURE_CHECK_APPROX(target.world_transform_of(child).value().position, math::vec3(11.0f, 2.0f, 3.0f));
        fixture.step();
        CONJURE_CHECK_APPROX(translation_of(fixture.world_matrix(child)), math::vec3(11.0f, 2.0f, 3.0f));
    }

    TEST_CASE("hierarchy - sibling index insert renumbers") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const scene::hierarchy &tree = target.tree();
        const ecs::entity root = fixture.create("root");
        const ecs::entity first = fixture.create("first", root);
        const ecs::entity second = fixture.create("second", root);
        const ecs::entity third = fixture.create("third", root);
        CHECK(tree.sibling_index(first) == 0);
        CHECK(tree.sibling_index(second) == 1);
        CHECK(tree.sibling_index(third) == 2);

        // Insert at the front: the others shift.
        const ecs::entity front = fixture.create("front");
        CONJURE_REQUIRE_OK(target.set_parent(front, root, true, 0));
        const std::span<const ecs::entity> list = tree.children_of(root);
        REQUIRE(list.size() == 4);
        CHECK(list[0] == front);
        CHECK(list[1] == first);
        CHECK(tree.sibling_index(front) == 0);
        CHECK(tree.sibling_index(first) == 1);
        CHECK(tree.sibling_index(second) == 2);
        CHECK(tree.sibling_index(third) == 3);

        // create_entity with an index inserts there.
        const core::result<ecs::entity> middle = target.create_entity(core::string_id("middle"), root, 2);
        CONJURE_REQUIRE_OK(middle);
        CHECK(tree.children_of(root)[2] == middle.value());
        CHECK(tree.sibling_index(second) == 3);
        CHECK(tree.sibling_index(third) == 4);

        // Removing one renumbers the tail.
        CONJURE_REQUIRE_OK(target.destroy_now(first));
        CHECK(tree.children_of(root).size() == 4);
        CHECK(tree.sibling_index(middle.value()) == 1);
        CHECK(tree.sibling_index(second) == 2);
        CHECK(tree.sibling_index(third) == 3);
    }

    TEST_CASE("hierarchy - set_sibling_index clamps") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const scene::hierarchy &tree = target.tree();
        const ecs::entity root = fixture.create("root");
        const ecs::entity a = fixture.create("a", root);
        const ecs::entity b = fixture.create("b", root);
        const ecs::entity c = fixture.create("c", root);

        CONJURE_REQUIRE_OK(target.set_sibling_index(a, 99));
        CHECK(tree.children_of(root)[2] == a);
        CHECK(tree.sibling_index(a) == 2);
        CHECK(tree.sibling_index(b) == 0);
        CHECK(tree.sibling_index(c) == 1);
        CONJURE_REQUIRE_OK(target.set_sibling_index(c, 0));
        CHECK(tree.children_of(root)[0] == c);
        CHECK(tree.sibling_index(b) == 1);

        // Roots are reordered the same way.
        const ecs::entity otherRoot = fixture.create("other");
        CHECK(target.roots()[0].entity == root);
        CONJURE_REQUIRE_OK(target.set_sibling_index(root, 5));
        CHECK(target.roots()[1].entity == root);
        CHECK(target.roots()[0].entity == otherRoot);
        CONJURE_REQUIRE_OK(target.set_sibling_index(root, 0));
        CHECK(target.roots()[0].entity == root);

        CONJURE_CHECK_ERROR(target.set_sibling_index(ecs::entity{900, 3}, 0), scene_error::INVALID_ENTITY);
        // The parent dies underneath: the child is destroyed with it, so the cascade shows as dead.
        CONJURE_REQUIRE_OK(target.destroy_now(root));
        CONJURE_CHECK_ERROR(target.set_sibling_index(a, 0), scene_error::INVALID_ENTITY);
    }

    TEST_CASE("hierarchy - sibling_index is 0 for roots and dead entities") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const scene::hierarchy &tree = target.tree();
        const ecs::entity first = fixture.create("first");
        const ecs::entity second = fixture.create("second");
        const ecs::entity third = fixture.create("third");
        REQUIRE(target.roots().size() == 3);
        CHECK(target.roots()[2].entity == third);
        // A root's position lives in scene::roots(), not in the hierarchy.
        CHECK(tree.sibling_index(first) == 0);
        CHECK(tree.sibling_index(second) == 0);
        CHECK(tree.sibling_index(third) == 0);
        CHECK(tree.sibling_index(ecs::entity{900, 1}) == 0);
    }

    TEST_CASE("hierarchy - root order after reparent to null") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity first = fixture.create("first");
        const ecs::entity second = fixture.create("second");
        const ecs::entity child = fixture.create("child", first);
        CONJURE_REQUIRE_OK(target.set_parent(child, ecs::NULL_ENTITY, true, 1));
        REQUIRE(target.roots().size() == 3);
        CHECK(target.roots()[0].entity == first);
        CHECK(target.roots()[1].entity == child);
        CHECK(target.roots()[2].entity == second);
        CHECK(target.roots()[1].owner == scene::SCENE_ID_AUTHORED);
        CONJURE_REQUIRE_OK(target.set_parent(second, ecs::NULL_ENTITY, true, 0));
        CHECK(target.roots()[0].entity == second);
        CHECK(target.roots()[1].entity == first);
        CHECK(target.roots()[2].entity == child);
    }

    TEST_CASE("hierarchy - subtree iteration is depth first sibling order without recursion (depth 31)") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const scene::hierarchy &tree = target.tree();
        const std::vector<ecs::entity> chain = make_chain(fixture, 31);
        // Side branches on the root so sibling order matters.
        const ecs::entity branchA = fixture.create("branch_a", chain[0]);
        const ecs::entity branchChild = fixture.create("branch_a_child", branchA);
        const ecs::entity branchB = fixture.create("branch_b", chain[0]);

        std::vector<ecs::entity> visited = {};
        tree.for_each_in_subtree(chain[0], [&](ecs::entity entity) {
            visited.push_back(entity);
        });
        REQUIRE(visited.size() == chain.size() + 3);
        // chain[1] is the first child, then its 30 descendants, then branch a, its child, then branch b.
        for (usize index = 0; index < chain.size(); ++index) {
            CAPTURE(index);
            CHECK(visited[index] == chain[index]);
        }
        CHECK(visited[chain.size()] == branchA);
        CHECK(visited[chain.size() + 1] == branchChild);
        CHECK(visited[chain.size() + 2] == branchB);

        std::vector<ecs::entity> collected = {};
        tree.collect_subtree(chain[0], collected);
        CHECK(collected == visited);
        CHECK(tree.subtree_size(chain[0]) == visited.size());
        CHECK(tree.subtree_height(chain[0]) == 31);
        CHECK(tree.subtree_height(branchA) == 1);
        CHECK(tree.root_of(chain[31]) == chain[0]);
        CHECK(tree.is_ancestor_of(chain[0], chain[31]));
        CHECK_FALSE(tree.is_ancestor_of(chain[31], chain[0]));
    }

    TEST_CASE("hierarchy - transformless ancestor contributes identity") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity grand = fixture.create("grand");
        const ecs::entity middle = fixture.create("middle", grand);
        const ecs::entity leaf = fixture.create("leaf", middle);
        CONJURE_REQUIRE_OK(target.set_local_transform(grand, at(100.0f, 0.0f, 0.0f)));
        CONJURE_REQUIRE_OK(target.set_local_transform(middle, at(10.0f, 0.0f, 0.0f)));
        CONJURE_REQUIRE_OK(target.set_local_transform(leaf, at(1.0f, 0.0f, 0.0f)));
        CONJURE_CHECK_APPROX(target.world_transform_of(leaf).value().position, math::vec3(111.0f, 0.0f, 0.0f));

        // Strip the middle entity's transform: it contributes the identity, the grandparent still applies.
        CONJURE_REQUIRE_OK(fixture.world.remove<scene::local_transform>(middle));
        CONJURE_REQUIRE_OK(fixture.world.remove<scene::world_transform>(middle));
        CONJURE_REQUIRE_OK(fixture.world.remove<scene::render_world_transform>(middle));
        CONJURE_REQUIRE_OK(fixture.world.remove<scene::transform_state>(middle));
        CHECK_FALSE(target.world_transform_of(middle).has_value());
        CONJURE_CHECK_APPROX(target.world_transform_of(leaf).value().position, math::vec3(101.0f, 0.0f, 0.0f));
        fixture.step();
        CONJURE_CHECK_APPROX(translation_of(fixture.world_matrix(leaf)), math::vec3(101.0f, 0.0f, 0.0f));

        // keepWorld through a transformless parent still preserves the world pose.
        const ecs::entity other = fixture.create("other");
        CONJURE_REQUIRE_OK(target.set_parent(leaf, other, true));
        CONJURE_CHECK_APPROX(target.world_transform_of(leaf).value().position, math::vec3(101.0f, 0.0f, 0.0f));
        CONJURE_REQUIRE_OK(target.set_parent(leaf, middle, true));
        CONJURE_CHECK_APPROX(target.local_transform_of(leaf).value().position, math::vec3(1.0f, 0.0f, 0.0f));

        // A transformless entity ignores keepWorld and set_world_transform refuses it.
        CONJURE_CHECK_OK(target.set_parent(middle, other, true));
        CONJURE_CHECK_ERROR(target.set_world_transform(middle, math::transform{}), scene_error::NO_TRANSFORM);
        CONJURE_CHECK_ERROR(target.set_local_transform(middle, scene::local_transform{}), scene_error::NO_TRANSFORM);
        CONJURE_CHECK_ERROR(target.set_local_transform(middle, math::transform{}), scene_error::NO_TRANSFORM);
    }
}
