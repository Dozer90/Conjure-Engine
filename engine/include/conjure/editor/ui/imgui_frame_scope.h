#pragma once

#include "conjure/core/api.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"

// A self-contained ImGui frame for code that runs without the engine's imgui_layer: the UI tests construct
// every panel against the null RHI and draw one frame inside this scope. When an ImGui context already exists
// (the engine's), the scope only brackets frames on it; otherwise it creates and later destroys one with the
// default font atlas built and docking enabled. Nothing is rendered.

struct ImGuiContext;

namespace conjure::editor::ui {

    class CONJURE_API imgui_frame_scope {
        private:
            ImGuiContext *mContext = nullptr;
            ImGuiContext *mPrevious = nullptr;
            math::vec2 mDisplaySize = math::vec2(1280.0f, 720.0f);
            u32 mFrames = 0;
            bool mOwnsContext = false;
            bool mFrameOpen = false;

        public:
            explicit imgui_frame_scope(math::vec2 displaySize = math::vec2(1280.0f, 720.0f));
            ~imgui_frame_scope();

            imgui_frame_scope(const imgui_frame_scope &) = delete;
            imgui_frame_scope &operator=(const imgui_frame_scope &) = delete;

        public:
            /// <summary>
            /// ImGui::NewFrame with the display size and a fixed 1/60 s delta.
            /// </summary>
            void begin_frame();

            /// <summary>
            /// ImGui::Render (the draw data is discarded).
            /// </summary>
            void end_frame();

            /// <summary>
            /// Simulates a mouse position and button state for the frame about to begin.
            /// </summary>
            void set_mouse(const math::vec2 &position, bool leftDown);

            [[nodiscard]] bool is_frame_open() const noexcept {
                return mFrameOpen;
            }

            [[nodiscard]] bool owns_context() const noexcept {
                return mOwnsContext;
            }

            [[nodiscard]] u32 frame_count() const noexcept {
                return mFrames;
            }

            [[nodiscard]] ImGuiContext *context() const noexcept {
                return mContext;
            }
    };
}
