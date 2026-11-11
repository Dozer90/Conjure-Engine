#pragma once

#include "conjure/editor/editor_context.h"
#include "conjure/editor/ui/editor_workspace.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/engine/engine_context.h"
#include "conjure/engine/layer.h"
#include "conjure/platform/event.h"

#include <memory>

// The engine layer that draws the editor (docs/design/04_editor.md 3.2, 5): on_update refreshes the panels'
// caches, on_imgui draws the workspace inside the frame the engine's imgui_layer opened. It is pushed after
// imgui_layer by the executable through editor_application's layer factory.

namespace conjure::editor::ui {

    class CONJURE_API editor_layer final : public engine::layer {
        private:
            editor_context *mContext = nullptr;
            std::unique_ptr<editor_workspace> mWorkspace = nullptr;
            u32 mFramesDrawn = 0;

        public:
            explicit editor_layer(editor_context &context);
            ~editor_layer() override;

        public:
            [[nodiscard]] core::result<void> on_attach(engine::engine_context &engineContext) override;
            void on_detach(engine::engine_context &engineContext) override;
            void on_update(engine::engine_context &engineContext) override;
            void on_imgui(engine::engine_context &engineContext) override;
            [[nodiscard]] bool on_event(engine::engine_context &engineContext, const platform::event &event) override;

            [[nodiscard]] editor_workspace *workspace() noexcept {
                return mWorkspace.get();
            }

            [[nodiscard]] u32 frames_drawn() const noexcept {
                return mFramesDrawn;
            }
    };

    /// <summary>
    /// The layer factory editor_application::initialise takes.
    /// </summary>
    [[nodiscard]] CONJURE_API std::unique_ptr<engine::layer> create_editor_layer(editor_context &context);
}
