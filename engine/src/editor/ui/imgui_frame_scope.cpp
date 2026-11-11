#include "conjure/editor/ui/imgui_frame_scope.h"

#include <imgui.h>

namespace conjure::editor::ui {

    imgui_frame_scope::imgui_frame_scope(math::vec2 displaySize) :
        mDisplaySize(displaySize) {
        mPrevious = ImGui::GetCurrentContext();
        if (mPrevious != nullptr) {
            mContext = mPrevious;
            return;
        }
        mContext = ImGui::CreateContext();
        mOwnsContext = true;
        ImGui::SetCurrentContext(mContext);
        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = ImVec2(mDisplaySize.x, mDisplaySize.y);
        io.DeltaTime = 1.0f / 60.0f;
        io.Fonts->AddFontDefault();
        if (!io.Fonts->Build()) {
            io.Fonts->Clear();
            io.Fonts->AddFontDefault();
            (void) io.Fonts->Build();
        }
    }

    imgui_frame_scope::~imgui_frame_scope() {
        if (mFrameOpen) {
            end_frame();
        }
        if (mOwnsContext) {
            ImGui::DestroyContext(mContext);
            ImGui::SetCurrentContext(mPrevious);
        }
    }

    void imgui_frame_scope::begin_frame() {
        if (mFrameOpen) {
            return;
        }
        ImGui::SetCurrentContext(mContext);
        ImGuiIO &io = ImGui::GetIO();
        if (mOwnsContext) {
            io.DisplaySize = ImVec2(mDisplaySize.x, mDisplaySize.y);
            io.DeltaTime = 1.0f / 60.0f;
        }
        ImGui::NewFrame();
        mFrameOpen = true;
    }

    void imgui_frame_scope::end_frame() {
        if (!mFrameOpen) {
            return;
        }
        ImGui::SetCurrentContext(mContext);
        // Render() ends the frame and builds draw lists nobody consumes; the layout state is what matters.
        ImGui::Render();
        mFrameOpen = false;
        ++mFrames;
    }

    void imgui_frame_scope::set_mouse(const math::vec2 &position, bool leftDown) {
        ImGui::SetCurrentContext(mContext);
        ImGuiIO &io = ImGui::GetIO();
        io.AddMousePosEvent(position.x, position.y);
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, leftDown);
    }
}
