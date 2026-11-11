#include "editor/core/commands/command_support.h"

#include "conjure/editor/log_category.h"

#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_meta.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/vfs/path_utils.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/renderer/material_asset.h"
#endif

#include <array>
#include <cstddef>
#include <format>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

// asset.* (docs/design/04_editor.md 4.4, R35): the editor-side asset database. Imports are synchronous on
// the main thread; every destructive command captures the source and its .meta so asset.restore can undo it
// with the same uuid.

namespace conjure::editor::support {

    namespace {
        std::string normalise_asset_path(std::string_view text) {
            std::string path = std::string(core::str::trim(text));
            if (path.find("://") == std::string::npos) {
                while (!path.empty() && (path.front() == '/' || path.front() == '\\')) {
                    path.erase(path.begin());
                }
                path = "assets://" + core::str::replace_all(path, "\\", "/");
            }
            return path;
        }

        /// <summary>
        /// The database-relative file path of an asset path ("assets://a/b.png" -> "<mount root>/a/b.png").
        /// </summary>
        core::result<std::string> file_path_for(assets::asset_database &database, std::string_view assetPath) {
            const std::optional<assets::asset_path_parts> parts = assets::split_asset_path(assetPath);
            if (!parts.has_value()) {
                return invalid_args(std::format("'{}' is not an asset path (mount://relative)", assetPath));
            }
            for (const assets::asset_mount &mount : database.mounts()) {
                if (mount.name == parts->mount) {
                    if (mount.rootPath.empty()) {
                        return std::string(parts->relativePath);
                    }
                    return mount.rootPath + "/" + std::string(parts->relativePath);
                }
            }
            return make_editor_error(
              protocol_error_code::ASSET_NOT_FOUND,
              std::format("unknown asset mount '{}'", parts->mount)
            );
        }

        core::result<assets::asset_database *> require_database(editor_context &context) {
            assets::asset_database *database = context.asset_database();
            if (database == nullptr) {
                return not_available("the asset database is not available (cooked project or no project)");
            }
            return database;
        }

        /// <summary>
        /// A uuid string or an asset path.
        /// </summary>
        core::result<assets::asset_id> resolve_asset(assets::asset_database &database, std::string_view text) {
            if (text.empty()) {
                return invalid_args("an asset reference must not be empty");
            }
            const std::optional<core::uuid> parsed = core::uuid::parse(text);
            if (parsed.has_value()) {
                if (!database.contains(*parsed)) {
                    return make_editor_error(
                      protocol_error_code::ASSET_NOT_FOUND,
                      std::format("no asset {}", parsed->to_string())
                    );
                }
                return *parsed;
            }
            const std::string path = normalise_asset_path(text);
            const std::optional<assets::asset_id> found = database.find_by_path(path);
            if (!found.has_value()) {
                return make_editor_error(protocol_error_code::ASSET_NOT_FOUND, std::format("no asset at '{}'", path));
            }
            return *found;
        }

        core::json::document describe_record(
          const assets::asset_database &database,
          const assets::asset_record &record
        ) {
            const std::optional<std::string> path = database.asset_path_of(record.id);
            std::string typeName = "";
            const assets::asset_type_desc *type = database.types().find(record.type);
            if (type != nullptr) {
                typeName = type->name;
            }
            return core::json::object({
              {"uuid", record.id.to_string()},
              {"path", path.value_or("")},
              {"type", typeName},
              {"status", assets::to_string(record.status)},
              {"importer", std::string_view(record.importerId)},
              {"displayName", std::string_view(record.displayName)},
              {"subPath", std::string_view(record.subPath)},
              {"parent", record.parentId.is_null() ? std::string("") : record.parentId.to_string()},
            });
        }

        core::json::document sub_assets_of(const assets::asset_database &database, const assets::asset_id &id) {
            core::json::document list = core::json::document::array();
            for (const assets::asset_record &record : database.records()) {
                if (record.parentId == id) {
                    const assets::asset_type_desc *type = database.types().find(record.type);
                    list.push_back(core::json::object({
                      {"uuid", record.id.to_string()},
                      {"name", std::string_view(record.subPath)},
                      {"type", type != nullptr ? std::string_view(type->name) : std::string_view("")},
                    }));
                }
            }
            return list;
        }

