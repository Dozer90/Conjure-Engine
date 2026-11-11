#pragma once

#include "conjure/editor/editor_context.h"
#include "conjure/editor/ui/editor_panel.h"

#include "conjure/core/api.h"
#include "conjure/core/types.h"

// The undo history: every entry with its label and client, the cursor, undo / redo / clear.

namespace conjure::editor::ui {

    class CONJURE_API history_panel final : public editor_panel {
        private:
            u32 mEntriesDrawn = 0;

        public:
            history_panel();
            ~history_panel() override;

        public:
            void draw_contents(editor_context &context) override;

            [[nodiscard]] u32 entries_drawn() const noexcept {
                return mEntriesDrawn;
            }
    };
}
