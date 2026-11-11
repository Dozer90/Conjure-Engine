#include "conjure/testing/doctest.h"

#include "conjure/testing/headless_engine.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "conjure/testing/world_builder.h"

#include "conjure/core/error.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/world.h"
#include "conjure/scene/components.h"
#include "conjure/scene/hierarchy.h"
#include "conjure/scene/register.h"
#include "conjure/scene/scene.h"

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #include "conjure/physics/components.h"
#endif
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    #include "conjure/scripting/script_component.h"
#endif

#include <algorithm>
#include <limits>
#include <span>
#include <vector>

namespace builder_test {

    struct position {
        conjure::f32 x = 0.0f;
        conjure::f32 y = 0.0f;
        conjure::f32 z = 0.0f;

        [[nodiscard]] bool operator==(const position &) const = default;
    };

    struct velocity {
        conjure::f32 dx = 0.0f;
        conjure::f32 dy = 0.0f;

        [[nodiscard]] bool operator==(const velocity &) const = default;
    };
}

CONJURE_REFLECT_STRUCT(builder_test::position, "builder_test.position",
    CONJURE_FIELD(x),
    CONJURE_FIELD(y),
    CONJURE_FIELD(z));
CONJURE_REFLECT_STRUCT(builder_test::velocity, "builder_test.velocity", CONJURE_FIELD(dx), CONJURE_FIELD(dy));

CONJURE_COMPONENT(builder_test::position, "builder_test.position");
CONJURE_COMPONENT(builder_test::velocity, "builder_test.velocity");

using namespace conjure;
using namespace conjure::testing;

namespace {
    [[nodiscard]] headless_engine_options quiet_options() {
        headless_engine_options options = {};
        options.enableRenderer = false;
        options.enableAudio = false;
        return options;
    }

    /// <summary>
    /// Appends the positions of 'chosen' inside 'all' (the selection as index set, independent of entity
    /// ids) to a vector the caller reserved before the fixture, so the result outlives the fixture
    /// without allocating during its run.
    /// </summary>
    void positions_of(std::span<const ecs::entity> all, std::span<const ecs::entity> chosen, std::vector<usize> &out) {
        out.clear();
        for (const ecs::entity &pick : chosen) {
            const auto found = std::find(all.begin(), all.end(), pick);
            REQUIRE(found != all.end());
            out.push_back(static_cast<usize>(found - all.begin()));
        }
        std::sort(out.begin(), out.end());
    }

    [[nodiscard]] u32 count_dirty(const ecs::world &world, std::span<const ecs::entity> entities) {
        u32 dirty = 0;
        for (const ecs::entity &entity : entities) {
            if (world.has<scene::transform_dirty>(entity)) {
                ++dirty;
            }
        }
        return dirty;
    }
}

