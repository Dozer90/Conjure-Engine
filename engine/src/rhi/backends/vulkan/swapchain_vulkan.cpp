#include "rhi/backends/vulkan/swapchain_vulkan.h"

#include "rhi/backends/vulkan/device_vulkan.h"
#include "rhi/message_callback.h"
#include "rhi/texture_format_mapping.h"

#include "conjure/rhi/log_category.h"
#include "conjure/rhi/rhi_error.h"

#include "conjure/core/error.h"
#include "conjure/core/log.h"

#include <algorithm>
#include <cstdint>
#include <format>
#include <limits>
#include <utility>

using namespace conjure;
using namespace conjure::rhi;
using namespace conjure::rhi::vulkan_backend;

namespace {
    // The platform surface create-info structures, spelled out so no X11 / Wayland / Win32 header is
    // needed here: each is sType, pNext, flags and the two platform handles (VK_KHR_*_surface specs).
    struct win32_surface_create_info {
        VkStructureType sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        const void *pNext = nullptr;
        VkFlags flags = 0;
        void *hinstance = nullptr;
        void *hwnd = nullptr;
    };

    struct xlib_surface_create_info {
        VkStructureType sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        const void *pNext = nullptr;
        VkFlags flags = 0;
        void *dpy = nullptr;
        unsigned long window = 0;
    };

    struct wayland_surface_create_info {
        VkStructureType sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        const void *pNext = nullptr;
        VkFlags flags = 0;
        void *display = nullptr;
        void *surface = nullptr;
    };

    using create_surface_fn = VkResult(
      VKAPI_PTR *
    )(VkInstance, const void *, const VkAllocationCallbacks *, VkSurfaceKHR *);

    core::error swapchain_failure(const char *what, VkResult result) {
        return core::make_error(
          rhi_error::SWAPCHAIN_CREATION_FAILED,
          "{} failed with {}",
          what,
          describe_vk_result(result)
        );
    }

    core::error empty_surface_error(VkExtent2D extent) {
        return core::make_error(
          rhi_error::OUT_OF_DATE,
          "the surface is currently {}x{}; retry after the window is restored",
          extent.width,
          extent.height
        );
    }

    core::error unusable_error() {
        return core::make_error(
          rhi_error::OUT_OF_DATE,
          "the swapchain has no images (the last resize did not complete); resize it again"
        );
    }

    const char *surface_function_name(native_window_kind kind) noexcept {
        switch (kind) {
            case native_window_kind::WINDOWS: {
                return "vkCreateWin32SurfaceKHR";
            }
            case native_window_kind::X11: {
                return "vkCreateXlibSurfaceKHR";
            }
            case native_window_kind::WAYLAND: {
                return "vkCreateWaylandSurfaceKHR";
            }
            default: {
                return "";
            }
        }
    }

