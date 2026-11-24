#include "editor/core/commands/command_support.h"
#include "editor/core/component_access.h"

#include "conjure/editor/entity_ref.h"
#include "conjure/editor/log_category.h"

#include "conjure/core/string_utils.h"
#include "conjure/scene/components.h"

#include <algorithm>
#include <format>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

// entity.* (docs/design/04_editor.md 3.6, 4.4): creation (single and bulk), destruction with subtree
// restore as inverse, duplication, renaming, reparenting, enabling, transforms and the queries.

namespace conjure::editor::support {

    namespace {
        constexpr u32 MAX_CREATE_MANY = 100000;

        /// <summary>
        /// Adds the components of a {type: json} object to a fresh entity; the transform is written in place.
        /// </summary>
        core::result<void> apply_components(
          command_invocation &invocation,
          ecs::entity entity,
          const core::json::value &components
        ) {
            if (!components.is_valid()) {
                return core::result<void>::success();
            }
            if (!components.is_object()) {
                return invalid_args("args.components: expected {type: value}");
            }
            editor_context &context = invocation.context;
            scene::scene &scene = context.scene();
            for (const core::json::member entry : components.members()) {
                CONJURE_TRY_ASSIGN(
                  const ecs::component_id id,
                  access::resolve_component_type(context.components(), entry.key, &invocation.errorDetails)
                );
                const core::json::document emptyObject = core::json::document::object();
                const core::json::value value = entry.item.is_object() ? entry.item : emptyObject.root();
                // The name component is another way to name the entity: the same rule as args.name applies.
                if (id == scene.ids().name && value["value"].is_string() && !context.dispatcher().replaying()) {
                    CONJURE_TRY(check_entity_name(value["value"].as_string(), "args.components.name.value"));
                }
                if (scene.world().has_raw(entity, id)) {
                    CONJURE_TRY(access::write_component(scene, entity, id, value));
                } else {
                    CONJURE_TRY(access::add_component(scene, entity, id, value));
                }
            }
            return core::result<void>::success();
        }

        core::result<ecs::entity> optional_parent(command_invocation &invocation, const core::json::value &reference) {
            if (!reference.is_valid() || reference.is_null()) {
                return ecs::NULL_ENTITY;
            }
            return resolve_entity(invocation, reference);
        }

        core::result<std::optional<core::uuid>> optional_guid(const core::json::value &value, std::string_view name) {
            if (!value.is_valid() || value.is_null()) {
                return std::optional<core::uuid>(std::nullopt);
            }
            if (!value.is_string()) {
                return invalid_args(std::format("args.{}: expected a uuid string", name));
            }
            const std::optional<core::uuid> guid = core::uuid::parse(value.as_string());
            if (!guid.has_value() || guid->is_null()) {
                return invalid_args(std::format("args.{}: '{}' is not a uuid", name, value.as_string()));
            }
            return guid;
        }

        std::string format_name(std::string_view format, u32 index) {
            const std::string number = std::to_string(index);
            if (format.find("{i}") == std::string_view::npos) {
                return std::format("{}_{}", format, number);
            }
            return core::str::replace_all(format, "{i}", number);
        }

        core::result<command_outcome> entity_create(command_invocation &invocation) {
            editor_context &context = invocation.context;
            scene::scene &scene = context.scene();
            const std::string_view name = string_or(invocation.args, "name", "Entity");
            CONJURE_TRY(check_entity_name(name, "args.name"));
            CONJURE_TRY_ASSIGN(const ecs::entity parent, optional_parent(invocation, invocation.args["parent"]));
            CONJURE_TRY_ASSIGN(
              const std::optional<core::uuid> wantedGuid,
              optional_guid(invocation.args["guid"], "guid")
            );
            CONJURE_TRY_ASSIGN(const u32 index, u32_or(invocation.args, "index", scene::APPEND_CHILD));
            if (wantedGuid.has_value() && !scene.find_guid(*wantedGuid).is_null()) {
                return invalid_args(std::format("args.guid: {} is already in use", wantedGuid->to_string()));
            }
            const core::result<ecs::entity> created = scene.create_entity(core::string_id(name), parent, index);
            if (!created.has_value()) {
                return wrap(created.get_error());
            }
            const ecs::entity entity = created.value();
            if (wantedGuid.has_value()) {
                const core::result<void> renamed = scene.set_guid(entity, *wantedGuid);
                if (!renamed.has_value()) {
                    (void) scene.destroy_now(entity);
                    return wrap(renamed.get_error());
                }
            }
            const core::result<void> applied = apply_components(invocation, entity, invocation.args["components"]);
            if (!applied.has_value()) {
                (void) scene.destroy_now(entity);
                return core::error(applied.get_error());
            }
            CONJURE_TRY_ASSIGN(const std::string guid, guid_string(scene, entity));

            command_outcome outcome = {};
            outcome.label = std::format("Create '{}'", name);
            outcome.result = core::json::object({{"guid", guid}, {"name", name}});
            outcome.inverse = command_inverse{"entity.destroy", core::json::object({{"entity", guid}})};
            command_request replay = {};
            replay.name = "entity.create";
            replay.args = invocation.args.is_valid() ? core::json::document::from_value(
              invocation.args
            ) : core::json::document::object();
            replay.args.set("guid", guid);
            outcome.replay = std::move(replay);
            return outcome;
        }

