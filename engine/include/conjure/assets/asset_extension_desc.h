#pragma once

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_loader.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/assets/asset_types.h"

#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#include <memory>
#include <type_traits>

// Trivially copyable extension records a plugin module registers (docs/design/05_assets.md 8,
// docs/design/09_cross_area_resolutions.md R37). The engine's assets_module_consumer calls the
// factories and forwards the results to register_importer / register_type on attach.

namespace conjure::assets {

    /// <summary>
    /// One importer offered by a module.
    /// </summary>
    struct importer_extension_desc {
        static constexpr core::string_id EXTENSION_POINT{"assets.importer"};

        const char *id = "";                                       // must equal the created importer's id()
        std::unique_ptr<asset_importer> (*create)() = nullptr;
    };

    /// <summary>
    /// One asset type offered by a module.
    /// </summary>
    struct asset_type_extension_desc {
        static constexpr core::string_id EXTENSION_POINT{"assets.type"};

        const char *name = "";                                     // "vendor.module.type"
        u32 version = 1;
        asset_type_flags flags = asset_type_flags::NONE;
        std::unique_ptr<asset_loader> (*createLoader)() = nullptr;
    };

    static_assert(std::is_trivially_copyable_v<importer_extension_desc>);
    static_assert(std::is_trivially_copyable_v<asset_type_extension_desc>);

    /// <summary>
    /// Instantiates the importer of a record and registers it. INVALID_ARGUMENT when the factory
    /// is missing, returns nothing or the created importer's id differs from the record's.
    /// </summary>
    [[nodiscard]] core::result<void> register_importer_extension(
      importer_registry &importers,
      const importer_extension_desc &desc
    );

    /// <summary>
    /// Instantiates the loader of a record and registers the type. INVALID_ARGUMENT when the
    /// factory is missing, returns nothing or the loader's type differs from the record's name.
    /// </summary>
    [[nodiscard]] core::result<void> register_asset_type_extension(
      asset_type_registry &types,
      const asset_type_extension_desc &desc
    );
}
