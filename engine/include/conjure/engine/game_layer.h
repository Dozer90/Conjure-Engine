#pragma once

#include "conjure/engine/engine_context.h"
#include "conjure/engine/layer.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/platform/event.h"

// The game bootstrap layer (docs/design/06_engine_loop.md 4.2): pushed by conjure_runtime. Scene loading
// is startup step 20, so on_attach does nothing; the one job is to quit on the window's close event.

namespace conjure::engine {

    class CONJURE_API game_layer final : public layer {
        private:
            u32 mCloseRequests = 0;

        public:
            game_layer();
            ~game_layer() override;

        public:
            [[nodiscard]] core::result<void> on_attach(engine_context &context) override;

            /// <summary>
            /// A close_event requests exit_code::OK through the application; nothing else is consumed.
            /// </summary>
            [[nodiscard]] bool on_event(engine_context &context, const platform::event &event) override;

            /// <summary>
            /// Close events seen so far.
            /// </summary>
            [[nodiscard]] u32 close_requests() const noexcept {
                return mCloseRequests;
            }
    };
}
