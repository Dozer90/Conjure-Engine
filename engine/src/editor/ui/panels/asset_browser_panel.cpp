#include "conjure/editor/ui/panels/asset_browser_panel.h"

#include "editor/ui/ui_support.h"

#include "conjure/editor/play_controller.h"

#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/core/string_utils.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace conjure::editor::ui {

    namespace {
        struct asset_row {
            std::string path = "";
            std::string type = "";
            std::string status = "";
            std::string uuid = "";
        };

        constexpr const char *DELETE_SCENE_POPUP = "Delete Open Scene";

        /// <summary>
        /// True when the row's asset is the file behind the open scene, by the one test asset.delete itself applies
        /// (editor_context::is_open_scene), so the dialog comes up exactly when the command would refuse.
        /// </summary>
        bool is_open_scene(editor_context &context, std::string_view uuid) {
            const std::optional<assets::asset_id> id = core::uuid::parse(uuid);
            return id.has_value() && context.is_open_scene(*id);
        }
    }

    asset_browser_panel::asset_browser_panel() :
        editor_panel("Assets") {}

    asset_browser_panel::~asset_browser_panel() = default;

    void asset_browser_panel::request_delete(editor_context &context, std::string_view uuid, std::string_view path) {
        if (!is_open_scene(context, uuid)) {
            support::fire_command(context, "asset.delete", core::json::object({{"asset", uuid}}));
            return;
        }
        mPendingDeleteUuid = std::string(uuid);
        mPendingDeletePath = std::string(path);
        mConfirmOpenRequested = true;
    }

    void asset_browser_panel::confirm_pending_delete(editor_context &context) {
        if (!has_pending_delete()) {
            return;
        }
        // closeOpenScene is refused while playing or paused (WRONG_PLAY_STATE, D20): the question stays up until
        // play is stopped instead of turning into a request that cannot succeed.
        if (!context.play().is_editing()) {
            return;
        }
        // Taken out first: the command closes the scene, and nothing here may outlive what it changes.
        const std::string uuid = std::move(mPendingDeleteUuid);
        cancel_pending_delete();
        support::fire_command(
          context,
          "asset.delete",
          core::json::object({{"asset", uuid}, {"closeOpenScene", true}})
        );
    }

    void asset_browser_panel::cancel_pending_delete() noexcept {
        mPendingDeleteUuid.clear();
        mPendingDeletePath.clear();
        mConfirmOpenRequested = false;
    }

    void asset_browser_panel::draw_contents(editor_context &context) {
        mRowsDrawn = 0;
        const assets::asset_database *database = context.asset_database();
        if (database == nullptr) {
            // The project went away with a confirmation up: there is nothing left to ask about.
            cancel_pending_delete();
            support::hint_text("No asset database (no project open, or a cooked project).");
            return;
        }
        draw_table(context, *database);
        draw_delete_confirmation(context);
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // Private methods

    void asset_browser_panel::draw_table(editor_context &context, const assets::asset_database &database) {
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
        for (const assets::asset_record &record : database.records()) {
            if (record.is_sub_asset()) {
                continue;
            }
            asset_row row = {};
            row.path = database.asset_path_of(record.id).value_or(record.relativePath);
            if (!mFilter.empty() && row.path.find(mFilter) == std::string::npos) {
                continue;
            }
            const assets::asset_type_desc *type = database.types().find(record.type);
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
                        request_delete(context, row.uuid, row.path);
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

    void asset_browser_panel::draw_delete_confirmation(editor_context &context) {
        // The Delete item runs inside the row's context menu, so the modal opens here, at the panel's own ID stack
        // level, on the next draw.
        if (mConfirmOpenRequested) {
            ImGui::OpenPopup(DELETE_SCENE_POPUP);
            mConfirmOpenRequested = false;
        }
        if (!ImGui::BeginPopupModal(DELETE_SCENE_POPUP, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (!ImGui::IsPopupOpen(DELETE_SCENE_POPUP)) {
                cancel_pending_delete();
            }
            return;
        }
        // Answered from elsewhere, or the scene was closed or replaced while the dialog was up: the question no
        // longer applies, and a plain delete must not slip through under an old confirmation.
        if (!has_pending_delete() || !is_open_scene(context, mPendingDeleteUuid)) {
            cancel_pending_delete();
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        ImGui::TextUnformatted(mPendingDeletePath.c_str());
        ImGui::TextUnformatted("This is the open scene. Deleting it closes the scene.");
        if (context.scene_dirty()) {
            ImGui::TextUnformatted("The scene has unsaved changes, which will be lost.");
        }
        support::hint_text("Undo restores the file; it does not reopen the scene.");
        // The command closes the scene, which it refuses while playing or paused: the button says so up front.
        const bool editing = context.play().is_editing();
        if (!editing) {
            ImGui::TextUnformatted("Stop playing before deleting the open scene.");
        }
        ImGui::BeginDisabled(!editing);
        if (ImGui::Button("Close and Delete")) {
            confirm_pending_delete(context);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            cancel_pending_delete();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
