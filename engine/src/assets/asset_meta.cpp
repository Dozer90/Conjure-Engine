#include "conjure/assets/asset_meta.h"
#include "assets/fnv1a.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/json.h"
#include "conjure/core/json/value.h"
#include "conjure/core/string_utils.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::assets;

namespace {
    constexpr std::string_view META_SUFFIX = ".meta";
    constexpr std::string_view KEY_SCHEMA = "schema";
    constexpr std::string_view KEY_UUID = "uuid";
    constexpr std::string_view KEY_IMPORTER = "importer";
    constexpr std::string_view KEY_IMPORTER_VERSION = "importerVersion";
    constexpr std::string_view KEY_SETTINGS = "settings";
    constexpr std::string_view KEY_SUB_ASSETS = "subAssets";

    bool is_known_key(std::string_view key) noexcept {
        return key == KEY_SCHEMA || key == KEY_UUID || key == KEY_IMPORTER || key == KEY_IMPORTER_VERSION ||
          key == KEY_SETTINGS || key == KEY_SUB_ASSETS;
    }

    core::error invalid(std::string message) {
        return make_asset_error(asset_error_code::META_INVALID, std::move(message));
    }

    std::vector<std::byte> to_bytes(std::string_view text) {
        std::vector<std::byte> bytes(text.size());
        for (usize index = 0; index < text.size(); ++index) {
            bytes[index] = static_cast<std::byte>(text[index]);
        }
        return bytes;
    }
}

const sub_asset_entry *asset_meta::find_sub_asset(std::string_view subPath) const noexcept {
    for (const sub_asset_entry &entry : subAssets) {
        if (entry.subPath == subPath) {
            return &entry;
        }
    }
    return nullptr;
}

std::string conjure::assets::meta_path_of(std::string_view sourcePath) {
    std::string path(sourcePath);
    path.append(META_SUFFIX);
    return path;
}

bool conjure::assets::is_meta_path(std::string_view path) noexcept {
    return core::str::ends_with(path, META_SUFFIX);
}

core::result<asset_meta> conjure::assets::parse_asset_meta(std::string_view text) {
    core::result<core::json::document> parsed = core::json::document::parse(text);
    if (!parsed.has_value()) {
        return invalid(std::format("meta is not valid JSON: {}", parsed.get_error().message()));
    }
    const core::json::document &doc = parsed.value();
    const core::json::value root = doc.root();
    if (!root.is_object()) {
        return invalid("meta root is not an object");
    }

    asset_meta meta = {};

    const core::json::value schema = root[KEY_SCHEMA];
    if (schema.is_valid() && (!schema.is_integer() || schema.as_u64() != META_SCHEMA_VERSION)) {
        return invalid(std::format("meta schema {} is not supported", core::json::to_string(schema)));
    }

    const core::json::value id = root[KEY_UUID];
    if (!id.is_string()) {
        return invalid("meta has no uuid");
    }
    const std::optional<core::uuid> parsedId = core::uuid::parse(id.as_string());
    if (!parsedId.has_value() || parsedId->is_null()) {
        return invalid(std::format("meta uuid '{}' is malformed", id.as_string()));
    }
    meta.id = *parsedId;

    const core::json::value importer = root[KEY_IMPORTER];
    if (importer.is_valid() && !importer.is_string()) {
        return invalid("meta importer is not a string");
    }
    meta.importerId = std::string(importer.as_string());

    const core::json::value importerVersion = root[KEY_IMPORTER_VERSION];
    if (importerVersion.is_valid() && !importerVersion.is_integer()) {
        return invalid("meta importerVersion is not an integer");
    }
    meta.importerVersion = static_cast<u32>(importerVersion.as_u64());

    const core::json::value settings = root[KEY_SETTINGS];
    if (settings.is_valid()) {
        if (!settings.is_object()) {
            return invalid("meta settings is not an object");
        }
        meta.settings = core::json::document::from_value(settings);
    }

    const core::json::value subAssets = root[KEY_SUB_ASSETS];
    if (subAssets.is_valid()) {
        if (!subAssets.is_object()) {
            return invalid("meta subAssets is not an object");
        }
        for (const auto [key, item] : subAssets.members()) {
            if (!item.is_string()) {
                return invalid(std::format("meta sub-asset '{}' has no uuid string", key));
            }
            const std::optional<core::uuid> subId = core::uuid::parse(item.as_string());
            if (!subId.has_value() || subId->is_null()) {
                return invalid(std::format("meta sub-asset '{}' has a malformed uuid", key));
            }
            if (key.empty()) {
                return invalid("meta sub-asset has an empty sub path");
            }
            meta.subAssets.push_back(sub_asset_entry{std::string(key), *subId});
        }
        std::sort(meta.subAssets.begin(), meta.subAssets.end(), [](const sub_asset_entry &a, const sub_asset_entry &b) {
            return a.subPath < b.subPath;
        });
    }

    for (const auto [key, item] : root.members()) {
        if (!is_known_key(key)) {
            meta.unknown.set(key, item);
        }
    }
    return meta;
}

