#pragma once

#include "conjure/assets/asset_ref.h"
#include "conjure/core/containers/small_vector.h"
#include "conjure/core/enum_flags.h"
#include "conjure/core/math/aabb.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/math/transform.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"

#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

// The scene components (docs/design/13_scene.md 2.1). Plain structs with camelCase fields (R31), named
// "conjure.scene.<name>" and registered in a fixed order by register_components() (register.h) so the
// ids agree in every world of a process. The hierarchy is `parent` (the single serialised truth) plus the
// TRANSIENT `children` cache; depth is encoded as one depth_tag<D> per level so the transform pass can
// run one level at a time over chunks (S2). interpolated_transform is defined here but added, written and
// removed by physics (R47).

namespace conjure::scene {

    struct prefab_asset;

    /// <summary>
    /// Depth 0 is a root; children carry depth_tag<1> .. depth_tag<31>.
    /// </summary>
    inline constexpr u32 MAX_HIERARCHY_DEPTH = 32;

    /// <summary>
    /// Sibling index meaning "after the last child" (or "after the last root").
    /// </summary>
    inline constexpr u32 APPEND_CHILD = 0xFFFFFFFFu;

    /// <summary>
    /// Version written into every scene and prefab file; a newer version is UNSUPPORTED_VERSION.
    /// </summary>
    inline constexpr u32 SCENE_FORMAT_VERSION = 1;

    /// <summary>
    /// Link to the parent entity plus the position among its siblings. Roots have no parent component.
    /// </summary>
    struct parent {
        ecs::entity value = ecs::NULL_ENTITY;
        u32 siblingIndex = 0;

        [[nodiscard]] bool operator==(const parent &) const = default;
    };

    /// <summary>
    /// Cache of the children in sibling order, rebuilt from `parent` on load (TRANSIENT, S3).
    /// </summary>
    struct children {
        core::small_vector<ecs::entity, 4> entities = {};
    };

    /// <summary>
    /// Position, rotation and scale relative to the parent (or to the world for roots). Byte-identical
    /// to math::transform and mirrored by the C# LocalTransform struct.
    /// </summary>
    struct local_transform {
        math::vec3 position = math::vec3(0.0f);
        math::quat rotation = math::quat(1.0f, 0.0f, 0.0f, 0.0f);
        math::vec3 scale = math::vec3(1.0f);

        /// <summary>
        /// The same values as a math::transform.
        /// </summary>
        [[nodiscard]] math::transform to_transform() const noexcept;

        /// <summary>
        /// A local_transform holding the values of a math::transform.
        /// </summary>
        [[nodiscard]] static local_transform from_transform(const math::transform &value) noexcept;

        [[nodiscard]] bool operator==(const local_transform &) const = default;
    };

    static_assert(sizeof(local_transform) == sizeof(math::transform), "local_transform mirrors math::transform");
    static_assert(sizeof(local_transform) == 40, "local_transform must be 10 floats for the C# mirror");
    static_assert(std::is_trivially_copyable_v<local_transform>, "local_transform must be trivially copyable");
    static_assert(offsetof(local_transform, position) == offsetof(math::transform, position));
    static_assert(offsetof(local_transform, rotation) == offsetof(math::transform, rotation));
    static_assert(offsetof(local_transform, scale) == offsetof(math::transform, scale));

    /// <summary>
    /// Simulation truth: the world matrix computed by scene.propagate_transforms (TRANSIENT).
    /// </summary>
    struct world_transform {
        math::mat4 matrix = math::mat4(1.0f);
    };

    /// <summary>
    /// What renderer extraction reads: the world matrix, or the interpolated pose for physics-driven
    /// entities and their descendants (TRANSIENT).
    /// </summary>
    struct render_world_transform {
        math::mat4 matrix = math::mat4(1.0f);
    };

    /// <summary>
    /// World-space poses of the two most recent fixed steps. Scene defines and registers it; physics
    /// adds, writes and removes it (R47).
    /// </summary>
    struct interpolated_transform {
        math::transform previous = {};
        math::transform current = {};
    };

    /// <summary>
    /// Per-entity propagation flags.
    /// </summary>
    enum class transform_state_flags : u8 {
        NONE = 0,
        INTERPOLATED_SUBTREE = 1 << 0    // self or an ancestor carries interpolated_transform
    };
    CONJURE_ENUM_FLAGS(transform_state_flags);

    /// <summary>
    /// Bookkeeping of the propagation pass: the run that last recomputed the world matrix (children
    /// compare it against the current run) and the flags (TRANSIENT, EDITOR_HIDDEN).
    /// </summary>
    struct transform_state {
        u32 lastUpdateRun = 0;
        transform_state_flags flags = transform_state_flags::NONE;
    };

    /// <summary>
    /// Set by structural hierarchy changes; cleared by the propagation pass (TRANSIENT, EDITOR_HIDDEN).
    /// </summary>
    struct transform_dirty {};

