#pragma once

#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_types.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

// The db.bin snapshot (docs/design/05_assets.md 2.7): records, their dependency lists and side
// files, written little-endian with a versioned header. Purely a cache: when it is missing,
// corrupt, from another version or lists different mounts, scan() rebuilds it from the sources.

namespace conjure::assets::detail {

    /// <summary>
    /// One record as persisted.
    /// </summary>
    struct snapshot_record {
        asset_record record = {};
        std::vector<asset_id> dependencies = {};
        std::vector<std::string> sideFiles = {};
    };

    /// <summary>
    /// Everything the snapshot holds.
    /// </summary>
    struct database_snapshot {
        std::vector<std::string> mountNames = {};
        std::vector<snapshot_record> records = {};
    };

    /// <summary>
    /// Serialises a snapshot.
    /// </summary>
    [[nodiscard]] std::vector<std::byte> write_database_snapshot(const database_snapshot &snapshot);

    /// <summary>
    /// Parses a snapshot. CACHE_CORRUPT for anything malformed or from another format version.
    /// </summary>
    [[nodiscard]] core::result<database_snapshot> parse_database_snapshot(std::span<const std::byte> bytes);
}
