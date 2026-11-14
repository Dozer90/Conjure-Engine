#pragma once

#include "conjure/rhi/backend.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/native_window.h"
#include "conjure/rhi/swapchain.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <nvrhi/nvrhi.h>

#include <memory>

// The interface every backend (null, d3d12, vulkan) implements behind device and swapchain
// (docs/design/02_renderer.md 4.1, 4.3). A device_backend owns the API objects (adapter, queues,
// VkInstance, ...) and the nvrhi::IDevice built over them; a swapchain_backend owns the API
// swapchain and the NVRHI textures wrapping its buffers. Both are private to engine/src/rhi.

namespace conjure::rhi {

    class message_callback;

    namespace null_backend {
        class null_device;
    }

    class swapchain_backend {
        public:
            swapchain_backend();
            virtual ~swapchain_backend();

            swapchain_backend(const swapchain_backend &) = delete;
            swapchain_backend &operator=(const swapchain_backend &) = delete;

        public:
            [[nodiscard]] virtual core::result<u32> acquire() = 0;
            [[nodiscard]] virtual core::result<void> present() = 0;
            [[nodiscard]] virtual core::result<void> resize(u32 width, u32 height) = 0;
            [[nodiscard]] virtual nvrhi::ITexture *back_buffer(u32 index) const noexcept = 0;
            [[nodiscard]] virtual u32 buffer_count() const noexcept = 0;
            [[nodiscard]] virtual u32 current_index() const noexcept = 0;
            [[nodiscard]] virtual const swapchain_desc &desc() const noexcept = 0;
    };

    class device_backend {
        public:
            device_backend();
            virtual ~device_backend();

            device_backend(const device_backend &) = delete;
            device_backend &operator=(const device_backend &) = delete;

        public:
            /// <summary>
            /// The NVRHI device built over the API objects (not the validation wrapper).
            /// </summary>
            [[nodiscard]] virtual nvrhi::IDevice *nvrhi_device() const noexcept = 0;

            /// <summary>
            /// Adapter name, limits and queues the backend discovered at creation.
            /// </summary>
            [[nodiscard]] virtual const device_capabilities &capabilities() const noexcept = 0;

            /// <summary>
            /// Creates the API swapchain. The description was validated by swapchain::create.
            /// </summary>
            [[nodiscard]] virtual core::result<std::unique_ptr<swapchain_backend>> create_swapchain(
              const native_window &window,
              const swapchain_desc &desc
            ) = 0;

            /// <summary>
            /// Asks the API whether the device was removed (GetDeviceRemovedReason, VK_ERROR_DEVICE_LOST
            /// seen by a queue operation). The null backend never is.
            /// </summary>
            [[nodiscard]] virtual bool poll_device_removed() noexcept = 0;

            /// <summary>
            /// Blocks until the work the API queued outside NVRHI has left the queues. nvrhi's waitForIdle
            /// only covers the command lists NVRHI executed itself, so a D3D12 backend waits here for the
            /// DXGI present that followed them; device::wait_idle() calls it after waitForIdle. The default
            /// does nothing (null: no queue; Vulkan: vkDeviceWaitIdle already covers every queue). Never
            /// blocks on a removed device.
            /// </summary>
            virtual void flush_queues() noexcept;

            /// <summary>
            /// True when the device runs under an API debug layer it did not ask for. The D3D12 layer is
            /// process-wide: once a device enabled it, every later device of the process has it too.
            /// </summary>
            [[nodiscard]] virtual bool debug_layer_inherited() const noexcept;

            /// <summary>
            /// This backend as the null device, or nullptr.
            /// </summary>
            [[nodiscard]] virtual null_backend::null_device *as_null() noexcept;
    };

    /// <summary>
    /// Validates the swapchain description shared by every backend: non-zero size, 2..16 buffers and
    /// a format the swap effect can present. Returns rhi_error::INVALID_DESC naming the field.
    /// </summary>
    [[nodiscard]] core::result<void> validate_swapchain_desc(const swapchain_desc &desc);

    /// <summary>
    /// True for the formats a swapchain may be created with (RGBA8_UNORM, RGBA8_SRGB, BGRA8_SRGB,
    /// RGBA16_FLOAT).
    /// </summary>
    [[nodiscard]] bool is_presentable_format(texture_format format) noexcept;
}
