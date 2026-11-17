#include "editor/core/commands/command_support.h"
#include "editor/core/component_access.h"

#include "conjure/editor/editor_camera.h"
#include "conjure/editor/entity_ref.h"
#include "conjure/editor/log_category.h"
#include "conjure/editor/picker.h"
#include "conjure/editor/screenshot_service.h"

#include "conjure/core/math/ray.h"

#include <format>
#include <utility>

// view.* (docs/design/04_editor.md 3.8, 3.10, 4.4, 7): the editor camera lives in core, so set_camera, focus
// and pick work headless; screenshots need a device (NOT_AVAILABLE on the null rhi); debug_draw flags are
// stored for the viewport panel.

namespace conjure::editor::support {

    namespace {
        core::result<command_outcome> view_screenshot(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(screenshot_request request, screenshot_service::parse_request(invocation.args));
            request.path = project_relative_path(context, std::move(request.path));
            if (!context.screenshots().available()) {
                if (context.offscreen()) {
                    return not_available(
                      "view.screenshot: off-screen rendering is not available in this build; run with a window"
                    );
                }
                return not_available("view.screenshot needs a real device: run with --offscreen or a window");
            }
            if (request.camera != "editor" && request.camera != "game") {
                // An entity by guid, path or name: it must be live and carry a camera, and the service always
                // receives its guid.
                CONJURE_TRY_ASSIGN(const ecs::entity entity, resolve_entity(invocation, invocation.args["camera"]));
                const core::result<ecs::component_id> cameraType =
                  access::resolve_component_type(context.components(), "camera", nullptr);
                if (cameraType.has_value() && !context.scene().world().has_raw(entity, cameraType.value())) {
                    return make_editor_error(
                      protocol_error_code::COMPONENT_NOT_FOUND,
                      "args.camera: the entity has no camera component"
                    );
                }
                CONJURE_TRY_ASSIGN(request.camera, guid_string(context.scene(), entity));
            }
            context.screenshots().request(request, invocation.reply);
            return command_outcome{};
        }

        core::result<command_outcome> view_set_camera(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY(context.camera().apply_json(invocation.args));
            command_outcome outcome = {};
            outcome.result = context.camera().to_json();
            return outcome;
        }

        core::result<command_outcome> view_get_camera(command_invocation &invocation) {
            command_outcome outcome = {};
            outcome.result = invocation.context.camera().to_json();
            outcome.result.set(
              "mode",
              invocation.context.current_view_mode() == editor_context::view_mode::TWO_D ? "2d" : "3d"
            );
            return outcome;
        }

        core::result<command_outcome> view_focus(command_invocation &invocation) {
            editor_context &context = invocation.context;
            scene::scene &scene = context.scene();
            math::aabb bounds = {};
            // Every branch frames whole subtrees: render bounds when a subtree has them, otherwise a 1 m box
            // around each of its entities (subtree_bounds_or_boxes), so a parent without render bounds still
            // covers its children.
            if (invocation.args.contains("entity")) {
                CONJURE_TRY_ASSIGN(const ecs::entity entity, resolve_entity(invocation, invocation.args["entity"]));
                bounds = subtree_bounds_or_boxes(scene, entity).bounds;
            } else if (invocation.args.contains("entities")) {
                CONJURE_TRY_ASSIGN(
                  const std::vector<ecs::entity> entities,
                  entity_ref::resolve_many(invocation.args["entities"], scene, &invocation.errorDetails)
                );
                if (entities.empty()) {
                    return invalid_args("args.entities: expected at least one entity");
                }
                for (const ecs::entity entity : entities) {
                    bounds.expand(subtree_bounds_or_boxes(scene, entity).bounds);
                }
            } else {
                // The selection, or the whole scene.
                for (const core::uuid &guid : context.selection().guids()) {
                    const ecs::entity entity = scene.find_guid(guid);
                    if (entity.is_null()) {
                        continue;
                    }
                    bounds.expand(subtree_bounds_or_boxes(scene, entity).bounds);
                }
                if (!bounds.is_valid()) {
                    for (const scene::root_entry &root : scene.roots()) {
                        bounds.expand(subtree_bounds_or_boxes(scene, root.entity).bounds);
                    }
                }
                if (!bounds.is_valid()) {
                    bounds.min = math::vec3(-1.0f);
                    bounds.max = math::vec3(1.0f);
                }
            }
            if (bounds.min == bounds.max) {
                bounds.min -= math::vec3(0.5f);
                bounds.max += math::vec3(0.5f);
            }
            context.camera().focus(bounds);
            command_outcome outcome = {};
            outcome.result = context.camera().to_json();
            outcome.result.set("bounds", aabb_json(bounds));
            return outcome;
        }

