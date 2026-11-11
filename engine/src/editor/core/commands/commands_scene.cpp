#include "editor/core/commands/command_support.h"
#include "editor/core/component_access.h"

#include "conjure/editor/log_category.h"
#include "conjure/editor/reflection_path.h"

#include "conjure/assets/asset_database.h"
#include "conjure/core/string_utils.h"
#include "conjure/scene/scene_manager.h"
#include "conjure/scene/scene_serializer.h"

#include <algorithm>
#include <array>
#include <format>
#include <utility>

// scene.* (docs/design/04_editor.md 4.4): new, open, save, close, info, query, hierarchy and settings.

namespace conjure::editor::support {

    namespace {
        constexpr u32 MAX_HIERARCHY_DEPTH_ARG = 64;

        void register_scene_file(editor_context &context, std::string_view path) {
            assets::asset_database *database = context.asset_database();
            if (database == nullptr || database->find_by_path(path).has_value()) {
                return;
            }
            const core::result<assets::asset_id> added = database->add_file(path);
            if (!added.has_value()) {
                CONJURE_LOG_WARN(
                  LOG_EDITOR,
                  "scene file '{}' was not registered as an asset: {}",
                  path,
                  added.get_error().message()
                );
                return;
            }
            if (context.engine_context().jobs != nullptr) {
                const std::array<assets::asset_id, 1> ids = {added.value()};
                const core::result<assets::import_report> imported = database->import(
                  ids,
                  *context.engine_context().jobs
                );
                (void) imported;
            }
        }

        core::result<command_outcome> scene_new(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(const std::string_view name, require_string(invocation.args, "name"));
            const std::string path = editor_context::normalise_scene_path(string_or(invocation.args, "path", ""));
            context.scene_manager().unload_all();
            context.scene().set_name(name);
            context.undo().clear();
            context.selection().clear();
            context.set_scene_state(true, path);
            context.events().emit(editor_event::SCENE_LOADED, core::json::object({{"path", path}, {"name", name}}));
            command_outcome outcome = {};
            outcome.label = std::format("New scene '{}'", name);
            outcome.result = core::json::object({{"path", path}, {"name", name}});
            return outcome;
        }

        core::result<command_outcome> scene_open(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(const std::string_view pathText, require_string(invocation.args, "path"));
            const std::string path = editor_context::normalise_scene_path(pathText);
            if (!context.filesystem().exists(path)) {
                return make_editor_error(
                  protocol_error_code::ASSET_NOT_FOUND,
                  std::format("no scene file at '{}'", path)
                );
            }
            const core::result<scene::scene_id> loaded =
              context.scene_manager().load_file(context.filesystem(), path, scene::load_mode::REPLACE);
            if (!loaded.has_value()) {
                return wrap(loaded.get_error());
            }
            context.undo().clear();
            context.selection().clear();
            context.set_scene_state(true, path);
            context.events().emit(
              editor_event::SCENE_LOADED,
              core::json::object({{"path", path}, {"name", std::string(context.scene().name())}})
            );
            command_outcome outcome = {};
            outcome.result = core::json::object({
              {"path", path},
              {"name", std::string(context.scene().name())},
              {"entityCount", context.world().entity_count()},
            });
            return outcome;
        }

        core::result<void> save_to(editor_context &context, const std::string &path) {
            const core::result<void> saved = scene::scene_serializer::save_scene_file(
              context.scene(),
              context.filesystem(),
              path
            );
            if (!saved.has_value()) {
                return io_error(std::format("cannot save '{}': {}", path, saved.get_error().message()));
            }
            register_scene_file(context, path);
            return core::result<void>::success();
        }

        core::result<command_outcome> scene_save(command_invocation &invocation) {
            editor_context &context = invocation.context;
            if (context.scene_path().empty()) {
                return invalid_args("the scene has no path yet: use scene.save_as");
            }
            const std::string path = context.scene_path();
            CONJURE_TRY(save_to(context, path));
            context.set_scene_dirty(false);
            command_outcome outcome = {};
            outcome.label = "Save scene";
            outcome.result = core::json::object({{"path", path}});
            return outcome;
        }