        core::result<command_outcome> entity_create_many(command_invocation &invocation) {
            editor_context &context = invocation.context;
            scene::scene &scene = context.scene();
            CONJURE_TRY_ASSIGN(const u32 count, u32_or(invocation.args, "count", 0));
            if (count == 0 || count > MAX_CREATE_MANY) {
                return invalid_args(std::format("args.count: expected 1..{}", MAX_CREATE_MANY));
            }
            const std::string_view nameFormat = string_or(invocation.args, "nameFormat", "Entity_{i}");
            // {i} becomes digits, so the format decides whether every name is addressable.
            CONJURE_TRY(check_entity_name(nameFormat, "args.nameFormat"));
            CONJURE_TRY_ASSIGN(const ecs::entity parent, optional_parent(invocation, invocation.args["parent"]));
            const core::json::value overrides = invocation.args["overrides"];
            if (overrides.is_valid() && (!overrides.is_array() || overrides.size() > count)) {
                return invalid_args("args.overrides: expected an array with at most count entries");
            }
            const core::json::value guids = invocation.args["guids"];
            std::vector<core::uuid> wantedGuids = {};
            if (guids.is_valid()) {
                if (!guids.is_array() || guids.size() != count) {
                    return invalid_args("args.guids: expected exactly count uuids");
                }
                for (const core::json::value guidValue : guids) {
                    const std::optional<core::uuid> guid = core::uuid::parse(guidValue.as_string());
                    if (!guid.has_value() || guid->is_null()) {
                        return invalid_args(std::format("args.guids: '{}' is not a uuid", guidValue.as_string()));
                    }
                    if (!scene.find_guid(*guid).is_null()) {
                        return invalid_args(std::format("args.guids: {} is already in use", guid->to_string()));
                    }
                    wantedGuids.push_back(*guid);
                }
            }

            // The template carries the shared components; every copy gets its own guid and name.
            const core::result<ecs::entity> templateCreated = scene.create_entity(core::string_id(nameFormat), parent);
            if (!templateCreated.has_value()) {
                return wrap(templateCreated.get_error());
            }
            const ecs::entity templateEntity = templateCreated.value();
            const core::result<void> applied = apply_components(
              invocation,
              templateEntity,
              invocation.args["components"]
            );
            if (!applied.has_value()) {
                (void) scene.destroy_now(templateEntity);
                return core::error(applied.get_error());
            }
            std::vector<ecs::entity> created(count, ecs::NULL_ENTITY);
            const core::result<scene::instantiate_report> report = scene.create_entities(
              count,
              templateEntity,
              parent,
              created
            );
            (void) scene.destroy_now(templateEntity);
            if (!report.has_value()) {
                return wrap(report.get_error());
            }

            std::vector<core::uuid> resultGuids = {};
            resultGuids.reserve(count);
            std::optional<core::error> failure = std::nullopt;
            for (u32 index = 0; index < count && !failure.has_value(); ++index) {
                const ecs::entity entity = created[index];
                const core::result<void> named = scene.set_name(
                  entity,
                  core::string_id(format_name(nameFormat, index))
                );
                if (!named.has_value()) {
                    failure = wrap(named.get_error());
                    break;
                }
                if (!wantedGuids.empty()) {
                    const core::result<void> set = scene.set_guid(entity, wantedGuids[index]);
                    if (!set.has_value()) {
                        failure = wrap(set.get_error());
                        break;
                    }
                }
                const core::result<core::uuid> guid = scene.ensure_guid(entity);
                if (!guid.has_value()) {
                    failure = wrap(guid.get_error());
                    break;
                }
                resultGuids.push_back(guid.value());
                if (overrides.is_valid() && index < overrides.size()) {
                    const core::json::value patch = overrides[index];
                    if (patch.is_object()) {
                        const core::result<void> patched = apply_components(invocation, entity, patch);
                        if (!patched.has_value()) {
                            failure = patched.get_error();
                            break;
                        }
                    }
                }
            }
            if (failure.has_value()) {
                for (const ecs::entity entity : created) {
                    if (scene.contains(entity)) {
                        (void) scene.destroy_now(entity);
                    }
                }
                return *failure;
            }

            command_outcome outcome = {};
            outcome.label = std::format("Create {} entities", count);
            outcome.result = core::json::object({{"guids", guid_array(resultGuids)}, {"count", count}});
            outcome.inverse = command_inverse{"entity.destroy_many", core::json::object(
              {{"entities", guid_array(resultGuids)}}
            )};
            command_request replay = {};
            replay.name = "entity.create_many";
            replay.args = core::json::document::from_value(invocation.args);
            replay.args.set("guids", guid_array(resultGuids));
            outcome.replay = std::move(replay);
            return outcome;
        }