        core::result<void> import_now(
          editor_context &context,
          assets::asset_database &database,
          const assets::asset_id &id
        ) {
            if (context.engine_context().jobs == nullptr) {
                return not_available("no job system to import with");
            }
            const std::array<assets::asset_id, 1> ids = {id};
            const core::result<assets::import_report> imported = database.import(ids, *context.engine_context().jobs);
            if (!imported.has_value()) {
                return wrap(imported.get_error());
            }
            for (const assets::import_failure &failure : imported.value().failures) {
                if (failure.id == id) {
                    return invalid_args(std::format("import of '{}' failed: {}", failure.assetPath, failure.message));
                }
            }
            return core::result<void>::success();
        }

        /// <summary>
        /// The source bytes and .meta text of a primary asset, for asset.restore.
        /// </summary>
        core::result<command_inverse> restore_inverse_for(
          assets::asset_database &database,
          const assets::asset_id &id
        ) {
            const std::optional<assets::asset_record> record = database.find(id);
            if (!record.has_value()) {
                return make_editor_error(
                  protocol_error_code::ASSET_NOT_FOUND,
                  std::format("no asset {}", id.to_string())
                );
            }
            const assets::asset_id primary = record->parentId.is_null() ? id : record->parentId;
            const std::optional<std::string> assetPath = database.asset_path_of(primary);
            const std::optional<std::string> filePath = database.file_path_of(primary);
            if (!assetPath.has_value() || !filePath.has_value()) {
                return make_editor_error(protocol_error_code::ASSET_NOT_FOUND, "the asset has no source file");
            }
            core::result<std::vector<std::byte>> bytes = database.vfs().read_file(*filePath);
            if (!bytes.has_value()) {
                return io_error(std::format("cannot read '{}': {}", *filePath, bytes.get_error().message()));
            }
            command_inverse inverse = {};
            inverse.name = "asset.restore";
            inverse.args = core::json::object({{"path", *assetPath}, {"contentBase64", base64_encode(bytes.value())}});
            core::result<std::vector<std::byte>> meta = database.vfs().read_file(assets::meta_path_of(*filePath));
            if (meta.has_value()) {
                inverse.args.set("metaBase64", base64_encode(meta.value()));
            }
            return inverse;
        }

        core::result<assets::asset_id> register_and_import(
          editor_context &context,
          assets::asset_database &database,
          const std::string &assetPath
        ) {
            std::optional<assets::asset_id> id = database.find_by_path(assetPath);
            if (!id.has_value()) {
                core::result<assets::asset_id> added = database.add_file(assetPath);
                if (!added.has_value()) {
                    return wrap(added.get_error());
                }
                id = added.value();
            }
            CONJURE_TRY(import_now(context, database, *id));
            return *id;
        }

        /*-------------------------------------------------------------------*/
        // Handlers

