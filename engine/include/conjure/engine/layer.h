#pragma once

#include "conjure/engine/engine_context.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/platform/event.h"

#include <string>
#include <string_view>

// Layers (docs/design/06_engine_loop.md 2.8): hooks for executables (editor workspace, ImGui bracket,
// debug overlay, game bootstrap). Game logic lives in ECS systems and scripts, never in layers. Events
// travel from the top (overlays) downwards and stop at the first layer returning true; the frame hooks
// travel bottom-up.

namespace conjure::engine {

    class CONJURE_API layer {
        private:
            std::string mName = "";

        public:
            explicit layer(std::string name);
            virtual ~layer();

            layer(const layer &) = delete;
            layer &operator=(const layer &) = delete;

        public:
            [[nodiscard]] std::string_view name() const noexcept {
                return mName;
            }

            /// <summary>
            /// Startup step 19 (or layer_stack::apply_pending for a layer pushed later). A failure fails
            /// initialise() with LAYER_ATTACH_FAILED.
            /// </summary>
            [[nodiscard]] virtual core::result<void> on_attach(engine_context &context);

            /// <summary>
            /// Shutdown step 3 (reverse stack order) or the pop that removes the layer.
            /// </summary>
            virtual void on_detach(engine_context &context);

            /// <summary>
            /// Frame step 5: after the input snapshot, before FRAME_BEGIN.
            /// </summary>
            virtual void on_frame_begin(engine_context &context);

            /// <summary>
            /// Frame step 7: after UPDATE, before LATE_UPDATE (skipped systems aside, also while PAUSED).
            /// </summary>
            virtual void on_update(engine_context &context);

            /// <summary>
            /// Frame step 10: after SYNC, before EXTRACT, with the ImGui frame open (editor builds).
            /// </summary>
            virtual void on_imgui(engine_context &context);

            /// <summary>
            /// Frame step 10, after every layer's on_imgui: imgui_layer ends the ImGui frame here and hands
            /// the draw data to the renderer, so the editor panels drawn in on_imgui are complete.
            /// </summary>
            virtual void on_imgui_end(engine_context &context);

            /// <summary>
            /// Frame step 13: after FRAME_END.
            /// </summary>
            virtual void on_frame_end(engine_context &context);

            /// <summary>
            /// Frame step 3, top-down. Return true to consume the event (it then never reaches the input
            /// state or the layers below).
            /// </summary>
            [[nodiscard]] virtual bool on_event(engine_context &context, const platform::event &event);
    };
}
