#pragma once

#include "rhi/backends/vulkan/vulkan_common.h"
#include "rhi/backends/vulkan/vulkan_loader.h"
#include "rhi/device_backend.h"

#include "conjure/rhi/backend.h"
#include "conjure/rhi/device.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <memory>
#include <string>
#include <vector>

// The Vulkan backend (docs/design/02_renderer.md 9 step 3): instance (with the surface extension
// for the window kind, validation layers and debug utils when the debug layer is on), physical
// device selection (discrete > integrated > virtual, CPU devices only when preferSoftwareAdapter),
// one graphics queue plus optional dedicated compute and transfer queues, Vulkan 1.2 timeline
// semaphores (NVRHI needs them) and nvrhi::vulkan::createDevice over the result.

namespace conjure::rhi {
    class message_callback;
}

namespace conjure::rhi::vulkan_backend {

    class vulkan_device_backend final : public device_backend {
        private:
            message_callback &mMessages;   // set by the only constructor; device::impl owns it and outlives the backend
            device_capabilities mCapabilities = {};
            vulkan_loader mLoader = {};
            VkInstance mInstance = VK_NULL_HANDLE;
            VkDebugUtilsMessengerEXT mMessenger = VK_NULL_HANDLE;
            VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
            VkDevice mDevice = VK_NULL_HANDLE;
            u32 mGraphicsFamily = NO_QUEUE_FAMILY;
            u32 mComputeFamily = NO_QUEUE_FAMILY;
            u32 mTransferFamily = NO_QUEUE_FAMILY;
            VkQueue mGraphicsQueue = VK_NULL_HANDLE;
            VkQueue mComputeQueue = VK_NULL_HANDLE;
            VkQueue mTransferQueue = VK_NULL_HANDLE;
            std::vector<std::string> mInstanceExtensions = {};
            std::vector<std::string> mDeviceExtensions = {};
            std::vector<const char *> mInstanceExtensionPointers = {};
            std::vector<const char *> mDeviceExtensionPointers = {};
            bool mSwapchainSupported = false;
            bool mBufferDeviceAddress = false;
            nvrhi::vulkan::DeviceHandle mNvrhiDevice = nullptr;

        public:
            explicit vulkan_device_backend(message_callback &messages);
            ~vulkan_device_backend() override;

        public:
            /// <summary>
            /// Creates the API objects. Fails with rhi_error::DEVICE_CREATION_FAILED quoting the VkResult.
            /// </summary>
            [[nodiscard]] core::result<void> initialise(const device_desc &desc);

            [[nodiscard]] nvrhi::IDevice *nvrhi_device() const noexcept override;
            [[nodiscard]] const device_capabilities &capabilities() const noexcept override;
            [[nodiscard]] core::result<std::unique_ptr<swapchain_backend>> create_swapchain(
              const native_window &window,
              const swapchain_desc &desc
            ) override;
            [[nodiscard]] bool poll_device_removed() noexcept override;

            [[nodiscard]] VkInstance instance() const noexcept {
                return mInstance;
            }
            [[nodiscard]] VkPhysicalDevice physical_device() const noexcept {
                return mPhysicalDevice;
            }
            [[nodiscard]] VkDevice vk_device() const noexcept {
                return mDevice;
            }
            [[nodiscard]] VkQueue graphics_queue() const noexcept {
                return mGraphicsQueue;
            }
            [[nodiscard]] u32 graphics_family() const noexcept {
                return mGraphicsFamily;
            }
            [[nodiscard]] bool swapchain_supported() const noexcept {
                return mSwapchainSupported;
            }
            [[nodiscard]] nvrhi::vulkan::IDevice *nvrhi_vulkan_device() const noexcept {
                return mNvrhiDevice.Get();
            }
            [[nodiscard]] const vulkan_loader &loader() const noexcept {
                return mLoader;
            }
            [[nodiscard]] message_callback &messages() const noexcept {
                return mMessages;
            }

        private:
            [[nodiscard]] core::result<void> create_instance(const device_desc &desc);
            [[nodiscard]] core::result<void> select_physical_device(const device_desc &desc);
            [[nodiscard]] core::result<void> create_device(const device_desc &desc);
            void fill_capabilities();
    };

    /// <summary>
    /// The instance extension that creates surfaces for a window kind; "" for NONE.
    /// </summary>
    [[nodiscard]] const char *surface_extension_for(native_window_kind kind) noexcept;

    /// <summary>
    /// Creates the Vulkan backend for the description.
    /// </summary>
    [[nodiscard]] core::result<std::unique_ptr<device_backend>> create_vulkan_backend(
      const device_desc &desc,
      message_callback &messages
    );
}
