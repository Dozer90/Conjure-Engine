#include "editor/core/commands/command_support.h"
#include "editor/core/component_access.h"

#include "conjure/editor/reflection_path.h"

#include <format>
#include <utility>

// component.* (docs/design/04_editor.md 3.5, 4.4): add, remove, set (one path or the whole component),
// set_many, get and list. Values move through JSON only (R40).

namespace conjure::editor::support {

    namespace {
        struct resolved_path {
            reflection_path path = {};
            ecs::component_id component = {};
            field_ref reference = {};
            std::string typeName = "";
        };

        /// <summary>
        /// A type name written in full is several path segments: the longest leading run that names a
        /// registered component wins ("conjure.renderer.sprite.size"). Null when none does.
        /// </summary>
        ecs::component_id qualified_component(const ecs::component_registry &registry, reflection_path &path) {
            const std::span<const path_segment> segments = path.segments();
            for (usize count = segments.size(); count >= 2; --count) {
                std::string candidate = "";
                bool indexed = false;
                for (usize index = 0; index < count; ++index) {
                    indexed = indexed || segments[index].index.has_value();
                    if (index > 0) {
                        candidate += '.';
                    }
                    candidate += segments[index].name;
                }
                if (indexed) {
                    continue;
                }
                const ecs::component_id found = registry.find(core::string_id(candidate));
                if (found.is_valid() && path.merge_component(count)) {
                    return found;
                }
            }
            return ecs::component_id{};
        }

        core::result<resolved_path> resolve_path(command_invocation &invocation, std::string_view pathText) {
            editor_context &context = invocation.context;
            resolved_path resolved = {};
            CONJURE_TRY_ASSIGN(resolved.path, reflection_path::parse(pathText));
            // A full registered name wins over an alias, short-name or suffix reading of its first segment, as in
            // resolve_component_type's exact-first rule: "transform.rig.weight" with a registered "transform.rig"
            // is that type's field, not a field of the transform alias.
            const ecs::component_id qualified = qualified_component(context.components(), resolved.path);
            if (qualified.is_valid()) {
                resolved.component = qualified;
            } else {
                core::json::document details = core::json::document::object();
                core::result<ecs::component_id> component =
                  access::resolve_component_type(context.components(), resolved.path.component(), &details);
                if (!component.has_value()) {
                    invocation.errorDetails = std::move(details);
                }
                CONJURE_TRY_ASSIGN(resolved.component, std::move(component));
            }
            CONJURE_TRY_ASSIGN(
              resolved.reference,
              resolved.path.resolve(access::reflection_of(context.components(), resolved.component))
            );
            resolved.typeName = access::component_name(context.components(), resolved.component);
            return resolved;
        }

        struct applied_set {
            std::string guid = "";
            std::string path = "";
            core::json::document previous = {};
        };

        /// <summary>
        /// Reads the current value at the path, writes the new one and returns the previous value. 'restoring'
        /// marks a write that puts back a value the entity held before (a rollback), which skips the name rule.
        /// </summary>
        core::result<applied_set> apply_set(
          command_invocation &invocation,
          ecs::entity entity,
          const resolved_path &resolved,
          const core::json::value &value,
          bool restoring
        ) {
            scene::scene &scene = invocation.context.scene();
            if (!scene.world().has_raw(entity, resolved.component)) {
                return core::make_error(
                  protocol_error_code::COMPONENT_NOT_FOUND,
                  std::format("the entity has no '{}' component", resolved.typeName)
                );
            }
            CONJURE_TRY_ASSIGN(core::json::document current, access::read_component(scene, entity, resolved.component));
            CONJURE_TRY_ASSIGN(core::json::document previous, read_at(current.root(), resolved.reference));
            if (resolved.reference.is_whole_component() && !value.is_object()) {
                return invalid_args(std::format(
                  "args.value: '{}' names a whole component; expected an object",
                  resolved.path.to_string()
                ));
            }
            CONJURE_TRY(write_at(current, resolved.reference, value));
            // Setting the name component renames the entity: entity.rename's rule applies, except while undo puts
            // back a name the entity already had.
            const core::json::value written = current.root()["value"];
            const bool renames = resolved.component == scene.ids().name && written.is_string();
            if (renames && !restoring && !invocation.context.dispatcher().replaying()) {
                CONJURE_TRY(check_entity_name(written.as_string(), "args.value"));
            }
            CONJURE_TRY(access::write_component(scene, entity, resolved.component, current.root()));
            applied_set result = {};
            CONJURE_TRY_ASSIGN(result.guid, guid_string(scene, entity));
            result.path = resolved.path.to_string();
            result.previous = std::move(previous);
            return result;
        }

