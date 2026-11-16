#pragma once

#include "conjure/editor/editor_context.h"
#include "conjure/editor/ui/editor_panel.h"

#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <memory>
#include <string>
#include <utility>

// The export dialog: the output directory, the project.export command and the progress events it emits. An
// export is always a Distribution export, so there is no configuration to choose (code sweep F650). Each dialog
// owns its export's state; the event client and response sink it registers are released when the export completes
// or the dialog is destroyed, whichever comes first.

namespace conjure::editor::ui {

    class CONJURE_API export_dialog final : public editor_panel {
        private:
            // Shared with the callbacks the export registers, so a late callback never touches a dead dialog.
            struct export_state;

            std::shared_ptr<export_state> mState = nullptr;
            editor_context *mContext = nullptr;         // the context the running export registered with
            std::string mDirectory = "";
            std::string mStatus = "";
            f32 mProgress = 0.0f;
            bool mRunning = false;

        public:
            export_dialog();
            ~export_dialog() override;

        public:
            void update(editor_context &context) override;
            void draw_contents(editor_context &context) override;

            [[nodiscard]] bool is_running() const noexcept {
                return mRunning;
            }

            [[nodiscard]] const std::string &status() const noexcept {
                return mStatus;
            }

            /// <summary>
            /// Starts the export of the directory field (tests call it directly).
            /// </summary>
            void start_export(editor_context &context);

            void set_directory(std::string directory) {
                mDirectory = std::move(directory);
            }

        private:
            void release_callbacks();
    };
}