        core::result<command_outcome> scene_save_as(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(const std::string_view pathText, require_string(invocation.args, "path"));
            const std::string path = editor_context::normalise_scene_path(pathText);
            if (path.empty()) {
                return invalid_args("args.path: a scene path is required");
            }
            CONJURE_TRY(save_to(context, path));
            context.set_scene_state(true, path);
            command_outcome outcome = {};
            outcome.label = "Save scene as";
            outcome.result = core::json::object({{"path", path}});
            return outcome;
        }

        core::result<command_outcome> scene_close(command_invocation &invocation) {
            editor_context &context = invocation.context;
            if (!context.has_scene()) {
                return make_editor_error(protocol_error_code::SCENE_NOT_OPEN, "no scene is open");
            }
            const bool discard = bool_or(invocation.args, "discard", false);
            if (context.scene_dirty() && !discard) {
                return invalid_args("the scene has unsaved changes: save it or pass discard: true");
            }
            context.scene_manager().unload_all();
            context.undo().clear();
            context.selection().clear();
            context.set_scene_state(false, "");
            command_outcome outcome = {};
            outcome.result = core::json::object({{"closed", true}});
            return outcome;
        }

        core::result<command_outcome> scene_info(command_invocation &invocation) {
            editor_context &context = invocation.context;
            const scene::scene &scene = context.scene();
            command_outcome outcome = {};
            outcome.result = core::json::object({
              {"path", std::string_view(context.scene_path())},
              {"name", std::string(scene.name())},
              {"entityCount", scene.world().entity_count()},
              {"rootCount", scene.roots().size()},
              {"structureVersion", scene.structure_version()},
              {"dirty", context.scene_dirty()},
              {"revision", context.revision()},
              {"settings", scene.settings()},
            });
            return outcome;
        }

        core::result<std::vector<ecs::component_id>> resolve_types(
          command_invocation &invocation,
          const core::json::value &names
        ) {
            std::vector<ecs::component_id> ids = {};
            if (!names.is_valid()) {
                return ids;
            }
            for (const core::json::value name : names) {
                CONJURE_TRY_ASSIGN(
                  const ecs::component_id id,
                  access::resolve_component_type(
                    invocation.context.components(),
                    name.as_string(),
                    &invocation.errorDetails
                  )
                );
                ids.push_back(id);
            }
            return ids;
        }

        core::result<command_outcome> scene_query(command_invocation &invocation) {
            editor_context &context = invocation.context;
            scene::scene &scene = context.scene();
            const ecs::world &world = scene.world();
            CONJURE_TRY_ASSIGN(
              const std::vector<ecs::component_id> with,
              resolve_types(invocation, invocation.args["with"])
            );
            CONJURE_TRY_ASSIGN(
              const std::vector<ecs::component_id> without,
              resolve_types(invocation, invocation.args["without"])
            );
            CONJURE_TRY_ASSIGN(const u32 limit, u32_or(invocation.args, "limit", 1000));
            CONJURE_TRY_ASSIGN(const u32 offset, u32_or(invocation.args, "offset", 0));

            struct field_request {
                std::string path = "";
                ecs::component_id component = {};
                field_ref reference = {};
            };
            std::vector<field_request> fields = {};
            for (const core::json::value pathValue : invocation.args["fields"]) {
                CONJURE_TRY_ASSIGN(const reflection_path path, reflection_path::parse(pathValue.as_string()));
                CONJURE_TRY_ASSIGN(
                  const ecs::component_id id,
                  access::resolve_component_type(context.components(), path.component(), &invocation.errorDetails)
                );
                CONJURE_TRY_ASSIGN(field_ref reference, path.resolve(access::reflection_of(context.components(), id)));
                fields.push_back(field_request{std::string(pathValue.as_string()), id, std::move(reference)});
            }

            std::vector<ecs::entity> ordered = {};
            scene::scene_serializer::collect_hierarchy_order(scene, ordered);
            core::json::document rows = core::json::document::array();
            u32 total = 0;
            for (const ecs::entity entity : ordered) {
                bool matches = true;
                for (const ecs::component_id id : with) {
                    if (!world.has_raw(entity, id)) {
                        matches = false;
                        break;
                    }
                }
                for (const ecs::component_id id : without) {
                    if (matches && world.has_raw(entity, id)) {
                        matches = false;
                        break;
                    }
                }
                if (!matches) {
                    continue;
                }
                ++total;
                if (total <= offset || rows.root().size() >= limit) {
                    continue;
                }
                core::json::document row = entity_summary(scene, entity);
                core::json::document values = core::json::document::object();
                for (const field_request &field : fields) {
                    if (!world.has_raw(entity, field.component)) {
                        values.set_null(field.path);
                        continue;
                    }
                    core::result<core::json::document> component = access::read_component(
                      scene,
                      entity,
                      field.component
                    );
                    if (!component.has_value()) {
                        values.set_null(field.path);
                        continue;
                    }
                    core::result<core::json::document> value = read_at(component.value().root(), field.reference);
                    if (value.has_value()) {
                        values.set(field.path, value.value());
                    } else {
                        values.set_null(field.path);
                    }
                }
                row.set("fields", values);
                rows.push_back(row);
            }
            command_outcome outcome = {};
            outcome.result = core::json::object({{"rows", rows}, {"total", total}});
            return outcome;
        }