        core::result<command_outcome> entity_destroy(command_invocation &invocation) {
            scene::scene &scene = invocation.context.scene();
            CONJURE_TRY_ASSIGN(const ecs::entity entity, resolve_entity(invocation, invocation.args["entity"]));
            const std::string name = scene.name_of(entity).to_string();
            CONJURE_TRY_ASSIGN(command_inverse inverse, restore_inverse(scene, entity));
            const u32 subtreeSize = scene.tree().subtree_size(entity);
            std::vector<core::uuid> removed = {};
            scene.tree().for_each_in_subtree(entity, [&](ecs::entity member) {
                const std::optional<core::uuid> guid = scene.guid_of(member);
                if (guid.has_value()) {
                    removed.push_back(*guid);
                }
            });
            const core::result<void> destroyed = scene.destroy_now(entity);
            if (!destroyed.has_value()) {
                return wrap(destroyed.get_error());
            }
            invocation.context.selection().remove(removed);
            command_outcome outcome = {};
            outcome.label = std::format("Destroy '{}'", name);
            outcome.result = core::json::object({{"destroyed", subtreeSize}});
            outcome.inverse = std::move(inverse);
            return outcome;
        }

        core::result<command_outcome> entity_destroy_many(command_invocation &invocation) {
            scene::scene &scene = invocation.context.scene();
            CONJURE_TRY_ASSIGN(
              const std::vector<ecs::entity> requested,
              entity_ref::resolve_many(invocation.args["entities"], scene, &invocation.errorDetails)
            );
            const std::vector<ecs::entity> outermostRoots = outermost(scene, requested);
            // The inverse is one editor.batch whose children run forwards: sorted by ascending sibling index,
            // every restore finds its lower siblings already in place and lands at its original position
            // even when surviving siblings sit between the destroyed ones. The positions of scene roots come
            // from one pass over scene.roots(), not one scan per root (undo of a bulk create lands here).
            const std::span<const scene::root_entry> sceneRoots = scene.roots();
            std::unordered_map<ecs::entity, u32> rootIndex = {};
            rootIndex.reserve(sceneRoots.size());
            for (usize index = 0; index < sceneRoots.size(); ++index) {
                rootIndex.emplace(sceneRoots[index].entity, static_cast<u32>(index));
            }
            std::vector<std::pair<u32, ecs::entity>> positioned = {};
            positioned.reserve(outermostRoots.size());
            for (const ecs::entity root : outermostRoots) {
                u32 position = scene::APPEND_CHILD;
                if (!scene.tree().parent_of(root).is_null()) {
                    position = scene.tree().sibling_index(root);
                } else {
                    const auto found = rootIndex.find(root);
                    if (found != rootIndex.end()) {
                        position = found->second;
                    }
                }
                positioned.emplace_back(position, root);
            }
            std::stable_sort(positioned.begin(), positioned.end(), [](const auto &left, const auto &right) {
                return left.first < right.first;
            });
            std::vector<ecs::entity> roots = {};
            roots.reserve(positioned.size());
            std::vector<command_inverse> inverses = {};
            inverses.reserve(positioned.size());
            for (const auto &[position, root] : positioned) {
                CONJURE_TRY_ASSIGN(command_inverse inverse, restore_inverse(scene, root, position));
                inverses.push_back(std::move(inverse));
                roots.push_back(root);
            }
            std::vector<core::uuid> removed = {};
            u32 destroyed = 0;
            for (usize index = 0; index < roots.size(); ++index) {
                const ecs::entity root = roots[index];
                scene.tree().for_each_in_subtree(root, [&](ecs::entity member) {
                    const std::optional<core::uuid> guid = scene.guid_of(member);
                    if (guid.has_value()) {
                        removed.push_back(*guid);
                    }
                });
                destroyed += scene.tree().subtree_size(root);
                const core::result<void> gone = scene.destroy_now(root);
                if (!gone.has_value()) {
                    // Every root was live a moment ago, so this is an engine fault; put the subtrees already
                    // destroyed back from their archives so the command leaves nothing behind (D4).
                    for (usize restoreIndex = 0; restoreIndex < index; ++restoreIndex) {
                        const core::result<ecs::entity> restored = apply_restore_inverse(scene, inverses[restoreIndex]);
                        if (!restored.has_value()) {
                            CONJURE_LOG_ERROR(
                              LOG_EDITOR,
                              "entity.destroy_many: rollback of a destroyed subtree failed: {}",
                              restored.get_error().message()
                            );
                        }
                    }
                    return internal(std::format(
                      "entity.destroy_many: destroying '{}' failed: {}",
                      scene.name_of(root).to_string(),
                      gone.get_error().message()
                    ));
                }
            }
            invocation.context.selection().remove(removed);
            command_outcome outcome = {};
            outcome.label = std::format("Destroy {} entities", roots.size());
            outcome.result = core::json::object({{"destroyed", destroyed}});
            outcome.inverse = command_inverse{"editor.batch", batch_args(inverses, "Restore entities")};
            return outcome;
        }

