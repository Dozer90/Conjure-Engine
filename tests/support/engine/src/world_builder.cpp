#include "conjure/testing/world_builder.h"

#include "conjure/testing/log_category.h"

#include "conjure/core/error.h"
#include "conjure/core/hash.h"
#include "conjure/core/log.h"
#include "conjure/core/random.h"
#include "conjure/core/string_id.h"
#include "conjure/scene/components.h"

#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    #include "conjure/core/json/document.h"
    #include "conjure/scripting/script_class_registry.h"
    #include "conjure/scripting/script_component.h"
    #include "conjure/scripting/script_runtime.h"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

using namespace conjure;
using namespace conjure::testing;

/*===========================================================================*/
/*===========================================================================*/
// Helpers

namespace {
    // Constant-evaluated ids are hashed, never interned (string_id.h): spawning inside a memory_guard
    // must not grow the process-wide name table.
    constexpr core::string_id ROOT_NAME("root");
    constexpr core::string_id NODE_NAME("node");
    constexpr core::string_id BOX_NAME("box");

    // Levels wider than this are never spawned (entity counts are u32); the per-level arithmetic saturates
    // here instead of overflowing, and a plan whose total exceeds a u32 is unusable.
    constexpr u64 MAX_LEVEL_ENTITIES = u64(1) << 40;
    constexpr u64 MAX_PLAN_ENTITIES = std::numeric_limits<u32>::max();

    /// <summary>
    /// What spawn_hierarchy will create for a shape: the entities per level (level 0 = roots) after the
    /// last level was padded or trimmed to the requested total, and their sum. Fixed storage: the tree is
    /// capped at the scene's depth limit (roots are depth 0, the deepest level the scene accepts is
    /// MAX_HIERARCHY_DEPTH - 1), so hierarchy_entity_count() can stay allocation-free and honestly noexcept.
    /// </summary>
    struct hierarchy_plan {
        std::array<u64, scene::MAX_HIERARCHY_DEPTH> levels = {};
        u32 levelCount = 0;
        u64 total = 0;
    };

