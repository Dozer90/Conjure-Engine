#pragma once

#include <nvrhi/nvrhi.h>

#include <atomic>

// Routes NVRHI messages to LOG_RHI (docs/design/02_renderer.md 6): Info -> INFO, Warning -> WARN,
// Error -> ERROR, Fatal -> ERROR plus the lost flag that device::is_lost() reads. One instance per
// device; it must outlive the NVRHI device it was handed to.

namespace conjure::rhi {

    class message_callback final : public nvrhi::IMessageCallback {
        private:
            std::atomic<bool> mLost = false;

        public:
            message_callback();
            ~message_callback() override;

        public:
            /// <summary>
            /// NVRHI entry point; safe from any thread.
            /// </summary>
            void message(nvrhi::MessageSeverity severity, const char *messageText) override;

            /// <summary>
            /// True once a Fatal message arrived or mark_lost() was called.
            /// </summary>
            [[nodiscard]] bool is_lost() const noexcept {
                return mLost.load(std::memory_order_acquire);
            }

            /// <summary>
            /// Records device removal detected by a backend (DXGI_ERROR_DEVICE_REMOVED, VK_ERROR_DEVICE_LOST).
            /// </summary>
            void mark_lost() noexcept {
                mLost.store(true, std::memory_order_release);
            }
    };
}
