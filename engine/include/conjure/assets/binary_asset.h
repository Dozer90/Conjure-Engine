#pragma once

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_loader.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/assets/asset_types.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

// The one built-in asset type: an opaque byte blob (docs/design/05_assets.md D7). It exercises
// the whole pipeline without any other module and serves raw data files.

namespace conjure::assets {

    /// <summary>
    /// The published payload: the source bytes as imported.
    /// </summary>
    struct binary_asset {
        std::vector<std::byte> bytes = {};
    };
}

CONJURE_DECLARE_ASSET_TYPE(conjure::assets::binary_asset, "conjure.assets.binary");

namespace conjure::assets {

    /// <summary>
    /// Imports .bin / .dat / .bytes / .raw files verbatim.
    /// </summary>
    class binary_importer final : public asset_importer {
        public:
            static constexpr std::string_view ID = "conjure.assets.binary";
            static constexpr u32 VERSION = 1;

        public:
            binary_importer();
            ~binary_importer() override;

        public:
            [[nodiscard]] std::string_view id() const noexcept override;
            [[nodiscard]] u32 version() const noexcept override;
            [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override;
            [[nodiscard]] core::result<import_output> import(import_context &context) const override;
    };

    /// <summary>
    /// Copies the payload bytes; finalise is trivial. The fallback is an empty asset.
    /// </summary>
    class binary_loader final : public typed_asset_loader<binary_asset, binary_asset> {
        public:
            binary_loader();
            ~binary_loader() override;

        protected:
            [[nodiscard]] core::result<binary_asset> load_cpu_typed(
              const load_context &context,
              std::span<const std::byte> payloadBytes
            ) const override;
            [[nodiscard]] core::result<binary_asset> finalise_typed(
              const finalise_context &context,
              binary_asset &&intermediate
            ) const override;
            [[nodiscard]] std::optional<binary_asset> create_fallback_typed() const override;
    };

    /// <summary>
    /// Registers the type and, when a registry is given, the importer.
    /// </summary>
    [[nodiscard]] core::result<void> register_binary_asset_type(
      asset_type_registry &types,
      importer_registry *importers = nullptr
    );
}
