#include "conjure/assets/asset_types.h"

#include <string_view>

using namespace conjure;
using namespace conjure::assets;

std::string_view conjure::assets::to_string(asset_state state) noexcept {
    switch (state) {
        case asset_state::UNLOADED:
            return "UNLOADED";
        case asset_state::QUEUED:
            return "QUEUED";
        case asset_state::LOADING:
            return "LOADING";
        case asset_state::FINALISING:
            return "FINALISING";
        case asset_state::LOADED:
            return "LOADED";
        case asset_state::RELOADING:
            return "RELOADING";
        case asset_state::FAILED:
            return "FAILED";
    }
    return "UNKNOWN";
}
