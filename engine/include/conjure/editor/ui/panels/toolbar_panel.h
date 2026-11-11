#pragma once

#include "conjure/editor/editor_context.h"
#include "conjure/editor/ui/editor_panel.h"

#include "conjure/core/api.h"
#include "conjure/core/types.h"

// The play bar: play / pause / resume / step / stop (play.*), the current play state and frame, undo and
// redo buttons (history.*), the scene path and dirty flag.

namespace conjure::editor::ui {

    class CONJURE_API toolbar_panel final : public editor_panel {
        private:
            i32 mStepCount = 1;

        public:
            toolbar_panel();
            ~toolbar_panel() override;

        public:
            void draw_contents(editor_context &context) override;
    };
}