        core::result<command_outcome> asset_import(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(assets::asset_database *database, require_database(context));
            CONJURE_TRY_ASSIGN(const std::string_view sourceText, require_string(invocation.args, "source"));
            if (sourceText.empty()) {
                return invalid_args("args.source: a file or asset path is required");
            }
            std::string assetPath = "";
            bool copiedIn = false;
            if (
              sourceText.find(
                "://"
              ) != std::string_view::npos || !std::filesystem::path(std::string(sourceText)).is_absolute()
            ) {
                // Already inside the project: an asset path, or a path relative to assets://.
                const std::string candidate = normalise_asset_path(sourceText);
                if (context.filesystem().exists(candidate)) {
                    assetPath = candidate;
                }
            }
            if (assetPath.empty()) {
                copiedIn = true;
                // A file outside the assets root: copy it in.
                const std::filesystem::path source = std::filesystem::path(std::string(sourceText));
                std::ifstream input(source, std::ios::binary);
                if (!input.is_open()) {
                    return make_editor_error(
                      protocol_error_code::ASSET_NOT_FOUND,
                      std::format("cannot read '{}'", source.generic_string())
                    );
                }
                std::stringstream buffer = {};
                buffer << input.rdbuf();
                const std::string content = buffer.str();
                const std::string_view destination = string_or(invocation.args, "destination", "");
                assetPath = destination.empty() ? normalise_asset_path(
                  source.filename().generic_string()
                ) : normalise_asset_path(destination);
                const core::result<void> written = context.filesystem().write_file(
                  assetPath,
                  std::span<const std::byte>(reinterpret_cast<const std::byte *>(content.data()), content.size())
                );
                if (!written.has_value()) {
                    return io_error(std::format("cannot write '{}': {}", assetPath, written.get_error().message()));
                }
            }
            std::optional<assets::asset_id> existing = database->find_by_path(assetPath);
            if (!existing.has_value()) {
                core::result<assets::asset_id> added = database->add_file(assetPath);
                if (!added.has_value()) {
                    return wrap(added.get_error());
                }
                existing = added.value();
            }
            const assets::asset_id id = *existing;
            const core::json::value options = invocation.args["options"];
            if (options.is_object()) {
                core::result<assets::asset_meta> meta = database->read_meta(id);
                if (meta.has_value()) {
                    meta.value().settings.merge_patch(options);
                    const core::result<void> written = database->write_meta(id, meta.value());
                    if (!written.has_value()) {
                        return wrap(written.get_error());
                    }
                }
            }
            CONJURE_TRY(import_now(context, *database, id));
            const std::optional<assets::asset_record> record = database->find(id);
            if (!record.has_value()) {
                return internal("the imported asset vanished from the database");
            }
            CONJURE_TRY_ASSIGN(command_inverse restore, restore_inverse_for(*database, id));
            core::json::document result = describe_record(*database, *record);
            result.set("subAssets", sub_assets_of(*database, id));
            context.events().emit(
              editor_event::ASSET_IMPORTED,
              core::json::object({{"uuid", id.to_string()}, {"path", assetPath}})
            );
            command_outcome outcome = {};
            outcome.label = std::format("Import '{}'", assetPath);
            outcome.result = std::move(result);
            outcome.result.set("copied", copiedIn);
            if (!copiedIn) {
                // Registering or re-cooking a file that already lived under assets:// destroyed nothing, so
                // there is nothing to undo: deleting the user's file on undo would be destructive.
                outcome.skipRecording = true;
                return outcome;
            }
            outcome.inverse = command_inverse{"asset.delete", core::json::object({{"asset", id.to_string()}})};
            command_request replay = {};
            replay.name = restore.name;
            replay.args = std::move(restore.args);
            outcome.replay = std::move(replay);
            return outcome;
        }

        core::result<command_outcome> asset_list(command_invocation &invocation) {
            CONJURE_TRY_ASSIGN(assets::asset_database *database, require_database(invocation.context));
            const std::string_view filter = string_or(invocation.args, "filter", "");
            const std::string_view typeFilter = string_or(invocation.args, "type", "");
            const bool includeSubAssets = bool_or(invocation.args, "subAssets", false);
            core::json::document list = core::json::document::array();
            for (const assets::asset_record &record : database->records()) {
                if (!includeSubAssets && record.is_sub_asset()) {
                    continue;
                }
                const std::optional<std::string> path = database->asset_path_of(record.id);
                if (!filter.empty() && (!path.has_value() || path->find(filter) == std::string::npos)) {
                    continue;
                }
                if (!typeFilter.empty()) {
                    const assets::asset_type_desc *type = database->types().find(record.type);
                    if (type == nullptr || type->name != typeFilter) {
                        continue;
                    }
                }
                list.push_back(describe_record(*database, record));
            }
            command_outcome outcome = {};
            outcome.result = core::json::object({{"assets", list}, {"count", list.root().size()}});
            return outcome;
        }

        core::result<command_outcome> asset_info(command_invocation &invocation) {
            CONJURE_TRY_ASSIGN(assets::asset_database *database, require_database(invocation.context));
            CONJURE_TRY_ASSIGN(const std::string_view reference, require_string(invocation.args, "asset"));
            CONJURE_TRY_ASSIGN(const assets::asset_id id, resolve_asset(*database, reference));
            const std::optional<assets::asset_record> record = database->find(id);
            if (!record.has_value()) {
                return make_editor_error(
                  protocol_error_code::ASSET_NOT_FOUND,
                  std::format("no asset {}", id.to_string())
                );
            }
            core::json::document result = describe_record(*database, *record);
            core::json::document dependencies = core::json::document::array();
            for (const assets::asset_id &dependency : database->dependencies_of(id)) {
                dependencies.push_back(dependency.to_string());
            }
            result.set("dependencies", dependencies);
            result.set("subAssets", sub_assets_of(*database, id));
            const core::result<assets::asset_meta> meta = database->read_meta(id);
            if (meta.has_value()) {
                result.set("settings", meta.value().settings);
            }
            if (invocation.context.assets() != nullptr) {
                result.set("state", assets::to_string(invocation.context.assets()->state_of(id)));
            }
            command_outcome outcome = {};
            outcome.result = std::move(result);
            return outcome;
        }

