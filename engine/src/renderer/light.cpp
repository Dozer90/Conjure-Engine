#include "conjure/renderer/light.h"

using namespace conjure;
using namespace conjure::renderer;

u16 conjure::renderer::clamp_shadow_resolution(u16 requested) noexcept {
    // Nearest of {256, 512, 1024}: the midpoints are 384 and 768.
    if (requested < 384) {
        return MIN_SHADOW_TILE_SIZE;
    }
    if (requested < 768) {
        return 512;
    }
    return MAX_SHADOW_TILE_SIZE;
}

std::string_view conjure::renderer::to_string(light_type type) noexcept {
    switch (type) {
        case light_type::DIRECTIONAL: {
            return "DIRECTIONAL";
        }
        case light_type::POINT: {
            return "POINT";
        }
        case light_type::SPOT: {
            return "SPOT";
        }
    }
    return "POINT";
}

std::string_view conjure::renderer::to_string(shadow_filter filter) noexcept {
    switch (filter) {
        case shadow_filter::DEFAULT: {
            return "DEFAULT";
        }
        case shadow_filter::HARD: {
            return "HARD";
        }
        case shadow_filter::PCF: {
            return "PCF";
        }
        case shadow_filter::PCSS: {
            return "PCSS";
        }
    }
    return "DEFAULT";
}
