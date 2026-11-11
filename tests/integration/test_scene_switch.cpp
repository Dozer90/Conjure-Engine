#include "conjure/testing/doctest.h"

#include "conjure/testing/headless_engine.h"
#include "integration_engine_support.h"

#include "conjure/core/types.h"
#include "conjure/ecs/world.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"

#include <string>
#include <vector>

// docs/design/08_testing.md 6.1 test_scene_switch: load A, add B, unload A, reload A: A's handles are invalid, B
// is intact, the world is empty after both are unloaded and the fixture's memory check is clean.

using namespace conjure;
using namespace conjure::testing;

namespace {

    [[nodiscard]] std::vector<ecs::entity> entities_of(const scene::scene_manager &scenes, scene::scene_id id) {
        const std::optional<scene::loaded_scene_info> info = scenes.info(id);
        REQUIRE(info.has_value());
        std::vector<ecs::entity> all = {};
        for (const ecs::entity root : info->roots) {
            all.push_back(root);
            for (const ecs::entity child : scenes.active().tree().children_of(root)) {
                all.push_back(child);
            }
        }
        return all;
    }

    [[nodiscard]] bool all_alive(const ecs::world &world, const std::vector<ecs::entity> &entities) {
        for (const ecs::entity entity : entities) {
            if (!world.is_alive(entity)) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool none_alive(const ecs::world &world, const std::vector<ecs::entity> &entities) {
        for (const ecs::entity entity : entities) {
            if (world.is_alive(entity)) {
                return false;
            }
        }
        return true;
    }
}

TEST_SUITE("integration/scene_switch") {

    TEST_CASE("scene_switch - additive load, unload and reload keep the other scene intact and leave nothing behind") {
        headless_engine engine(integration_test::quiet_options());
        const std::string sceneA = integration_test::author_small_scene(engine, "A");
        const std::string sceneB = integration_test::author_small_scene(engine, "B");
        REQUIRE(engine.world().entity_count() == 0);

        const core::result<scene::scene_id> loadedA = engine.load_scene_json(sceneA, scene::load_mode::REPLACE);
        CONJURE_REQUIRE_OK(loadedA);
        const std::vector<ecs::entity> entitiesA = entities_of(engine.scenes(), loadedA.value());
        CHECK(entitiesA.size() == 3);
        CHECK(engine.world().entity_count() == 3);

        const core::result<scene::scene_id> loadedB = engine.load_scene_json(sceneB, scene::load_mode::ADDITIVE);
        CONJURE_REQUIRE_OK(loadedB);
        const std::vector<ecs::entity> entitiesB = entities_of(engine.scenes(), loadedB.value());
        CHECK(entitiesB.size() == 3);
        CHECK(engine.world().entity_count() == 6);
        CHECK(engine.scenes().loaded_scenes().size() == 2);
        engine.step_frames(1);

        CONJURE_REQUIRE_OK(engine.scenes().unload(loadedA.value()));
        engine.step_frames(1);
        CHECK(none_alive(engine.world(), entitiesA));
        CHECK(all_alive(engine.world(), entitiesB));
        CHECK(engine.world().entity_count() == 3);
        CHECK_FALSE(engine.scenes().is_loaded(loadedA.value()));
        CHECK(engine.scenes().is_loaded(loadedB.value()));
        CHECK(engine.find_entity("A_Root").is_null());
        CHECK_FALSE(engine.find_entity("B_Root").is_null());

        const core::result<scene::scene_id> reloadedA = engine.load_scene_json(sceneA, scene::load_mode::ADDITIVE);
        CONJURE_REQUIRE_OK(reloadedA);
        CHECK(reloadedA.value() != loadedA.value());
        const std::vector<ecs::entity> entitiesA2 = entities_of(engine.scenes(), reloadedA.value());
        CHECK(entitiesA2.size() == 3);
        CHECK(all_alive(engine.world(), entitiesA2));
        CHECK(all_alive(engine.world(), entitiesB));
        CHECK(engine.world().entity_count() == 6);
        engine.step_frames(1);
        CHECK_FALSE(engine.find_entity("A_Root/A_Child").is_null());

        CONJURE_REQUIRE_OK(engine.scenes().unload(reloadedA.value()));
        CONJURE_REQUIRE_OK(engine.scenes().unload(loadedB.value()));
        engine.step_frames(1);
        CHECK(engine.world().entity_count() == 0);
        CHECK(engine.active_scene().roots().empty());
        CHECK(engine.scenes().loaded_scenes().empty());
        CHECK(none_alive(engine.world(), entitiesA2));
        CHECK(none_alive(engine.world(), entitiesB));
    }
}