        core::result<command_outcome> asset_create_folder(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(const std::string_view pathText, require_string(invocation.args, "path"));
            const std::string path = normalise_asset_path(pathText);
            if (context.filesystem().exists(path)) {
                return invalid_args(std::format("'{}' already exists", path));
            }
            const core::result<void> created = context.filesystem().create_directories(path);
            if (!created.has_value()) {
                return io_error(std::format("cannot create '{}': {}", path, created.get_error().message()));
            }
            command_outcome outcome = {};
            outcome.label = std::format("Create folder '{}'", path);
            outcome.result = core::json::object({{"path", path}});
            outcome.inverse = command_inverse{"asset.delete", core::json::object({{"asset", path}})};
            return outcome;
        }

        core::result<command_outcome> asset_delete(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(assets::asset_database *database, require_database(context));
            CONJURE_TRY_ASSIGN(const std::string_view reference, require_string(invocation.args, "asset"));
            const core::result<assets::asset_id> resolved = resolve_asset(*database, reference);
            if (!resolved.has_value()) {
                // Not an asset: an empty folder may be deleted.
                const std::string path = normalise_asset_path(reference);
                const core::result<core::vfs::file_stat> stat = context.filesystem().stat(path);
                if (!stat.has_value() || !stat.value().isDirectory) {
                    return core::error(resolved.get_error());
                }
                const core::result<void> removed = context.filesystem().remove(path);
                if (!removed.has_value()) {
                    return io_error(std::format("cannot remove '{}': {}", path, removed.get_error().message()));
                }
                command_outcome outcome = {};
                outcome.label = std::format("Delete folder '{}'", path);
                outcome.result = core::json::object({{"path", path}, {"folder", true}});
                outcome.inverse = command_inverse{"asset.create_folder", core::json::object({{"path", path}})};
                return outcome;
            }
            const assets::asset_id id = resolved.value();
            const std::optional<assets::asset_record> record = database->find(id);
            if (!record.has_value() || record->is_sub_asset()) {
                return invalid_args("only primary assets can be deleted");
            }
            CONJURE_TRY_ASSIGN(command_inverse restore, restore_inverse_for(*database, id));
            const std::optional<std::string> assetPath = database->asset_path_of(id);
            const core::result<void> deleted = database->delete_asset(id);
            if (!deleted.has_value()) {
                return wrap(deleted.get_error());
            }
            command_outcome outcome = {};
            outcome.label = std::format("Delete '{}'", assetPath.value_or(id.to_string()));
            outcome.result = core::json::object({{"uuid", id.to_string()}, {"path", assetPath.value_or("")}});
            outcome.inverse = std::move(restore);
            return outcome;
        }

        core::result<command_outcome> asset_reimport(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(assets::asset_database *database, require_database(context));
            CONJURE_TRY_ASSIGN(const std::string_view reference, require_string(invocation.args, "asset"));
            CONJURE_TRY_ASSIGN(const assets::asset_id id, resolve_asset(*database, reference));
            CONJURE_TRY(import_now(context, *database, id));
            if (context.assets() != nullptr) {
                context.assets()->reload(id);
            }
            const std::optional<assets::asset_record> record = database->find(id);
            command_outcome outcome = {};
            outcome.result = record.has_value() ? describe_record(*database, *record) : core::json::document::object();
            return outcome;
        }

