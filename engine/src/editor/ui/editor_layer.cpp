#include "conjure/editor/ui/editor_layer.h"

#include "editor/ui/ui_support.h"

#include "conjure/editor/log_category.h"
#include "conjure/editor/ui/editor_icon.h"

#include "conjure/engine/engine_context.h"
#include "conjure/platform/platform_error.h"
#include "conjure/platform/window.h"

#include <utility>

namespace conjure::editor::ui {

    namespace {
        // The icon is cosmetic, so it never fails the attach. FEATURE_UNAVAILABLE is the normal state on macOS
        // and Wayland, where the window shows the bundle's or the desktop file's icon: an INFO line, not a WARN.
        void apply_window_icon(engine::engine_context &engineContext) {
            if (engineContext.window == nullptr) {
                return;
            }
            const core::result<void> applied = apply_editor_icon(*engineContext.window);
            if (applied.has_value()) {
                return;
            }
            if (applied.get_error().is(platform::platform_error::FEATURE_UNAVAILABLE)) {
                CONJURE_LOG_INFO(
                  LOG_EDITOR,
                  "window icon left to the application bundle or desktop file ({})",
                  applied.get_error().message()
                );
                return;
            }
            CONJURE_LOG_WARN(LOG_EDITOR, "window icon not set: {}", applied.get_error().to_string());
        }
    }

    editor_layer::editor_layer(editor_context &context) :
        layer("editor-ui"),
        mContext(&context) {}

    editor_layer::~editor_layer() = default;

    core::result<void> editor_layer::on_attach(engine::engine_context &engineContext) {
        apply_window_icon(engineContext);
        mWorkspace = std::make_unique<editor_workspace>();
        if (mContext->project() != nullptr) {
            mWorkspace->set_layout_file(mContext->project()->root_directory() / ".conjure" / "editor_layout.ini");
        }
        mWorkspace->create_default_panels(*mContext);
        CONJURE_LOG_INFO(LOG_EDITOR, "editor ui attached with {} panel(s)", mWorkspace->open_panel_count());
        return core::result<void>::success();
    }

    void editor_layer::on_detach(engine::engine_context &engineContext) {
        (void) engineContext;
        if (mWorkspace != nullptr) {
            mWorkspace->save_layout();
            mWorkspace.reset();
        }
    }

    void editor_layer::on_update(engine::engine_context &engineContext) {
        (void) engineContext;
        if (mWorkspace != nullptr) {
            mWorkspace->update(*mContext);
        }
    }

    void editor_layer::on_imgui(engine::engine_context &engineContext) {
        (void) engineContext;
        if (mWorkspace == nullptr || !support::frame_is_open()) {
            return;
        }
        mWorkspace->draw(*mContext);
        ++mFramesDrawn;
    }

    bool editor_layer::on_event(engine::engine_context &engineContext, const platform::event &event) {
        (void) engineContext;
        (void) event;
        // The engine's imgui_layer consumes what ImGui wants; the panels read ImGui's own input state.
        return false;
    }

    std::unique_ptr<engine::layer> create_editor_layer(editor_context &context) {
        return std::make_unique<editor_layer>(context);
    }
}
