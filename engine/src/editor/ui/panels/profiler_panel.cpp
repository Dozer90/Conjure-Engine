#include "conjure/editor/ui/panels/profiler_panel.h"

#include "editor/ui/ui_support.h"

#include "conjure/ecs/system_phase.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/application.h"
#include "conjure/engine/frame_stats.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <format>

namespace conjure::editor::ui {

    profiler_panel::profiler_panel() :
        editor_panel("Profiler", false) {}

    profiler_panel::~profiler_panel() = default;

    void profiler_panel::draw_contents(editor_context &context) {
        const engine::frame_stats &stats = context.engine().stats();
        ImGui::Checkbox("Pause", &mPaused);
        ImGui::SameLine();
        const engine::frame_sample &latest = stats.latest();
        const std::string summary = std::format(
          "frame {:.2f} ms (avg {:.2f} ms over {} frames) | {} fixed step(s) | {} job(s)",
          latest.frameMilliseconds,
          stats.average_frame_milliseconds(60),
          std::min<usize>(60, stats.sample_count()),
          latest.fixedSteps,
          latest.jobsExecuted
        );
        ImGui::TextUnformatted(summary.c_str());

        // Chronological frame times, oldest first.
        static std::array<f32, engine::frame_stats::HISTORY_SIZE> history = {};
        if (!mPaused) {
            const usize count = std::min<usize>(stats.sample_count(), history.size());
            for (usize index = 0; index < count; ++index) {
                history[index] = stats.sample_at(count - 1 - index).frameMilliseconds;
            }
            mSamplesPlotted = static_cast<u32>(count);
        }
        ImGui::PlotLines(
          "##frame_ms",
          history.data(),
          static_cast<int>(mSamplesPlotted),
          0,
          "frame ms",
          0.0f,
          std::max(33.3f, latest.frameMilliseconds * 1.5f),
          ImVec2(-1.0f, 80.0f)
        );

        if (ImGui::BeginTable("phases", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Phase");
            ImGui::TableSetupColumn("ms");
            ImGui::TableHeadersRow();
            for (u32 phase = 0; phase < ecs::SYSTEM_PHASE_COUNT; ++phase) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                const std::string_view name = ecs::to_string(static_cast<ecs::system_phase>(phase));
                ImGui::TextUnformatted(name.data(), name.data() + name.size());
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", latest.phaseMilliseconds[phase]);
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("submit");
            ImGui::TableNextColumn();
            ImGui::Text("%.3f", latest.submitMilliseconds);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("pacer sleep");
            ImGui::TableNextColumn();
            ImGui::Text("%.3f", latest.pacerSleepMilliseconds);
            ImGui::EndTable();
        }
        const std::string counters = std::format(
          "entities {} | frame memory high water {} KiB | dropped fixed time {}",
          context.world().entity_count(),
          latest.frameMemoryHighWater / 1024,
          latest.droppedFixedTime ? "yes" : "no"
        );
        ImGui::TextUnformatted(counters.c_str());
    }
}
