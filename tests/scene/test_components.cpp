#include "conjure/testing/doctest.h"
#include "scene_test_fixture.h"

#include "conjure/assets/asset_ref.h"
#include "conjure/assets/binary_asset.h"
#include "conjure/core/math/transform.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/scene/components.h"
#include "conjure/scene/register.h"

#include <cstring>
#include <string_view>
#include <type_traits>

using namespace conjure;
using namespace scene_test;

TEST_SUITE("scene/components") {

    TEST_CASE("local_transform - matches math transform layout") {
        CHECK(sizeof(scene::local_transform) == sizeof(math::transform));
        CHECK(std::is_trivially_copyable_v<scene::local_transform>);
        math::transform source = {};
        source.position = math::vec3(1.0f, 2.0f, 3.0f);
        source.rotation = glm::angleAxis(0.5f, math::vec3(0.0f, 1.0f, 0.0f));
        source.scale = math::vec3(2.0f, 3.0f, 4.0f);
        const scene::local_transform local = scene::local_transform::from_transform(source);
        CHECK(local.position == source.position);
        CHECK(local.rotation == source.rotation);
        CHECK(local.scale == source.scale);
        CHECK(local.to_transform() == source);

        // Byte identical: the C# LocalTransform mirror memcpys both ways.
        scene::local_transform copied = {};
        std::memcpy(&copied, &source, sizeof(source));
        CHECK(copied == local);
        const scene::local_transform defaults = {};
        CHECK(defaults.to_transform() == math::transform{});
    }

    TEST_CASE("register_components - assigns all ids and is idempotent") {
        ecs::component_registry registry = {};
        CONJURE_REQUIRE_OK(scene::register_components(registry));
        const scene::scene_component_ids first = scene::scene_component_ids::resolve(registry);
        CHECK(first.is_complete());
        CHECK_FALSE(first.depth[0].is_valid());
        for (u32 level = 1; level < scene::MAX_HIERARCHY_DEPTH; ++level) {
            CAPTURE(level);
            CHECK(first.depth[level].is_valid());
        }
        const u32 count = registry.count();

        // Registering again yields the same ids and no new types.
        CONJURE_REQUIRE_OK(scene::register_components(registry));
        const scene::scene_component_ids second = scene::scene_component_ids::resolve(registry);
        CHECK(registry.count() == count);
        CHECK(second.parent == first.parent);
        CHECK(second.prefabLink == first.prefabLink);
        CHECK(second.depth[31] == first.depth[31]);

        // Ids follow registration order so every registry of the process agrees.
        CHECK(first.parent.value < first.children.value);
        CHECK(first.children.value < first.localTransform.value);
        CHECK(first.transformDirty.value < first.depth[1].value);
        CHECK(first.depth[31].value < first.name.value);
        CHECK(first.prefabInstance.value < first.prefabLink.value);
        CHECK(registry.find(core::string_id("conjure.scene.parent")) == first.parent);
        CHECK(registry.find(core::string_id("conjure.scene.prefab_link")) == first.prefabLink);
    }

    TEST_CASE("scene_component_ids - incomplete on a registry without the scene types") {
        ecs::component_registry registry = {};
        const scene::scene_component_ids ids = scene::scene_component_ids::resolve(registry);
        CHECK_FALSE(ids.is_complete());
        CHECK_FALSE(ids.parent.is_valid());
        CHECK(ids.depth_of(ecs::component_mask{}) == 0);
    }

    TEST_CASE("depth_tag - tags are transient hidden tags") {
        ecs::component_registry registry = {};
        CONJURE_REQUIRE_OK(scene::register_components(registry));
        const scene::scene_component_ids ids = scene::scene_component_ids::resolve(registry);
        CHECK(ecs::component_name<scene::depth_tag<1>>::VALUE == "conjure.scene.depth_01");
        CHECK(ecs::component_name<scene::depth_tag<17>>::VALUE == "conjure.scene.depth_17");
        CHECK(ecs::component_name<scene::depth_tag<31>>::VALUE == "conjure.scene.depth_31");
        for (u32 level = 1; level < scene::MAX_HIERARCHY_DEPTH; ++level) {
            CAPTURE(level);
            const ecs::component_info &info = registry.info(ids.depth[level]);
            CHECK(info.is_tag());
            CHECK(info.is_transient());
            CHECK(has_flag(info.flags, ecs::component_flags::EDITOR_HIDDEN));
            ecs::component_mask mask = {};
            mask.set(ids.depth[level]);
            CHECK(ids.depth_of(mask) == level);
        }
        const ecs::component_info &dirty = registry.info(ids.transformDirty);
        CHECK(dirty.is_tag());
        CHECK(has_flag(dirty.flags, ecs::component_flags::EDITOR_HIDDEN));
        const ecs::component_info &state = registry.info(ids.transformState);
        CHECK(state.is_transient());
        CHECK(has_flag(state.flags, ecs::component_flags::EDITOR_HIDDEN));
        CHECK(registry.info(ids.inactive).is_transient());
        CHECK_FALSE(registry.info(ids.disabled).is_transient());
    }

    TEST_CASE("children - is non trivial and transient") {
        ecs::component_registry registry = {};
        CONJURE_REQUIRE_OK(scene::register_components(registry));
        const scene::scene_component_ids ids = scene::scene_component_ids::resolve(registry);
        const ecs::component_info &info = registry.info(ids.children);
        CHECK(info.is_transient());
        CHECK_FALSE(info.is_trivial());
        CHECK(info.ops.copy != nullptr);
        CHECK(info.ops.destruct != nullptr);
        CHECK(info.ops.relocate != nullptr);
        CHECK(info.size == sizeof(scene::children));
        CHECK(info.serializer.writeJson == nullptr);
        CHECK_FALSE(std::is_trivially_copyable_v<scene::children>);
        scene::children list = {};
        CHECK(list.entities.is_inline());
        for (u32 index = 0; index < 5; ++index) {
            list.entities.push_back(ecs::entity{index + 1, 1});
        }
        CHECK_FALSE(list.entities.is_inline());
        CHECK(list.entities.size() == 5);
    }

    TEST_CASE("asset_ref_raw - layout matches asset_ref") {
        // The bind pass views asset_ref<T> as {uuid, slot pointer}: the uuid leads, 24 bytes in all.
        CHECK(sizeof(assets::asset_ref<assets::binary_asset>) == 24);
        CHECK(sizeof(scene::prefab_instance) == 24);
        CHECK(std::is_standard_layout_v<assets::asset_ref<assets::binary_asset>>);
        const assets::asset_id id = test_uuid(7);
        assets::asset_slot slot = {};
        const assets::asset_ref<assets::binary_asset> reference(id, &slot);
        assets::asset_id leading = {};
        std::memcpy(&leading, &reference, sizeof(leading));
        CHECK(leading == id);
        const assets::asset_slot *trailing = nullptr;
        std::memcpy(&trailing, reinterpret_cast<const std::byte *>(&reference) + 16, sizeof(trailing));
        CHECK(trailing == &slot);

        // The registry records the field offset the bind pass walks.
        ecs::component_registry registry = {};
        CONJURE_REQUIRE_OK(scene::register_components(registry));
        const scene::scene_component_ids ids = scene::scene_component_ids::resolve(registry);
        const ecs::component_info &info = registry.info(ids.prefabInstance);
        REQUIRE(info.assetRefFieldOffsets.size() == 1);
        CHECK(info.assetRefFieldOffsets.front() == 0);
    }

    TEST_CASE("components - reflected components serialise their fields") {
        ecs::component_registry registry = {};
        CONJURE_REQUIRE_OK(scene::register_components(registry));
        const scene::scene_component_ids ids = scene::scene_component_ids::resolve(registry);
        CHECK(registry.info(ids.parent).entityFieldOffsets.size() == 1);
        CHECK(registry.info(ids.parent).entityFieldOffsets.front() == 0);
        CHECK(registry.info(ids.parent).serializer.writeJson != nullptr);
        CHECK(registry.info(ids.localTransform).serializer.writeJson != nullptr);
        CHECK(registry.info(ids.guid).serializer.writeJson != nullptr);
        CHECK(registry.info(ids.layer).size == 1);
        CHECK(registry.info(ids.disabled).is_tag());
        CHECK(registry.info(ids.worldTransform).serializer.writeJson == nullptr);
        CHECK(registry.info(ids.worldTransform).reflection != nullptr);
        CHECK(registry.info(ids.interpolatedTransform).reflection != nullptr);
        CHECK(sizeof(scene::parent) == 12);
        CHECK(sizeof(scene::transform_state) == 8);
        CHECK_FALSE(scene::transform_state{}.pending);
        CHECK(scene::MAX_HIERARCHY_DEPTH == 32);
        CHECK(scene::APPEND_CHILD == 0xFFFFFFFFu);
        CHECK(scene::SCENE_FORMAT_VERSION == 1);
    }
}
