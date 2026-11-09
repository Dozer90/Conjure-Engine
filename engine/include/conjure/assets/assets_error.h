#pragma once

#include "conjure/core/error.h"
#include "conjure/core/types.h"

#include <string>
#include <string_view>

// The assets error category (docs/design/05_assets.md 2.1, 6). Every recoverable failure of the
// module is reported through core::result with one of these codes; the generic core codes are used
// only where the failure is not asset-specific (an invalid argument, a registry overflow).

namespace conjure::assets {

    /// <summary>
    /// Failure conditions of the asset pipeline.
    /// </summary>
    enum class asset_error_code : u16 {
        NONE = 0,
        NOT_FOUND,              // unknown uuid, or the source of a record is gone
        NO_IMPORTER,            // no importer registered for the source's extension
        NO_LOADER,              // no loader registered for the blob's type
        META_INVALID,           // the .meta sidecar does not parse or lacks required fields
        DUPLICATE_UUID,         // two .meta files declare the same uuid
        IO_ERROR,               // the file system refused a read or write
        IMPORT_FAILED,          // the importer reported an error or produced invalid output
        CACHE_MISS,             // no cooked blob for the uuid at load time
        CACHE_CORRUPT,          // the cooked blob failed validation
        LOAD_FAILED,            // load_cpu failed
        FINALISE_FAILED,        // finalise failed
        TYPE_MISMATCH,          // bound type differs from the recorded or cooked type
        TYPE_UNKNOWN,           // the type id is not registered
        TYPE_DUPLICATE,         // register_type with an id that is taken
        CYCLIC_DEPENDENCY,      // the dependency graph contains a cycle through this asset
        PACK_CORRUPT,           // the pack header, TOC or content hash failed validation
        UNBOUND,                // the operation needs a bound slot
        SHUTTING_DOWN,          // the manager refuses new work after shutdown()
        TIMEOUT,                // load_blocking exceeded its timeout
        COUNT
    };

    /// <summary>
    /// Stable upper-case name of a code ("CACHE_MISS"); "" for values outside the enumeration.
    /// </summary>
    [[nodiscard]] std::string_view asset_error_code_name(asset_error_code code) noexcept;

    /// <summary>
    /// Builds an error in the assets category.
    /// </summary>
    [[nodiscard]] core::error make_asset_error(asset_error_code code, std::string message = {});
}

CONJURE_DECLARE_ERROR_CATEGORY(conjure::assets::asset_error_code, "assets", conjure::assets::asset_error_code_name);
