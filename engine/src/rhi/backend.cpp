#include "conjure/rhi/backend.h"

#include "conjure/core/error.h"
#include "conjure/core/string_utils.h"

using namespace conjure;
using namespace conjure::rhi;

// CONJURE_RHI_D3D12 / CONJURE_RHI_VULKAN come from the build (ConjureWarnings.cmake, platform
// defines); a backend is compiled exactly when its option is on, so these are feature flags, not
// platform conditions.
#ifndef CONJURE_RHI_D3D12
    #define CONJURE_RHI_D3D12 0
#endif
#ifndef CONJURE_RHI_VULKAN
    #define CONJURE_RHI_VULKAN 0
#endif

std::string_view conjure::rhi::to_string(graphics_backend backend) noexcept {
    switch (backend) {
        case graphics_backend::NULL_DEVICE: {
            return "null";
        }
        case graphics_backend::D3D12: {
            return "d3d12";
        }
        case graphics_backend::VULKAN: {
            return "vulkan";
        }
        case graphics_backend::COUNT: {
            break;
        }
    }
    return "null";
}

core::result<graphics_backend> conjure::rhi::parse_graphics_backend(std::string_view name) noexcept {
    const std::string lowered = core::str::to_lower(core::str::trim(name));
    if (lowered == "null") {
        return graphics_backend::NULL_DEVICE;
    }
    if (lowered == "d3d12") {
        return graphics_backend::D3D12;
    }
    if (lowered == "vulkan") {
        return graphics_backend::VULKAN;
    }
    return core::make_error(core::error_code::INVALID_ARGUMENT, "unknown graphics backend '{}'", name);
}

bool conjure::rhi::is_backend_compiled(graphics_backend backend) noexcept {
    switch (backend) {
        case graphics_backend::NULL_DEVICE: {
            return true;
        }
        case graphics_backend::D3D12: {
            return CONJURE_RHI_D3D12 != 0;
        }
        case graphics_backend::VULKAN: {
            return CONJURE_RHI_VULKAN != 0;
        }
        case graphics_backend::COUNT: {
            break;
        }
    }
    return false;
}

graphics_backend conjure::rhi::default_graphics_backend() noexcept {
    if (is_backend_compiled(graphics_backend::D3D12)) {
        return graphics_backend::D3D12;
    }
    if (is_backend_compiled(graphics_backend::VULKAN)) {
        return graphics_backend::VULKAN;
    }
    return graphics_backend::NULL_DEVICE;
}
