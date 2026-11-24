#include "conjure/modules/module_manifest.h"

#include "conjure/modules/module_error.h"
#include "modules/log_category.h"
#include "modules/long_path.h"
#include "modules/manifest_paths.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/log.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

namespace {

    constexpr std::array<std::string_view, 14> KNOWN_KEYS = {
        "schema", "name", "displayName", "description", "version", "kind", "engine", "platforms",
        "requires", "binary", "dependencies", "managed", "content", "editorOnly",
    };

    constexpr std::array<std::string_view, 3> KNOWN_DEPENDENCY_KEYS = {"name", "version", "optional"};

    /*=======================================================================*/
    /*=======================================================================*/
    // Error helpers: every message carries the source and the JSON pointer of the member.

    error invalid(std::string_view source, std::string_view pointer, std::string_view reason) {
        return core::make_error(module_error::MANIFEST_INVALID, "{}: {}: {}", source, pointer, reason);
    }

    error invalid_type(
      std::string_view source,
      std::string_view pointer,
      std::string_view expected,
      json::value found
    ) {
        if (!found.is_valid()) {
            return invalid(source, pointer, std::format("required member missing (expected {})", expected));
        }
        return invalid(source, pointer, std::format("expected {}, found {}", expected, json::to_string(found.type())));
    }

    /*=======================================================================*/
    /*=======================================================================*/
    // Member readers

    result<std::string> read_string(json::value object, std::string_view key, std::string_view source, bool required) {
        const json::value member = object[key];
        if (!member.is_valid()) {
            if (required) {
                return invalid_type(source, std::format("/{}", key), "string", member);
            }
            return std::string();
        }
        if (!member.is_string()) {
            return invalid_type(source, std::format("/{}", key), "string", member);
        }
        return std::string(member.as_string());
    }

    result<std::vector<std::string>> read_relative_paths(
      json::value object,
      std::string_view key,
      std::string_view source
    ) {
        const json::value member = object[key];
        if (!member.is_valid()) {
            return std::vector<std::string>();
        }
        if (!member.is_array()) {
            return invalid_type(source, std::format("/{}", key), "array of strings", member);
        }

        std::vector<std::string> items = {};
        items.reserve(member.size());
        usize index = 0;
        for (const json::value item : member) {
            if (!item.is_string()) {
                return invalid_type(source, std::format("/{}/{}", key, index), "string", item);
            }
            if (!modules::detail::stays_inside_root(item.as_string())) {
                return invalid(
                  source,
                  std::format("/{}/{}", key, index),
                  std::format("'{}' must be a relative path inside the module root", item.as_string())
                );
            }
            items.emplace_back(item.as_string());
            ++index;
        }
        return items;
    }

    result<version_range> read_range(json::value member, std::string_view pointer, std::string_view source) {
        if (!member.is_string()) {
            return invalid_type(source, pointer, "version range string", member);
        }
        result<version_range> range = parse_version_range(member.as_string());
        if (!range.has_value()) {
            return invalid(source, pointer, range.get_error().message());
        }
        return range;
    }

    // A typo such as "versoin" would otherwise silently become the default; 'pointer' is empty for
    // the root object.
    void warn_unknown_keys(
      json::value object,
      std::span<const std::string_view> knownKeys,
      std::string_view source,
      std::string_view pointer
    ) {
        for (const json::member entry : object.members()) {
            const bool known = std::find(knownKeys.begin(), knownKeys.end(), entry.key) != knownKeys.end();
            if (known) {
                continue;
            }
            if (pointer.empty()) {
                CONJURE_LOG_WARN(LOG_MODULES, "{}: unknown manifest key '{}' ignored", source, entry.key);
                continue;
            }
            CONJURE_LOG_WARN(LOG_MODULES, "{}: unknown manifest key '{}' at {} ignored", source, entry.key, pointer);
        }
    }