    /// <summary>
    /// Depth marker: entities at hierarchy depth Depth carry depth_tag&lt;Depth&gt;; roots carry none
    /// (TRANSIENT, EDITOR_HIDDEN). Registered as "conjure.scene.depth_01" .. "conjure.scene.depth_31".
    /// </summary>
    template <u8 Depth>
    struct depth_tag {
        static_assert(Depth >= 1 && Depth < MAX_HIERARCHY_DEPTH, "depth tags cover depths 1 .. 31");
    };

    /// <summary>
    /// Display name, interned in every configuration.
    /// </summary>
    struct name {
        core::string_id value = {};

        [[nodiscard]] bool operator==(const name &) const = default;
    };

    /// <summary>
    /// Gameplay tag.
    /// </summary>
    struct tag {
        core::string_id value = {};

        [[nodiscard]] bool operator==(const tag &) const = default;
    };

    /// <summary>
    /// Layer 0..31; bit (1 &lt;&lt; index) matches the renderer's layerMask. Names live in layer_table.
    /// </summary>
    struct layer {
        u8 index = 0;

        [[nodiscard]] bool operator==(const layer &) const = default;
    };

    /// <summary>
    /// Editor identity, indexed by guid_index.
    /// </summary>
    struct guid {
        core::uuid value = {};

        [[nodiscard]] bool operator==(const guid &) const = default;
    };

    /// <summary>
    /// Self state: the entity is switched off (serialised as {}).
    /// </summary>
    struct disabled {};

    /// <summary>
    /// On every descendant of a disabled entity; consumers filter without&lt;disabled&gt;().without&lt;inactive&gt;()
    /// (TRANSIENT, EDITOR_HIDDEN).
    /// </summary>
    struct inactive {};

    /// <summary>
    /// Local-space bounds written by engine subsystems; scene::world_bounds transforms them (TRANSIENT).
    /// </summary>
    struct local_bounds {
        math::aabb box = {};
    };

    /// <summary>
    /// On the root of a prefab instance: the prefab asset it was instantiated from (24 bytes, the C#
    /// PrefabInstance mirror).
    /// </summary>
    struct prefab_instance {
        assets::asset_ref<prefab_asset> prefab = {};
    };

    /// <summary>
    /// On every entity instantiated from a prefab: the guid of the matching entry in the prefab file.
    /// </summary>
    struct prefab_link {
        core::uuid assetGuid = {};

        [[nodiscard]] bool operator==(const prefab_link &) const = default;
    };

    static_assert(sizeof(prefab_instance) == 24, "prefab_instance is the 24-byte C# PrefabInstance mirror");
    static_assert(std::is_trivially_copyable_v<prefab_instance>, "prefab_instance must be trivially copyable");

    namespace detail {
        inline constexpr std::string_view DEPTH_TAG_PREFIX = "conjure.scene.depth_";
        inline constexpr usize DEPTH_TAG_NAME_LENGTH = DEPTH_TAG_PREFIX.size() + 2;

        /// <summary>
        /// Builds "conjure.scene.depth_DD" at compile time.
        /// </summary>
        [[nodiscard]] constexpr std::array<char, DEPTH_TAG_NAME_LENGTH> make_depth_tag_name(u8 depth) noexcept {
            std::array<char, DEPTH_TAG_NAME_LENGTH> text = {};
            for (usize index = 0; index < DEPTH_TAG_PREFIX.size(); ++index) {
                text[index] = DEPTH_TAG_PREFIX[index];
            }
            text[DEPTH_TAG_PREFIX.size()] = static_cast<char>('0' + depth / 10);
            text[DEPTH_TAG_PREFIX.size() + 1] = static_cast<char>('0' + depth % 10);
            return text;
        }

        /// <summary>
        /// Static storage for the name of one depth tag so a string_view may point at it.
        /// </summary>
        template <u8 Depth>
        struct depth_tag_name {
            static constexpr std::array<char, DEPTH_TAG_NAME_LENGTH> CHARACTERS = make_depth_tag_name(Depth);
            static constexpr std::string_view VALUE = std::string_view(CHARACTERS.data(), CHARACTERS.size());
        };
    }

    /// <summary>
    /// The component ids of every scene component in one registry, resolved once (depth[0] is INVALID:
    /// roots carry no tag).
    /// </summary>
    struct scene_component_ids {
        ecs::component_id parent = {};
        ecs::component_id children = {};
        ecs::component_id localTransform = {};
        ecs::component_id worldTransform = {};
        ecs::component_id renderWorldTransform = {};
        ecs::component_id interpolatedTransform = {};
        ecs::component_id transformState = {};
        ecs::component_id transformDirty = {};
        std::array<ecs::component_id, MAX_HIERARCHY_DEPTH> depth = {};
        ecs::component_id name = {};
        ecs::component_id tag = {};
        ecs::component_id layer = {};
        ecs::component_id guid = {};
        ecs::component_id disabled = {};
        ecs::component_id inactive = {};
        ecs::component_id localBounds = {};
        ecs::component_id prefabInstance = {};
        ecs::component_id prefabLink = {};

