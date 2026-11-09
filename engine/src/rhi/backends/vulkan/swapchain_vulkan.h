#pragma once

#include "rhi/backends/vulkan/vulkan_common.h"
#include "rhi/device_backend.h"

#include "conjure/rhi/native_window.h"
#include "conjure/rhi/swapchain.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <vector>

// The Vulkan swapchain: a VkSurfaceKHR created from the native window handle, a VkSwapchainKHR
// whose images are wrapped by nvrhi::IDevice::createHandleForNativeTexture in the Present state,
// and binary semaphores that tie acquisition and presentation into NVRHI's queue through
// queueWaitForSemaphore / queueSignalSemaphore (the pattern NVIDIA's Donut framework uses).
// Vulkan has no windowless swapchain, so a NONE handle is INVALID_DESC here.
//
// Lifetime rules that keep every handle valid exactly once: mSwapchain is VK_NULL_HANDLE whenever
// no usable swapchain exists (before creation, after a failed resize); resize() hands the old
// swapchain to vkCreateSwapchainKHR as oldSwapchain, which retires it whether or not the new one is
// created, and destroys it exactly once after the call; acquire() and present() report OUT_OF_DATE
// instead of touching a swapchain or semaphore set that does not exist.

namespace conjure::rhi::vulkan_backend {

    class vulkan_device_backend;

    class vulkan_swapchain final : public swapchain_backend {
        private:
            vulkan_device_backend &mBackend;   // set by the only constructor; the backend outlives its swapchains
            swapchain_desc mDesc = {};
            VkSurfaceKHR mSurface = VK_NULL_HANDLE;
            VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
            VkFormat mFormat = VK_FORMAT_UNDEFINED;
            VkColorSpaceKHR mColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            VkPresentModeKHR mPresentMode = VK_PRESENT_MODE_FIFO_KHR;
            std::vector<VkImage> mImages = {};
            std::vector<nvrhi::TextureHandle> mBackBuffers = {};
            std::vector<VkSemaphore> mAcquireSemaphores = {};    // one per frame slot, cycled by mFrameSlot
            std::vector<VkSemaphore> mPresentSemaphores = {};    // one per image
            u32 mCurrentImage = 0;
            u32 mFrameSlot = 0;
            bool mImageAcquired = false;

        public:
            vulkan_swapchain(vulkan_device_backend &backend, const swapchain_desc &desc);
            ~vulkan_swapchain() override;

        public:
            /// <summary>
            /// Creates the surface, the swapchain and the semaphores. Fails with rhi_error::INVALID_DESC for a
            /// NONE or COCOA handle, rhi_error::OUT_OF_DATE while the surface has no extent (minimised
            /// window) and rhi_error::SWAPCHAIN_CREATION_FAILED otherwise.
            /// </summary>
            [[nodiscard]] core::result<void> initialise(const native_window &window);

            [[nodiscard]] core::result<u32> acquire() override;
            [[nodiscard]] core::result<void> present() override;
            [[nodiscard]] core::result<void> resize(u32 width, u32 height) override;
            [[nodiscard]] nvrhi::ITexture *back_buffer(u32 index) const noexcept override;
            [[nodiscard]] u32 buffer_count() const noexcept override;
            [[nodiscard]] u32 current_index() const noexcept override;
            [[nodiscard]] const swapchain_desc &desc() const noexcept override;

        private:
            [[nodiscard]] bool is_usable() const noexcept;
            [[nodiscard]] core::result<void> create_surface(const native_window &window);
            [[nodiscard]] core::result<VkSurfaceCapabilitiesKHR> query_surface_capabilities() const;
            [[nodiscard]] core::result<void> create_swapchain(
              VkSwapchainKHR previous,
              const VkSurfaceCapabilitiesKHR &capabilities,
              VkExtent2D extent
            );
            [[nodiscard]] core::result<void> wrap_images(VkSwapchainKHR swapchain);
            [[nodiscard]] core::result<void> create_semaphores(
              usize count,
              std::vector<VkSemaphore> &acquireSemaphores,
              std::vector<VkSemaphore> &presentSemaphores
            ) const;
            void destroy_semaphores(
              std::vector<VkSemaphore> &acquireSemaphores,
              std::vector<VkSemaphore> &presentSemaphores
            ) const noexcept;
            void destroy_swapchain() noexcept;
            void flush_pending_acquire();
            void release_images() noexcept;
    };
}
