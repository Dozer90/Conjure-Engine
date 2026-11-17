#include "editor/core/commands/command_support.h"

#include "conjure/editor/log_category.h"

#include "conjure/core/math/ray.h"
#include "conjure/core/platform_detect.h"

#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #include "conjure/physics/physics_layers.h"
    #include "conjure/physics/physics_world.h"
    #include "conjure/physics/queries.h"
#endif

#include <array>
#include <format>
#include <utility>
#include <vector>

// physics.* (docs/design/04_editor.md 4.4, R13): read-only queries against the physics world. The fixed loop
// does not run while EDITING, so the bodies are exactly what the scene describes.

namespace conjure::editor::support {

    namespace {
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
        constexpr u32 MAX_OVERLAP_RESULTS = 4096;

        core::result<physics::physics_world *> require_physics(editor_context &context) {
            physics::physics_world *world = context.physics();
            if (world == nullptr) {
                return not_available("the physics world is not running (physics.enabled is false)");
            }
            return world;
        }

        /// <summary>
        /// {layers?: [names], includeTriggers?, ignore?} into a query filter.
        /// </summary>
        core::result<physics::query_filter> read_filter(
          command_invocation &invocation,
          const physics::physics_world &world
        ) {
            const core::json::value &args = invocation.args;
            physics::query_filter filter = {};
            filter.includeTriggers = bool_or(args, "includeTriggers", false);
            filter.includeStatic = bool_or(args, "includeStatic", true);
            if (args.contains("ignore") && !args["ignore"].is_null()) {
                CONJURE_TRY_ASSIGN(filter.ignore, resolve_entity(invocation, args["ignore"]));
            }
            const core::json::value layers = args["layers"];
            if (layers.is_array()) {
                physics::physics_layer_mask mask = physics::physics_layer_mask::none();
                for (usize index = 0; index < layers.size(); ++index) {
                    const core::json::value entry = layers[index];
                    if (entry.is_number()) {
                        // A whole real such as 3.0 names layer 3, as the schema's "integer" allows; as_i64
                        // answers -1 for a fraction.
                        const i64 layer = entry.as_i64(-1);
                        if (layer < 0 || layer >= static_cast<i64>(physics::MAX_PHYSICS_LAYERS)) {
                            return invalid_args(std::format("args.layers[{}]: expected 0..31", index));
                        }
                        mask = mask.with(static_cast<u8>(layer));
                        continue;
                    }
                    if (!entry.is_string()) {
                        return invalid_args(std::format("args.layers[{}]: expected a layer name or index", index));
                    }
                    const std::optional<u8> layer = world.layers().find(core::string_id(entry.as_string()));
                    if (!layer.has_value()) {
                        return invalid_args(std::format(
                          "args.layers[{}]: unknown physics layer '{}'",
                          index,
                          entry.as_string()
                        ));
                    }
                    mask = mask.with(*layer);
                }
                filter.layers = mask;
            }
            return filter;
        }

        core::json::document hit_json(
          scene::scene &scene,
          const physics::physics_world &world,
          const physics::raycast_hit &hit
        ) {
            core::json::document result = core::json::object({
              {"guid", guid_or_empty(scene, hit.entity)},
              {"point", vec3_json(hit.point)},
              {"normal", vec3_json(hit.normal)},
              {"distance", hit.distance},
              {"fraction", hit.fraction},
              {"layer", world.layers().name(hit.layer).to_string()},
              {"isTrigger", hit.isTrigger},
            });
            if (scene.world().is_alive(hit.entity)) {
                result.set("entity", entity_summary(scene, hit.entity));
            }
            return result;
        }

        core::result<command_outcome> physics_raycast(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(physics::physics_world *world, require_physics(context));
            math::ray ray = {};
            CONJURE_TRY_ASSIGN(ray.origin, read_vec3(invocation.args["origin"], "args.origin"));
            CONJURE_TRY_ASSIGN(ray.direction, read_vec3(invocation.args["direction"], "args.direction"));
            const f32 length = glm::length(ray.direction);
            if (length <= 1.0e-6f) {
                return invalid_args("args.direction: a zero direction is not a ray");
            }
            ray.direction /= length;
            CONJURE_TRY_ASSIGN(const f32 maxDistance, f32_or(invocation.args, "maxDistance", 1000.0f));
            if (maxDistance <= 0.0f) {
                return invalid_args("args.maxDistance: expected a positive distance");
            }
            CONJURE_TRY_ASSIGN(const physics::query_filter filter, read_filter(invocation, *world));
            const bool all = bool_or(invocation.args, "all", false);
            scene::scene &scene = context.scene();
            core::json::document result = core::json::object({{"hit", false}, {"maxDistance", maxDistance}});
            if (all) {
                std::array<physics::raycast_hit, 64> hits = {};
                const u32 count = world->raycast_all(ray, maxDistance, hits, filter);
                core::json::document list = core::json::document::array();
                for (u32 index = 0; index < count; ++index) {
                    list.push_back(hit_json(scene, *world, hits[index]));
                }
                result.set("hit", count > 0);
                result.set("hits", list);
                if (count > 0) {
                    result.merge_patch(hit_json(scene, *world, hits[0]).root());
                }
            } else {
                const std::optional<physics::raycast_hit> hit = world->raycast(ray, maxDistance, filter);
                if (hit.has_value()) {
                    result.set("hit", true);
                    result.merge_patch(hit_json(scene, *world, *hit).root());
                }
            }
            command_outcome outcome = {};
            outcome.result = std::move(result);
            return outcome;
        }