        core::result<command_outcome> component_add(command_invocation &invocation) {
            editor_context &context = invocation.context;
            scene::scene &scene = context.scene();
            CONJURE_TRY_ASSIGN(const ecs::entity entity, resolve_entity(invocation, invocation.args["entity"]));
            CONJURE_TRY_ASSIGN(const std::string_view typeName, require_string(invocation.args, "type"));
            CONJURE_TRY_ASSIGN(
              const ecs::component_id id,
              access::resolve_component_type(context.components(), typeName, &invocation.errorDetails)
            );
            const core::json::value value = invocation.args["value"];
            CONJURE_TRY(access::add_component(
              scene,
              entity,
              id,
              value.is_object() ? value : core::json::document::object().root()
            ));
            CONJURE_TRY_ASSIGN(const std::string guid, guid_string(scene, entity));
            CONJURE_TRY_ASSIGN(const core::json::document stored, access::read_component(scene, entity, id));
            const std::string registered = access::component_name(context.components(), id);
            command_outcome outcome = {};
            outcome.label = std::format("Add {}", registered);
            outcome.result = core::json::object({{"guid", guid}, {"type", registered}, {"value", stored}});
            outcome.inverse = command_inverse{"component.remove", core::json::object({
              {"entity", guid},
              {"type", registered},
            })};
            command_request replay = {};
            replay.name = "component.add";
            replay.args = core::json::object({{"entity", guid}, {"type", registered}, {"value", stored}});
            outcome.replay = std::move(replay);
            return outcome;
        }

        core::result<command_outcome> component_remove(command_invocation &invocation) {
            editor_context &context = invocation.context;
            scene::scene &scene = context.scene();
            CONJURE_TRY_ASSIGN(const ecs::entity entity, resolve_entity(invocation, invocation.args["entity"]));
            CONJURE_TRY_ASSIGN(const std::string_view typeName, require_string(invocation.args, "type"));
            CONJURE_TRY_ASSIGN(
              const ecs::component_id id,
              access::resolve_component_type(context.components(), typeName, &invocation.errorDetails)
            );
            if (id == scene.ids().localTransform) {
                return invalid_args("the transform cannot be removed: every scene entity carries one");
            }
            CONJURE_TRY_ASSIGN(const core::json::document previous, access::read_component(scene, entity, id));
            CONJURE_TRY(access::remove_component(scene, entity, id));
            CONJURE_TRY_ASSIGN(const std::string guid, guid_string(scene, entity));
            const std::string registered = access::component_name(context.components(), id);
            command_outcome outcome = {};
            outcome.label = std::format("Remove {}", registered);
            outcome.result = core::json::object({{"guid", guid}, {"type", registered}});
            outcome.inverse = command_inverse{
              "component.add",
              core::json::object({{"entity", guid}, {"type", registered}, {"value", previous}}),
            };
            return outcome;
        }

        core::result<command_outcome> component_set(command_invocation &invocation) {
            CONJURE_TRY_ASSIGN(const ecs::entity entity, resolve_entity(invocation, invocation.args["entity"]));
            CONJURE_TRY_ASSIGN(const std::string_view pathText, require_string(invocation.args, "path"));
            const core::json::value value = invocation.args["value"];
            if (!value.is_valid()) {
                return invalid_args("args.value: a value is required");
            }
            CONJURE_TRY_ASSIGN(const resolved_path resolved, resolve_path(invocation, pathText));
            CONJURE_TRY_ASSIGN(applied_set applied, apply_set(invocation, entity, resolved, value, false));
            command_outcome outcome = {};
            outcome.label = std::format("Set {}", applied.path);
            outcome.result = core::json::object({
              {"guid", applied.guid},
              {"path", applied.path},
              {"previous", applied.previous},
            });
            outcome.inverse = command_inverse{
              "component.set",
              core::json::object({{"entity", applied.guid}, {"path", applied.path}, {"value", applied.previous}}),
            };
            return outcome;
        }

