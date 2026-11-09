#pragma once

// The one place the Vulkan headers enter the rhi module. NVRHI's static Vulkan backend uses
// vulkan.hpp's dynamic dispatcher (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC, storage defined in
// nvrhi_vk) and leaves initialising it to the application, so this backend loads the Vulkan
// library itself (vulkan_loader), feeds vkGetInstanceProcAddr to the dispatcher and makes every
// call through it. VK_NO_PROTOTYPES (a compile definition of the Vulkan sources) keeps the C
// prototypes out so nothing links against vulkan-1; a machine without a loader fails
// device::create with DEVICE_CREATION_FAILED instead of failing to start.

#include "conjure/core/types.h"

#include <nvrhi/nvrhi.h>
#include <nvrhi/vulkan.h>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include <string>

namespace conjure::rhi::vulkan_backend {

    /// <summary>
    /// The dynamic dispatcher every Vulkan call goes through.
    /// </summary>
    [[nodiscard]] inline vk::detail::DispatchLoaderDynamic &dispatch() noexcept {
        return VULKAN_HPP_DEFAULT_DISPATCHER;
    }

    /// <summary>
    /// "VK_ERROR_DEVICE_LOST"-style text for error messages.
    /// </summary>
    [[nodiscard]] inline std::string describe_vk_result(VkResult result) {
        const char *name = nvrhi::vulkan::resultToString(result);
        return name != nullptr ? std::string(name) : std::to_string(static_cast<int>(result));
    }

    /// <summary>
    /// True for the results that mean the device is gone for good.
    /// </summary>
    [[nodiscard]] constexpr bool is_device_lost_result(VkResult result) noexcept {
        return result == VK_ERROR_DEVICE_LOST || result == VK_ERROR_INITIALIZATION_FAILED;
    }

    inline constexpr u32 NO_QUEUE_FAMILY = 0xFFFFFFFFu;
}
