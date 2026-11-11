#pragma once

#include "engine/engine_services.h"

#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_subsystem.h"

#include "conjure/core/result.h"
#include "conjure/platform/native_handle.h"
#include "conjure/rhi/backend.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/native_window.h"

#include <vector>

// Startup step 7 (docs/design/06_engine_loop.md 3.4): the graphics device and, for a real window, the
// swapchain. Skipped when rendering.enabled == false (the context pointer stays null). AUTO tries the
// platform default and then the other compiled backend once; the native window handle is copied field by
// field into rhi::native_window because the rhi never includes platform (09 R30).

namespace conjure::engine {

    class rhi_subsystem final : public engine_subsystem {
        private:
            engine_services *mServices = nullptr;
            bool mSkipped = false;

        public:
            explicit rhi_subsystem(engine_services &services);
            ~rhi_subsystem() override;

        public:
            [[nodiscard]] std::string_view name() const noexcept override;
            [[nodiscard]] core::result<void> startup(engine_context &context) override;
            void shutdown(engine_context &context) override;

            /// <summary>
            /// True when rendering was disabled and no device exists.
            /// </summary>
            [[nodiscard]] bool is_skipped() const noexcept {
                return mSkipped;
            }

            /// <summary>
            /// The backends to try in order for a choice: the explicit one, or for AUTO the platform default
            /// followed by the other compiled real backend.
            /// </summary>
            [[nodiscard]] static std::vector<rhi::graphics_backend> candidate_backends(rhi_backend_choice choice);

            /// <summary>
            /// The rhi copy of a platform window handle.
            /// </summary>
            [[nodiscard]] static rhi::native_window to_native_window(
              const platform::native_window_handle &handle
            ) noexcept;
    };
}