        core::result<command_outcome> physics_overlap_sphere(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(physics::physics_world *world, require_physics(context));
            CONJURE_TRY_ASSIGN(const math::vec3 center, read_vec3(invocation.args["center"], "args.center"));
            CONJURE_TRY_ASSIGN(const f32 radius, f32_or(invocation.args, "radius", 1.0f));
            if (radius <= 0.0f) {
                return invalid_args("args.radius: expected a positive radius");
            }
            CONJURE_TRY_ASSIGN(const u32 maxResults, u32_or(invocation.args, "maxResults", 256));
            if (maxResults == 0 || maxResults > MAX_OVERLAP_RESULTS) {
                return invalid_args(std::format("args.maxResults: expected 1..{}", MAX_OVERLAP_RESULTS));
            }
            CONJURE_TRY_ASSIGN(const physics::query_filter filter, read_filter(invocation, *world));
            std::vector<ecs::entity> found(maxResults, ecs::NULL_ENTITY);
            const u32 count = world->overlap_sphere(center, radius, found, filter);
            scene::scene &scene = context.scene();
            core::json::document entities = core::json::document::array();
            for (u32 index = 0; index < count; ++index) {
                if (scene.world().is_alive(found[index])) {
                    entities.push_back(entity_summary(scene, found[index]));
                }
            }
            command_outcome outcome = {};
            outcome.result = core::json::object({
              {"count", count},
              {"entities", entities},
              {"center", vec3_json(center)},
              {"radius", radius},
              {"truncated", count >= maxResults},
            });
            return outcome;
        }
#else
        core::result<command_outcome> physics_raycast(command_invocation &invocation) {
            (void) invocation;
            return not_available("physics.raycast needs the physics module, which this build lacks");
        }

        core::result<command_outcome> physics_overlap_sphere(command_invocation &invocation) {
            (void) invocation;
            return not_available("physics.overlap_sphere needs the physics module, which this build lacks");
        }
#endif
    }

    void register_physics_commands(command_registry &registry) {
        const std::string entityRef = std::string(ENTITY_REF_SCHEMA);
        const std::string filterSchema = std::format(
          R"("layers":{{"type":"array","items":{{"type":["string","integer"]}}}},)"
          R"("includeTriggers":{{"type":"boolean"}},)"
          R"("includeStatic":{{"type":"boolean"}},"ignore":{})",
          entityRef
        );
        add_command(
          registry,
          make_command(
            "physics.raycast",
            "Casts a ray against the physics bodies: the closest hit, or every hit sorted by distance with all:true.",
            std::format(
              R"({{"type":"object","properties":{{"origin":{{"type":"array","items":{{"type":"number"}},"minItems":3,)"
              R"("maxItems":3}},)"
              R"("direction":{{"type":"array","items":{{"type":"number"}},"minItems":3,"maxItems":3}},)"
              R"("maxDistance":{{"type":"number"}},)"
              R"("all":{{"type":"boolean"}},{}}},"required":["origin","direction"]}})",
              filterSchema
            ),
            ANY_OBJECT_SCHEMA.data(),
            command_flags::REQUIRES_SCENE | command_flags::READ_ONLY,
            &physics_raycast
          )
        );
        add_command(
          registry,
          make_command(
            "physics.overlap_sphere",
            "The entities whose bodies overlap a sphere.",
            std::format(
              R"({{"type":"object","properties":{{"center":{{"type":"array","items":{{"type":"number"}},"minItems":3,)"
              R"("maxItems":3}},)"
              R"("radius":{{"type":"number"}},"maxResults":{{"type":"integer","minimum":1}},{}}},"required":["center",)"
              R"("radius"]}})",
              filterSchema
            ),
            ANY_OBJECT_SCHEMA.data(),
            command_flags::REQUIRES_SCENE | command_flags::READ_ONLY,
            &physics_overlap_sphere
          )
        );
    }
}
