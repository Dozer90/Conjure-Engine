#include "editor/core/commands/command_support.h"
#include "editor/core/component_access.h"

#include "conjure/editor/entity_ref.h"
#include "conjure/editor/log_category.h"
#include "conjure/editor/play_controller.h"

#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_meta.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/vfs/path_utils.h"
#include "conjure/scene/components.h"
#include "conjure/scene/prefab.h"
#include "conjure/scene/register.h"
#include "conjure/scene/scene_serializer.h"

#include <algorithm>
#include <format>
#include <utility>

// prefab.* (docs/design/04_editor.md 4.4, 13_scene.md 3.12): create a prefab asset from a subtree (and mark
// it as the first instance), instantiate, apply an instance back to the asset and unpack an instance.

namespace conjure::editor::support {

    namespace {
        std::string normalise_prefab_path(std::string_view text) {
            std::string path = std::string(core::str::trim(text));
            if (path.find("://") == std::string::npos) {
                while (!path.empty() && (path.front() == '/' || path.front() == '\\')) {
                    path.erase(path.begin());
                }
                path = "assets://" + core::str::replace_all(path, "\\", "/");
            }
            if (!core::str::ends_with(path, ".prefab.json")) {
                if (core::str::ends_with(path, ".json")) {
                    path.erase(path.size() - 5);
                }
                path += ".prefab.json";
            }
            return path;
        }

        core::result<assets::asset_database *> require_database(editor_context &context) {
            assets::asset_database *database = context.asset_database();
            if (database == nullptr || context.assets() == nullptr) {
                return not_available("prefabs need the asset database (no project or a cooked project)");
            }
            return database;
        }

        core::result<assets::asset_id> resolve_prefab(assets::asset_database &database, std::string_view text) {
            const std::optional<core::uuid> parsed = core::uuid::parse(text);
            if (parsed.has_value()) {
                if (!database.contains(*parsed)) {
                    return make_editor_error(
                      protocol_error_code::ASSET_NOT_FOUND,
                      std::format("no prefab asset {}", parsed->to_string())
                    );
                }
                return *parsed;
            }
            const std::string path = normalise_prefab_path(text);
            const std::optional<assets::asset_id> found = database.find_by_path(path);
            if (!found.has_value()) {
                return make_editor_error(protocol_error_code::ASSET_NOT_FOUND, std::format("no prefab at '{}'", path));
            }
            return *found;
        }

        /// <summary>
        /// Returns the payload of the prefab asset, synchronously. A LOADED prefab (loaded earlier and not yet
        /// unloaded by asset_manager::collect(); no command keeps a handle on it) is used as it is. Any other
        /// state, and any prefab once the asset manager is shut down, goes through asset_manager::load_blocking,
        /// which inside a frame logs its once-per-frame hitch WARN: a prefab that was never loaded, was unloaded
        /// by collect(), or is RELOADING after prefab.apply (the wait gives the command the applied payload, not
        /// the previous one). A shut-down manager still refuses with SHUTTING_DOWN. Code sweep F736: a resident
        /// prefab costs the frame neither a blocking load nor that WARN. prefab.create normally blocks: its asset
        /// was just imported.
        /// </summary>
        core::result<const scene::prefab_asset *> load_prefab(editor_context &context, const assets::asset_id &id) {
            assets::asset_manager &manager = *context.assets();
            const bool resident = !manager.is_shut_down() && manager.state_of(id) == assets::asset_state::LOADED;
            if (!resident) {
                const core::result<assets::asset_handle> loaded = manager.load_blocking(id);
                if (!loaded.has_value()) {
                    return wrap(loaded.get_error());
                }
            }
            const scene::prefab_asset *asset = manager.bind<scene::prefab_asset>(id).get();
            if (asset == nullptr) {
                return make_editor_error(
                  protocol_error_code::ASSET_NOT_FOUND,
                  std::format("asset {} did not load as a prefab", id.to_string())
                );
            }
            return asset;
        }

        core::result<std::string> prefab_text(const scene::prefab_document &document) {
            core::json::writer out = {};
            const core::result<void> written = scene::scene_serializer::write_prefab(document, out);
            if (!written.has_value()) {
                return wrap(written.get_error());
            }
            return out.finish();
        }