TEST_SUITE("testing/world_builder") {

    /*-----------------------------------------------------------------------*/
    // spawn_many

    TEST_CASE("spawn_many - count in one archetype with the prototype values") {
        ecs::component_registry registry = {};
        CONJURE_REQUIRE_OK(registry.register_component<builder_test::position>());
        CONJURE_REQUIRE_OK(registry.register_component<builder_test::velocity>());
        ecs::world world(registry);

        const builder_test::position start = {1.0f, 2.0f, 3.0f};
        const builder_test::velocity speed = {0.5f, -0.5f};
        const std::vector<ecs::entity> spawned = spawn_many(world, 1000, start, speed);

        REQUIRE(spawned.size() == 1000);
        CHECK(world.entity_count() == 1000);
        // The empty archetype plus the one every spawned entity shares.
        CHECK(world.archetypes().size() == 2);
        for (usize index = 0; index < spawned.size(); index += 97) {
            CAPTURE(index);
            const builder_test::position *stored = world.get<builder_test::position>(spawned[index]);
            REQUIRE(stored != nullptr);
            CHECK(*stored == start);
            REQUIRE(world.get<builder_test::velocity>(spawned[index]) != nullptr);
            CHECK(*world.get<builder_test::velocity>(spawned[index]) == speed);
        }
        CHECK(world.is_alive(spawned.back()));
    }

    TEST_CASE("spawn_many - without prototypes creates empty entities, zero count creates none") {
        ecs::component_registry registry = {};
        CONJURE_REQUIRE_OK(registry.register_component<builder_test::position>());
        ecs::world world(registry);

        CHECK(spawn_many(world, 0, builder_test::position{}).empty());
        CHECK(world.entity_count() == 0);

        const std::vector<ecs::entity> bare = spawn_many(world, 3);
        CHECK(bare.size() == 3);
        CHECK(world.entity_count() == 3);
        CHECK_FALSE(world.has<builder_test::position>(bare[0]));
    }

    /*-----------------------------------------------------------------------*/
    // spawn_hierarchy

    TEST_CASE("spawn_hierarchy - total and depth match the default shape") {
        headless_engine engine(quiet_options());
        const hierarchy_shape shape = {};

        const std::vector<ecs::entity> tree = spawn_hierarchy(engine.active_scene(), shape);

        REQUIRE(tree.size() == shape.totalEntityCount);
        CHECK(hierarchy_entity_count(shape) == shape.totalEntityCount);
        CHECK(engine.active_scene().roots().size() == shape.rootCount);
        CHECK(engine.world().entity_count() == shape.totalEntityCount);

        const scene::hierarchy &hierarchy = engine.active_scene().tree();
        u8 deepest = 0;
        for (const ecs::entity &entity : tree) {
            deepest = std::max(deepest, hierarchy.depth_of(entity));
        }
        CHECK(deepest == shape.depth);
        CHECK(engine.active_scene().max_depth_in_use() == shape.depth);
        for (u32 index = 0; index < shape.rootCount; ++index) {
            CHECK(hierarchy.depth_of(tree[index]) == 0);
        }
        CHECK(hierarchy.depth_of(tree[shape.rootCount]) == 1);
        CHECK(engine.active_scene().local_transform_of(tree.back()).has_value());
        CONJURE_CHECK_OK(engine.active_scene().validate());
        CHECK(engine.find_entity("root") == tree[0]);
    }

    TEST_CASE("spawn_hierarchy - pads the last level to reach the total") {
        headless_engine engine(quiet_options());
        hierarchy_shape shape = {};
        shape.rootCount = 2;
        shape.depth = 2;
        shape.childrenPerNode = 2;
        shape.totalEntityCount = 20;   // the full tree holds 2 + 4 + 8 = 14

        const std::vector<ecs::entity> tree = spawn_hierarchy(engine.active_scene(), shape);

        REQUIRE(tree.size() == 20);
        CHECK(hierarchy_entity_count(shape) == 20);
        const scene::hierarchy &hierarchy = engine.active_scene().tree();
        u32 leaves = 0;
        for (const ecs::entity &entity : tree) {
            if (hierarchy.depth_of(entity) == 2) {
                ++leaves;
            }
        }
        CHECK(leaves == 14);
        // The padding is spread round-robin over the four level-1 parents: 14 leaves over 4 parents.
        CHECK(hierarchy.children_of(tree[2]).size() == 4);
        CHECK(hierarchy.children_of(tree[3]).size() == 4);
        CHECK(hierarchy.children_of(tree[4]).size() == 3);
        CHECK(hierarchy.children_of(tree[5]).size() == 3);
    }

    TEST_CASE("spawn_hierarchy - trims the last level and never the levels above it") {
        hierarchy_shape shape = {};
        shape.rootCount = 2;
        shape.depth = 2;
        shape.childrenPerNode = 2;
        u32 expected = 0;

        // The fixture is created after the SUBCASE selection (memory_guard.h on SUBCASE boundaries).
        SUBCASE("a total inside the last level") {
            shape.totalEntityCount = 9;   // 2 + 4 + 3
            expected = 9;
        }
        SUBCASE("a total below the upper levels keeps them whole") {
            shape.totalEntityCount = 5;   // 2 + 4 already exceed it
            expected = 6;
        }
        SUBCASE("zero means the full tree") {
            shape.totalEntityCount = 0;
            expected = 14;
        }

        headless_engine engine(quiet_options());
        const std::vector<ecs::entity> tree = spawn_hierarchy(engine.active_scene(), shape);
        CHECK(tree.size() == expected);
        CHECK(hierarchy_entity_count(shape) == expected);
        CHECK(engine.world().entity_count() == expected);
        CONJURE_CHECK_OK(engine.active_scene().validate());
    }

    TEST_CASE("spawn_hierarchy - an unusable shape spawns nothing and warns") {
        headless_engine engine(quiet_options());
        log_capture &logs = engine.logs();

        hierarchy_shape noRoots = {};
        noRoots.rootCount = 0;
        CHECK(spawn_hierarchy(engine.active_scene(), noRoots).empty());
        CHECK(hierarchy_entity_count(noRoots) == 0);

        hierarchy_shape tooDeep = {};
        tooDeep.depth = scene::MAX_HIERARCHY_DEPTH;
        CHECK(spawn_hierarchy(engine.active_scene(), tooDeep).empty());
        CHECK(hierarchy_entity_count(tooDeep) == 0);

        CHECK(engine.world().entity_count() == 0);
        CHECK(logs.count(core::log_level::WARN) == 2);
    }

    TEST_CASE("regression - zero children per node below the roots is unusable, roots alone are fine") {
        headless_engine engine(quiet_options());

        hierarchy_shape noChildren = {};
        noChildren.rootCount = 2;
        noChildren.depth = 2;
        noChildren.childrenPerNode = 0;
        noChildren.totalEntityCount = 10;   // used to predict 10 while 2 were spawned
        CHECK(hierarchy_entity_count(noChildren) == 0);
        CHECK(spawn_hierarchy(engine.active_scene(), noChildren).empty());
        CHECK(engine.world().entity_count() == 0);
        CHECK(engine.logs().count(core::log_level::WARN) == 1);

        hierarchy_shape rootsOnly = noChildren;
        rootsOnly.depth = 0;
        rootsOnly.totalEntityCount = 0;
        CHECK(hierarchy_entity_count(rootsOnly) == 2);
        CHECK(spawn_hierarchy(engine.active_scene(), rootsOnly).size() == 2);
        CHECK(engine.world().entity_count() == 2);
    }

    TEST_CASE("spawn_hierarchy - a total below the levels above the last one spawns exactly the prediction") {
        hierarchy_shape shape = {};
        shape.rootCount = 10;
        shape.childrenPerNode = 2;
        shape.totalEntityCount = 4;
        u32 expected = 0;

        // The fixture is created after the SUBCASE selection (memory_guard.h on SUBCASE boundaries).
        SUBCASE("roots only: the roots are the last level and are trimmed") {
            shape.depth = 0;
            expected = 4;
        }
        SUBCASE("two levels: the roots and the first level stay whole, the last level is empty") {
            shape.depth = 2;
            expected = 30;   // 10 + 20 + 0
        }

        headless_engine engine(quiet_options());
        const std::vector<ecs::entity> tree = spawn_hierarchy(engine.active_scene(), shape);
        CHECK(tree.size() == expected);
        CHECK(hierarchy_entity_count(shape) == expected);
        CHECK(engine.world().entity_count() == expected);
        CONJURE_CHECK_OK(engine.active_scene().validate());
    }

    TEST_CASE("regression - trees wider than a u32 are unusable instead of overflowing") {
        headless_engine engine(quiet_options());

        hierarchy_shape fullTree = {};
        fullTree.rootCount = 100;
        fullTree.depth = scene::MAX_HIERARCHY_DEPTH - 1;
        fullTree.childrenPerNode = 4;
        fullTree.totalEntityCount = 0;   // 100 * 4^31 entities: the arithmetic used to wrap around
        CHECK(hierarchy_entity_count(fullTree) == 0);
        CHECK(spawn_hierarchy(engine.active_scene(), fullTree).empty());

        hierarchy_shape trimmed = fullTree;
        trimmed.totalEntityCount = 10000;   // the untrimmed upper levels alone are far beyond a u32
        CHECK(hierarchy_entity_count(trimmed) == 0);
        CHECK(spawn_hierarchy(engine.active_scene(), trimmed).empty());

        hierarchy_shape wideRoots = {};
        wideRoots.rootCount = std::numeric_limits<u32>::max();
        wideRoots.depth = 1;
        wideRoots.childrenPerNode = std::numeric_limits<u32>::max();
        wideRoots.totalEntityCount = 0;
        CHECK(hierarchy_entity_count(wideRoots) == 0);

        CHECK(engine.world().entity_count() == 0);
        CHECK(engine.logs().count(core::log_level::WARN) == 2);
    }

    TEST_CASE("spawn_hierarchy - a depth-31 chain reaches the deepest level the scene allows") {
        headless_engine engine(quiet_options());
        hierarchy_shape chain = {};
        chain.rootCount = 1;
        chain.depth = scene::MAX_HIERARCHY_DEPTH - 1;
        chain.childrenPerNode = 1;
        chain.totalEntityCount = 0;

        const std::vector<ecs::entity> tree = spawn_hierarchy(engine.active_scene(), chain);

        REQUIRE(tree.size() == scene::MAX_HIERARCHY_DEPTH);
        CHECK(engine.active_scene().tree().depth_of(tree.back()) == scene::MAX_HIERARCHY_DEPTH - 1);
        CONJURE_CHECK_OK(engine.active_scene().validate());
    }

    /*-----------------------------------------------------------------------*/
    // Bodies and scripts

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    TEST_CASE("spawn_box_body - creates the transform, the rigid body and the collider") {
        headless_engine engine(quiet_options());
        scene::scene &scene = engine.active_scene();

        const core::result<ecs::entity> body =
          spawn_box_body(scene, math::vec3(1.0f, 5.0f, -2.0f), physics::motion_type::KINEMATIC);
        CONJURE_REQUIRE_OK(body);

        const ecs::world &world = engine.world();
        REQUIRE(world.get<physics::rigid_body>(body.value()) != nullptr);
        CHECK(world.get<physics::rigid_body>(body.value())->motionType == physics::motion_type::KINEMATIC);
        CHECK(world.has<physics::box_collider>(body.value()));
        REQUIRE(scene.local_transform_of(body.value()).has_value());
        CHECK(scene.local_transform_of(body.value())->position.y == doctest::Approx(5.0f));
        CHECK(engine.find_entity("box") == body.value());
        CHECK(scene.roots().size() == 1);

        // The body is picked up by the physics subsystem at the next push_transforms without complaint.
        engine.step_frames(2);
        CHECK(world.is_alive(body.value()));
    }

    TEST_CASE("spawn_box_body - fails when the physics components are not registered") {
        ecs::component_registry registry = {};
        CONJURE_REQUIRE_OK(scene::register_components(registry));
        ecs::world world(registry);
        scene::scene bare(world, nullptr);

        const core::result<ecs::entity> body =
          spawn_box_body(bare, math::vec3(0.0f), physics::motion_type::DYNAMIC);

        CONJURE_CHECK_ERROR(body, ecs::ecs_error::UNKNOWN_COMPONENT);
    }
#endif

#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    TEST_CASE("spawn_script_entity - fails without an active runtime") {
        headless_engine engine(quiet_options());

        const core::result<ecs::entity> scripted =
          spawn_script_entity(engine.active_scene(), "Probe", "Game.Probe");

        CONJURE_CHECK_ERROR(scripted, core::error_code::INVALID_STATE);
        CHECK(engine.world().entity_count() == 0);
    }

    TEST_CASE("spawn_script_entity - attaches a script entry through the null host runtime") {
        headless_engine_options options = quiet_options();
        options.enableScripting = true;
        options.useNullScriptHost = true;
        headless_engine engine(options);
        REQUIRE(engine.context().scripting != nullptr);

        const core::result<ecs::entity> scripted =
          spawn_script_entity(engine.active_scene(), "Probe", "Game.Probe");

        CONJURE_REQUIRE_OK(scripted);
        CHECK(engine.world().has<scripting::script_component>(scripted.value()));
        CHECK(engine.find_entity("Probe") == scripted.value());
        engine.step_frames(2);
        CHECK(engine.world().is_alive(scripted.value()));
    }