    result<module_dependency> read_dependency(json::value item, usize index, std::string_view source) {
        const std::string pointer = std::format("/dependencies/{}", index);
        if (!item.is_object()) {
            return invalid_type(source, pointer, "object", item);
        }

        module_dependency dependency = {};
        const json::value name = item["name"];
        if (!name.is_string()) {
            return invalid_type(source, pointer + "/name", "string", name);
        }
        dependency.name = std::string(name.as_string());
        if (!is_valid_module_name(dependency.name)) {
            return invalid(source, pointer + "/name", std::format("'{}' is not a valid module name", dependency.name));
        }

        if (item.contains("version")) {
            CONJURE_TRY_ASSIGN(dependency.version, read_range(item["version"], pointer + "/version", source));
        }

        const json::value optional = item["optional"];
        if (optional.is_valid()) {
            if (!optional.is_bool()) {
                return invalid_type(source, pointer + "/optional", "bool", optional);
            }
            dependency.optional = optional.as_bool();
        }

        warn_unknown_keys(item, KNOWN_DEPENDENCY_KEYS, source, pointer);
        return dependency;
    }

    json::document string_array(const std::vector<std::string> &items) {
        json::document array = json::document::array();
        for (const std::string &item : items) {
            array.push_back(std::string_view(item));
        }
        return array;
    }

    /*=======================================================================*/
    /*=======================================================================*/
    // Root object

