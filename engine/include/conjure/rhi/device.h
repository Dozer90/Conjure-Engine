#pragma once

#include "conjure/rhi/backend.h"
#include "conjure/rhi/native_window.h"
#include "conjure/rhi/null_device_stats.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <memory>
#include <optional>

// The graphics device (docs/design/02_renderer.md 4.1, R15). A device owns one nvrhi::IDevice
// created over the requested backend; conjure_renderer records into it through native_device().
// NVRHI itself stays in engine/src: this header forward-declares nvrhi::IDevice only.
//
// Threading: creation, destruction, run_garbage_collection and wait_idle belong to the render
// thread (the main thread in v1). is_lost() may be read from any thread.

namespace nvrhi {
    class IDevice;
}

namespace conjure::rhi {

    /// <summary>
    /// A failure the null backend can be told to report once, so the engine's error paths run in
    /// CTest without a GPU: WAIT_IDLE makes the next waitForIdle fail (wait_idle() then marks the
    /// device lost), PRESENT makes the next null swapchain present report device loss.
    /// ACQUIRE_OUT_OF_DATE makes the next null swapchain acquire fail with rhi_error::OUT_OF_DATE and
    /// PRESENT_FAILED the next present with rhi_error::PRESENT_FAILED; neither marks the device lost
    /// or advances the back-buffer index, the way a real swapchain refuses without device loss.
    /// </summary>
    enum class null_fault : u8 {
        NONE = 0,
        WAIT_IDLE,
        PRESENT,
        ACQUIRE_OUT_OF_DATE,
        PRESENT_FAILED
    };

    struct device_desc {
        graphics_backend backend = graphics_backend::NULL_DEVICE;
        bool enableDebugLayer = false;          // D3D12 debug layer / Vulkan validation layers + NVRHI validation
        bool enableGpuValidation = false;       // D3D12 GPU-based validation (needs enableDebugLayer)
        u32 adapterIndex = 0;                   // index among the hardware adapters, highest performance first
        bool preferSoftwareAdapter = false;     // WARP (D3D12) / a CPU ICD (Vulkan); golden tests and CI
        native_window window = {};              // Vulkan: selects the surface instance extensions; NONE = headless
    };

    class CONJURE_API device {
        private:
            struct impl;
            struct create_tag {};

            std::unique_ptr<impl> mImpl = nullptr;

            // The swapchain reaches the backend through the private state (engine/src/rhi/device_impl.h).
            friend class swapchain;

        public:
            explicit device(create_tag);
            ~device();

            device(const device &) = delete;
            device &operator=(const device &) = delete;

        public:
            /// <summary>
            /// Creates the NVRHI device for the requested backend. Fails with core::error_code::UNSUPPORTED
            /// when the backend is not compiled in, rhi_error::INVALID_DESC when the description is
            /// inconsistent, and rhi_error::DEVICE_CREATION_FAILED when the API refuses (its message is
            /// quoted).
            /// </summary>
            [[nodiscard]] static core::result<std::unique_ptr<device>> create(const device_desc &desc);

            /// <summary>
            /// The backend this device runs on.
            /// </summary>
            [[nodiscard]] graphics_backend backend() const noexcept;

            /// <summary>
            /// The description the device was created from.
            /// </summary>
            [[nodiscard]] const device_desc &desc() const noexcept;

            /// <summary>
            /// Adapter name, limits and queue availability of the created device.
            /// </summary>
            [[nodiscard]] const device_capabilities &capabilities() const noexcept;

            /// <summary>
            /// True once the API reported device removal or NVRHI raised a Fatal message. Every later
            /// present fails with rhi_error::DEVICE_LOST; the owner must recreate everything.
            /// </summary>
            [[nodiscard]] bool is_lost() const noexcept;

            /// <summary>
            /// Non-owning NVRHI device for in-engine modules. Never exposed to game code.
            /// </summary>
            [[nodiscard]] nvrhi::IDevice *native_device() const noexcept;

            /// <summary>
            /// Releases the resources of command lists that finished executing. Call once per frame.
            /// </summary>
            void run_garbage_collection();

            /// <summary>
            /// Blocks until the GPU is idle. A failed wait marks the device lost.
            /// </summary>
            void wait_idle();

            /// <summary>
            /// Snapshot of what the null backend recorded; nullopt on D3D12 and Vulkan.
            /// </summary>
            [[nodiscard]] std::optional<null_device_stats> null_stats() const;

            /// <summary>
            /// Resets the null backend's counters; no effect on other backends.
            /// </summary>
            void reset_null_stats();

            /// <summary>
            /// Behaves as if the API reported device removal: routes a Fatal message through the
            /// message callback so is_lost() becomes true. Exists for the engine's device-loss tests
            /// on every backend.
            /// </summary>
            void simulate_device_loss();

            /// <summary>
            /// Arms one failure on the null backend (null_fault); the next matching call reports it and
            /// disarms. NONE disarms without a failure. No effect on D3D12 and Vulkan.
            /// </summary>
            void inject_null_fault(null_fault fault);
    };
}
