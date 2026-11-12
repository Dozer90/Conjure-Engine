#pragma once

#include "conjure/editor/editor_context.h"
#include "conjure/editor/ui/editor_panel.h"

#include "conjure/core/api.h"
#include "conjure/core/types.h"
#include "conjure/engine/frame_stats.h"

#include <array>

// The profiler: frame time history from engine::frame_stats as a plot, the phase breakdown of the latest
// sample as a table, renderer and world counters.

namespace conjure::editor::ui {

    class CONJURE_API profiler_panel final : public editor_panel {
        private:
            std::array<f32, engine::frame_stats::HISTORY_SIZE> mHistory = {};  // chronological, oldest first
            u32 mSamplesPlotted = 0;
            bool mPaused = false;

        public:
            profiler_panel();
            ~profiler_panel() override;

        public:
            void draw_contents(editor_context &context) override;

            [[nodiscard]] u32 samples_plotted() const noexcept {
                return mSamplesPlotted;
            }
    };
}