#endif

    /*-----------------------------------------------------------------------*/
    // mark_dirty_fraction

    TEST_CASE("mark_dirty_fraction - tags the requested fraction and is deterministic for a seed") {
        hierarchy_shape shape = {};
        shape.rootCount = 10;
        shape.depth = 2;
        shape.childrenPerNode = 3;
        shape.totalEntityCount = 1000;

        // Reserved before the fixtures: filling them during a run must not count as a leak of that run.
        std::vector<usize> first = {};
        std::vector<usize> second = {};
        std::vector<usize> otherSeed = {};
        first.reserve(shape.totalEntityCount);
        second.reserve(shape.totalEntityCount);
        otherSeed.reserve(shape.totalEntityCount);
        {
            headless_engine engine(quiet_options());
            const std::vector<ecs::entity> tree = spawn_hierarchy(engine.active_scene(), shape);
            REQUIRE(tree.size() == 1000);
            // A fresh tree is dirty from creation: one frame propagates it and clears the tags.
            engine.step_frames(1);
            REQUIRE(count_dirty(engine.world(), tree) == 0);

            const std::vector<ecs::entity> tagged = mark_dirty_fraction(engine.active_scene(), tree, 0.1f, 7);
            CHECK(tagged.size() == 100);
            CHECK(count_dirty(engine.world(), tree) == 100);
            CHECK(std::is_sorted(tagged.begin(), tagged.end(), [](const ecs::entity &a, const ecs::entity &b) {
                return a.index < b.index;
            }));
            positions_of(tree, tagged, first);

            // The next frame consumes the tags.
            engine.step_frames(1);
            CHECK(count_dirty(engine.world(), tree) == 0);
        }
        {
            headless_engine engine(quiet_options());
            const std::vector<ecs::entity> tree = spawn_hierarchy(engine.active_scene(), shape);
            engine.step_frames(1);
            positions_of(tree, mark_dirty_fraction(engine.active_scene(), tree, 0.1f, 7), second);
            engine.step_frames(1);
            positions_of(tree, mark_dirty_fraction(engine.active_scene(), tree, 0.1f, 8), otherSeed);
        }

        CHECK(first == second);
        CHECK(first != otherSeed);
        CHECK(otherSeed.size() == 100);
    }

    TEST_CASE("mark_dirty_fraction - already dirty entities are skipped and the result excludes them") {
        headless_engine engine(quiet_options());
        hierarchy_shape shape = {};
        shape.rootCount = 20;
        shape.depth = 0;
        shape.totalEntityCount = 0;
        const std::vector<ecs::entity> roots = spawn_hierarchy(engine.active_scene(), shape);
        REQUIRE(roots.size() == 20);
        REQUIRE(count_dirty(engine.world(), roots) == 20);   // fresh entities carry the tag

        CHECK(mark_dirty_fraction(engine.active_scene(), roots, 1.0f, 1).empty());
        engine.step_frames(1);
        REQUIRE(count_dirty(engine.world(), roots) == 0);

        const std::vector<ecs::entity> half = mark_dirty_fraction(engine.active_scene(), roots, 0.5f, 1);
        CHECK(half.size() == 10);
        const std::vector<ecs::entity> rest = mark_dirty_fraction(engine.active_scene(), roots, 1.0f, 2);
        CHECK(rest.size() == 10);
        CHECK(count_dirty(engine.world(), roots) == 20);
    }

    TEST_CASE("mark_dirty_fraction - edge fractions and empty inputs") {
        headless_engine engine(quiet_options());
        hierarchy_shape shape = {};
        shape.rootCount = 8;
        shape.depth = 0;
        shape.totalEntityCount = 0;
        const std::vector<ecs::entity> roots = spawn_hierarchy(engine.active_scene(), shape);
        engine.step_frames(1);

        CHECK(mark_dirty_fraction(engine.active_scene(), {}, 1.0f, 3).empty());
        CHECK(mark_dirty_fraction(engine.active_scene(), roots, 0.0f, 3).empty());
        CHECK(mark_dirty_fraction(engine.active_scene(), roots, -1.0f, 3).empty());
        CHECK(mark_dirty_fraction(engine.active_scene(), roots, 0.01f, 3).empty());   // rounds to zero
        CHECK(count_dirty(engine.world(), roots) == 0);

        CHECK(mark_dirty_fraction(engine.active_scene(), roots, 5.0f, 3).size() == 8);   // clamps to all
        CHECK(count_dirty(engine.world(), roots) == 8);
    }

    TEST_CASE("mark_dirty_fraction - entities without a transform are counted but never tagged") {
        headless_engine engine(quiet_options());
        std::vector<ecs::entity> bare = spawn_many(engine.world(), 10);
        CHECK(mark_dirty_fraction(engine.active_scene(), bare, 1.0f, 11).empty());
        CHECK(count_dirty(engine.world(), bare) == 0);
    }
}