        /// <summary>
        /// INVALID_ARGS unless the relative part of a prefab asset path stays inside its mount: no "." or ".."
        /// segment, no backslash (the path utilities treat it as a separator) and not absolute. Checked before
        /// anything is written.
        /// </summary>
        core::result<void> check_prefab_path(std::string_view pathText, std::string_view path) {
            const std::optional<assets::asset_path_parts> parts = assets::split_asset_path(path);
            bool contained = parts.has_value() && !parts->relativePath.empty() &&
              parts->relativePath.find('\\') == std::string_view::npos &&
              !core::vfs::path_utils::is_absolute(parts->relativePath);
            if (contained) {
                for (const std::string_view segment : core::str::split(parts->relativePath, '/', false)) {
                    if (segment == ".." || segment == ".") {
                        contained = false;
                        break;
                    }
                }
            }
            if (!contained) {
                return invalid_args(std::format(
                  "args.path: '{}' must stay inside its mount (no '..' or '.' segments, no backslash, no absolute "
                  "path)",
                  pathText
                ));
            }
            return core::result<void>::success();
        }

        core::result<command_outcome> prefab_create(command_invocation &invocation) {
            editor_context &context = invocation.context;
            scene::scene &scene = context.scene();
            CONJURE_TRY_ASSIGN(assets::asset_database *database, require_database(context));
            CONJURE_TRY_ASSIGN(const ecs::entity root, resolve_entity(invocation, invocation.args["entity"]));
            CONJURE_TRY_ASSIGN(const std::string_view pathText, require_string(invocation.args, "path"));
            const std::string path = normalise_prefab_path(pathText);
            CONJURE_TRY(check_prefab_path(pathText, path));
            if (database->find_by_path(path).has_value()) {
                return invalid_args(std::format("'{}' already exists", path));
            }
            std::optional<core::uuid> wanted = std::nullopt;
            if (invocation.args.contains("uuid")) {
                wanted = core::uuid::parse(string_or(invocation.args, "uuid", ""));
                if (!wanted.has_value() || wanted->is_null()) {
                    return invalid_args("args.uuid: expected a uuid");
                }
            }
            const std::string name = scene.name_of(root).to_string();
            const core::result<scene::prefab_document> document = scene::prefab::build_document(scene, root, name);
            if (!document.has_value()) {
                return wrap(document.get_error());
            }
            CONJURE_TRY_ASSIGN(const std::string text, prefab_text(document.value()));
            const std::span<const std::byte> bytes(reinterpret_cast<const std::byte *>(text.data()), text.size());

            // The file and its .meta are written the same way whether the uuid is chosen (a redo) or fresh, so an
            // undo / redo round trip leaves identical bytes on disk.
            CONJURE_TRY_ASSIGN(const std::string filePath, file_path_for(*database, path));
            const core::result<void> written = database->vfs().write_file(filePath, bytes);
            if (!written.has_value()) {
                return io_error(std::format("cannot write '{}': {}", filePath, written.get_error().message()));
            }
            assets::asset_meta meta = {};
            meta.id = wanted.has_value() ? *wanted : assets::asset_id::generate();
            meta.importerId = std::string(scene::PREFAB_IMPORTER_ID);
            const core::result<void> metaWritten = assets::write_asset_meta(
              database->vfs(),
              assets::meta_path_of(filePath),
              meta
            );
            // From here on a failure removes what was written (D4: a failed command leaves nothing behind).
            const auto failed = [&](const core::error &error) -> core::result<command_outcome> {
                const std::optional<assets::asset_id> registered = database->find_by_path(path);
                if (registered.has_value()) {
                    (void) database->delete_asset(*registered);
                } else {
                    (void) database->vfs().remove(filePath);
                    (void) database->vfs().remove(assets::meta_path_of(filePath));
                }
                return error;
            };
            if (!metaWritten.has_value()) {
                return failed(io_error(std::format("cannot write the .meta of '{}'", filePath)));
            }
            const core::result<assets::asset_id> added = database->add_file(path);
            if (!added.has_value()) {
                return failed(wrap(added.get_error()));
            }
            const assets::asset_id id = added.value();
            const core::result<void> imported = import_now(context, *database, id);
            if (!imported.has_value()) {
                return failed(imported.get_error());
            }
            const core::result<const scene::prefab_asset *> loaded = load_prefab(context, id);
            if (!loaded.has_value()) {
                return failed(loaded.get_error());
            }
            const core::result<void> marked = scene::prefab::mark_instance(
              scene,
              root,
              context.assets()->bind<scene::prefab_asset>(id)
            );
            if (!marked.has_value()) {
                return failed(wrap(marked.get_error()));
            }
            CONJURE_TRY_ASSIGN(const std::string guid, guid_string(scene, root));
            context.events().emit(
              editor_event::ASSET_IMPORTED,
              core::json::object({{"uuid", id.to_string()}, {"path", path}})
            );

            std::vector<command_inverse> inverses = {};
            inverses.push_back(command_inverse{"asset.delete", core::json::object({{"asset", id.to_string()}})});
            inverses.push_back(command_inverse{"prefab.unpack", core::json::object({{"entity", guid}})});
            command_outcome outcome = {};
            outcome.label = std::format("Create prefab '{}'", path);
            outcome.result = core::json::object({{"uuid", id.to_string()}, {"path", path}, {"guid", guid}});
            outcome.inverse = command_inverse{"editor.batch", batch_args(inverses, "Undo prefab")};
            command_request replay = {};
            replay.name = "prefab.create";
            replay.args = core::json::object({{"entity", guid}, {"path", path}, {"uuid", id.to_string()}});
            outcome.replay = std::move(replay);
            return outcome;
        }

