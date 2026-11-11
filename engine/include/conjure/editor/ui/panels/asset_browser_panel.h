#pragma once

#include "conjure/editor/editor_context.h"
#include "conjure/editor/ui/editor_panel.h"

#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <string>

// The asset browser: the database's records as a filterable table (path, type, status), import by path,
// reimport, delete, open scenes and instantiate prefabs; all through asset.*, scene.open and prefab.instantiate.

namespace conjure::editor::ui {

    class CONJURE_API asset_browser_panel final : public editor_panel {
        private:
            std::string mFilter = "";
            std::string mImportPath = "";
            std::string mSelectedPath = "";
            u32 mRowsDrawn = 0;

        public:
            asset_browser_panel();
            ~asset_browser_panel() override;

        public:
            void draw_contents(editor_context &context) override;

            [[nodiscard]] u32 rows_drawn() const noexcept {
                return mRowsDrawn;
            }

            [[nodiscard]] const std::string &selected_path() const noexcept {
                return mSelectedPath;
            }
    };
}