        core::result<command_outcome> entity_restore(command_invocation &invocation) {
            scene::scene &scene = invocation.context.scene();
            CONJURE_TRY_ASSIGN(const ecs::entity parent, optional_parent(invocation, invocation.args["parent"]));
            CONJURE_TRY_ASSIGN(const u32 index, u32_or(invocation.args, "index", scene::APPEND_CHILD));
            CONJURE_TRY_ASSIGN(
              const ecs::entity root,
              restore_subtree(scene, invocation.args["archive"], parent, index)
            );
            CONJURE_TRY_ASSIGN(const std::string guid, guid_string(scene, root));
            command_outcome outcome = {};
            outcome.label = std::format("Restore '{}'", scene.name_of(root).to_string());
            outcome.result = core::json::object({{"guid", guid}, {"entities", scene.tree().subtree_size(root)}});
            outcome.inverse = command_inverse{"entity.destroy", core::json::object({{"entity", guid}})};
            return outcome;
        }

        core::result<command_outcome> entity_duplicate(command_invocation &invocation) {
            scene::scene &scene = invocation.context.scene();
            CONJURE_TRY_ASSIGN(const ecs::entity source, resolve_entity(invocation, invocation.args["entity"]));
            CONJURE_TRY_ASSIGN(const u32 count, u32_or(invocation.args, "count", 1));
            if (count == 0 || count > MAX_CREATE_MANY) {
                return invalid_args(std::format("args.count: expected 1..{}", MAX_CREATE_MANY));
            }
            math::vec3 offset = math::vec3(0.0f);
            if (invocation.args.contains("offset")) {
                CONJURE_TRY_ASSIGN(offset, read_vec3(invocation.args["offset"], "args.offset"));
            }
            std::vector<ecs::entity> copies = {};
            std::vector<command_inverse> replays = {};
            for (u32 index = 0; index < count; ++index) {
                const core::result<ecs::entity> copy = scene.duplicate_subtree(source);
                if (!copy.has_value()) {
                    for (const ecs::entity made : copies) {
                        (void) scene.destroy_now(made);
                    }
                    return wrap(copy.get_error());
                }
                copies.push_back(copy.value());
                if (offset != math::vec3(0.0f)) {
                    const std::optional<math::transform> local = scene.local_transform_of(copy.value());
                    if (local.has_value()) {
                        math::transform moved = *local;
                        moved.position += offset * static_cast<f32>(index + 1);
                        (void) scene.set_local_transform(copy.value(), moved);
                    }
                }
            }
            std::vector<core::uuid> guids = {};
            for (const ecs::entity copy : copies) {
                CONJURE_TRY_ASSIGN(const core::uuid guid, entity_ref::guid_of(scene, copy));
                guids.push_back(guid);
                CONJURE_TRY_ASSIGN(command_inverse restore, restore_inverse(scene, copy));
                replays.push_back(std::move(restore));
            }
            command_outcome outcome = {};
            outcome.label = std::format("Duplicate '{}'", scene.name_of(source).to_string());
            outcome.result = core::json::object({{"guids", guid_array(guids)}});
            outcome.inverse = command_inverse{"entity.destroy_many", core::json::object(
              {{"entities", guid_array(guids)}}
            )};
            command_request replay = {};
            replay.name = "editor.batch";
            replay.args = batch_args(replays, "Duplicate");
            outcome.replay = std::move(replay);
            return outcome;
        }