    // Everything after the JSON parse. load_module_manifest() hands it the document it read, so a
    // file is parsed once and never re-serialised.
    result<module_manifest> parse_manifest_root(json::value root, std::string_view sourceName) {
        if (!root.is_object()) {
            return invalid_type(sourceName, "", "object", root);
        }

        module_manifest manifest = {};

        // Required members first: schema, name, version, engine.
        const json::value schema = root["schema"];
        if (!schema.is_integer()) {
            return invalid_type(sourceName, "/schema", "integer", schema);
        }
        if (schema.as_u64() != MODULE_MANIFEST_SCHEMA_VERSION) {
            return invalid(
              sourceName,
              "/schema",
              std::format(
                "unsupported schema {} (this engine reads schema {})",
                schema.as_i64(),
                MODULE_MANIFEST_SCHEMA_VERSION
              )
            );
        }
        manifest.schemaVersion = static_cast<u32>(schema.as_u64());

        CONJURE_TRY_ASSIGN(manifest.name, read_string(root, "name", sourceName, true));
        if (!is_valid_module_name(manifest.name)) {
            return core::make_error(
              module_error::INVALID_NAME,
              "{}: /name: '{}' is not a valid module name ([a-z0-9_.], 1 to {} characters)",
              sourceName,
              manifest.name,
              MAX_MODULE_NAME_LENGTH
            );
        }

        const json::value version = root["version"];
        if (!version.is_string()) {
            return invalid_type(sourceName, "/version", "version string", version);
        }
        result<semantic_version> parsedVersion = parse_version(version.as_string());
        if (!parsedVersion.has_value()) {
            return invalid(sourceName, "/version", parsedVersion.get_error().message());
        }
        manifest.version = parsedVersion.value();

        const json::value engine = root["engine"];
        if (!engine.is_valid()) {
            return invalid_type(sourceName, "/engine", "version range string", engine);
        }
        CONJURE_TRY_ASSIGN(manifest.engineVersion, read_range(engine, "/engine", sourceName));

        // Optional members.
        CONJURE_TRY_ASSIGN(manifest.displayName, read_string(root, "displayName", sourceName, false));
        CONJURE_TRY_ASSIGN(manifest.description, read_string(root, "description", sourceName, false));
        CONJURE_TRY_ASSIGN(manifest.binary, read_string(root, "binary", sourceName, false));
        if (!manifest.binary.empty() && !modules::detail::is_base_name(manifest.binary)) {
            const std::string reason =
              std::format("'{}' must be a file name without a directory part", manifest.binary);
            return invalid(sourceName, "/binary", reason);
        }

        const json::value kind = root["kind"];
        if (kind.is_valid()) {
            if (!kind.is_string()) {
                return invalid_type(sourceName, "/kind", "string", kind);
            }
            const std::optional<module_kind> parsedKind = parse_module_kind(kind.as_string());
            if (!parsedKind.has_value()) {
                const std::string reason = std::format("unknown kind '{}' (engine, plugin, game)", kind.as_string());
                return invalid(sourceName, "/kind", reason);
            }
            manifest.kind = *parsedKind;
        }

        const json::value platforms = root["platforms"];
        if (platforms.is_valid()) {
            if (!platforms.is_array()) {
                return invalid_type(sourceName, "/platforms", "array of strings", platforms);
            }
            manifest.platforms = module_platform_flags::NONE;
            usize index = 0;
            for (const json::value item : platforms) {
                const std::string pointer = std::format("/platforms/{}", index);
                if (!item.is_string()) {
                    return invalid_type(sourceName, pointer, "string", item);
                }
                const std::optional<module_platform> platform = parse_module_platform(item.as_string());
                if (!platform.has_value()) {
                    return invalid(sourceName, pointer, std::format("unknown platform '{}'", item.as_string()));
                }
                manifest.platforms |= to_flag(*platform);
                ++index;
            }
        }

        const json::value requiredFeatures = root["requires"];
        if (requiredFeatures.is_valid()) {
            if (!requiredFeatures.is_array()) {
                return invalid_type(sourceName, "/requires", "array of strings", requiredFeatures);
            }
            usize index = 0;
            for (const json::value item : requiredFeatures) {
                const std::string pointer = std::format("/requires/{}", index);
                if (!item.is_string()) {
                    return invalid_type(sourceName, pointer, "string", item);
                }
                const std::optional<module_feature_flags> feature = parse_module_feature(item.as_string());
                if (!feature.has_value()) {
                    return invalid(sourceName, pointer, std::format("unknown feature '{}'", item.as_string()));
                }
                manifest.requiredFeatures |= *feature;
                ++index;
            }
        }

        const json::value dependencies = root["dependencies"];
        if (dependencies.is_valid()) {
            if (!dependencies.is_array()) {
                return invalid_type(sourceName, "/dependencies", "array of objects", dependencies);
            }
            usize index = 0;
            for (const json::value item : dependencies) {
                CONJURE_TRY_ASSIGN(module_dependency dependency, read_dependency(item, index, sourceName));
                if (dependency.name == manifest.name) {
                    const std::string pointer = std::format("/dependencies/{}/name", index);
                    return invalid(sourceName, pointer, "a module cannot depend on itself");
                }
                const auto sameName = [&dependency](const module_dependency &existing) {
                    return existing.name == dependency.name;
                };
                if (std::any_of(manifest.dependencies.begin(), manifest.dependencies.end(), sameName)) {
                    return invalid(
                      sourceName,
                      std::format("/dependencies/{}/name", index),
                      std::format("duplicate dependency '{}'", dependency.name)
                    );
                }
                manifest.dependencies.push_back(std::move(dependency));
                ++index;
            }
        }

        CONJURE_TRY_ASSIGN(manifest.managedAssemblies, read_relative_paths(root, "managed", sourceName));
        CONJURE_TRY_ASSIGN(manifest.contentDirectories, read_relative_paths(root, "content", sourceName));

        const json::value editorOnly = root["editorOnly"];
        if (editorOnly.is_valid()) {
            if (!editorOnly.is_bool()) {
                return invalid_type(sourceName, "/editorOnly", "bool", editorOnly);
            }
            manifest.editorOnly = editorOnly.as_bool();
        }

        warn_unknown_keys(root, KNOWN_KEYS, sourceName, "");
        return manifest;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Validation

bool conjure::modules::is_valid_module_name(std::string_view name) noexcept {
    if (name.empty() || name.size() > MAX_MODULE_NAME_LENGTH) {
        return false;
    }
    for (const char character : name) {
        const bool lower = character >= 'a' && character <= 'z';
        const bool digit = character >= '0' && character <= '9';
        if (!lower && !digit && character != '_' && character != '.') {
            return false;
        }
    }
    return true;
}


/*===========================================================================*/
/*===========================================================================*/
// Parsing

result<module_manifest> conjure::modules::parse_module_manifest(
  std::string_view jsonText,
  std::string_view sourceName
) {
    result<json::document> parsed = json::document::parse(jsonText);
    if (!parsed.has_value()) {
        return invalid(sourceName, "", parsed.get_error().message());
    }
    return parse_manifest_root(parsed->root(), sourceName);
}

result<module_manifest> conjure::modules::load_module_manifest(const std::filesystem::path &path) {
    // A manifest in a deep project folder is read through the long form (long_path.h); messages name
    // the path as it was given.
    const std::filesystem::path readablePath = modules::detail::extend_path(path);
    std::error_code ignored = {};
    if (!std::filesystem::is_regular_file(readablePath, ignored)) {
        return core::make_error(
          module_error::MANIFEST_NOT_FOUND,
          "manifest '{}' does not exist",
          path.generic_string()
        );
    }

    result<json::document> parsed = json::document::parse_file(readablePath);
    if (!parsed.has_value()) {
        return invalid(path.generic_string(), "", parsed.get_error().message());
    }
    return parse_manifest_root(parsed->root(), path.generic_string());
}


/*===========================================================================*/
/*===========================================================================*/
// Writing

json::document conjure::modules::write_module_manifest(const module_manifest &manifest, bool includeEditorOnly) {
    json::document document = json::document::object();
    document.set("schema", manifest.schemaVersion);
    document.set("name", std::string_view(manifest.name));
    if (!manifest.displayName.empty()) {
        document.set("displayName", std::string_view(manifest.displayName));
    }
    if (!manifest.description.empty()) {
        document.set("description", std::string_view(manifest.description));
    }
    document.set("version", std::string_view(to_string(manifest.version)));
    document.set("kind", to_string(manifest.kind));
    document.set("engine", std::string_view(to_string(manifest.engineVersion)));

    json::document platforms = json::document::array();
    for (const module_platform platform : {module_platform::WINDOWS, module_platform::LINUX, module_platform::MACOS}) {
        if (has_flag(manifest.platforms, to_flag(platform))) {
            platforms.push_back(to_string(platform));
        }
    }
    document.set("platforms", platforms);

    json::document requiredFeatures = json::document::array();
    for (const std::string_view feature : {"renderer", "physics", "audio", "scripting", "editor"}) {
        if (has_flag(manifest.requiredFeatures, *parse_module_feature(feature))) {
            requiredFeatures.push_back(feature);
        }
    }
    document.set("requires", requiredFeatures);

    if (!manifest.binary.empty()) {
        document.set("binary", std::string_view(manifest.binary));
    }

    json::document dependencies = json::document::array();
    for (const module_dependency &dependency : manifest.dependencies) {
        json::document entry = json::document::object();
        entry.set("name", std::string_view(dependency.name));
        entry.set("version", std::string_view(to_string(dependency.version)));
        entry.set("optional", dependency.optional);
        dependencies.push_back(entry);
    }
    document.set("dependencies", dependencies);
    document.set("managed", string_array(manifest.managedAssemblies));
    document.set("content", string_array(manifest.contentDirectories));

    if (includeEditorOnly) {
        document.set("editorOnly", manifest.editorOnly);
    }
    return document;
}


/*===========================================================================*/
/*===========================================================================*/
// Binary location

std::string conjure::modules::decorate_binary_name(std::string_view baseName, module_platform platform) {
    if (baseName.empty()) {
        return {};
    }
    switch (platform) {
        case module_platform::WINDOWS:
            return std::format("{}.dll", baseName);
        case module_platform::LINUX:
            return std::format("lib{}.so", baseName);
        case module_platform::MACOS:
            return std::format("lib{}.dylib", baseName);
    }
    return std::string(baseName);
}

std::filesystem::path conjure::modules::module_binary_directory(
  const std::filesystem::path &rootDirectory,
  const binary_triplet &triplet
) {
    return rootDirectory / "bin" / to_string(triplet);
}

std::filesystem::path conjure::modules::module_binary_path(
  const module_manifest &manifest,
  const std::filesystem::path &rootDirectory,
  const binary_triplet &triplet
) {
    if (manifest.binary.empty()) {
        return {};
    }
    return module_binary_directory(rootDirectory, triplet) / decorate_binary_name(manifest.binary, triplet.platform);
}