        /// <summary>
        /// Looks every scene component up by name. Ids of unregistered types stay INVALID; see
        /// is_complete().
        /// </summary>
        [[nodiscard]] static scene_component_ids resolve(const ecs::component_registry &registry) noexcept;

        /// <summary>
        /// True when every scene component (the 31 depth tags included) is registered.
        /// </summary>
        [[nodiscard]] bool is_complete() const noexcept;

        /// <summary>
        /// The depth encoded in a component mask: the D of the depth_tag it carries, 0 for none.
        /// </summary>
        [[nodiscard]] u8 depth_of(const ecs::component_mask &mask) const noexcept;

        /// <summary>
        /// True when the mask holds every transform companion of local_transform.
        /// </summary>
        [[nodiscard]] bool has_transform_companions(const ecs::component_mask &mask) const noexcept;
    };
}

CONJURE_REFLECT_STRUCT(conjure::scene::parent, "conjure.scene.parent",
    CONJURE_FIELD(value),
    CONJURE_FIELD(siblingIndex, .readOnly = true));
CONJURE_REFLECT_STRUCT(conjure::scene::local_transform, "conjure.scene.local_transform",
    CONJURE_FIELD(position),
    CONJURE_FIELD(rotation),
    CONJURE_FIELD(scale));
CONJURE_REFLECT_STRUCT(conjure::scene::world_transform, "conjure.scene.world_transform",
    CONJURE_FIELD(matrix, .readOnly = true));
CONJURE_REFLECT_STRUCT(conjure::scene::render_world_transform, "conjure.scene.render_world_transform",
    CONJURE_FIELD(matrix, .readOnly = true));
CONJURE_REFLECT_STRUCT(conjure::scene::interpolated_transform, "conjure.scene.interpolated_transform",
    CONJURE_FIELD(previous, .readOnly = true),
    CONJURE_FIELD(current, .readOnly = true));
CONJURE_REFLECT_STRUCT(conjure::scene::name, "conjure.scene.name", CONJURE_FIELD(value));
CONJURE_REFLECT_STRUCT(conjure::scene::tag, "conjure.scene.tag", CONJURE_FIELD(value));
CONJURE_REFLECT_STRUCT(conjure::scene::layer, "conjure.scene.layer", CONJURE_FIELD(index, .min = 0.0, .max = 31.0));
CONJURE_REFLECT_STRUCT(conjure::scene::guid, "conjure.scene.guid", CONJURE_FIELD(value, .readOnly = true));
CONJURE_REFLECT_STRUCT(conjure::scene::prefab_instance, "conjure.scene.prefab_instance", CONJURE_FIELD(prefab));
CONJURE_REFLECT_STRUCT(conjure::scene::prefab_link, "conjure.scene.prefab_link",
    CONJURE_FIELD(assetGuid, .readOnly = true));

CONJURE_COMPONENT(conjure::scene::parent, "conjure.scene.parent");
CONJURE_COMPONENT(conjure::scene::children, "conjure.scene.children");
CONJURE_COMPONENT(conjure::scene::local_transform, "conjure.scene.local_transform");
CONJURE_COMPONENT(conjure::scene::world_transform, "conjure.scene.world_transform");
CONJURE_COMPONENT(conjure::scene::render_world_transform, "conjure.scene.render_world_transform");
CONJURE_COMPONENT(conjure::scene::interpolated_transform, "conjure.scene.interpolated_transform");
CONJURE_COMPONENT(conjure::scene::transform_state, "conjure.scene.transform_state");
CONJURE_COMPONENT(conjure::scene::transform_dirty, "conjure.scene.transform_dirty");
CONJURE_COMPONENT(conjure::scene::name, "conjure.scene.name");
CONJURE_COMPONENT(conjure::scene::tag, "conjure.scene.tag");
CONJURE_COMPONENT(conjure::scene::layer, "conjure.scene.layer");
CONJURE_COMPONENT(conjure::scene::guid, "conjure.scene.guid");
CONJURE_COMPONENT(conjure::scene::disabled, "conjure.scene.disabled");
CONJURE_COMPONENT(conjure::scene::inactive, "conjure.scene.inactive");
CONJURE_COMPONENT(conjure::scene::local_bounds, "conjure.scene.local_bounds");
CONJURE_COMPONENT(conjure::scene::prefab_instance, "conjure.scene.prefab_instance");
CONJURE_COMPONENT(conjure::scene::prefab_link, "conjure.scene.prefab_link");

// Every depth_tag<D> is named "conjure.scene.depth_DD" through one partial specialisation.
template <conjure::u8 Depth>
struct conjure::ecs::component_name<conjure::scene::depth_tag<Depth>> {
    static constexpr std::string_view VALUE = conjure::scene::detail::depth_tag_name<Depth>::VALUE;
};
