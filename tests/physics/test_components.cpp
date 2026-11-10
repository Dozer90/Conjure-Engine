#include "conjure/testing/doctest.h"
#include "physics_test_fixture.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/reflect/type_info.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/serialization.h"
#include "conjure/ecs/world.h"
#include "conjure/ecs/world_hash.h"
#include "conjure/physics/components.h"
#include "conjure/physics/register.h"

#include <cstddef>
#include <string>
#include <type_traits>

using namespace conjure;
using namespace physics_test;

namespace {

    template <typename ComponentType>
    void check_field(std::string_view name, usize offset) {
        const core::reflect::type_info *info = core::reflect::type_info_of<ComponentType>();
        REQUIRE(info != nullptr);
        const core::reflect::field_info *field = info->find_field(name);
        REQUIRE_MESSAGE(field != nullptr, std::string(name));
        CHECK(field->offset == offset);
    }
}

TEST_SUITE("physics/components") {

    TEST_CASE("components - registration of all eight types succeeds and is idempotent") {
        ecs::component_registry registry = {};
        CONJURE_REQUIRE_OK(physics::register_components(registry));
        const u32 count = registry.count();
        CONJURE_REQUIRE_OK(physics::register_components(registry));
        CHECK(registry.count() == count);
        CHECK(registry.id_of<physics::rigid_body>().is_valid());
        CHECK(registry.id_of<physics::box_collider>().is_valid());
        CHECK(registry.id_of<physics::sphere_collider>().is_valid());
        CHECK(registry.id_of<physics::capsule_collider>().is_valid());
        CHECK(registry.id_of<physics::convex_collider>().is_valid());
        CHECK(registry.id_of<physics::mesh_collider>().is_valid());
        CHECK(registry.id_of<physics::physics_material>().is_valid());
        CHECK(registry.id_of<physics::character_controller>().is_valid());
        CHECK(registry.find(core::string_id("conjure.physics.rigid_body")) == registry.id_of<physics::rigid_body>());
        const ecs::component_id materialId = registry.id_of<physics::physics_material>();
        CHECK(registry.find(core::string_id("conjure.physics.material")) == materialId);
        const ecs::component_info &info = registry.info(registry.id_of<physics::convex_collider>());
        REQUIRE(info.assetRefFieldOffsets.size() == 1);
        CHECK(info.assetRefFieldOffsets[0] == offsetof(physics::convex_collider, mesh));
        CHECK(registry.info(registry.id_of<physics::mesh_collider>()).assetRefFieldOffsets.size() == 1);
    }

    TEST_CASE("components - registration fails cleanly on a conflicting registry") {
        ecs::component_registry registry = {};
        ecs::dynamic_component_desc impostor = {};
        impostor.name = core::string_id("conjure.physics.material");
        impostor.size = 16;
        impostor.alignment = 8;
        const std::array<ecs::dynamic_field_desc, 1> fields = {
            ecs::dynamic_field_desc{core::string_id("a"), core::reflect::type_id_of<u64>(), 0},
        };
        impostor.fields = fields;
        CONJURE_REQUIRE_OK(registry.register_dynamic(impostor));
        CONJURE_CHECK_ERROR(physics::register_components(registry), ecs::ecs_error::NAME_CONFLICT);
    }

    TEST_CASE("components - reflection offsets match offsetof") {
        check_field<physics::rigid_body>("motionType", offsetof(physics::rigid_body, motionType));
        check_field<physics::rigid_body>("layer", offsetof(physics::rigid_body, layer));
        check_field<physics::rigid_body>("flags", offsetof(physics::rigid_body, flags));
        check_field<physics::rigid_body>("mass", offsetof(physics::rigid_body, mass));
        check_field<physics::rigid_body>("linearDamping", offsetof(physics::rigid_body, linearDamping));
        check_field<physics::rigid_body>("angularDamping", offsetof(physics::rigid_body, angularDamping));
        check_field<physics::rigid_body>("gravityFactor", offsetof(physics::rigid_body, gravityFactor));
        check_field<physics::rigid_body>("maxLinearVelocity", offsetof(physics::rigid_body, maxLinearVelocity));
        check_field<physics::rigid_body>("linearVelocity", offsetof(physics::rigid_body, linearVelocity));
        check_field<physics::rigid_body>("angularVelocity", offsetof(physics::rigid_body, angularVelocity));
        check_field<physics::rigid_body>("isSleeping", offsetof(physics::rigid_body, isSleeping));
        check_field<physics::box_collider>("halfExtents", offsetof(physics::box_collider, halfExtents));
        check_field<physics::box_collider>("rotation", offsetof(physics::box_collider, rotation));
        check_field<physics::box_collider>("isTrigger", offsetof(physics::box_collider, isTrigger));
        check_field<physics::sphere_collider>("radius", offsetof(physics::sphere_collider, radius));
        check_field<physics::capsule_collider>("halfHeight", offsetof(physics::capsule_collider, halfHeight));
        check_field<physics::convex_collider>("mesh", offsetof(physics::convex_collider, mesh));
        check_field<physics::convex_collider>("convexRadius", offsetof(physics::convex_collider, convexRadius));
        check_field<physics::mesh_collider>("mesh", offsetof(physics::mesh_collider, mesh));
        check_field<physics::physics_material>("restitution", offsetof(physics::physics_material, restitution));

        const core::reflect::type_info *body = core::reflect::type_info_of<physics::rigid_body>();
        REQUIRE(body != nullptr);
        CHECK(body->size == sizeof(physics::rigid_body));
        CHECK(body->fields.size() == 11);
        CHECK(body->find_field("linearVelocity")->attributes.readOnly);
        CHECK(body->find_field("isSleeping")->attributes.readOnly);
        CHECK_FALSE(body->find_field("mass")->attributes.readOnly);
        CHECK(body->find_field("flags")->kind == core::reflect::field_kind::ENUM);
        CHECK(body->find_field("flags")->enumType->isFlags);
        CHECK(body->find_field("motionType")->enumType->find_value("KINEMATIC") == 1);
    }

    TEST_CASE("components - rigid_body is 52 bytes and trivially copyable") {
        static_assert(sizeof(physics::rigid_body) == 52);
        static_assert(alignof(physics::rigid_body) == 4);
        static_assert(std::is_trivially_copyable_v<physics::rigid_body>);
        static_assert(sizeof(physics::box_collider) == 44);
        static_assert(sizeof(physics::sphere_collider) == 20);
        static_assert(sizeof(physics::capsule_collider) == 40);
        static_assert(sizeof(physics::convex_collider) == 64);
        static_assert(sizeof(physics::mesh_collider) == 32);
        const physics::rigid_body body = {};
        CHECK(body.motionType == physics::motion_type::DYNAMIC);
        CHECK(body.flags == physics::rigid_body_flags::ALLOW_SLEEPING);
        CHECK(body.mass == 1.0f);
        CHECK(body.maxLinearVelocity == 500.0f);
        CHECK_FALSE(body.isSleeping);
    }

    TEST_CASE("rigid_body_flags - flag operators compose and test") {
        using physics::rigid_body_flags;
        rigid_body_flags flags = rigid_body_flags::ALLOW_SLEEPING | rigid_body_flags::LOCK_ROTATION;
        CHECK(has_flag(flags, rigid_body_flags::ALLOW_SLEEPING));
        CHECK(has_flag(flags, rigid_body_flags::LOCK_ROTATION));
        CHECK_FALSE(has_flag(flags, rigid_body_flags::START_ASLEEP));
        flags |= rigid_body_flags::START_ASLEEP;
        CHECK(has_flag(flags, rigid_body_flags::START_ASLEEP));
        flags &= ~rigid_body_flags::ALLOW_SLEEPING;
        CHECK_FALSE(has_flag(flags, rigid_body_flags::ALLOW_SLEEPING));
        CHECK(to_underlying(rigid_body_flags::USE_GYROSCOPIC_FORCE) == (1u << 11));
        CHECK(to_underlying(rigid_body_flags::NONE) == 0u);
    }

    TEST_CASE("components - component_to_json and from_json round trip including asset_ref uuids") {
        ecs::component_registry registry = {};
        register_physics_test_components(registry);
        ecs::world world(registry);
        const core::result<ecs::entity> created = world.create_entity();
        REQUIRE(created.has_value());
        const ecs::entity entity = created.value();

        physics::rigid_body body = {};
        body.motionType = physics::motion_type::KINEMATIC;
        body.layer = 3;
        body.flags = physics::rigid_body_flags::LOCK_ROTATION | physics::rigid_body_flags::CONTINUOUS_COLLISION;
        body.mass = 2.5f;
        body.linearVelocity = math::vec3(1.0f, 2.0f, 3.0f);
        body.isSleeping = true;
        CONJURE_REQUIRE_OK(world.add<physics::rigid_body>(entity, body));
        physics::convex_collider convex = {};
        convex.mesh = assets::asset_ref<physics::collision_mesh_asset>(core::uuid(
          0x1122334455667788ull,
          0x99AABBCCDDEEFF00ull
        ));
        convex.convexRadius = 0.1f;
        convex.isTrigger = true;
        CONJURE_REQUIRE_OK(world.add<physics::convex_collider>(entity, convex));

        core::json::writer out = {};
        CONJURE_REQUIRE_OK(ecs::component_to_json(world, entity, registry.id_of<physics::rigid_body>(), out));
        const std::string bodyText = out.finish();
        CHECK(bodyText.find("\"motionType\":\"KINEMATIC\"") != std::string::npos);
        CHECK(bodyText.find("CONTINUOUS_COLLISION|LOCK_ROTATION") != std::string::npos);
        CHECK(bodyText.find("\"isSleeping\":true") != std::string::npos);

        core::json::writer convexOut = {};
        CONJURE_REQUIRE_OK(ecs::component_to_json(
          world,
          entity,
          registry.id_of<physics::convex_collider>(),
          convexOut
        ));
        const std::string convexText = convexOut.finish();
        CHECK(convexText.find("\"mesh\":\"11223344-5566-7788-99aa-bbccddeeff00\"") != std::string::npos);

        // Read into a second entity.
        const core::result<ecs::entity> other = world.create_entity();
        REQUIRE(other.has_value());
        CONJURE_REQUIRE_OK(world.add<physics::rigid_body>(other.value()));
        CONJURE_REQUIRE_OK(world.add<physics::convex_collider>(other.value()));
        const core::result<core::json::document> bodyDocument = core::json::document::parse(bodyText);
        CONJURE_REQUIRE_OK(bodyDocument);
        CONJURE_REQUIRE_OK(ecs::component_from_json(
          world,
          other.value(),
          registry.id_of<physics::rigid_body>(),
          bodyDocument.value().root()
        ));
        const physics::rigid_body *restored = world.get<physics::rigid_body>(other.value());
        REQUIRE(restored != nullptr);
        CHECK(*restored == body);

        const core::result<core::json::document> convexDocument = core::json::document::parse(convexText);
        CONJURE_REQUIRE_OK(convexDocument);
        CONJURE_REQUIRE_OK(ecs::component_from_json(
          world,
          other.value(),
          registry.id_of<physics::convex_collider>(),
          convexDocument.value().root()
        ));
        const physics::convex_collider *restoredConvex = world.get<physics::convex_collider>(other.value());
        REQUIRE(restoredConvex != nullptr);
        CHECK(restoredConvex->mesh.id() == convex.mesh.id());
        CHECK_FALSE(restoredConvex->mesh.is_bound());
        CHECK(restoredConvex->convexRadius == doctest::Approx(0.1f));
        CHECK(restoredConvex->isTrigger);
        CONJURE_CHECK_OK(world.validate());
    }

    TEST_CASE("components - world_hash changes when mass changes") {
        ecs::component_registry registry = {};
        register_physics_test_components(registry);
        ecs::world world(registry);
        const core::result<ecs::entity> created = world.create_entity();
        REQUIRE(created.has_value());
        CONJURE_REQUIRE_OK(world.add<physics::rigid_body>(created.value()));
        const u64 before = ecs::world_hash(world);
        physics::rigid_body *body = world.get<physics::rigid_body>(created.value());
        REQUIRE(body != nullptr);
        body->mass = 7.0f;
        CHECK(ecs::world_hash(world) != before);
        body->mass = 1.0f;
        CHECK(ecs::world_hash(world) == before);
    }

    TEST_CASE("physics_material - defaults combine as the design states") {
        const physics::physics_material material = {};
        CHECK(material.friction == doctest::Approx(0.5f));
        CHECK(material.restitution == doctest::Approx(0.0f));
        static_assert(sizeof(physics::physics_material) == 8);
    }
}
