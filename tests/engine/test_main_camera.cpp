#include "conjure/testing/doctest.h"

#include "conjure/engine/main_camera.h"

#include "conjure/core/reflect/type_registry.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/world.h"
#include "conjure/scene/components.h"
#include "conjure/scene/register.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/renderer/components.h"
    #include "engine/subsystems/renderer_components.h"
#endif

// main_camera.h: find_main_camera names the camera extraction shows first (an enabled camera with a render world
// transform; a main one first, then the highest priority, then world order), and the null entity when there is
// none or the world never registered the components.

using namespace conjure;
using namespace conjure::engine;

namespace {

    /// <summary>
    /// A world over the scene components (and, with the renderer, the camera component when 'withCamera').
    /// </summary>
    struct camera_world {
        ecs::component_registry registry = {};
        ecs::world world;           // no default: built on the registry in the constructor

        explicit camera_world(bool withCamera) :
            world((register_all(registry, withCamera), registry)) {}

        static void register_all(ecs::component_registry &registry, bool withCamera) {
            CONJURE_REQUIRE_OK(scene::register_components(registry));
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
            if (withCamera) {
                CONJURE_REQUIRE_OK(renderer::register_components(core::reflect::registry()));
                CONJURE_REQUIRE_OK(registry.register_component<renderer::camera_component>());
            }
#else
            static_cast<void>(withCamera);
#endif
        }

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
        /// <summary>
        /// An entity with a camera_component and a render_world_transform.
        /// </summary>
        [[nodiscard]] ecs::entity add_camera(bool isMain, i32 priority) {
            const core::result<ecs::entity> created = world.create_entity();
            CONJURE_REQUIRE_OK(created);
            renderer::camera_component camera = {};
            camera.isMain = isMain;
            camera.priority = priority;
            CONJURE_REQUIRE_OK(world.add<renderer::camera_component>(created.value(), camera));
            CONJURE_REQUIRE_OK(world.add<scene::render_world_transform>(created.value()));
            return created.value();
        }
#endif
    };
}

TEST_SUITE("engine/main_camera") {

    TEST_CASE("find_main_camera - a world without cameras has none") {
        SUBCASE("the camera component is not registered") {
            camera_world fixture(false);
            const core::result<ecs::entity> plain = fixture.world.create_entity();
            CONJURE_REQUIRE_OK(plain);
            CHECK(find_main_camera(fixture.world).is_null());
        }
        SUBCASE("the camera component is registered but unused") {
            camera_world fixture(true);
            CHECK(find_main_camera(fixture.world).is_null());
        }
    }

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    TEST_CASE("find_main_camera - a main camera comes before any other, then priority, then world order") {
        camera_world fixture(true);
        SUBCASE("a main camera beats a higher priority one that is not main") {
            const ecs::entity other = fixture.add_camera(false, 100);
            const ecs::entity main = fixture.add_camera(true, 0);
            CHECK(find_main_camera(fixture.world) == main);
            CHECK(find_main_camera(fixture.world) != other);
        }
        SUBCASE("among main cameras the highest priority wins") {
            const ecs::entity low = fixture.add_camera(true, 1);
            const ecs::entity high = fixture.add_camera(true, 5);
            CHECK(find_main_camera(fixture.world) == high);
            CHECK(find_main_camera(fixture.world) != low);
        }
        SUBCASE("without a main camera the highest priority wins") {
            const ecs::entity low = fixture.add_camera(false, -3);
            const ecs::entity high = fixture.add_camera(false, 2);
            CHECK(find_main_camera(fixture.world) == high);
            CHECK(find_main_camera(fixture.world) != low);
        }
        SUBCASE("equal cameras: the first in world order") {
            const ecs::entity first = fixture.add_camera(true, 0);
            const ecs::entity second = fixture.add_camera(true, 0);
            CHECK(find_main_camera(fixture.world) == first);
            CHECK(find_main_camera(fixture.world) != second);
        }
    }

    TEST_CASE("find_main_camera - disabled, inactive and untransformed cameras are skipped") {
        camera_world fixture(true);
        const ecs::entity fallback = fixture.add_camera(false, 0);
        SUBCASE("disabled") {
            const ecs::entity skipped = fixture.add_camera(true, 10);
            CONJURE_REQUIRE_OK(fixture.world.add<scene::disabled>(skipped));
            CHECK(find_main_camera(fixture.world) == fallback);
        }
        SUBCASE("inactive (a disabled ancestor)") {
            const ecs::entity skipped = fixture.add_camera(true, 10);
            CONJURE_REQUIRE_OK(fixture.world.add<scene::inactive>(skipped));
            CHECK(find_main_camera(fixture.world) == fallback);
        }
        SUBCASE("no render world transform") {
            const core::result<ecs::entity> skipped = fixture.world.create_entity();
            CONJURE_REQUIRE_OK(skipped);
            renderer::camera_component camera = {};
            camera.priority = 10;
            CONJURE_REQUIRE_OK(fixture.world.add<renderer::camera_component>(skipped.value(), camera));
            CHECK(find_main_camera(fixture.world) == fallback);
        }
        SUBCASE("every camera skipped") {
            CONJURE_REQUIRE_OK(fixture.world.add<scene::disabled>(fallback));
            CHECK(find_main_camera(fixture.world).is_null());
        }
    }
#endif
}