        core::result<command_outcome> entity_rename(command_invocation &invocation) {
            scene::scene &scene = invocation.context.scene();
            CONJURE_TRY_ASSIGN(const ecs::entity entity, resolve_entity(invocation, invocation.args["entity"]));
            CONJURE_TRY_ASSIGN(const std::string_view name, require_string(invocation.args, "name"));
            // Undo restores whatever name the entity had, also one a scene file or an import gave it that a
            // client could not send.
            if (!invocation.context.dispatcher().replaying()) {
                CONJURE_TRY(check_entity_name(name, "args.name"));
            }
            const std::string previous = scene.name_of(entity).to_string();
            const core::result<void> renamed = scene.set_name(entity, core::string_id(name));
            if (!renamed.has_value()) {
                return wrap(renamed.get_error());
            }
            CONJURE_TRY_ASSIGN(const std::string guid, guid_string(scene, entity));
            command_outcome outcome = {};
            outcome.label = std::format("Rename '{}'", previous);
            outcome.result = core::json::object({{"guid", guid}, {"name", name}});
            outcome.inverse = command_inverse{"entity.rename", core::json::object({
              {"entity", guid},
              {"name", previous},
            })};
            return outcome;
        }

        core::result<command_outcome> entity_set_parent(command_invocation &invocation) {
            scene::scene &scene = invocation.context.scene();
            CONJURE_TRY_ASSIGN(const ecs::entity entity, resolve_entity(invocation, invocation.args["entity"]));
            CONJURE_TRY_ASSIGN(const ecs::entity parent, optional_parent(invocation, invocation.args["parent"]));
            const bool keepWorld = bool_or(invocation.args, "keepWorld", true);
            CONJURE_TRY_ASSIGN(const u32 index, u32_or(invocation.args, "index", scene::APPEND_CHILD));
            std::optional<math::transform> wantedLocal = std::nullopt;
            if (invocation.args.contains("localTransform")) {
                CONJURE_TRY_ASSIGN(
                  wantedLocal,
                  read_transform(invocation.args["localTransform"], "args.localTransform", math::transform())
                );
            }
            const ecs::entity previousParent = scene.tree().parent_of(entity);
            const u32 previousIndex = tree_position(scene, entity);
            const std::optional<math::transform> previousLocal = scene.local_transform_of(entity);
            const scene::reparent_mode mode =
              keepWorld ? scene::reparent_mode::KEEP_WORLD : scene::reparent_mode::KEEP_LOCAL;
            const core::result<void> moved = scene.set_parent(entity, parent, mode, index);
            if (!moved.has_value()) {
                return wrap(moved.get_error());
            }
            if (wantedLocal.has_value() && previousLocal.has_value()) {
                CONJURE_TRY(scene.set_local_transform(entity, *wantedLocal));
            }
            CONJURE_TRY_ASSIGN(const std::string guid, guid_string(scene, entity));
            core::json::document inverseArgs = core::json::object({
              {"entity", guid},
              {"keepWorld", false},
              {"index", previousIndex},
            });
            if (previousParent.is_null()) {
                inverseArgs.set_null("parent");
            } else {
                inverseArgs.set("parent", guid_or_empty(scene, previousParent));
            }
            if (previousLocal.has_value()) {
                inverseArgs.set("localTransform", transform_json(*previousLocal));
            }
            command_outcome outcome = {};
            outcome.label = std::format("Reparent '{}'", scene.name_of(entity).to_string());
            outcome.result = core::json::object({{"guid", guid}});
            // A root answers parent null, as entity.get does.
            if (parent.is_null()) {
                outcome.result.set_null("parent");
            } else {
                outcome.result.set("parent", guid_or_empty(scene, parent));
            }
            outcome.inverse = command_inverse{"entity.set_parent", std::move(inverseArgs)};
            return outcome;
        }

        core::result<command_outcome> entity_set_enabled(command_invocation &invocation) {
            scene::scene &scene = invocation.context.scene();
            CONJURE_TRY_ASSIGN(const ecs::entity entity, resolve_entity(invocation, invocation.args["entity"]));
            const core::json::value enabledValue = invocation.args["enabled"];
            if (!enabledValue.is_bool()) {
                return invalid_args("args.enabled: expected a boolean");
            }
            const bool previous = scene.is_enabled(entity);
            CONJURE_TRY(scene.set_enabled(entity, enabledValue.as_bool()));
            CONJURE_TRY_ASSIGN(const std::string guid, guid_string(scene, entity));
            command_outcome outcome = {};
            outcome.label = std::format(
              "{} '{}'",
              enabledValue.as_bool() ? "Enable" : "Disable",
              scene.name_of(entity).to_string()
            );
            outcome.result = core::json::object({{"guid", guid}, {"enabled", enabledValue.as_bool()}});
            outcome.inverse = command_inverse{"entity.set_enabled", core::json::object({
              {"entity", guid},
              {"enabled", previous},
            })};
            return outcome;
        }

