#pragma once

#include "engine/engine_services.h"

#include "conjure/engine/engine_subsystem.h"

#include "conjure/core/result.h"
#include "conjure/platform/window.h"

// Startup step 6 (docs/design/06_engine_loop.md 3.4): the window and the input state machine. Headless
// picks platform::backend_kind::NULL_BACKEND and the window_settings size (1280 x 720 under
// headless_defaults(), R33); everything else maps window_settings onto window_desc the way
// tests/platform/test_backend_selection.cpp shows. The frame loop pumps the events itself (frame step 3)
// because it owns the layer dispatch, the swapchain resize and the pacer focus flag.

namespace conjure::engine {

    class platform_subsystem final : public engine_subsystem {
        private:
            engine_services *mServices = nullptr;

        public:
            explicit platform_subsystem(engine_services &services);
            ~platform_subsystem() override;

        public:
            [[nodiscard]] std::string_view name() const noexcept override;
            [[nodiscard]] core::result<void> startup(engine_context &context) override;
            void shutdown(engine_context &context) override;

            /// <summary>
            /// The window_desc the engine derives from its configuration (headless picks the null backend).
            /// </summary>
            [[nodiscard]] static platform::window_desc make_window_desc(const engine_config &config);
    };
}
