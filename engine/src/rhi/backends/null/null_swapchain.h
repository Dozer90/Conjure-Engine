#pragma once

#include "rhi/device_backend.h"

#include "conjure/rhi/swapchain.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <nvrhi/nvrhi.h>

#include <vector>

// The null swapchain (docs/design/02_renderer.md 7): bufferCount null textures in the Present
// state; acquire() returns the current index and present() advances it, so acquire/present pairs
// cycle 0, 1, ..., bufferCount - 1, 0. Every call is counted in null_device_stats. An armed
// null_fault::PRESENT makes one present() report device loss the way a real backend does, while
// null_fault::ACQUIRE_OUT_OF_DATE and null_fault::PRESENT_FAILED make one acquire() or present()
// refuse with OUT_OF_DATE or PRESENT_FAILED and leave the device healthy and the index where it was.
// resize() refuses with SWAPCHAIN_CREATION_FAILED while a back buffer is referenced outside the
// swapchain, as IDXGISwapChain::ResizeBuffers does (the chain is left as it was).

namespace conjure::rhi::null_backend {

    class null_device;

    class null_swapchain final : public swapchain_backend {
        private:
            null_device &mDevice;   // set by the only constructor; the device backend outlives its swapchains
            swapchain_desc mDesc = {};
            std::vector<nvrhi::TextureHandle> mBackBuffers = {};
            u32 mCurrentIndex = 0;

        public:
            null_swapchain(null_device &device, const swapchain_desc &desc);
            ~null_swapchain() override;

        public:
            [[nodiscard]] core::result<u32> acquire() override;
            [[nodiscard]] core::result<void> present() override;
            [[nodiscard]] core::result<void> resize(u32 width, u32 height) override;
            [[nodiscard]] nvrhi::ITexture *back_buffer(u32 index) const noexcept override;
            [[nodiscard]] u32 buffer_count() const noexcept override;
            [[nodiscard]] u32 current_index() const noexcept override;
            [[nodiscard]] const swapchain_desc &desc() const noexcept override;

        private:
            void create_back_buffers();
    };
}
