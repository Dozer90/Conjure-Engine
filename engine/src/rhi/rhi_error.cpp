#include "conjure/rhi/rhi_error.h"

using namespace conjure;
using namespace conjure::rhi;

std::string_view conjure::rhi::rhi_error_name(rhi_error code) noexcept {
    switch (code) {
        case rhi_error::DEVICE_CREATION_FAILED: {
            return "DEVICE_CREATION_FAILED";
        }
        case rhi_error::DEVICE_LOST: {
            return "DEVICE_LOST";
        }
        case rhi_error::OUT_OF_DATE: {
            return "OUT_OF_DATE";
        }
        case rhi_error::SWAPCHAIN_CREATION_FAILED: {
            return "SWAPCHAIN_CREATION_FAILED";
        }
        case rhi_error::INVALID_DESC: {
            return "INVALID_DESC";
        }
        case rhi_error::PRESENT_FAILED: {
            return "PRESENT_FAILED";
        }
        case rhi_error::COUNT: {
            break;
        }
    }
    return "";
}
