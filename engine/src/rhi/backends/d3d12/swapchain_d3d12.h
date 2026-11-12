#pragma once

#include "rhi/device_backend.h"

#include "conjure/rhi/native_window.h"
#include "conjure/rhi/swapchain.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <nvrhi/nvrhi.h>

#include <dxgi1_6.h>
#include <wrl/client.h>

#include <vector>

// The D3D12 swapchain: a flip-model IDXGISwapChain3 created for the HWND (or for composition when
// no window is given, which never shows anything but exercises the whole present path), with each
// buffer wrapped by nvrhi::IDevice::createHandleForNativeTexture in the Present state. Flip model
// forbids sRGB buffer formats, so the buffers are created UNORM and the NVRHI wrapper carries the
// sRGB format the renderer sees.

namespace conjure::rhi::d3d12_backend {

    class d3d12_device_backend;

    class d3d12_swapchain final : public swapchain_backend {
        private:
            d3d12_device_backend &mBackend;   // set by the only constructor; the backend outlives its swapchains
            swapchain_desc mDesc = {};
            Microsoft::WRL::ComPtr<IDXGISwapChain3> mSwapchain = nullptr;
            std::vector<nvrhi::TextureHandle> mBackBuffers = {};
            u32 mCurrentIndex = 0;
            UINT mFlags = 0;

        public:
            d3d12_swapchain(d3d12_device_backend &backend, const swapchain_desc &desc);
            ~d3d12_swapchain() override;

        public:
            /// <summary>
            /// Creates the DXGI swapchain and wraps its buffers. Fails with rhi_error::INVALID_DESC for a
            /// window kind other than WINDOWS or NONE and rhi_error::SWAPCHAIN_CREATION_FAILED otherwise.
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
            // True when every buffer of the description is wrapped; false after a failed resize().
            [[nodiscard]] bool has_back_buffers() const noexcept;
            [[nodiscard]] core::result<void> wrap_back_buffers();
            void release_back_buffers();
    };
}
