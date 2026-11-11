#include "conjure/editor/picker.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/string_id.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/world.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/renderer/components.h"
    #include "conjure/renderer/mesh_asset.h"
#endif
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #include "conjure/physics/physics_world.h"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace conjure::editor {

    namespace {
        constexpr f32 FIXED_HALF_EXTENT = 0.5f;
        constexpr usize PARALLEL_THRESHOLD = 4096;

        constexpr std::string_view MESH_RENDERER_NAME = "conjure.renderer.mesh_renderer";
        constexpr std::string_view SPRITE_NAME = "conjure.renderer.sprite";
        constexpr std::string_view CAMERA_NAME = "conjure.renderer.camera";
        constexpr std::string_view LIGHT_NAME = "conjure.renderer.light";

        struct broad_hit {
            usize candidate = 0;
            f32 entry = 0.0f;
            f32 volume = 0.0f;
        };

        [[nodiscard]] math::aabb fixed_box(const math::vec3 &position) noexcept {
            return math::aabb::from_center_extents(position, math::vec3(FIXED_HALF_EXTENT));
        }

        [[nodiscard]] std::optional<f32> intersect_mesh(
          const math::ray &ray,
          const math::mat4 &worldMatrix,
          std::span<const math::vec3> positions,
          std::span<const u32> indices,
          math::vec3 &outNormal
        ) {
            // Test in object space: transform the ray by the inverse world matrix.
            const math::mat4 objectFromWorld = glm::inverse(worldMatrix);
            const math::vec3 origin = math::vec3(objectFromWorld * math::vec4(ray.origin, 1.0f));
            const math::vec3 directionObject = math::vec3(objectFromWorld * math::vec4(ray.direction, 0.0f));
            const f32 scale = glm::length(directionObject);
            if (scale <= 0.0f) {
                return std::nullopt;
            }
            math::ray local = {};
            local.origin = origin;
            local.direction = directionObject / scale;
            std::optional<f32> best = std::nullopt;
            math::vec3 bestNormal = math::vec3(0.0f, 1.0f, 0.0f);
            for (usize index = 0; index + 2 < indices.size(); index += 3) {
                const u32 a = indices[index];
                const u32 b = indices[index + 1];
                const u32 c = indices[index + 2];
                if (a >= positions.size() || b >= positions.size() || c >= positions.size()) {
                    continue;
                }
                const std::optional<f32> hit = local.intersect_triangle(
                  positions[a],
                  positions[b],
                  positions[c],
                  false
                );
                if (hit.has_value() && (!best.has_value() || *hit < *best)) {
                    best = hit;
                    bestNormal = glm::cross(positions[b] - positions[a], positions[c] - positions[a]);
                }
            }
            if (!best.has_value()) {
                return std::nullopt;
            }
            const math::vec3 worldNormal = math::vec3(glm::transpose(objectFromWorld) * math::vec4(bestNormal, 0.0f));
            outNormal = math::safe_normalize(worldNormal, math::vec3(0.0f, 1.0f, 0.0f));
            // The object-space distance is in object units along a direction of length 1 / scale world units.
            return *best / scale;
        }

        enum class narrow_result : u8 {
            NO_GEOMETRY,        // nothing resident to test: the bounds hit stands
            MISS,               // triangles present, the ray passes between them
            HIT
        };

        [[nodiscard]] narrow_result narrow_phase(
          const math::ray &ray,
          const pick_candidate &candidate,
          const pick_context &context,
          f32 &outDistance,
          math::vec3 &outNormal
        ) {
            if (context.scene == nullptr) {
                return narrow_result::NO_GEOMETRY;
            }
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
            const scene::scene &scene = *context.scene;
            const ecs::world &world = scene.world();
            const ecs::component_registry &registry = world.registry();
            const std::optional<math::transform> worldTransform = scene.world_transform_of(candidate.entity);
            if (!worldTransform.has_value()) {
                return narrow_result::NO_GEOMETRY;
            }
            const math::mat4 worldMatrix = worldTransform->to_matrix();

            const ecs::component_id meshId = registry.find(core::string_id(MESH_RENDERER_NAME));
            if (meshId.is_valid() && world.has_raw(candidate.entity, meshId)) {
                const void *raw = world.get_raw(candidate.entity, meshId);
                if (raw != nullptr) {
                    const renderer::mesh_renderer_component &component =
                      *static_cast<const renderer::mesh_renderer_component *>(raw);
                    const renderer::mesh_asset *mesh = component.mesh.get();
                    if (mesh != nullptr) {
                        const renderer::cpu_geometry_view geometry = mesh->cpu_geometry();
                        if (!geometry.positions.empty() && !geometry.indices.empty()) {
                            const std::optional<f32> hit =
                              intersect_mesh(ray, worldMatrix, geometry.positions, geometry.indices, outNormal);
                            if (!hit.has_value()) {
                                return narrow_result::MISS;
                            }
                            outDistance = *hit;
                            return narrow_result::HIT;
                        }
                    }
                }
                return narrow_result::NO_GEOMETRY;
            }

            const ecs::component_id spriteId = registry.find(core::string_id(SPRITE_NAME));
            if (spriteId.is_valid() && world.has_raw(candidate.entity, spriteId)) {
                const void *raw = world.get_raw(candidate.entity, spriteId);
                if (raw != nullptr) {
                    const renderer::sprite_component &sprite = *static_cast<const renderer::sprite_component *>(raw);
                    const math::vec3 half(sprite.size.x * 0.5f, sprite.size.y * 0.5f, 0.0f);
                    const std::array<math::vec3, 4> corners = {
                        math::vec3(-half.x, -half.y, 0.0f),
                        math::vec3(half.x, -half.y, 0.0f),
                        math::vec3(half.x, half.y, 0.0f),
                        math::vec3(-half.x, half.y, 0.0f),
                    };
                    const std::array<u32, 6> indices = {0, 1, 2, 0, 2, 3};
                    const std::optional<f32> hit = intersect_mesh(ray, worldMatrix, corners, indices, outNormal);
                    if (!hit.has_value()) {
                        return narrow_result::MISS;
                    }
                    outDistance = *hit;
                    return narrow_result::HIT;
                }
            }
#else
            (void) ray;
            (void) candidate;
            (void) outDistance;
            (void) outNormal;
#endif
            return narrow_result::NO_GEOMETRY;
        }

        [[nodiscard]] bool has_geometry(const pick_candidate &candidate, const pick_context &context) noexcept {
            if (context.scene == nullptr) {
                return false;
            }
            const ecs::world &world = context.scene->world();
            const ecs::component_registry &registry = world.registry();
            const ecs::component_id meshId = registry.find(core::string_id(MESH_RENDERER_NAME));
            const ecs::component_id spriteId = registry.find(core::string_id(SPRITE_NAME));
            return (meshId.is_valid() && world.has_raw(candidate.entity, meshId)) ||
              (spriteId.is_valid() && world.has_raw(candidate.entity, spriteId));
        }
    }

    std::string_view to_string(pick_mode mode) noexcept {
        switch (mode) {
            case pick_mode::MESH:
                return "mesh";
            case pick_mode::BOUNDS:
                return "bounds";
            case pick_mode::PHYSICS:
                return "physics";
        }
        return "";
    }

    std::optional<pick_mode> parse_pick_mode(std::string_view text) noexcept {
        if (text == "mesh") {
            return pick_mode::MESH;
        }
        if (text == "bounds") {
            return pick_mode::BOUNDS;
        }
        if (text == "physics") {
            return pick_mode::PHYSICS;
        }
        return std::nullopt;
    }

    core::json::document pick_result::to_json() const {
        core::json::document document = core::json::document::object();
        document.set("hit", hit);
        document.set("guid", hit ? guid.to_string() : std::string(""));
        core::json::document pointArray = core::json::document::array();
        pointArray.push_back(point.x);
        pointArray.push_back(point.y);
        pointArray.push_back(point.z);
        document.set("point", pointArray);
        core::json::document normalArray = core::json::document::array();
        normalArray.push_back(normal.x);
        normalArray.push_back(normal.y);
        normalArray.push_back(normal.z);
        document.set("normal", normalArray);
        document.set("distance", distance);
        return document;
    }

    std::vector<pick_candidate> gather_pick_candidates(const scene::scene &scene) {
        std::vector<pick_candidate> candidates = {};
        const ecs::world &world = scene.world();
        const ecs::component_registry &registry = world.registry();
        const ecs::component_id meshId = registry.find(core::string_id(MESH_RENDERER_NAME));
        const ecs::component_id spriteId = registry.find(core::string_id(SPRITE_NAME));
        const ecs::component_id cameraId = registry.find(core::string_id(CAMERA_NAME));
        const ecs::component_id lightId = registry.find(core::string_id(LIGHT_NAME));

        std::vector<ecs::entity> entities = {};
        for (const scene::root_entry &root : scene.roots()) {
            scene.tree().collect_subtree(root.entity, entities);
        }
        candidates.reserve(entities.size());
        for (const ecs::entity entity : entities) {
            const std::optional<math::transform> worldTransform = scene.world_transform_of(entity);
            if (!worldTransform.has_value()) {
                continue;
            }
            const bool renderable = (meshId.is_valid() && world.has_raw(entity, meshId)) ||
              (spriteId.is_valid() && world.has_raw(entity, spriteId));
            const bool framed = (cameraId.is_valid() && world.has_raw(entity, cameraId)) ||
              (lightId.is_valid() && world.has_raw(entity, lightId));
            if (!renderable && !framed) {
                continue;
            }
            pick_candidate candidate = {};
            candidate.entity = entity;
            candidate.guid = scene.guid_of(entity).value_or(core::uuid::null());
            std::optional<math::aabb> bounds = renderable ? scene.world_bounds(entity) : std::nullopt;
            if (!bounds.has_value() || !bounds->is_valid()) {
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
                if (meshId.is_valid() && world.has_raw(entity, meshId)) {
                    const void *raw = world.get_raw(entity, meshId);
                    const renderer::mesh_asset *mesh =
                      raw == nullptr ? nullptr : static_cast<const renderer::mesh_renderer_component *>(
                        raw
                      )->mesh.get();
                    if (mesh != nullptr && mesh->bounds().is_valid()) {
                        bounds = mesh->bounds().transformed(worldTransform->to_matrix());
                    }
                } else if (spriteId.is_valid() && world.has_raw(entity, spriteId)) {
                    const void *raw = world.get_raw(entity, spriteId);
                    if (raw != nullptr) {
                        const renderer::sprite_component &sprite = *static_cast<const renderer::sprite_component *>(
                          raw
                        );
                        const math::aabb local = math::aabb::from_center_extents(
                          math::vec3(0.0f),
                          math::vec3(sprite.size.x * 0.5f, sprite.size.y * 0.5f, 0.001f)
                        );
                        bounds = local.transformed(worldTransform->to_matrix());
                    }
                }
#endif
            }
            if (!bounds.has_value() || !bounds->is_valid()) {
                bounds = fixed_box(worldTransform->position);
            }
            candidate.worldBounds = *bounds;
            candidates.push_back(candidate);
        }
        return candidates;
    }

    pick_result pick(
      const math::ray &ray,
      std::span<const pick_candidate> candidates,
      pick_mode mode,
      const pick_context &context
    ) {
        pick_result result = {};
        if (mode == pick_mode::PHYSICS) {
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
            if (context.physics == nullptr || context.scene == nullptr) {
                return result;
            }
            const std::optional<physics::raycast_hit> hit = context.physics->raycast(ray, context.maxDistance);
            if (!hit.has_value()) {
                return result;
            }
            result.hit = true;
            result.point = hit->point;
            result.normal = hit->normal;
            result.distance = hit->distance;
            result.guid = context.scene->guid_of(hit->entity).value_or(core::uuid::null());
#endif
            return result;
        }

        // Broad phase: slab test over every candidate, parallel above the threshold.
        std::vector<broad_hit> hits = {};
        std::vector<f32> entries(candidates.size(), -1.0f);
        const auto test = [&](usize begin, usize end) {
            for (usize index = begin; index < end; ++index) {
                const std::optional<f32> entry = ray.intersect(candidates[index].worldBounds);
                entries[index] = entry.has_value() ? *entry : -1.0f;
            }
        };
        if (context.jobs != nullptr && candidates.size() > PARALLEL_THRESHOLD) {
            context.jobs->parallel_for(candidates.size(), 1024, test);
        } else {
            test(0, candidates.size());
        }
        for (usize index = 0; index < candidates.size(); ++index) {
            if (entries[index] >= 0.0f && entries[index] <= context.maxDistance) {
                hits.push_back(broad_hit{index, entries[index], candidates[index].worldBounds.volume()});
            }
        }
        if (hits.empty()) {
            return result;
        }
        std::sort(hits.begin(), hits.end(), [](const broad_hit &left, const broad_hit &right) {
            if (left.entry != right.entry) {
                return left.entry < right.entry;
            }
            if (left.volume != right.volume) {
                return left.volume < right.volume;
            }
            return left.candidate < right.candidate;
        });

        // Narrow phase: in entry order until a confirmed hit is closer than the next entry distance. A
        // candidate without CPU geometry counts as hit at its entry distance.
        std::optional<f32> bestDistance = std::nullopt;
        f32 bestVolume = 0.0f;
        for (const broad_hit &hit : hits) {
            if (bestDistance.has_value() && hit.entry > *bestDistance) {
                break;
            }
            const pick_candidate &candidate = candidates[hit.candidate];
            f32 distance = hit.entry;
            math::vec3 normal = math::vec3(0.0f, 1.0f, 0.0f);
            if (mode == pick_mode::MESH && has_geometry(candidate, context)) {
                const narrow_result tested = narrow_phase(ray, candidate, context, distance, normal);
                if (tested == narrow_result::MISS) {
                    continue;
                }
                if (tested == narrow_result::NO_GEOMETRY) {
                    distance = hit.entry;
                    normal = math::vec3(0.0f, 1.0f, 0.0f);
                }
            }
            const bool closer = !bestDistance.has_value() || distance < *bestDistance ||
              (distance == *bestDistance && hit.volume < bestVolume);
            if (closer) {
                bestDistance = distance;
                bestVolume = hit.volume;
                result.hit = true;
                result.guid = candidate.guid;
                result.distance = distance;
                result.point = ray.at(distance);
                result.normal = normal;
            }
        }
        return result;
    }
}
