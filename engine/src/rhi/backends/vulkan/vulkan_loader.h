#pragma once

#include "rhi/backends/vulkan/vulkan_common.h"

#include "conjure/core/dynamic_library.h"
#include "conjure/core/result.h"

// Runtime loading of the Vulkan loader library (vulkan-1.dll, libvulkan.so.1, libvulkan.1.dylib /
// libMoltenVK.dylib) and initialisation of vulkan.hpp's dynamic dispatcher in the three stages
// Vulkan requires: global functions from vkGetInstanceProcAddr, instance-level functions once the
// instance exists, device-level functions once the device exists.

namespace conjure::rhi::vulkan_backend {

    class vulkan_loader {
        private:
            core::dynamic_library mLibrary = {};
            PFN_vkGetInstanceProcAddr mGetInstanceProcAddr = nullptr;

        public:
            vulkan_loader();
            ~vulkan_loader();

            vulkan_loader(const vulkan_loader &) = delete;
            vulkan_loader &operator=(const vulkan_loader &) = delete;

        public:
            /// <summary>
            /// Loads the platform's Vulkan loader and the global entry points. Fails with
            /// rhi_error::DEVICE_CREATION_FAILED naming the libraries that were tried.
            /// </summary>
            [[nodiscard]] core::result<void> load();

            /// <summary>
            /// Loads the instance-level entry points (surface, physical device, device creation).
            /// </summary>
            void init_instance(VkInstance instance);

            /// <summary>
            /// Loads the device-level entry points (queues, swapchain, synchronisation).
            /// </summary>
            void init_device(VkDevice device);

            [[nodiscard]] bool is_loaded() const noexcept {
                return mGetInstanceProcAddr != nullptr;
            }

            [[nodiscard]] PFN_vkGetInstanceProcAddr get_instance_proc_addr() const noexcept {
                return mGetInstanceProcAddr;
            }

            /// <summary>
            /// An instance-level function by name, for the platform surface entry points the
            /// dispatcher does not carry on every platform; nullptr when the extension is absent.
            /// </summary>
            [[nodiscard]] PFN_vkVoidFunction instance_function(VkInstance instance, const char *name) const noexcept;
    };
}