        core::result<command_outcome> component_set_many(command_invocation &invocation) {
            const core::json::value sets = invocation.args["sets"];
            if (!sets.is_array() || sets.size() == 0) {
                return invalid_args("args.sets: expected a non-empty array of {entity, path, value}");
            }
            // Resolve everything before the first write (D4).
            struct planned {
                ecs::entity entity = {};
                resolved_path resolved = {};
                core::json::document value = {};
            };
            std::vector<planned> plan = {};
            usize index = 0;
            for (const core::json::value entry : sets) {
                planned item = {};
                CONJURE_TRY_ASSIGN(item.entity, resolve_entity(invocation, entry["entity"]));
                const core::json::value pathValue = entry["path"];
                if (!pathValue.is_string()) {
                    return invalid_args(std::format("args.sets[{}].path: expected a string", index));
                }
                CONJURE_TRY_ASSIGN(item.resolved, resolve_path(invocation, pathValue.as_string()));
                const core::json::value value = entry["value"];
                if (!value.is_valid()) {
                    return invalid_args(std::format("args.sets[{}].value: a value is required", index));
                }
                item.value = core::json::document::from_value(value);
                plan.push_back(std::move(item));
                ++index;
            }
            std::vector<applied_set> applied = {};
            for (const planned &item : plan) {
                core::result<applied_set> result =
                  apply_set(invocation, item.entity, item.resolved, item.value.root(), false);
                if (!result.has_value()) {
                    // Roll back the sets already applied, newest first.
                    scene::scene &scene = invocation.context.scene();
                    for (usize done = applied.size(); done > 0; --done) {
                        const applied_set &undo = applied[done - 1];
                        const std::optional<core::uuid> guid = core::uuid::parse(undo.guid);
                        const ecs::entity entity = guid.has_value() ? scene.find_guid(*guid) : ecs::NULL_ENTITY;
                        core::result<resolved_path> resolved = resolve_path(invocation, undo.path);
                        if (!entity.is_null() && resolved.has_value()) {
                            (void) apply_set(invocation, entity, resolved.value(), undo.previous.root(), true);
                        }
                    }
                    return core::error(result.get_error());
                }
                applied.push_back(std::move(result).value());
            }
            std::vector<command_inverse> inverses = {};
            core::json::document inverseSets = core::json::document::array();
            for (usize done = applied.size(); done > 0; --done) {
                const applied_set &entry = applied[done - 1];
                inverseSets.push_back(core::json::object({
                  {"entity", entry.guid},
                  {"path", entry.path},
                  {"value", entry.previous},
                }));
            }
            command_outcome outcome = {};
            outcome.label = std::format("Set {} values", applied.size());
            outcome.result = core::json::object({{"applied", applied.size()}});
            outcome.inverse = command_inverse{"component.set_many", core::json::object({{"sets", inverseSets}})};
            return outcome;
        }

        core::result<command_outcome> component_get(command_invocation &invocation) {
            scene::scene &scene = invocation.context.scene();
            CONJURE_TRY_ASSIGN(const ecs::entity entity, resolve_entity(invocation, invocation.args["entity"]));
            CONJURE_TRY_ASSIGN(const std::string_view pathText, require_string(invocation.args, "path"));
            CONJURE_TRY_ASSIGN(const resolved_path resolved, resolve_path(invocation, pathText));
            CONJURE_TRY_ASSIGN(
              const core::json::document current,
              access::read_component(scene, entity, resolved.component)
            );
            CONJURE_TRY_ASSIGN(const core::json::document value, read_at(current.root(), resolved.reference));
            command_outcome outcome = {};
            outcome.result = core::json::object({
              {"value", value},
              {"type", resolved.typeName},
              {"path", resolved.path.to_string()},
              {"kind", core::reflect::to_string(resolved.reference.kind)},
            });
            return outcome;
        }

