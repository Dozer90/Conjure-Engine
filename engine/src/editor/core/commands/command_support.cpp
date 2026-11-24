#include "editor/core/commands/command_support.h"

#include "conjure/editor/entity_ref.h"
#include "conjure/editor/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/vfs/path_utils.h"
#include "conjure/engine/project.h"
#include "conjure/scene/components.h"
#include "conjure/scene/hierarchy.h"
#include "conjure/scene/scene_serializer.h"

#include <array>
#include <cmath>
#include <format>
#include <initializer_list>
#include <limits>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace conjure::editor::support {

    /*===========================================================================*/
    /*===========================================================================*/
    // Errors

    core::error invalid_args(std::string message) {
        return core::make_error(protocol_error_code::INVALID_ARGS, std::move(message));
    }

    core::error not_available(std::string message) {
        return core::make_error(protocol_error_code::NOT_AVAILABLE, std::move(message));
    }

    core::error io_error(std::string message) {
        return core::make_error(protocol_error_code::IO_ERROR, std::move(message));
    }

    core::error path_not_found(std::string message) {
        return core::make_error(protocol_error_code::PATH_NOT_FOUND, std::move(message));
    }

    core::error internal(std::string message) {
        return core::make_error(protocol_error_code::INTERNAL, std::move(message));
    }

    core::error unusable_path(const std::filesystem::path &path, std::string_view reason) {
        std::error_code error = {};
        const std::filesystem::file_status status = std::filesystem::status(path, error);
        if (status.type() == std::filesystem::file_type::not_found) {
            return path_not_found(std::format("cannot read '{}': no such file or folder", path.generic_string()));
        }
        // file_type::none: status() itself failed for a reason other than "not found", and that reason says more
        // than the caller's.
        if (status.type() == std::filesystem::file_type::none && error) {
            return io_error(std::format("cannot read '{}': {}", path.generic_string(), error.message()));
        }
        if (reason.empty()) {
            return io_error(std::format("cannot read '{}'", path.generic_string()));
        }
        return io_error(std::format("cannot read '{}': {}", path.generic_string(), reason));
    }

    core::error read_failure(std::string_view path, const core::error &failure) {
        std::string message = std::format("cannot read '{}': {}", path, failure.message());
        if (failure.generic_code() == core::error_code::NOT_FOUND) {
            return path_not_found(std::move(message));
        }
        return io_error(std::move(message));
    }

    std::filesystem::path project_relative_path(const editor_context &context, std::filesystem::path path) {
        const engine::project *project = context.project();
        if (path.empty() || path.is_absolute() || project == nullptr) {
            return path;
        }
        return project->root_directory() / path;
    }

    std::filesystem::path project_relative_path(const editor_context &context, std::string_view text) {
        return project_relative_path(context, std::filesystem::path(std::string(text)));
    }

    core::error wrap(const core::error &failure) {
        if (failure.is_category<protocol_error_code>()) {
            return failure;
        }
        return core::make_error(classify_error(failure), failure.to_string());
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // Arguments

    core::result<std::string_view> require_string(const core::json::value &args, std::string_view key) {
        const core::json::value value = args[key];
        if (!value.is_string()) {
            return invalid_args(std::format("args.{}: expected a string", key));
        }
        return value.as_string();
    }

    std::string_view string_or(const core::json::value &args, std::string_view key, std::string_view fallback) {
        const core::json::value value = args[key];
        return value.is_string() ? value.as_string() : fallback;
    }

    bool bool_or(const core::json::value &args, std::string_view key, bool fallback) {
        const core::json::value value = args[key];
        return value.is_bool() ? value.as_bool() : fallback;
    }

    core::result<u32> u32_or(const core::json::value &args, std::string_view key, u32 fallback) {
        const core::json::value value = args[key];
        if (!value.is_valid()) {
            return fallback;
        }
        // A whole real such as 2.0 counts, as the schema's "integer" does; as_i64 answers -1 for 2.5 and for
        // anything outside i64.
        if (!value.is_number() || value.as_i64(-1) < 0 || value.as_u64() > 0xFFFFFFFFull) {
            return invalid_args(std::format("args.{}: expected a non-negative integer", key));
        }
        return static_cast<u32>(value.as_u64());
    }

    core::result<f32> f32_or(const core::json::value &args, std::string_view key, f32 fallback) {
        const core::json::value value = args[key];
        if (!value.is_valid()) {
            return fallback;
        }
        // as_f32 answers its fallback for a non-number and for a number outside the f32 range, so a NaN fallback
        // turns both into a refusal instead of a silent 0.
        const f32 number = value.as_f32(std::numeric_limits<f32>::quiet_NaN());
        if (!std::isfinite(number)) {
            return invalid_args(std::format("args.{}: expected a finite number within the f32 range", key));
        }
        return number;
    }

    core::result<math::vec3> read_vec3(const core::json::value &value, std::string_view name) {
        if (!value.is_array() || value.size() != 3) {
            return invalid_args(std::format("{}: expected [x, y, z]", name));
        }
        math::vec3 result = math::vec3(0.0f);
        for (usize index = 0; index < 3; ++index) {
            const f32 component = value[index].as_f32(std::numeric_limits<f32>::quiet_NaN());
            if (!std::isfinite(component)) {
                return invalid_args(std::format("{}: expected finite numbers within the f32 range", name));
            }
            result[static_cast<glm::length_t>(index)] = component;
        }
        return result;
    }

    core::result<math::quat> read_rotation(const core::json::value &value, std::string_view name) {
        if (!value.is_array() || (value.size() != 3 && value.size() != 4)) {
            return invalid_args(std::format("{}: expected [x, y, z, w] or euler degrees [x, y, z]", name));
        }
        std::array<f32, 4> components = {0.0f, 0.0f, 0.0f, 1.0f};
        for (usize index = 0; index < value.size(); ++index) {
            const f32 component = value[index].as_f32(std::numeric_limits<f32>::quiet_NaN());
            if (!std::isfinite(component)) {
                return invalid_args(std::format("{}: expected finite numbers within the f32 range", name));
            }
            components[index] = component;
        }
        if (value.size() == 3) {
            const math::vec3 radians = math::vec3(
              math::radians(components[0]),
              math::radians(components[1]),
              math::radians(components[2])
            );
            return glm::normalize(math::quat(radians));
        }
        math::quat quaternion = math::quat(components[3], components[0], components[1], components[2]);
        const f32 length = glm::length(quaternion);
        if (length <= 1.0e-6f) {
            return invalid_args(std::format("{}: a zero quaternion is not a rotation", name));
        }
        return quaternion / length;
    }

    core::result<math::transform> read_transform(
      const core::json::value &value,
      std::string_view name,
      const math::transform &base
    ) {
        if (!value.is_object()) {
            return invalid_args(std::format("{}: expected {{position, rotation, scale}}", name));
        }
        math::transform result = base;
        if (value.contains("position")) {
            CONJURE_TRY_ASSIGN(result.position, read_vec3(value["position"], std::format("{}.position", name)));
        }
        if (value.contains("rotation")) {
            CONJURE_TRY_ASSIGN(result.rotation, read_rotation(value["rotation"], std::format("{}.rotation", name)));
        }
        if (value.contains("scale")) {
            CONJURE_TRY_ASSIGN(result.scale, read_vec3(value["scale"], std::format("{}.scale", name)));
        }
        return result;
    }

    core::json::document vec3_json(const math::vec3 &value) {
        return core::json::array({value.x, value.y, value.z});
    }

    core::json::document quat_json(const math::quat &value) {
        return core::json::array({value.x, value.y, value.z, value.w});
    }

    core::json::document transform_json(const math::transform &value) {
        return core::json::object({
          {"position", vec3_json(value.position)},
          {"rotation", quat_json(value.rotation)},
          {"scale", vec3_json(value.scale)},
        });
    }

    core::json::document aabb_json(const math::aabb &value) {
        return core::json::object({{"min", vec3_json(value.min)}, {"max", vec3_json(value.max)}});
    }

    core::json::document string_array(std::span<const std::string> values) {
        core::json::document array = core::json::document::array();
        for (const std::string &value : values) {
            array.push_back(std::string_view(value));
        }
        return array;
    }

    core::json::document guid_array(std::span<const core::uuid> values) {
        core::json::document array = core::json::document::array();
        for (const core::uuid &value : values) {
            array.push_back(value.to_string());
        }
        return array;
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // Entities

    core::result<void> check_entity_name(std::string_view name, std::string_view argument) {
        if (name.empty()) {
            return invalid_args(std::format("{}: an entity name must not be empty", argument));
        }
        if (name.find('/') != std::string_view::npos) {
            return invalid_args(std::format(
              "{}: an entity name must not contain '/' (hierarchy paths use it as the separator)",
              argument
            ));
        }
        return core::result<void>::success();
    }

    core::result<ecs::entity> resolve_entity(command_invocation &invocation, const core::json::value &reference) {
        return entity_ref::resolve(reference, invocation.context.scene(), &invocation.errorDetails);
    }

    core::result<std::string> guid_string(scene::scene &scene, ecs::entity entity) {
        CONJURE_TRY_ASSIGN(const core::uuid guid, entity_ref::guid_of(scene, entity));
        return guid.to_string();
    }

    std::string guid_or_empty(const scene::scene &scene, ecs::entity entity) {
        if (entity.is_null() || !scene.contains(entity)) {
            return "";
        }
        const std::optional<core::uuid> guid = scene.guid_of(entity);
        return guid.has_value() ? guid->to_string() : std::string("");
    }

    core::json::document entity_summary(scene::scene &scene, ecs::entity entity) {
        core::json::document summary = core::json::document::object();
        summary.set("guid", guid_or_empty(scene, entity));
        summary.set("name", scene.name_of(entity).to_string());
        summary.set("path", entity_ref::path_of(scene, entity));
        return summary;
    }

    core::result<core::json::document> capture_subtree(const scene::scene &scene, ecs::entity root) {
        core::json::writer out = {};
        const core::result<void> saved = scene::scene_serializer::save_subtree(scene, root, out);
        if (!saved.has_value()) {
            return wrap(saved.get_error());
        }
        core::result<core::json::document> parsed = core::json::document::parse(out.finish());
        if (!parsed.has_value()) {
            return internal(std::format("subtree capture produced invalid JSON: {}", parsed.get_error().message()));
        }
        return parsed;
    }

    core::result<ecs::entity> restore_subtree(
      scene::scene &scene,
      const core::json::value &archive,
      ecs::entity parent,
      u32 index
    ) {
        if (!archive.is_object()) {
            return invalid_args("args.archive: expected a prefab-format object");
        }
        const core::result<ecs::entity> restored = scene::scene_serializer::instantiate_subtree(
          scene,
          archive,
          parent,
          true,
          index
        );
        if (!restored.has_value()) {
            return wrap(restored.get_error());
        }
        return restored.value();
    }

    std::vector<ecs::entity> outermost(const scene::scene &scene, std::span<const ecs::entity> entities) {
        // Undo of a bulk create sends every created entity through here: look ancestors up in a set while
        // walking upwards (bounded by the hierarchy depth) instead of testing every pair.
        const std::unordered_set<ecs::entity> requested(entities.begin(), entities.end());
        std::unordered_set<ecs::entity> seen = {};
        seen.reserve(entities.size());
        std::vector<ecs::entity> result = {};
        for (const ecs::entity entity : entities) {
            if (!seen.insert(entity).second) {
                continue;
            }
            bool nested = false;
            ecs::entity ancestor = scene.tree().parent_of(entity);
            for (u32 depth = 0; !ancestor.is_null() && depth < scene::MAX_HIERARCHY_DEPTH; ++depth) {
                if (requested.contains(ancestor)) {
                    nested = true;
                    break;
                }
                ancestor = scene.tree().parent_of(ancestor);
            }
            if (!nested) {
                result.push_back(entity);
            }
        }
        return result;
    }

    u32 tree_position(const scene::scene &scene, ecs::entity entity) noexcept {
        if (entity.is_null() || !scene.contains(entity)) {
            return scene::APPEND_CHILD;
        }
        if (!scene.tree().parent_of(entity).is_null()) {
            return scene.tree().sibling_index(entity);
        }
        const std::span<const scene::root_entry> roots = scene.roots();
        for (usize index = 0; index < roots.size(); ++index) {
            if (roots[index].entity == entity) {
                return static_cast<u32>(index);
            }
        }
        return scene::APPEND_CHILD;
    }

    core::result<command_inverse> restore_inverse(scene::scene &scene, ecs::entity root) {
        return restore_inverse(scene, root, tree_position(scene, root));
    }

    core::result<command_inverse> restore_inverse(scene::scene &scene, ecs::entity root, u32 position) {
        CONJURE_TRY_ASSIGN(core::json::document archive, capture_subtree(scene, root));
        const ecs::entity parent = scene.tree().parent_of(root);
        command_inverse inverse = {};
        inverse.name = "entity.restore";
        inverse.args = core::json::document::object();
        inverse.args.set("archive", archive);
        if (!parent.is_null()) {
            inverse.args.set("parent", guid_or_empty(scene, parent));
        }
        inverse.args.set("index", position);
        return inverse;
    }

    subtree_box subtree_bounds_or_boxes(const scene::scene &scene, ecs::entity root) {
        subtree_box result = {};
        const std::optional<math::aabb> rendered = scene.subtree_bounds(root);
        if (rendered.has_value() && rendered->is_valid()) {
            result.bounds = *rendered;
            result.fromRenderBounds = true;
            return result;
        }
        scene.tree().for_each_in_subtree(root, [&scene, &result](ecs::entity member) {
            const math::vec3 position = scene.world_transform_of(member).value_or(math::transform()).position;
            result.bounds.expand(math::aabb::from_center_extents(position, math::vec3(0.5f)));
        });
        return result;
    }

    core::json::document batch_args(std::span<const command_inverse> children, std::string_view label) {
        core::json::document commands = core::json::document::array();
        for (const command_inverse &child : children) {
            core::json::document entry = core::json::document::object();
            entry.set("cmd", std::string_view(child.name));
            entry.set("args", child.args);
            commands.push_back(entry);
        }
        core::json::document args = core::json::document::object();
        args.set("commands", commands);
        args.set("label", label);
        args.set("atomic", true);
        return args;
    }

    core::result<ecs::entity> apply_restore_inverse(scene::scene &scene, const command_inverse &inverse) {
        const core::json::value args = inverse.args.root();
        ecs::entity parent = ecs::NULL_ENTITY;
        const core::json::value parentValue = args["parent"];
        if (parentValue.is_string()) {
            const std::optional<core::uuid> parentGuid = core::uuid::parse(parentValue.as_string());
            if (!parentGuid.has_value()) {
                return invalid_args("archive parent: not a guid");
            }
            parent = scene.find_guid(*parentGuid);
            if (parent.is_null()) {
                return core::make_error(protocol_error_code::ENTITY_NOT_FOUND, "archive parent: no such entity");
            }
        }
        const core::json::value indexValue = args["index"];
        const u32 index = indexValue.is_integer() ? static_cast<u32>(indexValue.as_u64()) : scene::APPEND_CHILD;
        return restore_subtree(scene, args["archive"], parent, index);
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // Assets

    core::result<std::string> contained_relative_path(std::string_view text, const assets::asset_path_parts &parts) {
        const std::string relative = core::vfs::path_utils::normalize(parts.relativePath);
        const bool leavesMount = relative == ".." || core::str::starts_with(relative, "../") ||
          core::vfs::path_utils::is_absolute(relative);
        if (leavesMount) {
            return invalid_args(std::format("'{}' leaves the {}:// root", text, parts.mount));
        }
        if (relative.empty()) {
            return invalid_args(std::format("'{}' names no path inside the {}:// root", text, parts.mount));
        }
        return relative;
    }

    core::result<std::string> file_path_for(assets::asset_database &database, std::string_view assetPath) {
        const std::optional<assets::asset_path_parts> parts = assets::split_asset_path(assetPath);
        if (!parts.has_value()) {
            return invalid_args(std::format("'{}' is not an asset path (mount://relative)", assetPath));
        }
        CONJURE_TRY_ASSIGN(const std::string relative, contained_relative_path(assetPath, *parts));
        for (const assets::asset_mount &mount : database.mounts()) {
            if (mount.name == parts->mount) {
                if (mount.rootPath.empty()) {
                    return relative;
                }
                return mount.rootPath + "/" + relative;
            }
        }
        return core::make_error(
          protocol_error_code::ASSET_NOT_FOUND,
          std::format("unknown asset mount '{}'", parts->mount)
        );
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

    /*===========================================================================*/
    /*===========================================================================*/
    // Encodings and tool output

    namespace {
        constexpr std::string_view BASE64_CHARACTERS =
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        constexpr std::string_view FATAL_ERROR_MARKER = ": fatal error";
        constexpr std::string_view ERROR_MARKER = ": error";
        constexpr std::string_view WARNING_MARKER = ": warning";

        [[nodiscard]] bool all_digits(std::string_view text) noexcept {
            if (text.empty()) {
                return false;
            }
            for (const char character : text) {
                if (!core::str::is_digit(character)) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] u32 parse_number(std::string_view text) noexcept {
            u32 value = 0;
            for (const char character : text) {
                value = value * 10 + static_cast<u32>(character - '0');
            }
            return value;
        }

        /// <summary>
        /// Splits "file(12,3)" (MSVC) or "file:12:3" / "file:12" (GCC/Clang) into its parts; a prefix in
        /// neither form is the file alone.
        /// </summary>
        void parse_location(std::string_view prefix, tool_diagnostic &out) {
            prefix = core::str::trim(prefix);
            if (!prefix.empty() && prefix.back() == ')') {
                const usize open = prefix.rfind('(');
                if (open != std::string_view::npos) {
                    const std::string_view inside = prefix.substr(open + 1, prefix.size() - open - 2);
                    const usize comma = inside.find(',');
                    const std::string_view lineText =
                      comma == std::string_view::npos ? inside : inside.substr(0, comma);
                    const std::string_view columnText = comma == std::string_view::npos ? "" : inside.substr(comma + 1);
                    if (all_digits(lineText) && (columnText.empty() || all_digits(columnText))) {
                        out.file = std::string(prefix.substr(0, open));
                        out.line = parse_number(lineText);
                        out.column = columnText.empty() ? 0 : parse_number(columnText);
                        return;
                    }
                }
            }
            // GCC/Clang: strip up to two trailing ":number" groups.
            std::string_view rest = prefix;
            std::array<u32, 2> numbers = {0, 0};
            u32 found = 0;
            while (found < 2) {
                const usize colon = rest.rfind(':');
                if (colon == std::string_view::npos || !all_digits(rest.substr(colon + 1))) {
                    break;
                }
                numbers[found] = parse_number(rest.substr(colon + 1));
                ++found;
                rest = rest.substr(0, colon);
            }
            out.file = std::string(rest);
            if (found == 1) {
                out.line = numbers[0];
            } else if (found == 2) {
                out.line = numbers[1];
                out.column = numbers[0];
            }
        }
    }

    std::string base64_encode(std::span<const std::byte> bytes) {
        std::string out = {};
        out.reserve((bytes.size() + 2) / 3 * 4);
        usize index = 0;
        while (index + 2 < bytes.size()) {
            const u32 triple = (static_cast<u32>(bytes[index]) << 16) | (static_cast<u32>(bytes[index + 1]) << 8) |
              static_cast<u32>(bytes[index + 2]);
            out.push_back(BASE64_CHARACTERS[(triple >> 18) & 63]);
            out.push_back(BASE64_CHARACTERS[(triple >> 12) & 63]);
            out.push_back(BASE64_CHARACTERS[(triple >> 6) & 63]);
            out.push_back(BASE64_CHARACTERS[triple & 63]);
            index += 3;
        }
        const usize rest = bytes.size() - index;
        if (rest == 1) {
            const u32 triple = static_cast<u32>(bytes[index]) << 16;
            out.push_back(BASE64_CHARACTERS[(triple >> 18) & 63]);
            out.push_back(BASE64_CHARACTERS[(triple >> 12) & 63]);
            out.append("==");
        } else if (rest == 2) {
            const u32 triple = (static_cast<u32>(bytes[index]) << 16) | (static_cast<u32>(bytes[index + 1]) << 8);
            out.push_back(BASE64_CHARACTERS[(triple >> 18) & 63]);
            out.push_back(BASE64_CHARACTERS[(triple >> 12) & 63]);
            out.push_back(BASE64_CHARACTERS[(triple >> 6) & 63]);
            out.push_back('=');
        }
        return out;
    }

    core::result<std::vector<std::byte>> base64_decode(std::string_view text) {
        std::vector<std::byte> out = {};
        out.reserve(text.size() / 4 * 3);
        u32 buffer = 0;
        u32 bits = 0;
        usize symbols = 0;
        u32 padding = 0;
        for (const char character : text) {
            if (core::str::is_space(character)) {
                continue;
            }
            ++symbols;
            if (character == '=') {
                ++padding;
                if (padding > 2) {
                    return invalid_args("base64: more than two padding characters");
                }
                continue;
            }
            if (padding > 0) {
                return invalid_args("base64: '=' is only allowed at the end");
            }
            const usize value = BASE64_CHARACTERS.find(character);
            if (value == std::string_view::npos) {
                return invalid_args("base64: invalid character");
            }
            buffer = (buffer << 6) | static_cast<u32>(value);
            bits += 6;
            if (bits >= 8) {
                bits -= 8;
                out.push_back(static_cast<std::byte>((buffer >> bits) & 0xFFu));
            }
        }
        if (symbols % 4 != 0) {
            return invalid_args("base64: the length is not a multiple of 4");
        }
        // A canonical encoder leaves the bits after the last whole byte zero; anything else is a corrupted or
        // truncated payload.
        if (bits >= 6 || (buffer & ((1u << bits) - 1u)) != 0) {
            return invalid_args("base64: non-zero bits after the last byte");
        }
        return out;
    }

    std::optional<tool_diagnostic> parse_tool_diagnostic(std::string_view line) {
        const std::string_view trimmed = core::str::trim(line);
        tool_diagnostic diagnostic = {};
        if (core::str::starts_with(trimmed, "CMake Error")) {
            diagnostic.isError = true;
            diagnostic.message = std::string(trimmed);
            return diagnostic;
        }
        if (core::str::starts_with(trimmed, "CMake Warning") || core::str::starts_with(trimmed, "CMake Deprecation")) {
            diagnostic.message = std::string(trimmed);
            return diagnostic;
        }
        // The earliest marker wins so a message mentioning the other word does not change the severity.
        usize position = std::string_view::npos;
        std::string_view marker = {};
        for (const std::string_view candidate : {FATAL_ERROR_MARKER, ERROR_MARKER, WARNING_MARKER}) {
            const usize found = trimmed.find(candidate);
            if (found != std::string_view::npos && (position == std::string_view::npos || found < position)) {
                position = found;
                marker = candidate;
            }
        }
        if (position == std::string_view::npos) {
            return std::nullopt;
        }
        // ": error" is also the head of ": error_handling"; a diagnostic continues with a space or a colon.
        std::string_view after = trimmed.substr(position + marker.size());
        if (!after.empty() && after.front() != ' ' && after.front() != ':') {
            return std::nullopt;
        }
        diagnostic.isError = marker != WARNING_MARKER;
        parse_location(trimmed.substr(0, position), diagnostic);
        after = core::str::trim(after);
        if (!after.empty() && after.front() != ':') {
            // "C2065: message" (MSVC, linker): a short alphanumeric code before the colon.
            const usize colon = after.find(':');
            const std::string_view code = colon == std::string_view::npos ? after : after.substr(0, colon);
            bool codeLike = !code.empty() && code.size() <= 16;
            for (const char character : code) {
                codeLike = codeLike && core::str::is_alnum(character);
            }
            if (codeLike) {
                diagnostic.code = std::string(code);
                after = colon == std::string_view::npos ? std::string_view() : after.substr(colon);
            }
        }
        if (!after.empty() && after.front() == ':') {
            after.remove_prefix(1);
        }
        diagnostic.message = std::string(core::str::trim(after));
        return diagnostic;
    }

    bool is_safe_tool_argument(std::string_view text) noexcept {
        if (text.empty() || text.front() == '-') {
            return false;
        }
        for (const char character : text) {
            const bool allowed = core::str::is_alnum(character) || character == '_' || character == '-' ||
              character == '.';
            if (!allowed) {
                return false;
            }
        }
        return true;
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // Registration

    command_descriptor make_command(
      std::string name,
      std::string summary,
      std::string argsSchema,
      std::string resultSchema,
      command_flags flags,
      command_handler handler
    ) {
        command_descriptor descriptor = {};
        descriptor.name = std::move(name);
        descriptor.summary = std::move(summary);
        descriptor.argsSchema = std::move(argsSchema);
        descriptor.resultSchema = std::move(resultSchema);
        descriptor.flags = flags;
        descriptor.owner = "editor";
        descriptor.handler = std::move(handler);
        return descriptor;
    }

    void add_command(command_registry &registry, command_descriptor descriptor) {
        const std::string name = descriptor.name;
        const core::result<void> added = registry.add(std::move(descriptor));
        if (!added.has_value()) {
            CONJURE_LOG_ERROR(
              LOG_EDITOR,
              "built-in command '{}' was not registered: {}",
              name,
              added.get_error().message()
            );
            CONJURE_ASSERT(false, "built-in command '{}' failed to register", name);
        }
    }
}
