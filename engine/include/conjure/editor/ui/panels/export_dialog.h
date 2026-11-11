#pragma once

#include "conjure/editor/editor_context.h"
#include "conjure/editor/ui/editor_panel.h"

#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <string>

// The export dialog: output directory and configuration, the project.export command and the progress events
// it emits.

namespace conjure::editor::ui {

    class CONJURE_API export_dialog final : public editor_panel {
        private:
            std::string mDirectory = "";
            std::string mConfiguration = "development";
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
    };
}