        core::json::document describe_node(scene::scene &scene, ecs::entity entity, u32 depth, u32 maxDepth) {
            core::json::document node = entity_summary(scene, entity);
            node.set("enabled", scene.is_enabled(entity));
            core::json::document children = core::json::document::array();
            if (depth < maxDepth) {
                for (const ecs::entity child : scene.tree().children_of(entity)) {
                    children.push_back(describe_node(scene, child, depth + 1, maxDepth));
                }
            }
            node.set("children", children);
            node.set("childCount", scene.tree().children_of(entity).size());
            return node;
        }

        core::result<command_outcome> scene_get_hierarchy(command_invocation &invocation) {
            editor_context &context = invocation.context;
            scene::scene &scene = context.scene();
            CONJURE_TRY_ASSIGN(u32 depth, u32_or(invocation.args, "depth", MAX_HIERARCHY_DEPTH_ARG));
            depth = std::min(depth, MAX_HIERARCHY_DEPTH_ARG);
            core::json::document roots = core::json::document::array();
            if (invocation.args.contains("root")) {
                CONJURE_TRY_ASSIGN(const ecs::entity root, resolve_entity(invocation, invocation.args["root"]));
                roots.push_back(describe_node(scene, root, 0, depth));
            } else {
                for (const scene::root_entry &root : scene.roots()) {
                    roots.push_back(describe_node(scene, root.entity, 0, depth));
                }
            }
            command_outcome outcome = {};
            outcome.result = core::json::object({{"roots", roots}, {"entityCount", scene.world().entity_count()}});
            return outcome;
        }

        core::result<command_outcome> scene_set_settings(command_invocation &invocation) {
            editor_context &context = invocation.context;
            const core::json::value settings = invocation.args["settings"];
            if (!settings.is_object()) {
                return invalid_args("args.settings: expected an object");
            }
            core::json::document previous = context.scene().settings();
            core::json::document next = bool_or(invocation.args, "replace", false)
              ? core::json::document::from_value(settings)
              : previous;
            if (!bool_or(invocation.args, "replace", false) && !next.merge_patch(settings)) {
                return internal("merging the scene settings failed");
            }
            context.scene().set_settings(next);
            command_outcome outcome = {};
            outcome.label = "Scene settings";
            outcome.result = core::json::object({{"settings", context.scene().settings()}});
            outcome.inverse = command_inverse{"scene.set_settings", core::json::object({
              {"settings", previous},
              {"replace", true},
            })};
            return outcome;
        }
    }