        core::result<command_outcome> entity_set_transform(command_invocation &invocation) {
            scene::scene &scene = invocation.context.scene();
            CONJURE_TRY_ASSIGN(const ecs::entity entity, resolve_entity(invocation, invocation.args["entity"]));
            const std::string_view space = string_or(invocation.args, "space", "local");
            const std::optional<math::transform> previousLocal = scene.local_transform_of(entity);
            if (!previousLocal.has_value()) {
                return core::make_error(protocol_error_code::COMPONENT_NOT_FOUND, "the entity has no transform");
            }
            if (space == "world") {
                const std::optional<math::transform> world = scene.world_transform_of(entity);
                CONJURE_TRY_ASSIGN(
                  const math::transform next,
                  read_transform(invocation.args, "args", world.value_or(math::transform()))
                );
                CONJURE_TRY(scene.set_world_transform(entity, next));
            } else {
                CONJURE_TRY_ASSIGN(const math::transform next, read_transform(invocation.args, "args", *previousLocal));
                CONJURE_TRY(scene.set_local_transform(entity, next));
            }
            CONJURE_TRY_ASSIGN(const std::string guid, guid_string(scene, entity));
            const math::transform resulting = scene.local_transform_of(entity).value_or(*previousLocal);
            command_outcome outcome = {};
            outcome.label = std::format("Transform '{}'", scene.name_of(entity).to_string());
            outcome.result = core::json::object({{"guid", guid}, {"local", transform_json(resulting)}});
            core::json::document inverseArgs = transform_json(*previousLocal);
            inverseArgs.set("entity", guid);
            inverseArgs.set("space", "local");
            outcome.inverse = command_inverse{"entity.set_transform", std::move(inverseArgs)};
            return outcome;
        }

        core::result<command_outcome> entity_get(command_invocation &invocation) {
            editor_context &context = invocation.context;
            scene::scene &scene = context.scene();
            CONJURE_TRY_ASSIGN(const ecs::entity entity, resolve_entity(invocation, invocation.args["entity"]));
            std::vector<ecs::component_id> ids = {};
            if (invocation.args.contains("components")) {
                for (const core::json::value name : invocation.args["components"]) {
                    CONJURE_TRY_ASSIGN(
                      const ecs::component_id id,
                      access::resolve_component_type(context.components(), name.as_string(), &invocation.errorDetails)
                    );
                    ids.push_back(id);
                }
            } else {
                ids = access::components_of(scene.world(), entity);
            }
            core::json::document components = core::json::document::object();
            for (const ecs::component_id id : ids) {
                if (!scene.world().has_raw(entity, id)) {
                    continue;
                }
                CONJURE_TRY_ASSIGN(const core::json::document value, access::read_component(scene, entity, id));
                components.set(access::component_name(context.components(), id), value);
            }
            core::json::document children = core::json::document::array();
            for (const ecs::entity child : scene.tree().children_of(entity)) {
                children.push_back(guid_or_empty(scene, child));
            }
            const ecs::entity parent = scene.tree().parent_of(entity);
            core::json::document result = entity_summary(scene, entity);
            if (parent.is_null()) {
                result.set_null("parent");
            } else {
                result.set("parent", guid_or_empty(scene, parent));
            }
            result.set("children", children);
            result.set("enabled", scene.is_enabled(entity));
            result.set("active", scene.is_active(entity));
            // An untagged entity has the null id, whose to_string() is a hash spelling, not a tag name.
            const core::string_id tag = scene.tag_of(entity);
            result.set("tag", tag.is_null() ? std::string() : tag.to_string());
            result.set("layer", scene.layer_of(entity));
            result.set("components", components);
            const std::optional<math::transform> world = scene.world_transform_of(entity);
            if (world.has_value()) {
                result.set("world", transform_json(*world));
            }
            command_outcome outcome = {};
            outcome.result = std::move(result);
            return outcome;
        }

        core::result<command_outcome> entity_find(command_invocation &invocation) {
            editor_context &context = invocation.context;
            scene::scene &scene = context.scene();
            CONJURE_TRY_ASSIGN(const u32 limit, u32_or(invocation.args, "limit", 1000));
            std::optional<ecs::component_id> component = std::nullopt;
            if (invocation.args.contains("component")) {
                CONJURE_TRY_ASSIGN(
                  const ecs::component_id id,
                  access::resolve_component_type(
                    context.components(),
                    string_or(invocation.args, "component", ""),
                    &invocation.errorDetails
                  )
                );
                component = id;
            }
            const bool byName = invocation.args.contains("name");
            const bool byTag = invocation.args.contains("tag");
            const bool byLayer = invocation.args.contains("layer");
            const core::string_id name(string_or(invocation.args, "name", ""));
            // {tag: ""} means untagged, which is the null id (entity.get reports it as ""), not the hash of "".
            const std::string_view tagText = string_or(invocation.args, "tag", "");
            const core::string_id tag = tagText.empty() ? core::string_id() : core::string_id(tagText);
            CONJURE_TRY_ASSIGN(const u32 layer, u32_or(invocation.args, "layer", 0));
            if (byLayer && layer > 31) {
                return invalid_args("args.layer: expected 0..31");
            }
            std::vector<ecs::entity> ordered = {};
            scene::scene_serializer::collect_hierarchy_order(scene, ordered);
            std::vector<core::uuid> guids = {};
            u32 total = 0;
            for (const ecs::entity entity : ordered) {
                if (byName && scene.name_of(entity) != name) {
                    continue;
                }
                if (byTag && scene.tag_of(entity) != tag) {
                    continue;
                }
                if (byLayer && scene.layer_of(entity) != layer) {
                    continue;
                }
                if (component.has_value() && !scene.world().has_raw(entity, *component)) {
                    continue;
                }
                ++total;
                if (guids.size() < limit) {
                    const std::optional<core::uuid> guid = scene.guid_of(entity);
                    guids.push_back(guid.value_or(core::uuid::null()));
                }
            }
            command_outcome outcome = {};
            outcome.result = core::json::object({{"guids", guid_array(guids)}, {"total", total}});
            return outcome;
        }