        core::result<command_outcome> view_pick(command_invocation &invocation) {
            editor_context &context = invocation.context;
            const core::json::value &args = invocation.args;
            math::ray ray = {};
            if (args.contains("origin") || args.contains("direction")) {
                CONJURE_TRY_ASSIGN(ray.origin, read_vec3(args["origin"], "args.origin"));
                CONJURE_TRY_ASSIGN(ray.direction, read_vec3(args["direction"], "args.direction"));
                const f32 length = glm::length(ray.direction);
                if (length <= 1.0e-6f) {
                    return invalid_args("args.direction: a zero direction is not a ray");
                }
                ray.direction /= length;
            } else {
                CONJURE_TRY_ASSIGN(const f32 x, f32_or(args, "x", 0.5f));
                CONJURE_TRY_ASSIGN(const f32 y, f32_or(args, "y", 0.5f));
                if (x < 0.0f || x > 1.0f || y < 0.0f || y > 1.0f) {
                    return invalid_args("args.x / args.y: expected normalised viewport coordinates in 0..1");
                }
                // Without an aspect the ray matches what the viewport panel shows (16:9 headless).
                CONJURE_TRY_ASSIGN(const f32 aspect, f32_or(args, "aspect", context.viewport_aspect()));
                if (aspect <= 0.0f) {
                    return invalid_args("args.aspect: expected a positive ratio");
                }
                ray = context.camera().screen_ray(math::vec2(x, y), aspect);
            }
            pick_mode mode = pick_mode::MESH;
            if (args.contains("mode")) {
                const std::optional<pick_mode> parsed = parse_pick_mode(string_or(args, "mode", ""));
                if (!parsed.has_value()) {
                    return invalid_args("args.mode: expected \"mesh\", \"bounds\" or \"physics\"");
                }
                mode = *parsed;
            }
            CONJURE_TRY_ASSIGN(const f32 maxDistance, f32_or(args, "maxDistance", 1.0e6f));
            if (maxDistance <= 0.0f) {
                return invalid_args("args.maxDistance: expected a positive distance");
            }
            scene::scene &scene = context.scene();
            pick_context pickContext = {};
            pickContext.scene = &scene;
            pickContext.physics = context.physics();
            pickContext.jobs = context.engine_context().jobs;
            pickContext.maxDistance = maxDistance;
            if (mode == pick_mode::PHYSICS && pickContext.physics == nullptr) {
                return not_available("mode \"physics\" needs the physics world");
            }
            const std::vector<pick_candidate> candidates = gather_pick_candidates(scene);
            const pick_result picked = pick(ray, candidates, mode, pickContext);
            core::json::document result = picked.to_json();
            result.set("mode", to_string(mode));
            result.set("candidates", candidates.size());
            result.set(
              "ray",
              core::json::object({{"origin", vec3_json(ray.origin)}, {"direction", vec3_json(ray.direction)}})
            );
            if (picked.hit) {
                const ecs::entity entity = scene.find_guid(picked.guid);
                if (!entity.is_null()) {
                    result.set("entity", entity_summary(scene, entity));
                }
            }
            command_outcome outcome = {};
            outcome.result = std::move(result);
            return outcome;
        }

        core::json::document mode_json(editor_context &context) {
            return core::json::object({
              {"mode", context.current_view_mode() == editor_context::view_mode::TWO_D ? "2d" : "3d"},
              {"orthographic", context.camera().is_orthographic()},
            });
        }

        core::result<command_outcome> view_mode(command_invocation &invocation) {
            command_outcome outcome = {};
            outcome.result = mode_json(invocation.context);
            return outcome;
        }

        core::result<command_outcome> view_set_mode(command_invocation &invocation) {
            editor_context &context = invocation.context;
            const std::string_view mode = string_or(invocation.args, "mode", "");
            if (mode == "3d") {
                context.set_view_mode(editor_context::view_mode::THREE_D);
                context.camera().set_orthographic(false);
            } else if (mode == "2d") {
                context.set_view_mode(editor_context::view_mode::TWO_D);
                context.camera().set_orthographic(true);
                const math::vec3 target = context.camera().target();
                context.camera().set_look_at(math::vec3(target.x, target.y, target.z + 10.0f), target);
            } else {
                return invalid_args("args.mode: expected \"3d\" or \"2d\"");
            }
            command_outcome outcome = {};
            outcome.result = mode_json(context);
            return outcome;
        }

        core::json::document debug_json(const editor_context::debug_draw_flags &flags) {
            return core::json::object({{"physics", flags.physics}, {"bounds", flags.bounds}, {"grid", flags.grid}});
        }

        core::result<command_outcome> view_debug_draw(command_invocation &invocation) {
            editor_context &context = invocation.context;
            if (context.headless() && !context.offscreen()) {
                return not_available("debug drawing needs a viewport: run with a window or --offscreen");
            }
            editor_context::debug_draw_flags &flags = context.debug_draw();
            const core::json::value &args = invocation.args;
            if (args.contains("physics")) {
                flags.physics = bool_or(args, "physics", flags.physics);
            }
            if (args.contains("bounds")) {
                flags.bounds = bool_or(args, "bounds", flags.bounds);
            }
            if (args.contains("grid")) {
                flags.grid = bool_or(args, "grid", flags.grid);
            }
            command_outcome outcome = {};
            outcome.result = debug_json(flags);
            return outcome;
        }
    }