        core::result<command_outcome> prefab_instantiate(command_invocation &invocation) {
            editor_context &context = invocation.context;
            scene::scene &scene = context.scene();
            CONJURE_TRY_ASSIGN(assets::asset_database *database, require_database(context));
            CONJURE_TRY_ASSIGN(const std::string_view reference, require_string(invocation.args, "prefab"));
            CONJURE_TRY_ASSIGN(const assets::asset_id id, resolve_prefab(*database, reference));
            CONJURE_TRY_ASSIGN(const scene::prefab_asset *asset, load_prefab(context, id));
            CONJURE_TRY_ASSIGN(const u32 count, u32_or(invocation.args, "count", 1));
            if (count == 0 || count > 100000) {
                return invalid_args("args.count: expected 1..100000");
            }
            scene::instantiate_options options = {};
            if (invocation.args.contains("parent") && !invocation.args["parent"].is_null()) {
                CONJURE_TRY_ASSIGN(options.parent, resolve_entity(invocation, invocation.args["parent"]));
            }
            if (invocation.args.contains("name")) {
                const std::string_view rootName = string_or(invocation.args, "name", "");
                CONJURE_TRY(check_entity_name(rootName, "args.name"));
                options.rootName = core::string_id(rootName);
            }
            if (invocation.args.contains("transform")) {
                CONJURE_TRY_ASSIGN(
                  const math::transform transform,
                  read_transform(invocation.args["transform"], "args.transform", math::transform())
                );
                options.rootTransform = scene::local_transform::from_transform(transform);
            }
            // Optional root guids (one per instance): a command file replays deterministically with them.
            std::vector<core::uuid> wantedGuids = {};
            const core::json::value wanted = invocation.args["guids"];
            if (wanted.is_valid()) {
                if (!wanted.is_array() || wanted.size() != count) {
                    return invalid_args("args.guids: expected one guid per instance");
                }
                for (const core::json::value entry : wanted) {
                    const std::optional<core::uuid> parsed = core::uuid::parse(entry.as_string(""));
                    if (!parsed.has_value() || parsed->is_null()) {
                        return invalid_args("args.guids: expected uuid strings");
                    }
                    if (!scene.find_guid(*parsed).is_null()) {
                        return invalid_args(std::format("args.guids: {} is already in use", parsed->to_string()));
                    }
                    if (std::find(wantedGuids.begin(), wantedGuids.end(), *parsed) != wantedGuids.end()) {
                        return invalid_args(std::format("args.guids: {} is listed twice", parsed->to_string()));
                    }
                    wantedGuids.push_back(*parsed);
                }
            }
            std::vector<ecs::entity> roots(count, ecs::NULL_ENTITY);
            const core::result<void> instantiated = scene::prefab::instantiate_many(
              scene,
              *asset,
              count,
              options,
              roots
            );
            if (!instantiated.has_value()) {
                return wrap(instantiated.get_error());
            }
            // Anything that fails after the instantiation takes the new roots out again (D4: all or nothing).
            const auto failed = [&roots, &scene](core::error error) -> core::result<command_outcome> {
                for (const ecs::entity root : roots) {
                    if (!root.is_null() && scene.contains(root)) {
                        (void) scene.destroy_now(root);
                    }
                }
                return error;
            };
            std::vector<core::uuid> guids = {};
            std::vector<command_inverse> restores = {};
            for (usize index = 0; index < roots.size(); ++index) {
                const ecs::entity root = roots[index];
                if (index < wantedGuids.size()) {
                    const core::result<void> renamed = scene.set_guid(root, wantedGuids[index]);
                    if (!renamed.has_value()) {
                        return failed(wrap(renamed.get_error()));
                    }
                }
                const core::result<core::uuid> guid = entity_ref::guid_of(scene, root);
                if (!guid.has_value()) {
                    return failed(guid.get_error());
                }
                guids.push_back(guid.value());
                core::result<command_inverse> restore = restore_inverse(scene, root);
                if (!restore.has_value()) {
                    return failed(restore.get_error());
                }
                restores.push_back(std::move(restore).value());
            }
            command_outcome outcome = {};
            outcome.label = std::format("Instantiate '{}'", asset->name);
            outcome.result = core::json::object({{"guids", guid_array(guids)}, {"prefab", id.to_string()}});
            outcome.inverse = command_inverse{"entity.destroy_many", core::json::object(
              {{"entities", guid_array(guids)}}
            )};
            command_request replay = {};
            replay.name = "editor.batch";
            replay.args = batch_args(restores, "Instantiate prefab");
            outcome.replay = std::move(replay);
            return outcome;
        }

