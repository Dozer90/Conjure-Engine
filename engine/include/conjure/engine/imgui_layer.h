#pragma once

#include "conjure/core/platform_detect.h"

// The Dear ImGui context owner (docs/design/06_engine_loop.md 2.8, R9): compiled only in editor builds
// (CONJURE_BUILD_EDITOR, never in Distribution); the header is empty otherwise so it still compiles alone.
// The layer creates the single ImGui context, feeds it the platform events, opens the frame in
// on_frame_begin (ImGui::NewFrame) and ends it in on_imgui_end (ImGui::Render +
// renderer::set_imgui_draw_data). renderer::imgui_renderer only draws; no module calls SetCurrentContext.

#if defined(CONJURE_BUILD_EDITOR) && CONJURE_BUILD_EDITOR

#include "conjure/engine/engine_context.h"
#include "conjure/engine/layer.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/platform/event.h"

#include <string>
#include <string_view>

struct ImGuiContext;

namespace conjure::engine {

    /// <summary>
    /// Counters of what the layer forwarded; read by the editor's diagnostics and the tests.
    /// </summary>
    struct imgui_layer_stats {
        u64 framesBegun = 0;
        u64 framesRendered = 0;
        u64 eventsForwarded = 0;
        u64 eventsConsumed = 0;

        [[nodiscard]] bool operator==(const imgui_layer_stats &) const = default;
    };

    class CONJURE_API imgui_layer final : public layer {
        private:
            ImGuiContext *mContext = nullptr;
            platform::window *mWindow = nullptr;        // clipboard and display size source (never owned)
            imgui_layer_stats mStats = {};
            std::string mClipboard = {};
            bool mFrameOpen = false;

        public:
            imgui_layer();
            ~imgui_layer() override;

        public:
            /// <summary>
            /// Creates the context, builds the default font atlas, sizes the display from the window and
            /// wires the clipboard to the platform window.
            /// </summary>
            [[nodiscard]] core::result<void> on_attach(engine_context &context) override;

            /// <summary>
            /// Destroys the context.
            /// </summary>
            void on_detach(engine_context &context) override;

            /// <summary>
            /// Updates display size, framebuffer scale and delta time, then ImGui::NewFrame().
            /// </summary>
            void on_frame_begin(engine_context &context) override;

            /// <summary>
            /// ImGui::Render() and renderer::set_imgui_draw_data (when a renderer exists).
            /// </summary>
            void on_imgui_end(engine_context &context) override;

            /// <summary>
            /// Forwards keys, text, mouse, scroll and focus to the ImGui IO. Mouse events are consumed
            /// while ImGui wants the mouse, key and text events while it wants the keyboard; window events
            /// are never consumed.
            /// </summary>
            [[nodiscard]] bool on_event(engine_context &context, const platform::event &event) override;

            /// <summary>
            /// The context this layer owns (nullptr before on_attach / after on_detach).
            /// </summary>
            [[nodiscard]] ImGuiContext *context() const noexcept {
                return mContext;
            }

            /// <summary>
            /// True between on_frame_begin and on_imgui_end of the current frame.
            /// </summary>
            [[nodiscard]] bool is_frame_open() const noexcept {
                return mFrameOpen;
            }

            [[nodiscard]] const imgui_layer_stats &stats() const noexcept {
                return mStats;
            }

            /// <summary>
            /// The platform clipboard as ImGui reads it (the string stays valid until the next call).
            /// </summary>
            [[nodiscard]] const char *clipboard_text();

            /// <summary>
            /// Writes the platform clipboard (ImGui's copy path).
            /// </summary>
            void set_clipboard_text(std::string_view text);
    };
}

#endif
