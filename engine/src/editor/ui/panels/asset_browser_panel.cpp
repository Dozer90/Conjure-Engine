#include "conjure/editor/ui/panels/asset_browser_panel.h"

#include "editor/ui/ui_support.h"

#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/core/string_utils.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <format>
#include <string>
#include <vector>

namespace conjure::editor::ui {

    namespace {
        struct asset_row {
            std::string path = "";
            std::string type = "";
            std::string status = "";
            std::string uuid = "";
        };
    }

    asset_browser_panel::asset_browser_panel() :
        editor_panel("Assets") {}

    asset_browser_panel::~asset_browser_panel() = default;

    void asset_browser_panel::draw_contents(editor_context &context) {
        mRowsDrawn = 0;
        assets::asset_database *database = context.asset_database();
        if (database == nullptr) {
            support::hint_text("No asset database (no project open, or a cooked project).");
            return;
        }
        ImGui::SetNextItemWidth(200.0f);
        ImGui::InputTextWithHint("##filter", "filter", &mFilter);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-70.0f);
        ImGui::InputTextWithHint("##import", "file to import (absolute, or relative to assets://)", &mImportPath);
        ImGui::SameLine();
        if (ImGui::Button("Import") && !mImportPath.empty()) {
            support::fire_command(context, "asset.import", core::json::object({{"source", mImportPath}}));
            mImportPath.clear();
        }

        std::vector<asset_row> rows = {};
        for (const assets::asset_record &record : database->records()) {
            if (record.is_sub_asset()) {
                continue;
            }
            asset_row row = {};
            row.path = database->asset_path_of(record.id).value_or(record.relativePath);
            if (!mFilter.empty() && row.path.find(mFilter) == std::string::npos) {
                continue;
            }
            const assets::asset_type_desc *type = database->types().find(record.type);
            row.type = type != nullptr ? type->name : std::string(record.importerId);
            row.status = std::string(assets::to_string(record.status));
            row.uuid = record.id.to_string();
            rows.push_back(std::move(row));
        }
        std::sort(rows.begin(), rows.end(), [](const asset_row &left, const asset_row &right) {
            return left.path < right.path;
        });

        const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable |
          ImGuiTableFlags_BordersInnerV;
        if (!ImGui::BeginTable("assets", 3, flags)) {
            return;
        }
        ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableHeadersRow();
        ImGuiListClipper clipper = {};
        clipper.Begin(static_cast<int>(rows.size()));
        while (clipper.Step()) {
            for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
                const asset_row &row = rows[static_cast<usize>(index)];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID(index);
                const bool selected = row.path == mSelectedPath;
                if (ImGui::Selectable(
                  row.path.c_str(),
                  selected,
                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick
                )) {
                    mSelectedPath = row.path;
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        if (core::str::ends_with(row.path, ".scene.json")) {
                            support::fire_command(context, "scene.open", core::json::object({{"path", row.path}}));
                        } else if (core::str::ends_with(row.path, ".prefab.json") && context.has_scene()) {
                            support::fire_command(
                              context,
                              "prefab.instantiate",
                              core::json::object({{"prefab", row.uuid}})
                            );
                        }
                    }
                }
                if (ImGui::BeginPopupContextItem("asset_menu")) {
                    mSelectedPath = row.path;
                    if (ImGui::MenuItem("Reimport")) {
                        support::fire_command(context, "asset.reimport", core::json::object({{"asset", row.uuid}}));
                    }
                    if (core::str::ends_with(row.path, ".scene.json") && ImGui::MenuItem("Open Scene")) {
                        support::fire_command(context, "scene.open", core::json::object({{"path", row.path}}));
                    }
                    if (core::str::ends_with(
                      row.path,
                      ".prefab.json"
                    ) && ImGui::MenuItem("Instantiate", nullptr, false, context.has_scene())) {
                        support::fire_command(
                          context,
                          "prefab.instantiate",
                          core::json::object({{"prefab", row.uuid}})
                        );
                    }
                    if (ImGui::MenuItem("Copy uuid")) {
                        ImGui::SetClipboardText(row.uuid.c_str());
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Delete")) {
                        support::fire_command(context, "asset.delete", core::json::object({{"asset", row.uuid}}));
                    }
                    ImGui::EndPopup();
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", row.uuid.c_str());
                }
                ImGui::PopID();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row.type.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row.status.c_str());
                ++mRowsDrawn;
            }
        }
        ImGui::EndTable();
    }
}
