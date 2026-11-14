#pragma once

#include "rhi/device_backend.h"

#include "conjure/rhi/backend.h"
#include "conjure/rhi/device.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <nvrhi/d3d12.h>
#include <nvrhi/nvrhi.h>

#include <dxgi1_6.h>
#include <wrl/client.h>

#include <memory>

// The D3D12 backend (docs/design/02_renderer.md 9 step 3): DXGI factory, adapter selection
// (hardware adapters by GPU preference or WARP when preferSoftwareAdapter), ID3D12Device with
// the optional debug layer, one queue per NVRHI queue type and nvrhi::d3d12::createDevice over
// them. Device removal is read from ID3D12Device::GetDeviceRemovedReason. With the debug layer on, its
// messages are routed to LOG_RHI (ID3D12InfoQueue1, Windows 11) so a validation error is in the engine log
// and fails a test's log_capture instead of only reaching an attached debugger.

namespace conjure::rhi {
    class message_callback;
}

namespace conjure::rhi::d3d12_backend {

    class d3d12_device_backend final : public device_backend {
        private:
            message_callback &mMessages;   // set by the only constructor; device::impl owns it and outlives the backend
            device_capabilities mCapabilities = {};
            Microsoft::WRL::ComPtr<IDXGIFactory4> mFactory = nullptr;
            Microsoft::WRL::ComPtr<IDXGIAdapter1> mAdapter = nullptr;
            Microsoft::WRL::ComPtr<ID3D12Device> mDevice = nullptr;
            Microsoft::WRL::ComPtr<ID3D12CommandQueue> mGraphicsQueue = nullptr;
            Microsoft::WRL::ComPtr<ID3D12CommandQueue> mComputeQueue = nullptr;
            Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCopyQueue = nullptr;
            nvrhi::d3d12::DeviceHandle mNvrhiDevice = nullptr;
            // flush_queues(): a fence signalled on the graphics queue behind whatever DXGI queued there.
            Microsoft::WRL::ComPtr<ID3D12Fence> mFlushFence = nullptr;
            void *mFlushEvent = nullptr;            // the Win32 event the fence completes
            u64 mFlushValue = 0;                    // the last value signalled on mFlushFence
            bool mAllowTearing = false;
            bool mDebugLayerInherited = false;      // the layer is on because an earlier device enabled it

        public:
            explicit d3d12_device_backend(message_callback &messages);
            ~d3d12_device_backend() override;

        public:
            /// <summary>
            /// Creates the API objects. Fails with rhi_error::DEVICE_CREATION_FAILED quoting the HRESULT.
            /// </summary>
            [[nodiscard]] core::result<void> initialise(const device_desc &desc);

            [[nodiscard]] nvrhi::IDevice *nvrhi_device() const noexcept override;
            [[nodiscard]] const device_capabilities &capabilities() const noexcept override;
            [[nodiscard]] core::result<std::unique_ptr<swapchain_backend>> create_swapchain(
              const native_window &window,
              const swapchain_desc &desc
            ) override;
            [[nodiscard]] bool poll_device_removed() noexcept override;

            /// <summary>
            /// Signals a fence on the graphics queue and waits for it, so every command list and every DXGI
            /// present queued so far has finished. A swapchain's buffers may only be released (destruction,
            /// ResizeBuffers) after this: nvrhi's waitForIdle stops at the last command list NVRHI executed
            /// and never sees the present behind it. Returns at once on a removed device.
            /// </summary>
            void flush_queues() noexcept override;
            [[nodiscard]] bool debug_layer_inherited() const noexcept override;

            [[nodiscard]] IDXGIFactory4 *factory() const noexcept {
                return mFactory.Get();
            }
            [[nodiscard]] ID3D12Device *d3d_device() const noexcept {
                return mDevice.Get();
            }
            [[nodiscard]] ID3D12CommandQueue *graphics_queue() const noexcept {
                return mGraphicsQueue.Get();
            }
            [[nodiscard]] bool allow_tearing() const noexcept {
                return mAllowTearing;
            }
            [[nodiscard]] message_callback &messages() const noexcept {
                return mMessages;
            }

        private:
            [[nodiscard]] core::result<void> select_adapter(const device_desc &desc);
            [[nodiscard]] core::result<void> create_queues();
            [[nodiscard]] core::result<void> create_flush_fence();
            void route_debug_messages();
            void release_flush_fence() noexcept;
    };

    /// <summary>
    /// Creates the D3D12 backend for the description.
    /// </summary>
    [[nodiscard]] core::result<std::unique_ptr<device_backend>> create_d3d12_backend(
      const device_desc &desc,
      message_callback &messages
    );

    /// <summary>
    /// "0x8007000E (E_OUTOFMEMORY)"-style text for error messages.
    /// </summary>
    [[nodiscard]] std::string describe_hresult(long result);
}