    void register_view_commands(command_registry &registry) {
        const std::string entityRef = std::string(ENTITY_REF_SCHEMA);
        const std::string cameraSchema =
          R"({"type":"object","properties":{"position":{"type":"array","items":{"type":"number"},"minItems":3,)"
          R"("maxItems":3},)"
          R"("target":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3},"fov":{"type":"number"},)"
          R"("orthographic":{"type":"boolean"},"orthographicHeight":{"type":"number"},"near":{"type":"number"},)"
          R"("far":{"type":"number"}}})";
        add_command(
          registry,
          make_command(
            "view.screenshot",
            "Renders width x height pixels through the chosen camera after waitFrames frames and writes a PNG (a "
            "relative path is relative to the project root).",
            R"({"type":"object","properties":{"path":{"type":"string"},"width":{"type":"integer","minimum":1},)"
            R"("height":{"type":"integer","minimum":1},"camera":{"type":"string"},"format":{"type":"string"},)"
            R"("waitFrames":{"type":"integer","minimum":0}},"required":["path"]})",
            R"({"type":"object","properties":{"path":{"type":"string"},"width":{"type":"integer"},)"
            R"("height":{"type":"integer"},"frame":{"type":"integer"}}})",
            command_flags::ASYNC,
            &view_screenshot
          )
        );
        add_command(
          registry,
          make_command(
            "view.set_camera",
            "Moves the editor camera: position, target, vertical fov (degrees, 0 < fov < 180), orthographic mode and "
            "planes.",
            cameraSchema,
            cameraSchema,
            command_flags::NONE,
            &view_set_camera
          )
        );
        add_command(
          registry,
          make_command(
            "view.get_camera",
            "The editor camera (fov in degrees).",
            std::string(EMPTY_SCHEMA),
            cameraSchema,
            command_flags::READ_ONLY,
            &view_get_camera
          )
        );
        add_command(
          registry,
          make_command(
            "view.focus",
            "Frames an entity's subtree, several entities, the selection or the whole scene.",
            std::format(
              R"({{"type":"object","properties":{{"entity":{},"entities":{{"type":"array","items":{}}}}}}})",
              entityRef,
              entityRef
            ),
            ANY_OBJECT_SCHEMA.data(),
            command_flags::REQUIRES_SCENE,
            &view_focus
          )
        );
        add_command(
          registry,
          make_command(
            "view.pick",
            "Picks the entity under a normalised viewport position (x, y with the editor camera) or along a world ray; "
            "mode mesh (triangles), bounds or physics.",
            R"({"type":"object","properties":{"x":{"type":"number"},"y":{"type":"number"},"aspect":{"type":"number"},)"
            R"("origin":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3},)"
            R"("direction":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3},)"
            R"("mode":{"type":"string","enum":["mesh","bounds","physics"]},"maxDistance":{"type":"number"}}})",
            R"({"type":"object","properties":{"hit":{"type":"boolean"},"guid":{"type":"string"},)"
            R"("point":{"type":"array"},)"
            R"("normal":{"type":"array"},"distance":{"type":"number"}},"additionalProperties":true})",
            command_flags::REQUIRES_SCENE | command_flags::READ_ONLY,
            &view_pick
          )
        );
        add_command(
          registry,
          make_command(
            "view.mode",
            "The viewport projection mode (3d or 2d).",
            std::string(EMPTY_SCHEMA),
            R"({"type":"object","properties":{"mode":{"type":"string"},"orthographic":{"type":"boolean"}}})",
            command_flags::READ_ONLY,
            &view_mode
          )
        );
        add_command(
          registry,
          make_command(
            "view.set_mode",
            "Switches the viewport between the 3d orbit camera and the 2d orthographic camera.",
            R"({"type":"object","properties":{"mode":{"type":"string","enum":["3d","2d"]}},"required":["mode"]})",
            R"({"type":"object","properties":{"mode":{"type":"string"},"orthographic":{"type":"boolean"}}})",
            command_flags::NONE,
            &view_set_mode
          )
        );
        add_command(
          registry,
          make_command(
            "view.debug_draw",
            "Toggles the viewport's physics shapes, bounds and grid overlays (NOT_AVAILABLE headless).",
            R"({"type":"object","properties":{"physics":{"type":"boolean"},"bounds":{"type":"boolean"},)"
            R"("grid":{"type":"boolean"}}})",
            R"({"type":"object","properties":{"physics":{"type":"boolean"},"bounds":{"type":"boolean"},)"
            R"("grid":{"type":"boolean"}}})",
            command_flags::NONE,
            &view_debug_draw
          )
        );
    }
}
