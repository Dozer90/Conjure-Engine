#include "conjure/testing/doctest.h"

#include "conjure/physics/physics.h"

#include <type_traits>

using namespace conjure;

TEST_SUITE("physics/physics") {

    TEST_CASE("physics - the umbrella header exposes every public type") {
        static_assert(std::is_default_constructible_v<physics::physics_settings>);
        static_assert(std::is_default_constructible_v<physics::physics_layer_table>);
        static_assert(std::is_trivially_copyable_v<physics::rigid_body>);
        static_assert(std::is_trivially_copyable_v<physics::character_controller>);
        static_assert(std::is_trivially_copyable_v<physics::contact_event>);
        static_assert(std::is_default_constructible_v<physics::query_filter>);
        static_assert(std::is_default_constructible_v<physics::collision_mesh_asset>);
        static_assert(std::is_abstract_v<physics::physics_debug_draw>);
        static_assert(!std::is_copy_constructible_v<physics::physics_world>);
        CHECK(physics::physics_error_name(physics::physics_error::NONE) == "NONE");
        CHECK(physics::SYSTEM_STEP == core::string_id("physics.step"));
        CHECK(physics::COLLISION_MESH_ASSET_TYPE == "conjure.physics.collision_mesh");
    }
}
