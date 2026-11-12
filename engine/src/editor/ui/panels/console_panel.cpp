#include "conjure/editor/ui/panels/console_panel.h"

#include "editor/ui/ui_support.h"

#include "conjure/editor/console_buffer.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <array>
#include <format>
#include <vector>

namespace conjure::editor::ui {

    namespace {
        constexpr usize MAX_LINES_DRAWN = 5000;

        constexpr std::array<const char *, 6> LEVEL_NAMES = {"trace", "debug", "info", "warn", "error", "fatal"};

        [[nodiscard]] ImVec4 colour_of(core::log_level level) noexcept {
            switch (level) {
                case core::log_level::TRACE:
                case core::log_level::DEBUG:
                    return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
                case core::log_level::WARN:
                    return ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
                case core::log_level::ERR:
                case core::log_level::FATAL:
                    return ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                case core::log_level::INFO:
                case core::log_level::OFF:
                    break;
            }
            return ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
        }
    }

    console_panel::console_panel() :
        editor_panel("Console") {}

    console_panel::~console_panel() = default;

    void console_panel::draw_contents(editor_context &context) {
        mLinesDrawn = 0;
        int level = static_cast<int>(mMinLevel);
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::Combo("##level", &level, LEVEL_NAMES.data(), static_cast<int>(LEVEL_NAMES.size()))) {
            mMinLevel = static_cast<core::log_level>(level);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputTextWithHint("##category", "category", &mCategoryFilter);
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &mAutoScroll);
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            support::fire_command(context, "console.clear", core::json::document::object());
        }
        ImGui::SameLine();
        const std::string summary = std::format("{} entries", context.console().size());
        ImGui::TextUnformatted(summary.c_str());

        if (!ImGui::BeginChild(
          "console_lines",
          ImVec2(0.0f, 0.0f),
          ImGuiChildFlags_None,
          ImGuiWindowFlags_HorizontalScrollbar
        )) {
            ImGui::EndChild();
            return;
        }
        console_query query = {};
        query.minLevel = mMinLevel;
        query.category = mCategoryFilter;
        query.limit = MAX_LINES_DRAWN;
        // The tail: with more matches than the panel draws, the oldest would otherwise fill it and new lines
        // would never appear.
        query.newest = true;
        const std::vector<console_record> records = context.console().read(query);
        mNewestSeqShown = records.empty() ? 0 : records.back().seq;
        ImGuiListClipper clipper = {};
        clipper.Begin(static_cast<int>(records.size()));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const console_record &record = records[static_cast<usize>(row)];
                const std::string line = std::format(
                  "[{:>6}] {:<5} {}: {}",
                  record.frame,
                  core::to_string(record.level),
                  record.category,
                  record.text
                );
                ImGui::PushStyleColor(ImGuiCol_Text, colour_of(record.level));
                ImGui::TextUnformatted(line.c_str());
                ImGui::PopStyleColor();
                ++mLinesDrawn;
            }
        }
        if (mAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }
}