        core::result<command_outcome> prefab_apply(command_invocation &invocation) {
            editor_context &context = invocation.context;
            scene::scene &scene = context.scene();
            CONJURE_TRY_ASSIGN(assets::asset_database *database, require_database(context));
            CONJURE_TRY_ASSIGN(const ecs::entity entity, resolve_entity(invocation, invocation.args["entity"]));
            const ecs::entity root = scene::prefab::instance_root_of(scene, entity);
            if (root.is_null()) {
                return invalid_args("the entity is not part of a prefab instance");
            }
            const scene::prefab_instance *instance = scene.world().get<scene::prefab_instance>(root);
            if (instance == nullptr) {
                return invalid_args("the entity is not part of a prefab instance");
            }
            const assets::asset_id id = instance->prefab.id();
            CONJURE_TRY(load_prefab(context, id));
            const core::result<scene::prefab_document> document = scene::prefab::apply(scene, root);
            if (!document.has_value()) {
                return wrap(document.get_error());
            }
            const std::optional<std::string> filePath = database->file_path_of(id);
            const std::optional<std::string> assetPath = database->asset_path_of(id);
            if (!filePath.has_value() || !assetPath.has_value()) {
                return make_editor_error(protocol_error_code::ASSET_NOT_FOUND, "the prefab asset has no source file");
            }
            core::result<std::vector<std::byte>> previous = database->vfs().read_file(*filePath);
            if (!previous.has_value()) {
                return read_failure(*filePath, previous.get_error());
            }
            CONJURE_TRY_ASSIGN(const std::string text, prefab_text(document.value()));
            const std::span<const std::byte> bytes(reinterpret_cast<const std::byte *>(text.data()), text.size());
            const core::result<void> written = database->vfs().write_file_atomic(*filePath, bytes);
            if (!written.has_value()) {
                return io_error(std::format("cannot write '{}': {}", *filePath, written.get_error().message()));
            }
            CONJURE_TRY(import_now(context, *database, id));
            context.assets()->reload(id);
            CONJURE_TRY_ASSIGN(const std::string guid, guid_string(scene, root));
            // The previous file content, base64 encoded through asset.restore's contract.
            const std::string encoded = base64_encode(previous.value());
            command_outcome outcome = {};
            outcome.label = std::format("Apply prefab '{}'", scene.name_of(root).to_string());
            outcome.result = core::json::object({{"uuid", id.to_string()}, {"path", *assetPath}, {"guid", guid}});
            outcome.inverse = command_inverse{"asset.restore", core::json::object({
              {"path", *assetPath},
              {"contentBase64", encoded},
            })};
            if (!context.play().is_editing()) {
                // The redo runs after play.stop restored the scene (D21): the instance may be gone or hold other
                // values by then, so it writes the bytes applied here back instead of applying again.
                command_request replay = {};
                replay.name = "asset.restore";
                replay.args = core::json::object({
                  {"path", *assetPath},
                  {"contentBase64", base64_encode(bytes)},
                });
                outcome.replay = std::move(replay);
            }
            return outcome;
        }