    /// <summary>
    /// Builds the plan; false when the shape is unusable: no roots, more levels than the hierarchy allows,
    /// children below the roots without any children per node, or more entities than a u32 can count (a
    /// full tree too wide to spawn, or upper levels, which are never trimmed, already past the limit).
    /// </summary>
    [[nodiscard]] bool plan_hierarchy(const hierarchy_shape &shape, hierarchy_plan &outPlan) noexcept {
        outPlan = hierarchy_plan{};
        if (shape.rootCount == 0 || shape.depth >= scene::MAX_HIERARCHY_DEPTH) {
            return false;
        }
        if (shape.depth > 0 && shape.childrenPerNode == 0) {
            return false;
        }

        // The full tree, each level saturating at MAX_LEVEL_ENTITIES (32 levels of at most 2^40 never
        // overflow their sum).
        u64 current = shape.rootCount;
        outPlan.levels[0] = current;
        outPlan.levelCount = 1;
        for (u32 level = 1; level <= shape.depth; ++level) {
            if (current > MAX_LEVEL_ENTITIES / shape.childrenPerNode) {
                current = MAX_LEVEL_ENTITIES;
            } else {
                current = std::min(current * shape.childrenPerNode, MAX_LEVEL_ENTITIES);
            }
            outPlan.levels[level] = current;
            ++outPlan.levelCount;
        }

        // A zero total means "the full tree"; otherwise the last level absorbs the difference, and the
        // levels above it are never trimmed (a total smaller than them yields an empty last level).
        if (shape.totalEntityCount != 0) {
            u64 above = 0;
            for (u32 level = 0; level + 1 < outPlan.levelCount; ++level) {
                above += outPlan.levels[level];
            }
            outPlan.levels[outPlan.levelCount - 1] =
              shape.totalEntityCount > above ? shape.totalEntityCount - above : 0;
        }

        outPlan.total = 0;
        for (u32 level = 0; level < outPlan.levelCount; ++level) {
            outPlan.total += outPlan.levels[level];
        }
        return outPlan.total <= MAX_PLAN_ENTITIES;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Hierarchies

u32 conjure::testing::hierarchy_entity_count(const hierarchy_shape &shape) noexcept {
    hierarchy_plan plan = {};
    if (!plan_hierarchy(shape, plan)) {
        return 0;
    }
    return static_cast<u32>(plan.total);
}

std::vector<ecs::entity> conjure::testing::spawn_hierarchy(scene::scene &scene, const hierarchy_shape &shape) {
    std::vector<ecs::entity> created = {};
    hierarchy_plan plan = {};
    if (!plan_hierarchy(shape, plan)) {
        CONJURE_LOG_WARN(
          LOG_TESTING,
          "spawn_hierarchy: shape with {} roots, depth {}, {} children per node and total {} is unusable, "
          "nothing spawned",
          shape.rootCount,
          shape.depth,
          shape.childrenPerNode,
          shape.totalEntityCount
        );
        return created;
    }
    created.reserve(static_cast<usize>(plan.total));

    // Level 0: the roots.
    for (u64 index = 0; index < plan.levels[0]; ++index) {
        const core::result<ecs::entity> root = scene.create_entity(ROOT_NAME);
        if (!root.has_value()) {
            CONJURE_LOG_WARN(LOG_TESTING, "spawn_hierarchy: root creation failed: {}", root.get_error().to_string());
            return created;
        }
        created.push_back(root.value());
    }

    // Deeper levels: the children of a level are spread over its parents round-robin so a padded or
    // trimmed last level stays balanced. Only the last level can be empty (the plan rejects zero children
    // per node), so every level below the roots has parents to hang from.
    usize parentBegin = 0;
    usize parentCount = created.size();
    for (u32 level = 1; level < plan.levelCount; ++level) {
        const u64 count = plan.levels[level];
        const usize childBegin = created.size();
        for (u64 index = 0; index < count; ++index) {
            const ecs::entity parent = created[parentBegin + static_cast<usize>(index % parentCount)];
            const core::result<ecs::entity> child = scene.create_entity(NODE_NAME, parent);
            if (!child.has_value()) {
                CONJURE_LOG_WARN(
                  LOG_TESTING,
                  "spawn_hierarchy: child creation failed at level {}: {}",
                  level,
                  child.get_error().to_string()
                );
                return created;
            }
            created.push_back(child.value());
        }
        parentBegin = childBegin;
        parentCount = created.size() - childBegin;
        if (parentCount == 0) {
            break;
        }
    }
    return created;
}


/*===========================================================================*/
/*===========================================================================*/
// Bodies and scripts

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
core::result<ecs::entity> conjure::testing::spawn_box_body(
  scene::scene &scene,
  const math::vec3 &position,
  physics::motion_type motion
) {
    CONJURE_TRY_ASSIGN(const ecs::entity body, scene.create_entity(BOX_NAME));

    scene::local_transform transform = {};
    transform.position = position;
    CONJURE_TRY(scene.set_local_transform(body, transform));

    physics::rigid_body rigidBody = {};
    rigidBody.motionType = motion;
    CONJURE_TRY(scene.world().add<physics::rigid_body>(body, rigidBody));
    CONJURE_TRY(scene.world().add<physics::box_collider>(body, physics::box_collider{}));
    return body;
}
#endif

#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
core::result<ecs::entity> conjure::testing::spawn_script_entity(
  scene::scene &scene,
  std::string_view name,
  std::string_view scriptType
) {
    scripting::script_runtime *runtime = scripting::script_runtime::active();
    if (runtime == nullptr) {
        return core::make_error(core::error_code::INVALID_STATE, "no script_runtime is active in this process");
    }

    // Hashed, not interned (see ROOT_NAME): headless_engine::find_entity resolves the same hash.
    const core::string_id nameId = core::string_id::from_hash(core::hash::fnv1a64(name));
    CONJURE_TRY_ASSIGN(const ecs::entity scripted, scene.create_entity(nameId));
    const core::json::document fields = core::json::document::object();
    const scripting::script_entry_id entry = runtime->add_script(
      scripted,
      scripting::script_name_hash(scriptType),
      fields.root()
    );
    if (!entry.is_valid()) {
        CONJURE_UNUSED(scene.destroy_now(scripted));
        return core::make_error(
          core::error_code::INVALID_STATE,
          "script_runtime refused the script '{}' on entity '{}'",
          scriptType,
          name
        );
    }
    return scripted;
}
#endif


/*===========================================================================*/
/*===========================================================================*/
// Dirty marking

std::vector<ecs::entity> conjure::testing::mark_dirty_fraction(
  scene::scene &scene,
  std::span<const ecs::entity> entities,
  f32 fraction,
  u64 seed
) {
    std::vector<ecs::entity> tagged = {};
    if (entities.empty()) {
        return tagged;
    }

    const f32 clamped = std::clamp(fraction, 0.0f, 1.0f);
    const usize wanted = static_cast<usize>(std::lround(clamped * static_cast<f32>(entities.size())));
    if (wanted == 0) {
        return tagged;
    }

    // Partial Fisher-Yates over an index table: the first 'wanted' slots hold a uniform sample that
    // depends only on the seed and the input order.
    std::vector<u32> indices(entities.size());
    for (u32 index = 0; index < indices.size(); ++index) {
        indices[index] = index;
    }
    core::random::random_engine generator(seed);
    const u32 lastIndex = static_cast<u32>(indices.size() - 1);
    for (u32 slot = 0; slot < wanted; ++slot) {
        const u32 pick = generator.uniform<u32>(slot, lastIndex);
        std::swap(indices[slot], indices[pick]);
    }

    ecs::world &world = scene.world();
    ecs::command_buffer commands(world);
    tagged.reserve(wanted);
    for (u32 slot = 0; slot < wanted; ++slot) {
        const ecs::entity target = entities[indices[slot]];
        if (!scene.local_transform_of(target).has_value() || world.has<scene::transform_dirty>(target)) {
            continue;
        }
        commands.add<scene::transform_dirty>(target);
        tagged.push_back(target);
    }
    CONJURE_UNUSED(world.flush(commands));

    std::sort(tagged.begin(), tagged.end(), [](const ecs::entity &a, const ecs::entity &b) {
        return a.index != b.index ? a.index < b.index : a.generation < b.generation;
    });
    return tagged;
}
