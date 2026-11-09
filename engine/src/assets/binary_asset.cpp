#include "conjure/assets/binary_asset.h"

#include <array>
#include <memory>
#include <utility>

using namespace conjure;
using namespace conjure::assets;

namespace {
    constexpr std::array<std::string_view, 4> EXTENSIONS = {".bin", ".dat", ".bytes", ".raw"};
}

/*===========================================================================*/
/*===========================================================================*/
// binary_importer

binary_importer::binary_importer() {}

binary_importer::~binary_importer() {}

std::string_view binary_importer::id() const noexcept {
    return ID;
}

u32 binary_importer::version() const noexcept {
    return VERSION;
}

std::span<const std::string_view> binary_importer::extensions() const noexcept {
    return EXTENSIONS;
}

core::result<import_output> binary_importer::import(import_context &context) const {
    imported_asset primary = {};
    primary.type = asset_type_traits<binary_asset>::id();
    primary.payload.assign(context.source_bytes().begin(), context.source_bytes().end());

    import_output output = {};
    output.assets.push_back(std::move(primary));
    return output;
}

/*===========================================================================*/
/*===========================================================================*/
// binary_loader

binary_loader::binary_loader() {}

binary_loader::~binary_loader() {}

core::result<binary_asset> binary_loader::load_cpu_typed(
  const load_context &,
  std::span<const std::byte> payloadBytes
) const {
    binary_asset asset = {};
    asset.bytes.assign(payloadBytes.begin(), payloadBytes.end());
    return asset;
}

core::result<binary_asset> binary_loader::finalise_typed(const finalise_context &, binary_asset &&intermediate) const {
    return std::move(intermediate);
}

std::optional<binary_asset> binary_loader::create_fallback_typed() const {
    return binary_asset();
}

/*===========================================================================*/
/*===========================================================================*/
// Registration

core::result<void> conjure::assets::register_binary_asset_type(
  asset_type_registry &types,
  importer_registry *importers
) {
    CONJURE_TRY(types.register_type(make_asset_type_desc<binary_asset>(std::make_unique<binary_loader>(), 1)));
    if (importers != nullptr) {
        CONJURE_TRY(importers->register_importer(std::make_unique<binary_importer>()));
    }
    return core::status::success();
}