        core::result<command_outcome> asset_create_material(command_invocation &invocation) {
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(assets::asset_database *database, require_database(context));
            CONJURE_TRY_ASSIGN(const std::string_view pathText, require_string(invocation.args, "path"));
            std::string path = normalise_asset_path(pathText);
            if (!core::str::ends_with(path, ".cmat")) {
                path += ".cmat";
            }
            if (database->find_by_path(path).has_value()) {
                return invalid_args(std::format("'{}' already exists", path));
            }
            renderer::material_desc desc = {};
            if (invocation.args.contains("baseColor")) {
                const core::json::value colour = invocation.args["baseColor"];
                if (!colour.is_array() || (colour.size() != 3 && colour.size() != 4)) {
                    return invalid_args("args.baseColor: expected [r, g, b] or [r, g, b, a]");
                }
                desc.baseColourFactor = math::vec4(colour[0].as_f32(), colour[1].as_f32(), colour[2].as_f32(),
                  colour.size() == 4 ? colour[3].as_f32() : 1.0f);
            }
            CONJURE_TRY_ASSIGN(desc.metallicFactor, f32_or(invocation.args, "metallic", desc.metallicFactor));
            CONJURE_TRY_ASSIGN(desc.roughnessFactor, f32_or(invocation.args, "roughness", desc.roughnessFactor));
            if (invocation.args.contains("emissive")) {
                CONJURE_TRY_ASSIGN(desc.emissiveFactor, read_vec3(invocation.args["emissive"], "args.emissive"));
            }
            const core::json::value textures = invocation.args["textures"];
            if (textures.is_object()) {
                const std::array<std::pair<std::string_view, renderer::material_texture>, 5> slots = {
                    std::pair{"albedo", renderer::material_texture::BASE_COLOUR},
                    std::pair{"metallicRoughness", renderer::material_texture::METALLIC_ROUGHNESS},
                    std::pair{"normal", renderer::material_texture::NORMAL},
                    std::pair{"occlusion", renderer::material_texture::OCCLUSION},
                    std::pair{"emissive", renderer::material_texture::EMISSIVE},
                };
                for (const auto &[key, slot] : slots) {
                    if (!textures.contains(key)) {
                        continue;
                    }
                    CONJURE_TRY_ASSIGN(
                      const assets::asset_id texture,
                      resolve_asset(*database, textures[key].as_string())
                    );
                    desc.textures[static_cast<usize>(slot)] = assets::asset_ref<renderer::texture_asset>(texture);
                }
            }
            const renderer::material_asset material(desc, std::string(core::vfs::path_utils::stem(path)));
            const std::string text = material.to_json();
            core::result<assets::asset_id> created = database->create_asset(
              path,
              renderer::MATERIAL_IMPORTER_ID,
              std::span<const std::byte>(reinterpret_cast<const std::byte *>(text.data()), text.size())
            );
            if (!created.has_value()) {
                return wrap(created.get_error());
            }
            const assets::asset_id id = created.value();
            CONJURE_TRY(import_now(context, *database, id));
            CONJURE_TRY_ASSIGN(command_inverse restore, restore_inverse_for(*database, id));
            context.events().emit(
              editor_event::ASSET_IMPORTED,
              core::json::object({{"uuid", id.to_string()}, {"path", path}})
            );
            command_outcome outcome = {};
            outcome.label = std::format("Create material '{}'", path);
            outcome.result = core::json::object({{"uuid", id.to_string()}, {"path", path}});
            outcome.inverse = command_inverse{"asset.delete", core::json::object({{"asset", id.to_string()}})};
            command_request replay = {};
            replay.name = restore.name;
            replay.args = std::move(restore.args);
            outcome.replay = std::move(replay);
            return outcome;
#else
            (void) invocation;
            return not_available("asset.create_material needs the renderer, which this build lacks");
#endif
        }