        core::result<command_outcome> entity_bounds(command_invocation &invocation) {
            scene::scene &scene = invocation.context.scene();
            CONJURE_TRY_ASSIGN(const ecs::entity entity, resolve_entity(invocation, invocation.args["entity"]));
            const subtree_box box = subtree_bounds_or_boxes(scene, entity);
            command_outcome outcome = {};
            outcome.result = aabb_json(box.bounds);
            outcome.result.set("fromRenderBounds", box.fromRenderBounds);
            return outcome;
        }
    }

    void register_entity_commands(command_registry &registry) {
        const std::string entityRef = std::string(ENTITY_REF_SCHEMA);
        const std::string parentRef = std::string(OPTIONAL_ENTITY_REF_SCHEMA);
        add_command(
          registry,
          make_command(
            "entity.create",
            "Creates an entity (with name, transform and guid) and the given components.",
            std::format(
              R"({{"type":"object","properties":{{"name":{{"type":"string"}},"parent":{},"guid":{{"type":"string"}},)"
              R"("index":{{"type":"integer","minimum":0}},"components":{{"type":"object",)"
              R"("additionalProperties":true}}}}}})",
              parentRef
            ),
            R"({"type":"object","properties":{"guid":{"type":"string"},"name":{"type":"string"},)"
            R"("revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_SCENE,
            &entity_create
          )
        );
        add_command(
          registry,
          make_command(
            "entity.create_many",
            "Creates count entities sharing the components, named by nameFormat ({i} = index), with per-entity "
            "overrides.",
            std::format(
              R"({{"type":"object","properties":{{"count":{{"type":"integer","minimum":1}},)"
              R"("nameFormat":{{"type":"string"}},)"
              R"("parent":{},"components":{{"type":"object","additionalProperties":true}},)"
              R"("overrides":{{"type":"array","items":{{"type":"object","additionalProperties":true}}}},)"
              R"("guids":{{"type":"array","items":{{"type":"string"}}}}}},"required":["count"]}})",
              parentRef
            ),
            R"({"type":"object","properties":{"guids":{"type":"array"},"count":{"type":"integer"},)"
            R"("revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_SCENE,
            &entity_create_many
          )
        );
        add_command(
          registry,
          make_command(
            "entity.destroy",
            "Destroys an entity and its subtree (undo restores it exactly).",
            std::format(R"({{"type":"object","properties":{{"entity":{}}},"required":["entity"]}})", entityRef),
            R"({"type":"object","properties":{"destroyed":{"type":"integer"},"revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_SCENE,
            &entity_destroy
          )
        );
        add_command(
          registry,
          make_command(
            "entity.destroy_many",
            "Destroys several entities and their subtrees.",
            std::format(
              R"({{"type":"object","properties":{{"entities":{{"type":"array","items":{}}}}},)"
              R"("required":["entities"]}})",
              entityRef
            ),
            R"({"type":"object","properties":{"destroyed":{"type":"integer"},"revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_SCENE,
            &entity_destroy_many
          )
        );
        add_command(
          registry,
          make_command(
            "entity.restore",
            "Re-creates a subtree from a prefab-format archive keeping its guids (the inverse of entity.destroy).",
            std::format(
              R"({{"type":"object","properties":{{"archive":{{"type":"object","additionalProperties":true}},)"
              R"("parent":{},)"
              R"("index":{{"type":"integer","minimum":0}}}},"required":["archive"]}})",
              parentRef
            ),
            R"({"type":"object","properties":{"guid":{"type":"string"},"entities":{"type":"integer"},)"
            R"("revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_SCENE,
            &entity_restore
          )
        );
        add_command(
          registry,
          make_command(
            "entity.duplicate",
            "Copies an entity's subtree count times next to it, each copy offset further.",
            std::format(
              R"({{"type":"object","properties":{{"entity":{},"count":{{"type":"integer","minimum":1}},)"
              R"("offset":{{"type":"array","items":{{"type":"number"}},"minItems":3,"maxItems":3}}}},)"
              R"("required":["entity"]}})",
              entityRef
            ),
            R"({"type":"object","properties":{"guids":{"type":"array"},"revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_SCENE,
            &entity_duplicate
          )
        );
        add_command(
          registry,
          make_command(
            "entity.rename",
            "Changes an entity's name.",
            std::format(R"({{"type":"object","properties":{{"entity":{},"name":{{"type":"string"}}}},)"
            R"("required":["entity","name"]}})", entityRef),
            R"({"type":"object","properties":{"guid":{"type":"string"},"name":{"type":"string"},)"
            R"("revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_SCENE,
            &entity_rename
          )
        );
        add_command(
          registry,
          make_command(
            "entity.set_parent",
            "Moves an entity under another (null = root) at a sibling index, keeping its world transform by default.",
            std::format(
              R"({{"type":"object","properties":{{"entity":{},"parent":{},)"
              R"("keepWorld":{{"type":"boolean"}},)"
              R"("index":{{"type":"integer","minimum":0}},"localTransform":{{"type":"object",)"
              R"("additionalProperties":true}}}},)"
              R"("required":["entity"]}})",
              entityRef,
              parentRef
            ),
            R"({"type":"object","properties":{"guid":{"type":"string"},"parent":{"type":["string","null"]},)"
            R"("revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_SCENE,
            &entity_set_parent
          )
        );
        add_command(
          registry,
          make_command(
            "entity.set_enabled",
            "Enables or disables an entity (and thereby its subtree).",
            std::format(R"({{"type":"object","properties":{{"entity":{},"enabled":{{"type":"boolean"}}}},)"
            R"("required":["entity","enabled"]}})", entityRef),
            R"({"type":"object","properties":{"guid":{"type":"string"},"enabled":{"type":"boolean"},)"
            R"("revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_SCENE,
            &entity_set_enabled
          )
        );
        add_command(
          registry,
          make_command(
            "entity.set_transform",
            "Sets position, rotation ([x,y,z,w] or euler degrees) and scale in local or world space.",
            std::format(
              R"({{"type":"object","properties":{{"entity":{},"position":{{"type":"array",)"
              R"("items":{{"type":"number"}}}},)"
              R"("rotation":{{"type":"array","items":{{"type":"number"}}}},"scale":{{"type":"array",)"
              R"("items":{{"type":"number"}}}},)"
              R"("space":{{"type":"string","enum":["local","world"]}}}},"required":["entity"]}})",
              entityRef
            ),
            R"({"type":"object","properties":{"guid":{"type":"string"},"local":{"type":"object",)"
            R"("additionalProperties":true},"revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_SCENE,
            &entity_set_transform
          )
        );
        add_command(
          registry,
          make_command(
            "entity.get",
            "Everything about an entity: identity, parent, children, enabled state and components as JSON.",
            std::format(
              R"({{"type":"object","properties":{{"entity":{},"components":{{"type":"array",)"
              R"("items":{{"type":"string"}}}}}},"required":["entity"]}})",
              entityRef
            ),
            ANY_OBJECT_SCHEMA.data(),
            command_flags::REQUIRES_SCENE | command_flags::READ_ONLY,
            &entity_get
          )
        );
        add_command(
          registry,
          make_command(
            "entity.find",
            "Entities matching every given filter (name, tag, layer, component), in hierarchy order.",
            R"({"type":"object","properties":{"name":{"type":"string"},"tag":{"type":"string"},)"
            R"("layer":{"type":"integer","minimum":0,"maximum":31},"component":{"type":"string"},)"
            R"("limit":{"type":"integer","minimum":1}}})",
            R"({"type":"object","properties":{"guids":{"type":"array"},"total":{"type":"integer"}}})",
            command_flags::REQUIRES_SCENE | command_flags::READ_ONLY,
            &entity_find
          )
        );
        add_command(
          registry,
          make_command(
            "entity.bounds",
            "The world bounds of an entity's subtree (a 1 m box around every entity of the subtree when nothing has "
            "render bounds).",
            std::format(R"({{"type":"object","properties":{{"entity":{}}},"required":["entity"]}})", entityRef),
            R"({"type":"object","properties":{"min":{"type":"array"},"max":{"type":"array"},)"
            R"("fromRenderBounds":{"type":"boolean"}}})",
            command_flags::REQUIRES_SCENE | command_flags::READ_ONLY,
            &entity_bounds
          )
        );
    }
}
