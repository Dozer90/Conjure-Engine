#include "conjure/renderer/render_settings.h"

using namespace conjure;
using namespace conjure::renderer;

std::string_view conjure::renderer::to_string(tonemap_operator value) noexcept {
    switch (value) {
        case tonemap_operator::ACES_FITTED: {
            return "ACES_FITTED";
        }
        case tonemap_operator::REINHARD_EXTENDED: {
            return "REINHARD_EXTENDED";
        }
        case tonemap_operator::AGX: {
            return "AGX";
        }
        case tonemap_operator::NONE: {
            return "NONE";
        }
    }
    return "ACES_FITTED";
}

std::string_view conjure::renderer::to_string(exposure_mode value) noexcept {
    switch (value) {
        case exposure_mode::AUTOMATIC: {
            return "AUTOMATIC";
        }
        case exposure_mode::MANUAL: {
            return "MANUAL";
        }
    }
    return "AUTOMATIC";
}