        core::result<command_outcome> asset_set(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(assets::asset_database *database, require_database(context));
            CONJURE_TRY_ASSIGN(const std::string_view reference, require_string(invocation.args, "asset"));
            CONJURE_TRY_ASSIGN(const std::string_view pathText, require_string(invocation.args, "path"));
            const core::json::value value = invocation.args["value"];
            if (!value.is_valid()) {
                return invalid_args("args.value: a value is required");
            }
            CONJURE_TRY_ASSIGN(const assets::asset_id id, resolve_asset(*database, reference));
            const std::optional<std::string> filePath = database->file_path_of(id);
            if (!filePath.has_value()) {
                return make_editor_error(protocol_error_code::ASSET_NOT_FOUND, "the asset has no source file");
            }
            core::result<std::string> text = database->vfs().read_text_file(*filePath);
            if (!text.has_value()) {
                return io_error(std::format("cannot read '{}': {}", *filePath, text.get_error().message()));
            }
            core::result<core::json::document> parsed = core::json::document::parse(text.value());
            if (!parsed.has_value()) {
                return invalid_args(std::format(
                  "'{}' is not a JSON asset; asset.set edits JSON assets only",
                  *filePath
                ));
            }
            std::string pointer = "";
            for (const std::string_view segment : core::str::split(pathText, '.', false)) {
                pointer += '/';
                pointer += std::string(segment);
            }
            if (pointer.empty()) {
                return invalid_args("args.path: a property path is required");
            }
            const core::json::value previousValue = parsed.value().at_pointer(pointer);
            if (!previousValue.is_valid()) {
                return make_editor_error(
                  protocol_error_code::PATH_NOT_FOUND,
                  std::format("path '{}' does not exist in the asset", pathText)
                );
            }
            core::json::document previous = previousValue.is_valid() ? core::json::document::from_value(
              previousValue
            ) : core::json::document();
            // The undo puts the file back byte for byte: a re-serialisation would change its formatting.
            CONJURE_TRY_ASSIGN(command_inverse restore, restore_inverse_for(*database, id));
            if (!parsed.value().set_pointer(pointer, value)) {
                return make_editor_error(
                  protocol_error_code::PATH_NOT_FOUND,
                  std::format("path '{}' cannot be set in the asset", pathText)
                );
            }
            const std::string updated = parsed.value().to_string(true);
            const core::result<void> written = database->vfs().write_file_atomic(
              *filePath,
              std::span<const std::byte>(reinterpret_cast<const std::byte *>(updated.data()), updated.size())
            );
            if (!written.has_value()) {
                return io_error(std::format("cannot write '{}': {}", *filePath, written.get_error().message()));
            }
            CONJURE_TRY(import_now(context, *database, id));
            if (context.assets() != nullptr) {
                context.assets()->reload(id);
            }
            command_outcome outcome = {};
            outcome.label = std::format("Set {} on asset", pathText);
            outcome.result = core::json::object({{"uuid", id.to_string()}, {"path", pathText}, {"previous", previous}});
            outcome.inverse = std::move(restore);
            return outcome;
        }

        core::result<command_outcome> asset_restore(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(assets::asset_database *database, require_database(context));
            CONJURE_TRY_ASSIGN(const std::string_view pathText, require_string(invocation.args, "path"));
            CONJURE_TRY_ASSIGN(const std::string_view contentText, require_string(invocation.args, "contentBase64"));
            const std::string assetPath = normalise_asset_path(pathText);
            CONJURE_TRY_ASSIGN(const std::string filePath, file_path_for(*database, assetPath));
            CONJURE_TRY_ASSIGN(const std::vector<std::byte> content, base64_decode(contentText));
            const core::result<void> written = database->vfs().write_file(filePath, content);
            if (!written.has_value()) {
                return io_error(std::format("cannot write '{}': {}", filePath, written.get_error().message()));
            }
            if (invocation.args.contains("metaBase64")) {
                CONJURE_TRY_ASSIGN(
                  const std::vector<std::byte> meta,
                  base64_decode(string_or(invocation.args, "metaBase64", ""))
                );
                const core::result<void> metaWritten = database->vfs().write_file(assets::meta_path_of(filePath), meta);
                if (!metaWritten.has_value()) {
                    return io_error(std::format("cannot write the .meta of '{}'", filePath));
                }
            }
            CONJURE_TRY_ASSIGN(const assets::asset_id id, register_and_import(context, *database, assetPath));
            context.events().emit(
              editor_event::ASSET_IMPORTED,
              core::json::object({{"uuid", id.to_string()}, {"path", assetPath}})
            );
            command_outcome outcome = {};
            outcome.label = std::format("Restore '{}'", assetPath);
            outcome.result = core::json::object({{"uuid", id.to_string()}, {"path", assetPath}});
            outcome.inverse = command_inverse{"asset.delete", core::json::object({{"asset", id.to_string()}})};
            // The replay carries the .meta the import wrote, so a redo keeps the uuid.
            CONJURE_TRY_ASSIGN(command_inverse replayInverse, restore_inverse_for(*database, id));
            command_request replay = {};
            replay.name = replayInverse.name;
            replay.args = std::move(replayInverse.args);
            outcome.replay = std::move(replay);
            return outcome;
        }
    }

