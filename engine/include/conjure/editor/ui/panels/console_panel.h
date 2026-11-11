#pragma once

#include "conjure/editor/editor_context.h"
#include "conjure/editor/ui/editor_panel.h"

#include "conjure/core/api.h"
#include "conjure/core/log.h"
#include "conjure/core/types.h"

#include <string>

// The console: the console_buffer ring with a level filter, a category filter, auto-scroll and clear.

namespace conjure::editor::ui {

    class CONJURE_API console_panel final : public editor_panel {
        private:
            core::log_level mMinLevel = core::log_level::INFO;
            std::string mCategoryFilter = "";
            u32 mLinesDrawn = 0;
            bool mAutoScroll = true;

        public:
            console_panel();
            ~console_panel() override;

        public:
            void draw_contents(editor_context &context) override;

            [[nodiscard]] u32 lines_drawn() const noexcept {
                return mLinesDrawn;
            }

            [[nodiscard]] core::log_level min_level() const noexcept {
                return mMinLevel;
            }

            void set_min_level(core::log_level level) noexcept {
                mMinLevel = level;
            }
    };
}
