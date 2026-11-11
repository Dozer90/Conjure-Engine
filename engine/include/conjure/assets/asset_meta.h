#pragma once

#include "conjure/assets/asset_types.h"

#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/vfs/file_system.h"

#include <string>
#include <string_view>
#include <vector>

// The .meta sidecar (docs/design/05_assets.md 2.6): a JSON file next to every source that pins
// the uuid, the importer and its settings, and the uuids of the sub-assets the importer produced.
// Written only in editor mode, always through tmp + rename; never overwritten when corrupt.

namespace conjure::assets {

    /// <summary>
    /// One sub-asset uuid, keyed by the stable sub path the importer uses ("mesh/Body").
    /// </summary>
    struct sub_asset_entry {
        std::string subPath = {};
        asset_id id = {};

        [[nodiscard]] bool operator==(const sub_asset_entry &) const = default;
    };

    /// <summary>
    /// The parsed sidecar. settings is opaque to the module; unknown top-level keys survive a
    /// read/write round trip in 'unknown'.
    /// </summary>
    struct CONJURE_API asset_meta {
        asset_id id = {};
        std::string importerId = {};
        u32 importerVersion = 0;
        core::json::document settings = core::json::document::object();
        std::vector<sub_asset_entry> subAssets = {};
        core::json::document unknown = core::json::document::object();

        /// <summary>
        /// The uuid recorded for a sub path, or nullopt.
        /// </summary>
        [[nodiscard]] const sub_asset_entry *find_sub_asset(std::string_view subPath) const noexcept;
    };

    /// <summary>
    /// Path of the sidecar of a source file: "<source>.meta".
    /// </summary>
    [[nodiscard]] CONJURE_API std::string meta_path_of(std::string_view sourcePath);

    /// <summary>
    /// True when the path ends in ".meta".
    /// </summary>
    [[nodiscard]] CONJURE_API bool is_meta_path(std::string_view path) noexcept;

    /// <summary>
    /// Parses sidecar text. META_INVALID for malformed JSON, a non-object root, a missing or
    /// malformed uuid, an unsupported schema or a malformed sub-asset table.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<asset_meta> parse_asset_meta(std::string_view text);

    /// <summary>
    /// Serialises a sidecar: sorted keys, sub-assets sorted by sub path, pretty printed, newline
    /// terminated. Deterministic for the same content.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string serialize_asset_meta(const asset_meta &meta);

    /// <summary>
    /// Reads and parses the sidecar at 'metaPath'. NOT_FOUND (assets category) when absent,
    /// IO_ERROR for other read failures, META_INVALID for bad content.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<asset_meta> read_asset_meta(
      core::vfs::file_system &fileSystem,
      std::string_view metaPath
    );

    /// <summary>
    /// Writes the sidecar atomically (tmp + rename). IO_ERROR when the file system refuses.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<void> write_asset_meta(
      core::vfs::file_system &fileSystem,
      std::string_view metaPath,
      const asset_meta &meta
    );

    /// <summary>
    /// Hash of the canonical form of a settings value (sorted keys, minified): independent of key
    /// order and whitespace. An invalid value hashes like an empty object.
    /// </summary>
    [[nodiscard]] CONJURE_API u64 canonical_settings_hash(const core::json::value &settings);
}