    void register_asset_commands(command_registry &registry) {
        add_command(
          registry,
          make_command(
            "asset.import",
            "Imports a source file (copied under assets:// when outside) and cooks it; options merge into the .meta "
            "settings. Undo removes a copied file; importing a file already inside the project records nothing.",
            R"({"type":"object","properties":{"source":{"type":"string"},"destination":{"type":"string"},)"
            R"("options":{"type":"object","additionalProperties":true}},"required":["source"]})",
            ANY_OBJECT_SCHEMA.data(),
            command_flags::MUTATES | command_flags::REQUIRES_PROJECT,
            &asset_import
          )
        );
        add_command(
          registry,
          make_command(
            "asset.list",
            "Every asset record, optionally filtered by path substring and type name.",
            R"({"type":"object","properties":{"filter":{"type":"string"},"type":{"type":"string"},)"
            R"("subAssets":{"type":"boolean"}}})",
            R"({"type":"object","properties":{"assets":{"type":"array"},"count":{"type":"integer"}}})",
            command_flags::REQUIRES_PROJECT,
            &asset_list
          )
        );
        add_command(
          registry,
          make_command(
            "asset.info",
            "One asset's record, dependencies, sub-assets and importer settings.",
            R"({"type":"object","properties":{"asset":{"type":"string"}},"required":["asset"]})",
            ANY_OBJECT_SCHEMA.data(),
            command_flags::REQUIRES_PROJECT,
            &asset_info
          )
        );
        add_command(
          registry,
          make_command(
            "asset.create_folder",
            "Creates a directory under assets://.",
            R"({"type":"object","properties":{"path":{"type":"string"}},"required":["path"]})",
            R"({"type":"object","properties":{"path":{"type":"string"},"revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_PROJECT,
            &asset_create_folder
          )
        );
        add_command(
          registry,
          make_command(
            "asset.delete",
            "Deletes an asset (source, .meta and cooked blobs) or an empty folder; undo restores it with the same "
            "uuid.",
            R"({"type":"object","properties":{"asset":{"type":"string"}},"required":["asset"]})",
            ANY_OBJECT_SCHEMA.data(),
            command_flags::MUTATES | command_flags::REQUIRES_PROJECT,
            &asset_delete
          )
        );
        add_command(
          registry,
          make_command(
            "asset.reimport",
            "Cooks an asset again and reloads it.",
            R"({"type":"object","properties":{"asset":{"type":"string"}},"required":["asset"]})",
            ANY_OBJECT_SCHEMA.data(),
            command_flags::REQUIRES_PROJECT,
            &asset_reimport
          )
        );
        add_command(
          registry,
          make_command(
            "asset.create_material",
            "Writes a .cmat material with the given factors and texture references and imports it.",
            R"({"type":"object","properties":{"path":{"type":"string"},)"
            R"("baseColor":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":4},)"
            R"("metallic":{"type":"number","minimum":0,"maximum":1},"roughness":{"type":"number","minimum":0,)"
            R"("maximum":1},)"
            R"("emissive":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3},)"
            R"("textures":{"type":"object","properties":{"albedo":{"type":"string"},"normal":{"type":"string"},)"
            R"("metallicRoughness":{"type":"string"},"occlusion":{"type":"string"},"emissive":{"type":"string"}}}},)"
            R"("required":["path"]})",
            R"({"type":"object","properties":{"uuid":{"type":"string"},"path":{"type":"string"},)"
            R"("revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_PROJECT,
            &asset_create_material
          )
        );
        add_command(
          registry,
          make_command(
            "asset.set",
            "Sets a member of a JSON asset (.cmat, .scene.json, .prefab.json) by dotted path and reimports it.",
            R"({"type":"object","properties":{"asset":{"type":"string"},"path":{"type":"string"},"value":{}},)"
            R"("required":["asset","path","value"]})",
            ANY_OBJECT_SCHEMA.data(),
            command_flags::MUTATES | command_flags::REQUIRES_PROJECT,
            &asset_set
          )
        );
        add_command(
          registry,
          make_command(
            "asset.restore",
            "Writes a source (and its .meta) back from base64 and imports it; the inverse of asset.delete.",
            R"({"type":"object","properties":{"path":{"type":"string"},"contentBase64":{"type":"string"},)"
            R"("metaBase64":{"type":"string"}},"required":["path","contentBase64"]})",
            R"({"type":"object","properties":{"uuid":{"type":"string"},"path":{"type":"string"},)"
            R"("revision":{"type":"integer"}}})",
            command_flags::MUTATES | command_flags::REQUIRES_PROJECT,
            &asset_restore
          )
        );
    }
}
