#include "conjure/editor/ui/panels/history_panel.h"

#include "editor/ui/ui_support.h"

#include "conjure/editor/undo_stack.h"

#include <imgui.h>

#include <format>
#include <string>
#include <vector>

namespace conjure::editor::ui {

    history_panel::history_panel() :
        editor_panel("History", false) {}

    history_panel::~history_panel() = default;

    void history_panel::draw_contents(editor_context &context) {
        mEntriesDrawn = 0;
        const undo_stack &stack = context.undo();
        ImGui::BeginDisabled(!stack.can_undo() || stack.frozen());
        if (ImGui::Button("Undo")) {
            support::fire_command(context, "history.undo", core::json::document::object());
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!stack.can_redo() || stack.frozen());
        if (ImGui::Button("Redo")) {
            support::fire_command(context, "history.redo", core::json::document::object());
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(stack.size() == 0);
        if (ImGui::Button("Clear")) {
            support::fire_command(context, "history.clear", core::json::document::object());
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        const std::string summary = std::format(
          "{} entries, cursor {}, {} KiB{}",
          stack.size(),
          stack.cursor(),
          stack.byte_size() / 1024,
          stack.frozen() ? " (frozen while playing)" : ""
        );
        ImGui::TextUnformatted(summary.c_str());

        if (!ImGui::BeginTable(
          "history_entries",
          3,
          ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable
        )) {
            return;
        }
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 40.0f);
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Client", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableHeadersRow();
        const std::vector<const undo_entry *> entries = stack.entries();
        ImGuiListClipper clipper = {};
        clipper.Begin(static_cast<int>(entries.size()));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const undo_entry &entry = *entries[static_cast<usize>(row)];
                const bool done = static_cast<usize>(row) < stack.cursor();
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%d", row);
                ImGui::TableNextColumn();
                if (done) {
                    ImGui::TextUnformatted(entry.label.c_str());
                } else {
                    ImGui::TextDisabled("%s", entry.label.c_str());
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                      "%zu undo / %zu redo command(s), %zu bytes",
                      entry.undo.size(),
                      entry.redo.size(),
                      entry.byteSize
                    );
                }
                ImGui::TableNextColumn();
                ImGui::Text("%u", entry.clientId);
                ++mEntriesDrawn;
            }
        }
        ImGui::EndTable();
    }
}
