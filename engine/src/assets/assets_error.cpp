#include "conjure/assets/assets_error.h"

#include <array>
#include <string_view>

using namespace conjure;
using namespace conjure::assets;

namespace {
    constexpr std::array<std::string_view, static_cast<usize>(assets_error::COUNT)> CODE_NAMES = {
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

std::string_view conjure::assets::assets_error_name(assets_error code) noexcept {
    const usize index = static_cast<usize>(code);
    if (index >= CODE_NAMES.size()) {
        return {};
    }
    return CODE_NAMES[index];
}
