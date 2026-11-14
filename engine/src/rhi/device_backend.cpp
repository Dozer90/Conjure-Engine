#include "rhi/device_backend.h"

#include "conjure/rhi/rhi_error.h"
#include "conjure/rhi/texture_format.h"

#include "conjure/core/error.h"

using namespace conjure;
using namespace conjure::rhi;

namespace {
    constexpr u32 MIN_SWAPCHAIN_BUFFERS = 2;
    constexpr u32 MAX_SWAPCHAIN_BUFFERS = 16;   // DXGI_MAX_SWAP_CHAIN_BUFFERS
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

swapchain_backend::swapchain_backend() {}

swapchain_backend::~swapchain_backend() {}

device_backend::device_backend() {}

device_backend::~device_backend() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

null_backend::null_device *device_backend::as_null() noexcept {
    return nullptr;
}

void device_backend::flush_queues() noexcept {}

bool device_backend::debug_layer_inherited() const noexcept {
    return false;
}


/*===========================================================================*/
/*===========================================================================*/
// Free functions

bool conjure::rhi::is_presentable_format(texture_format format) noexcept {
    switch (format) {
        case texture_format::RGBA8_UNORM:
        case texture_format::RGBA8_SRGB:
        case texture_format::BGRA8_SRGB:
        case texture_format::RGBA16_FLOAT: {
            return true;
        }
        default: {
            return false;
        }
    }
}

core::result<void> conjure::rhi::validate_swapchain_desc(const swapchain_desc &desc) {
    if (desc.width == 0 || desc.height == 0) {
        return core::make_error(
          rhi_error::INVALID_DESC,
          "swapchain_desc: width and height must be non-zero, got {}x{}",
          desc.width,
          desc.height
        );
    }
    if (desc.bufferCount < MIN_SWAPCHAIN_BUFFERS || desc.bufferCount > MAX_SWAPCHAIN_BUFFERS) {
        return core::make_error(
          rhi_error::INVALID_DESC,
          "swapchain_desc: bufferCount must be in {}..{}, got {}",
          MIN_SWAPCHAIN_BUFFERS,
          MAX_SWAPCHAIN_BUFFERS,
          desc.bufferCount
        );
    }
    if (!is_presentable_format(desc.format)) {
        return core::make_error(
          rhi_error::INVALID_DESC,
          "swapchain_desc: format {} cannot be presented (use RGBA8_UNORM, RGBA8_SRGB, BGRA8_SRGB or RGBA16_FLOAT)",
          to_string(desc.format)
        );
    }
    return core::result<void>::success();
}