    void register_scene_commands(command_registry &registry) {
        add_command(
          registry,
          make_command(
            "scene.new",
            "Replaces the open scene with an empty one.",
            R"({"type":"object","properties":{"name":{"type":"string"},"path":{"type":"string"}},"required":["name"]})",
            R"({"type":"object","properties":{"path":{"type":"string"},"name":{"type":"string"}}})",
            command_flags::MUTATES | command_flags::NO_UNDO | command_flags::REQUIRES_PROJECT |
              command_flags::EDITING_ONLY |
              command_flags::NOT_IN_BATCH,
            &scene_new
          )
        );
        add_command(
          registry,
          make_command(
            "scene.open",
            "Loads a scene file (asset path or path relative to assets://) in place of the open one.",
            R"({"type":"object","properties":{"path":{"type":"string"}},"required":["path"]})",
            R"({"type":"object","properties":{"path":{"type":"string"},"entityCount":{"type":"integer"}}})",
            command_flags::REQUIRES_PROJECT | command_flags::EDITING_ONLY | command_flags::NOT_IN_BATCH,
            &scene_open
          )
        );
        add_command(
          registry,
          make_command(
            "scene.save",
            "Writes the open scene to its path.",
            R"({"type":"object"})",
            R"({"type":"object","properties":{"path":{"type":"string"}}})",
            command_flags::MUTATES | command_flags::NO_UNDO | command_flags::REQUIRES_SCENE,
            &scene_save
          )
        );
        add_command(
          registry,
          make_command(
            "scene.save_as",
            "Writes the open scene to a new path, which becomes its path.",
            R"({"type":"object","properties":{"path":{"type":"string"}},"required":["path"]})",
            R"({"type":"object","properties":{"path":{"type":"string"}}})",
            command_flags::MUTATES | command_flags::NO_UNDO | command_flags::REQUIRES_SCENE,
            &scene_save_as
          )
        );
        add_command(
          registry,
          make_command(
            "scene.close",
            "Closes the open scene (discard: true drops unsaved changes).",
            R"({"type":"object","properties":{"discard":{"type":"boolean"}}})",
            R"({"type":"object","properties":{"closed":{"type":"boolean"}}})",
            command_flags::EDITING_ONLY | command_flags::NOT_IN_BATCH,
            &scene_close
          )
        );
        add_command(
          registry,
          make_command(
            "scene.info",
            "Path, name, entity count, structure version, dirty flag and settings of the open scene.",
            R"({"type":"object"})",
            ANY_OBJECT_SCHEMA.data(),
            command_flags::REQUIRES_SCENE,
            &scene_info
          )
        );
        add_command(
          registry,
          make_command(
            "scene.query",
            "Entities carrying every 'with' type and none of the 'without' types, with the asked field values.",
            R"({"type":"object","properties":{"with":{"type":"array","items":{"type":"string"}},)"
            R"("without":{"type":"array","items":{"type":"string"}},"fields":{"type":"array",)"
            R"("items":{"type":"string"}},)"
            R"("limit":{"type":"integer","minimum":1},"offset":{"type":"integer","minimum":0}}})",
            R"({"type":"object","properties":{"rows":{"type":"array"},"total":{"type":"integer"}}})",
            command_flags::REQUIRES_SCENE,
            &scene_query
          )
        );
        add_command(
          registry,
          make_command(
            "scene.get_hierarchy",
            "The scene tree as nested {guid, name, children} nodes.",
            R"({"type":"object","properties":{"root":{"type":"string"},"depth":{"type":"integer","minimum":0}}})",
            R"({"type":"object","properties":{"roots":{"type":"array"},"entityCount":{"type":"integer"}}})",
            command_flags::REQUIRES_SCENE,
            &scene_get_hierarchy
          )
        );
        add_command(
          registry,
          make_command(
            "scene.set_settings",
            "Merges members into the scene settings (ambient, gravity, ...).",
            R"({"type":"object","properties":{"settings":{"type":"object","additionalProperties":true},)"
            R"("replace":{"type":"boolean"}},"required":["settings"]})",
            R"({"type":"object","properties":{"settings":{"type":"object","additionalProperties":true},)"
            R"("revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_SCENE,
            &scene_set_settings
          )
        );
    }
}