    /// <summary>
    /// The swapchain extent for a window size: the surface's current extent when the platform fixes
    /// it, otherwise the requested size clamped to the surface limits. Zero means a minimised window.
    /// </summary>
    VkExtent2D resolve_extent(const VkSurfaceCapabilitiesKHR &capabilities, u32 width, u32 height) noexcept {
        VkExtent2D extent = capabilities.currentExtent;
        if (extent.width == std::numeric_limits<u32>::max()) {
            extent.width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            extent.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }
        return extent;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

vulkan_swapchain::vulkan_swapchain(vulkan_device_backend &backend, const swapchain_desc &desc) :
    mBackend(backend),
    mDesc(desc) {}

vulkan_swapchain::~vulkan_swapchain() {
    nvrhi::IDevice *device = mBackend.nvrhi_device();
    if (device != nullptr && !mBackend.messages().is_lost()) {
        flush_pending_acquire();
        device->waitForIdle();
    }
    release_images();
    if (device != nullptr) {
        device->runGarbageCollection();
    }
    destroy_semaphores(mAcquireSemaphores, mPresentSemaphores);
    destroy_swapchain();
    if (mSurface != VK_NULL_HANDLE) {
        dispatch().vkDestroySurfaceKHR(mBackend.instance(), mSurface, nullptr);
        mSurface = VK_NULL_HANDLE;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<void> vulkan_swapchain::initialise(const native_window &window) {
    if (window.is_headless()) {
        return core::make_error(rhi_error::INVALID_DESC, "the Vulkan backend needs a window to create a swapchain");
    }
    if (window.kind == native_window_kind::COCOA) {
        return core::make_error(
          rhi_error::INVALID_DESC,
          "Cocoa windows need a CAMetalLayer surface, which the platform module's create_vulkan_surface provides"
        );
    }
    if (!mBackend.swapchain_supported()) {
        return core::make_error(
          rhi_error::SWAPCHAIN_CREATION_FAILED,
          "the Vulkan device lacks {}",
          VK_KHR_SWAPCHAIN_EXTENSION_NAME
        );
    }
    CONJURE_TRY(create_surface(window));

    VkBool32 supported = VK_FALSE;
    const VkResult supportResult = dispatch().vkGetPhysicalDeviceSurfaceSupportKHR(
      mBackend.physical_device(),
      mBackend.graphics_family(),
      mSurface,
      &supported
    );
    if (supportResult != VK_SUCCESS) {
        return swapchain_failure("vkGetPhysicalDeviceSurfaceSupportKHR", supportResult);
    }
    if (supported == VK_FALSE) {
        return core::make_error(
          rhi_error::SWAPCHAIN_CREATION_FAILED,
          "the graphics queue cannot present to this surface"
        );
    }

    CONJURE_TRY_ASSIGN(const VkSurfaceCapabilitiesKHR capabilities, query_surface_capabilities());
    const VkExtent2D extent = resolve_extent(capabilities, mDesc.width, mDesc.height);
    if (extent.width == 0 || extent.height == 0) {
        return empty_surface_error(extent);
    }
    CONJURE_TRY(create_swapchain(VK_NULL_HANDLE, capabilities, extent));

    std::vector<VkSemaphore> acquireSemaphores = {};
    std::vector<VkSemaphore> presentSemaphores = {};
    CONJURE_TRY(create_semaphores(mImages.size(), acquireSemaphores, presentSemaphores));
    mAcquireSemaphores = std::move(acquireSemaphores);
    mPresentSemaphores = std::move(presentSemaphores);
    return core::result<void>::success();
}

core::result<u32> vulkan_swapchain::acquire() {
    if (!is_usable()) {
        return unusable_error();
    }
    if (mImageAcquired) {
        return mCurrentImage;
    }
    const VkSemaphore semaphore = mAcquireSemaphores[mFrameSlot];
    u32 imageIndex = 0;
    const VkResult acquired = dispatch().vkAcquireNextImageKHR(
      mBackend.vk_device(),
      mSwapchain,
      std::numeric_limits<u64>::max(),
      semaphore,
      VK_NULL_HANDLE,
      &imageIndex
    );
    if (is_device_lost_result(acquired)) {
        mBackend.messages().mark_lost();
        return core::make_error(
          rhi_error::DEVICE_LOST,
          "vkAcquireNextImageKHR reported {}",
          describe_vk_result(acquired)
        );
    }
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR || acquired == VK_ERROR_SURFACE_LOST_KHR) {
        return core::make_error(
          rhi_error::OUT_OF_DATE,
          "vkAcquireNextImageKHR reported {}; resize the swapchain",
          describe_vk_result(acquired)
        );
    }
    if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) {
        return swapchain_failure("vkAcquireNextImageKHR", acquired);
    }
    mCurrentImage = imageIndex;
    mImageAcquired = true;
    // The next submission on the graphics queue waits for the image before touching it.
    mBackend.nvrhi_vulkan_device()->queueWaitForSemaphore(nvrhi::CommandQueue::Graphics, semaphore, 0);
    return mCurrentImage;
}

core::result<void> vulkan_swapchain::present() {
    if (!is_usable()) {
        return unusable_error();
    }
    if (!mImageAcquired) {
        CONJURE_TRY(acquire());
    }
    nvrhi::vulkan::IDevice *device = mBackend.nvrhi_vulkan_device();
    const VkSemaphore renderFinished = mPresentSemaphores[mCurrentImage];
    // NVRHI buffers queue semaphores until the next submission; an empty submission flushes them.
    device->queueSignalSemaphore(nvrhi::CommandQueue::Graphics, renderFinished, 0);
    device->executeCommandLists(nullptr, 0, nvrhi::CommandQueue::Graphics);

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinished;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &mSwapchain;
    presentInfo.pImageIndices = &mCurrentImage;

    const VkResult presented = dispatch().vkQueuePresentKHR(mBackend.graphics_queue(), &presentInfo);
    mImageAcquired = false;
    mFrameSlot = (mFrameSlot + 1) % static_cast<u32>(mAcquireSemaphores.size());
    if (is_device_lost_result(presented)) {
        mBackend.messages().mark_lost();
        return core::make_error(rhi_error::DEVICE_LOST, "vkQueuePresentKHR reported {}", describe_vk_result(presented));
    }
    if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_ERROR_SURFACE_LOST_KHR) {
        return core::make_error(
          rhi_error::OUT_OF_DATE,
          "vkQueuePresentKHR reported {}; resize the swapchain",
          describe_vk_result(presented)
        );
    }
    if (presented != VK_SUCCESS && presented != VK_SUBOPTIMAL_KHR) {
        return core::make_error(
          rhi_error::PRESENT_FAILED,
          "vkQueuePresentKHR failed with {}",
          describe_vk_result(presented)
        );
    }
    return core::result<void>::success();
}

core::result<void> vulkan_swapchain::resize(u32 width, u32 height) {
    nvrhi::IDevice *device = mBackend.nvrhi_device();
    // An acquired image is discarded (swapchain.h); its semaphore wait is consumed before the GPU is
    // drained so the semaphore can be destroyed below.
    flush_pending_acquire();
    if (!device->waitForIdle()) {
        mBackend.messages().mark_lost();
        return core::make_error(rhi_error::DEVICE_LOST, "waitForIdle failed before the swapchain resize");
    }

    // A minimised window is the everyday failure: answer OUT_OF_DATE before touching the old swapchain
    // so it stays presentable and is retried, not destroyed.
    CONJURE_TRY_ASSIGN(const VkSurfaceCapabilitiesKHR capabilities, query_surface_capabilities());
    const VkExtent2D extent = resolve_extent(capabilities, width, height);
    if (extent.width == 0 || extent.height == 0) {
        return empty_surface_error(extent);
    }

    release_images();
    device->runGarbageCollection();
    mDesc.width = width;
    mDesc.height = height;

    // The old swapchain goes in as oldSwapchain, which retires it whether or not creation succeeds;
    // it is destroyed exactly once, after the call, and mSwapchain never names a destroyed handle.
    const VkSwapchainKHR previous = mSwapchain;
    mSwapchain = VK_NULL_HANDLE;
    const core::result<void> recreated = create_swapchain(previous, capabilities, extent);
    if (previous != VK_NULL_HANDLE) {
        dispatch().vkDestroySwapchainKHR(mBackend.vk_device(), previous, nullptr);
    }
    CONJURE_TRY(recreated);

    // Build the new semaphore set before the old one goes so a failure leaves a complete set in place;
    // without matching semaphores the new swapchain is unusable, so it is dropped and the caller
    // resizes again.
    std::vector<VkSemaphore> acquireSemaphores = {};
    std::vector<VkSemaphore> presentSemaphores = {};
    const core::result<void> semaphores = create_semaphores(mImages.size(), acquireSemaphores, presentSemaphores);
    if (!semaphores.has_value()) {
        destroy_swapchain();
    }
    CONJURE_TRY(semaphores);
    destroy_semaphores(mAcquireSemaphores, mPresentSemaphores);
    mAcquireSemaphores = std::move(acquireSemaphores);
    mPresentSemaphores = std::move(presentSemaphores);

    mImageAcquired = false;
    mFrameSlot = 0;
    mCurrentImage = 0;
    return core::result<void>::success();
}

nvrhi::ITexture *vulkan_swapchain::back_buffer(u32 index) const noexcept {
    if (index >= mBackBuffers.size()) {
        return nullptr;
    }
    return mBackBuffers[index].Get();
}

u32 vulkan_swapchain::buffer_count() const noexcept {
    return static_cast<u32>(mBackBuffers.size());
}

u32 vulkan_swapchain::current_index() const noexcept {
    return mCurrentImage;
}

const swapchain_desc &vulkan_swapchain::desc() const noexcept {
    return mDesc;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

bool vulkan_swapchain::is_usable() const noexcept {
    return mSwapchain != VK_NULL_HANDLE && !mImages.empty() && mAcquireSemaphores.size() == mImages.size() &&
      mPresentSemaphores.size() == mImages.size();
}

core::result<void> vulkan_swapchain::create_surface(const native_window &window) {
    const char *functionName = surface_function_name(window.kind);
    const create_surface_fn createSurface = reinterpret_cast<create_surface_fn>(
      mBackend.loader().instance_function(mBackend.instance(), functionName)
    );
    if (createSurface == nullptr) {
        return core::make_error(
          rhi_error::SWAPCHAIN_CREATION_FAILED,
          "{} is unavailable; the instance was created without {}",
          functionName,
          surface_extension_for(window.kind)
        );
    }

    VkResult created = VK_ERROR_INITIALIZATION_FAILED;
    switch (window.kind) {
        case native_window_kind::WINDOWS: {
            if (window.window == nullptr) {
                return core::make_error(rhi_error::INVALID_DESC, "vulkan swapchain: the WINDOWS window handle is null");
            }
            win32_surface_create_info info = {};
            info.hinstance = window.display;
            info.hwnd = window.window;
            created = createSurface(mBackend.instance(), &info, nullptr, &mSurface);
            break;
        }
        case native_window_kind::X11: {
            if (window.display == nullptr || window.x11Window == 0) {
                return core::make_error(rhi_error::INVALID_DESC, "vulkan swapchain: the X11 display or window is null");
            }
            xlib_surface_create_info info = {};
            info.dpy = window.display;
            info.window = static_cast<unsigned long>(window.x11Window);
            created = createSurface(mBackend.instance(), &info, nullptr, &mSurface);
            break;
        }
        case native_window_kind::WAYLAND: {
            if (window.display == nullptr || window.window == nullptr) {
                return core::make_error(
                  rhi_error::INVALID_DESC,
                  "vulkan swapchain: the Wayland display or surface is null"
                );
            }
            wayland_surface_create_info info = {};
            info.display = window.display;
            info.surface = window.window;
            created = createSurface(mBackend.instance(), &info, nullptr, &mSurface);
            break;
        }
        case native_window_kind::NONE:
        case native_window_kind::COCOA: {
            return core::make_error(
              rhi_error::INVALID_DESC,
              "vulkan swapchain: unsupported window kind {}",
              to_string(window.kind)
            );
        }
    }
    if (created != VK_SUCCESS) {
        mSurface = VK_NULL_HANDLE;
        return swapchain_failure(functionName, created);
    }
    return core::result<void>::success();
}

core::result<VkSurfaceCapabilitiesKHR> vulkan_swapchain::query_surface_capabilities() const {
    VkSurfaceCapabilitiesKHR capabilities = {};
    const VkResult queried = dispatch().vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      mBackend.physical_device(),
      mSurface,
      &capabilities
    );
    if (queried != VK_SUCCESS) {
        return swapchain_failure("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", queried);
    }
    return capabilities;
}

core::result<void> vulkan_swapchain::create_swapchain(
  VkSwapchainKHR previous,
  const VkSurfaceCapabilitiesKHR &capabilities,
  VkExtent2D extent
) {
    vk::detail::DispatchLoaderDynamic &vulkan = dispatch();
    const VkPhysicalDevice physical = mBackend.physical_device();

    u32 formatCount = 0;
    VkResult formatsResult = vulkan.vkGetPhysicalDeviceSurfaceFormatsKHR(physical, mSurface, &formatCount, nullptr);
    if (formatsResult != VK_SUCCESS) {
        return swapchain_failure("vkGetPhysicalDeviceSurfaceFormatsKHR", formatsResult);
    }
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    formatsResult = vulkan.vkGetPhysicalDeviceSurfaceFormatsKHR(physical, mSurface, &formatCount, formats.data());
    if (formatsResult != VK_SUCCESS) {
        return swapchain_failure("vkGetPhysicalDeviceSurfaceFormatsKHR", formatsResult);
    }
    formats.resize(formatCount);

    const VkFormat wanted = nvrhi::vulkan::convertFormat(to_nvrhi_format(mDesc.format));
    bool formatSupported = false;
    for (const VkSurfaceFormatKHR &candidate : formats) {
        if (candidate.format == wanted && candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            formatSupported = true;
            break;
        }
    }
    if (!formatSupported) {
        return core::make_error(
          rhi_error::SWAPCHAIN_CREATION_FAILED,
          "the surface does not support {} with the sRGB non-linear colour space",
          to_string(mDesc.format)
        );
    }
    mFormat = wanted;
    mColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    u32 presentModeCount = 0;
    std::vector<VkPresentModeKHR> presentModes = {};
    const VkResult modeCountResult =
      vulkan.vkGetPhysicalDeviceSurfacePresentModesKHR(physical, mSurface, &presentModeCount, nullptr);
    if (modeCountResult == VK_SUCCESS) {
        presentModes.resize(presentModeCount);
        const VkResult modesResult =
          vulkan.vkGetPhysicalDeviceSurfacePresentModesKHR(physical, mSurface, &presentModeCount, presentModes.data());
        if (modesResult != VK_SUCCESS) {
            presentModes.clear();
        }
        presentModes.resize(std::min<usize>(presentModeCount, presentModes.size()));
    }
    // FIFO is the one mode every implementation offers and is vsync; without vsync prefer mailbox
    // (no tearing, lowest latency), then immediate.
    mPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (!mDesc.vsync) {
        if (std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != presentModes.end()) {
            mPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        } else if (
          std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != presentModes.end()
        ) {
            mPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }

    u32 imageCount = std::max(mDesc.bufferCount, capabilities.minImageCount);
    if (capabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, capabilities.maxImageCount);
    }

    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if ((capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) == 0) {
        compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        if ((capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) == 0) {
            compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
        }
    }

    // Only COLOR_ATTACHMENT is guaranteed by the specification; TRANSFER_DST (what NVRHI's
    // clearTextureFloat uses on a non-UAV image) is requested only where the surface offers it.
    const VkImageUsageFlags transferDestination = capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (transferDestination == 0) {
        CONJURE_LOG_WARN(
          LOG_RHI,
          "vulkan: the surface offers no TRANSFER_DST usage; clear the back buffers through a render pass"
        );
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = mSurface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = mFormat;
    createInfo.imageColorSpace = mColorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | transferDestination;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = compositeAlpha;
    createInfo.presentMode = mPresentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = previous;

    VkSwapchainKHR created = VK_NULL_HANDLE;
    const VkResult createResult = vulkan.vkCreateSwapchainKHR(mBackend.vk_device(), &createInfo, nullptr, &created);
    if (createResult != VK_SUCCESS) {
        return swapchain_failure("vkCreateSwapchainKHR", createResult);
    }

    // Everything after creation can still fail; the created swapchain is only published as mSwapchain
    // once its images are wrapped, so a failure never leaves a half-initialised chain behind.
    mDesc.width = extent.width;
    mDesc.height = extent.height;
    const core::result<void> wrapped = wrap_images(created);
    if (!wrapped.has_value()) {
        release_images();
        vulkan.vkDestroySwapchainKHR(mBackend.vk_device(), created, nullptr);
    }
    CONJURE_TRY(wrapped);

    mSwapchain = created;
    mDesc.bufferCount = static_cast<u32>(mImages.size());
    return core::result<void>::success();
}

core::result<void> vulkan_swapchain::wrap_images(VkSwapchainKHR swapchain) {
    vk::detail::DispatchLoaderDynamic &vulkan = dispatch();
    u32 actualCount = 0;
    VkResult imagesResult = vulkan.vkGetSwapchainImagesKHR(mBackend.vk_device(), swapchain, &actualCount, nullptr);
    if (imagesResult != VK_SUCCESS) {
        return swapchain_failure("vkGetSwapchainImagesKHR", imagesResult);
    }
    mImages.assign(actualCount, VK_NULL_HANDLE);
    imagesResult = vulkan.vkGetSwapchainImagesKHR(mBackend.vk_device(), swapchain, &actualCount, mImages.data());
    if (imagesResult != VK_SUCCESS) {
        return swapchain_failure("vkGetSwapchainImagesKHR", imagesResult);
    }
    mImages.resize(actualCount);
    if (actualCount == 0) {
        return core::make_error(rhi_error::SWAPCHAIN_CREATION_FAILED, "vkGetSwapchainImagesKHR returned no images");
    }

    mBackBuffers.clear();
    mBackBuffers.reserve(actualCount);
    for (u32 index = 0; index < actualCount; ++index) {
        nvrhi::TextureDesc textureDesc = {};
        textureDesc.width = mDesc.width;
        textureDesc.height = mDesc.height;
        textureDesc.format = to_nvrhi_format(mDesc.format);
        textureDesc.dimension = nvrhi::TextureDimension::Texture2D;
        textureDesc.isRenderTarget = true;
        textureDesc.isShaderResource = false;
        textureDesc.initialState = nvrhi::ResourceStates::Present;
        textureDesc.keepInitialState = true;
        textureDesc.debugName = std::format("swapchain buffer {}", index);
        nvrhi::TextureHandle texture = mBackend.nvrhi_device()->createHandleForNativeTexture(
          nvrhi::ObjectTypes::VK_Image,
          nvrhi::Object(reinterpret_cast<void *>(mImages[index])),
          textureDesc
        );
        if (texture == nullptr) {
            return core::make_error(
              rhi_error::SWAPCHAIN_CREATION_FAILED,
              "nvrhi refused to wrap swapchain image {}",
              index
            );
        }
        mBackBuffers.emplace_back(std::move(texture));
    }
    return core::result<void>::success();
}

core::result<void> vulkan_swapchain::create_semaphores(
  usize count,
  std::vector<VkSemaphore> &acquireSemaphores,
  std::vector<VkSemaphore> &presentSemaphores
) const {
    vk::detail::DispatchLoaderDynamic &vulkan = dispatch();
    VkSemaphoreCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    acquireSemaphores.assign(count, VK_NULL_HANDLE);
    presentSemaphores.assign(count, VK_NULL_HANDLE);
    for (usize index = 0; index < count; ++index) {
        VkResult created = vulkan.vkCreateSemaphore(mBackend.vk_device(), &info, nullptr, &acquireSemaphores[index]);
        if (created != VK_SUCCESS) {
            destroy_semaphores(acquireSemaphores, presentSemaphores);
            return swapchain_failure("vkCreateSemaphore", created);
        }
        created = vulkan.vkCreateSemaphore(mBackend.vk_device(), &info, nullptr, &presentSemaphores[index]);
        if (created != VK_SUCCESS) {
            destroy_semaphores(acquireSemaphores, presentSemaphores);
            return swapchain_failure("vkCreateSemaphore", created);
        }
    }
    return core::result<void>::success();
}

void vulkan_swapchain::destroy_semaphores(
  std::vector<VkSemaphore> &acquireSemaphores,
  std::vector<VkSemaphore> &presentSemaphores
) const noexcept {
    vk::detail::DispatchLoaderDynamic &vulkan = dispatch();
    for (VkSemaphore semaphore : acquireSemaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vulkan.vkDestroySemaphore(mBackend.vk_device(), semaphore, nullptr);
        }
    }
    for (VkSemaphore semaphore : presentSemaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vulkan.vkDestroySemaphore(mBackend.vk_device(), semaphore, nullptr);
        }
    }
    acquireSemaphores.clear();
    presentSemaphores.clear();
}

void vulkan_swapchain::destroy_swapchain() noexcept {
    release_images();
    if (mSwapchain != VK_NULL_HANDLE) {
        dispatch().vkDestroySwapchainKHR(mBackend.vk_device(), mSwapchain, nullptr);
        mSwapchain = VK_NULL_HANDLE;
    }
}

void vulkan_swapchain::flush_pending_acquire() {
    if (!mImageAcquired) {
        return;
    }
    // acquire() handed its semaphore to NVRHI through queueWaitForSemaphore, which NVRHI submits with
    // the next executeCommandLists; an empty submission consumes the wait so the semaphore can be
    // destroyed without leaving a dangling wait on the queue. A lost device accepts no submissions.
    if (!mBackend.messages().is_lost()) {
        mBackend.nvrhi_vulkan_device()->executeCommandLists(nullptr, 0, nvrhi::CommandQueue::Graphics);
    }
    mImageAcquired = false;
}

void vulkan_swapchain::release_images() noexcept {
    mBackBuffers.clear();
    mImages.clear();
}