        core::result<command_outcome> prefab_unpack(command_invocation &invocation) {
            editor_context &context = invocation.context;
            scene::scene &scene = context.scene();
            CONJURE_TRY_ASSIGN(const ecs::entity entity, resolve_entity(invocation, invocation.args["entity"]));
            const ecs::entity root = scene::prefab::instance_root_of(scene, entity);
            if (root.is_null()) {
                return invalid_args("the entity is not part of a prefab instance");
            }
            const bool recursive = bool_or(invocation.args, "recursive", false);
            // Capture the markers so undo can put them back through component.add.
            std::vector<command_inverse> restores = {};
            const ecs::component_id instanceId = scene.ids().prefabInstance;
            const ecs::component_id linkId = scene.ids().prefabLink;
            std::vector<ecs::entity> members = {};
            scene.tree().collect_subtree(root, members);
            for (const ecs::entity member : members) {
                if (!recursive && member != root && scene.world().has_raw(member, instanceId)) {
                    // A nested instance keeps its markers (and so does its subtree).
                    continue;
                }
                const std::string guid = guid_or_empty(scene, member);
                if (scene.world().has_raw(member, linkId)) {
                    CONJURE_TRY_ASSIGN(const core::json::document link, access::read_component(scene, member, linkId));
                    restores.push_back(command_inverse{
                      "component.add",
                      core::json::object({{"entity", guid}, {"type", "conjure.scene.prefab_link"}, {"value", link}}),
                    });
                }
                if (scene.world().has_raw(member, instanceId)) {
                    CONJURE_TRY_ASSIGN(
                      const core::json::document instance,
                      access::read_component(scene, member, instanceId)
                    );
                    restores.push_back(command_inverse{
                      "component.add",
                      core::json::object({
                        {"entity", guid},
                        {"type", "conjure.scene.prefab_instance"},
                        {"value", instance},
                      }),
                    });
                }
            }
            const core::result<void> unpacked = scene::prefab::unpack(scene, root, recursive);
            if (!unpacked.has_value()) {
                return wrap(unpacked.get_error());
            }
            CONJURE_TRY_ASSIGN(const std::string guid, guid_string(scene, root));
            command_outcome outcome = {};
            outcome.label = std::format("Unpack '{}'", scene.name_of(root).to_string());
            outcome.result = core::json::object({{"guid", guid}, {"entities", members.size()}});
            outcome.inverse = command_inverse{"editor.batch", batch_args(restores, "Repack prefab")};
            return outcome;
        }
    }

    void register_prefab_commands(command_registry &registry) {
        const std::string entityRef = std::string(ENTITY_REF_SCHEMA);
        add_command(
          registry,
          make_command(
            "prefab.create",
            "Saves an entity's subtree as a .prefab.json asset and marks the subtree as its first instance. Editing "
            "state only: play.stop would discard the marked instance and keep the file.",
            std::format(
              R"({{"type":"object","properties":{{"entity":{},"path":{{"type":"string"}},"uuid":{{"type":"string"}}}},)"
              R"("required":["entity","path"]}})",
              entityRef
            ),
            R"({"type":"object","properties":{"uuid":{"type":"string"},"path":{"type":"string"},)"
            R"("guid":{"type":"string"},"revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_PROJECT | command_flags::REQUIRES_SCENE |
              command_flags::EDITING_ONLY,
            &prefab_create
          )
        );
        add_command(
          registry,
          make_command(
            "prefab.instantiate",
            "Instantiates a prefab (uuid or path) count times under a parent, with an optional root name, transform "
            "and root guids.",
            std::format(
              R"({{"type":"object","properties":{{"prefab":{{"type":"string"}},"parent":{{"type":["string","null"]}},)"
              R"("name":{{"type":"string"}},)"
              R"("transform":{{"type":"object","additionalProperties":true}},"count":{{"type":"integer","minimum":1}},)"
              R"("guids":{{"type":"array","items":{{"type":"string"}}}}}},"required":["prefab"]}})",
              entityRef
            ),
            R"({"type":"object","properties":{"guids":{"type":"array"},"prefab":{"type":"string"},)"
            R"("revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_SCENE,
            &prefab_instantiate
          )
        );
        add_command(
          registry,
          make_command(
            "prefab.apply",
            "Writes an instance's current state back into its prefab asset. While playing the file change is kept: "
            "play.stop records it for undo with the session's other file changes.",
            std::format(R"({{"type":"object","properties":{{"entity":{}}},"required":["entity"]}})", entityRef),
            R"({"type":"object","properties":{"uuid":{"type":"string"},"path":{"type":"string"},)"
            R"("guid":{"type":"string"},"revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_PROJECT | command_flags::REQUIRES_SCENE |
              command_flags::ON_DISK,
            &prefab_apply
          )
        );
        add_command(
          registry,
          make_command(
            "prefab.unpack",
            "Removes the prefab markers of an instance so it becomes plain entities.",
            std::format(R"({{"type":"object","properties":{{"entity":{},"recursive":{{"type":"boolean"}}}},)"
            R"("required":["entity"]}})", entityRef),
            R"({"type":"object","properties":{"guid":{"type":"string"},"entities":{"type":"integer"},)"
            R"("revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_SCENE,
            &prefab_unpack
          )
        );
    }
}