        core::result<command_outcome> component_list(command_invocation &invocation) {
            editor_context &context = invocation.context;
            scene::scene &scene = context.scene();
            CONJURE_TRY_ASSIGN(const ecs::entity entity, resolve_entity(invocation, invocation.args["entity"]));
            std::vector<std::string> names = {};
            for (const ecs::component_id id : access::components_of(scene.world(), entity)) {
                names.push_back(access::component_name(context.components(), id));
            }
            command_outcome outcome = {};
            outcome.result = core::json::object({{"types", string_array(names)}, {"count", names.size()}});
            return outcome;
        }
    }

    void register_component_commands(command_registry &registry) {
        const std::string entityRef = std::string(ENTITY_REF_SCHEMA);
        add_command(
          registry,
          make_command(
            "component.add",
            "Adds a component (by registered name or alias) with an optional initial value.",
            std::format(
              R"({{"type":"object","properties":{{"entity":{},"type":{{"type":"string"}},"value":{{"type":"object",)"
              R"("additionalProperties":true}}}},)"
              R"("required":["entity","type"]}})",
              entityRef
            ),
            R"({"type":"object","properties":{"guid":{"type":"string"},"type":{"type":"string"},)"
            R"("value":{"type":"object","additionalProperties":true},"revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_SCENE,
            &component_add
          )
        );
        add_command(
          registry,
          make_command(
            "component.remove",
            "Removes a component (undo restores its value).",
            std::format(R"({{"type":"object","properties":{{"entity":{},"type":{{"type":"string"}}}},)"
            R"("required":["entity","type"]}})", entityRef),
            R"({"type":"object","properties":{"guid":{"type":"string"},"type":{"type":"string"},)"
            R"("revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_SCENE,
            &component_remove
          )
        );
        add_command(
          registry,
          make_command(
            "component.set",
            "Sets a property by reflection path (\"transform.position.x\"); a bare type name replaces the whole "
            "component.",
            std::format(
              R"({{"type":"object","properties":{{"entity":{},"path":{{"type":"string"}},"value":{{}}}},)"
              R"("required":["entity","path","value"]}})",
              entityRef
            ),
            R"({"type":"object","properties":{"guid":{"type":"string"},"path":{"type":"string"},"previous":{},)"
            R"("revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_SCENE,
            &component_set
          )
        );
        add_command(
          registry,
          make_command(
            "component.set_many",
            "Sets several properties at once as one undo entry (all or nothing).",
            std::format(
              R"({{"type":"object","properties":{{"sets":{{"type":"array","items":{{"type":"object","properties":)"
              R"({{"entity":{},"path":{{"type":"string"}},"value":{{}}}},"required":["entity","path","value"]}}}}}},)"
              R"("required":["sets"]}})",
              entityRef
            ),
            R"({"type":"object","properties":{"applied":{"type":"integer"},"revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_SCENE,
            &component_set_many
          )
        );
        add_command(
          registry,
          make_command(
            "component.get",
            "Reads a property (or a whole component) by reflection path.",
            std::format(R"({{"type":"object","properties":{{"entity":{},"path":{{"type":"string"}}}},)"
            R"("required":["entity","path"]}})", entityRef),
            R"({"type":"object","properties":{"value":{},"type":{"type":"string"},"path":{"type":"string"},)"
            R"("kind":{"type":"string"}}})",
            command_flags::REQUIRES_SCENE | command_flags::READ_ONLY,
            &component_get
          )
        );
        add_command(
          registry,
          make_command(
            "component.list",
            "The editable component types an entity carries.",
            std::format(R"({{"type":"object","properties":{{"entity":{}}},"required":["entity"]}})", entityRef),
            R"({"type":"object","properties":{"types":{"type":"array"},"count":{"type":"integer"}}})",
            command_flags::REQUIRES_SCENE | command_flags::READ_ONLY,
            &component_list
          )
        );
    }
}