std::string conjure::assets::serialize_asset_meta(const asset_meta &meta) {
    core::json::document doc = core::json::document::object();
    doc.set(KEY_SCHEMA, META_SCHEMA_VERSION);
    doc.set(KEY_UUID, meta.id.to_string());
    doc.set(KEY_IMPORTER, meta.importerId);
    doc.set(KEY_IMPORTER_VERSION, meta.importerVersion);
    if (meta.settings.root().is_object()) {
        doc.set(KEY_SETTINGS, meta.settings);
    } else {
        doc.set(KEY_SETTINGS, core::json::document::object());
    }

    std::vector<sub_asset_entry> sorted = meta.subAssets;
    std::sort(sorted.begin(), sorted.end(), [](const sub_asset_entry &a, const sub_asset_entry &b) {
        return a.subPath < b.subPath;
    });
    core::json::document subAssets = core::json::document::object();
    for (const sub_asset_entry &entry : sorted) {
        subAssets.set(entry.subPath, entry.id.to_string());
    }
    doc.set(KEY_SUB_ASSETS, subAssets);

    for (const auto [key, item] : meta.unknown.root().members()) {
        if (!is_known_key(key)) {
            doc.set(key, item);
        }
    }

    // The canonical form sorts keys and is byte-stable; re-parsing and pretty printing keeps the
    // sorted order and adds the indentation humans expect in a committed file.
    const std::string canonical = doc.to_canonical_string();
    core::result<core::json::document> reparsed = core::json::document::parse(canonical);
    std::string text = reparsed.has_value() ? reparsed.value().to_string(true) : canonical;
    text.push_back('\n');
    return text;
}

core::result<asset_meta> conjure::assets::read_asset_meta(
  core::vfs::file_system &fileSystem,
  std::string_view metaPath
) {
    core::result<std::string> text = fileSystem.read_text_file(metaPath);
    if (!text.has_value()) {
        if (text.get_error().generic_code() == core::error_code::NOT_FOUND) {
            return make_asset_error(asset_error_code::NOT_FOUND, std::format("no meta at '{}'", metaPath));
        }
        return make_asset_error(
          asset_error_code::IO_ERROR,
          std::format("cannot read meta '{}': {}", metaPath, text.get_error().message())
        );
    }
    core::result<asset_meta> parsed = parse_asset_meta(text.value());
    if (!parsed.has_value()) {
        return invalid(std::format("'{}': {}", metaPath, parsed.get_error().message()));
    }
    return parsed;
}

core::result<void> conjure::assets::write_asset_meta(
  core::vfs::file_system &fileSystem,
  std::string_view metaPath,
  const asset_meta &meta
) {
    const std::vector<std::byte> bytes = to_bytes(serialize_asset_meta(meta));
    core::result<void> written = fileSystem.write_file_atomic(metaPath, bytes);
    if (!written.has_value()) {
        return make_asset_error(
          asset_error_code::IO_ERROR,
          std::format("cannot write meta '{}': {}", metaPath, written.get_error().message())
        );
    }
    return core::status::success();
}

u64 conjure::assets::canonical_settings_hash(const core::json::value &settings) {
    if (!settings.is_valid()) {
        return detail::fnv1a64("{}");
    }
    return detail::fnv1a64(core::json::to_string(settings, true));
}
