#include "conjure/assets/assets_error.h"

#include <array>
#include <string>
#include <string_view>
#include <utility>

using namespace conjure;
using namespace conjure::assets;

namespace {
    constexpr std::array<std::string_view, static_cast<usize>(asset_error_code::COUNT)> CODE_NAMES = {
        "NONE",
        "NOT_FOUND",
        "NO_IMPORTER",
        "NO_LOADER",
        "META_INVALID",
        "DUPLICATE_UUID",
        "IO_ERROR",
        "IMPORT_FAILED",
        "CACHE_MISS",
        "CACHE_CORRUPT",
        "LOAD_FAILED",
        "FINALISE_FAILED",
        "TYPE_MISMATCH",
        "TYPE_UNKNOWN",
        "TYPE_DUPLICATE",
        "CYCLIC_DEPENDENCY",
        "PACK_CORRUPT",
        "UNBOUND",
        "SHUTTING_DOWN",
        "TIMEOUT",
    };
}

std::string_view conjure::assets::asset_error_code_name(asset_error_code code) noexcept {
    const usize index = static_cast<usize>(code);
    if (index >= CODE_NAMES.size()) {
        return {};
    }
    return CODE_NAMES[index];
}

core::error conjure::assets::make_asset_error(asset_error_code code, std::string message) {
    return core::error::from(code, std::move(message));
}
