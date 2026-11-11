#include "conjure/editor/ui/panels/hierarchy_panel.h"

#include "editor/ui/ui_support.h"

#include "conjure/editor/entity_ref.h"
#include "conjure/editor/selection_set.h"

#include "conjure/scene/hierarchy.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <array>
#include <format>
#include <utility>

namespace conjure::editor::ui {

    namespace {
        constexpr const char *ENTITY_PAYLOAD = "CONJURE_ENTITY";
        constexpr f32 INDENT_PER_LEVEL = 14.0f;

        [[nodiscard]] std::string guid_text(scene::scene &scene, ecs::entity entity) {
            const core::result<core::uuid> guid = entity_ref::guid_of(scene, entity);
            return guid.has_value() ? guid.value().to_string() : std::string("");
        }
    }

    hierarchy_panel::hierarchy_panel() :
        editor_panel("Hierarchy") {}

    hierarchy_panel::~hierarchy_panel() = default;

    void hierarchy_panel::update(editor_context &context) {
        if (!context.has_scene()) {
            return;
        }
        // Selection changes do not bump the structure version: refresh the SELECTED flags each frame cheaply by
        // marking the cache dirty when the selection differs from what the rows carry.
        bool selectionDiffers = false;
        for (const hierarchy_row &row : mCache.rows()) {
            const bool selected = has_flag(row.flags, hierarchy_row_flags::SELECTED);
            if (selected != context.selection().contains(row.entity)) {
                selectionDiffers = true;
                break;
            }
        }
        if (selectionDiffers) {
            mCache.invalidate();
        }
        (void) mCache.refresh_if_dirty(context.scene(), context.selection());
    }

    void hierarchy_panel::draw_contents(editor_context &context) {
        mRowsDrawn = 0;
        if (!context.has_scene()) {
            support::hint_text("No scene open.");
            return;
        }
        std::string filter = mCache.filter();
        ImGui::SetNextItemWidth(-60.0f);
        if (ImGui::InputTextWithHint("##filter", "filter by name", &filter)) {
            mCache.set_filter(filter);
        }
        ImGui::SameLine();
        if (ImGui::Button("Add")) {
            core::json::document args = core::json::object({{"name", "Entity"}});
            const core::uuid primary = context.selection().primary();
            if (!primary.is_null()) {
                args.set("parent", primary.to_string());
            }
            support::fire_command(context, "entity.create", std::move(args));
        }
        (void) mCache.refresh_if_dirty(context.scene(), context.selection());

        if (!ImGui::BeginChild("rows", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_None)) {
            ImGui::EndChild();
            return;
        }
        const std::span<const hierarchy_row> rows = mCache.rows();
        ImGuiListClipper clipper = {};
        clipper.Begin(static_cast<int>(rows.size()), ImGui::GetTextLineHeightWithSpacing());
        while (clipper.Step()) {
            for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
                draw_row(context, rows[static_cast<usize>(index)], static_cast<usize>(index));
                ++mRowsDrawn;
            }
        }
        // Dropping on the empty area reparents to the root.
        ImGui::Dummy(ImVec2(-1.0f, ImGui::GetTextLineHeight()));
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ENTITY_PAYLOAD); payload != nullptr) {
                const std::string dragged(
                  static_cast<const char *>(payload->Data),
                  static_cast<usize>(payload->DataSize)
                );
                support::fire_command(
                  context,
                  "entity.set_parent",
                  core::json::object({{"entity", dragged}, {"parent", nullptr}})
                );
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
            context.selection().clear();
        }
        if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete) && !context.selection().empty()) {
            core::json::document guids = core::json::document::array();
            for (const core::uuid &guid : context.selection().guids()) {
                guids.push_back(guid.to_string());
            }
            support::fire_command(context, "entity.destroy_many", core::json::object({{"entities", guids}}));
        }
        ImGui::EndChild();
    }

    void hierarchy_panel::draw_row(editor_context &context, const hierarchy_row &row, usize index) {
        scene::scene &scene = context.scene();
        ImGui::PushID(static_cast<int>(index));
        ImGui::Indent(INDENT_PER_LEVEL * static_cast<f32>(row.depth) + 1.0f);
        const bool hasChildren = has_flag(row.flags, hierarchy_row_flags::HAS_CHILDREN);
        const bool expanded = has_flag(row.flags, hierarchy_row_flags::EXPANDED);
        if (hasChildren) {
            if (ImGui::ArrowButton("##expand", expanded ? ImGuiDir_Down : ImGuiDir_Right)) {
                mCache.set_expanded(row.entity, !expanded);
            }
        } else {
            ImGui::Dummy(ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
        }
        ImGui::SameLine();

        const bool selected = has_flag(row.flags, hierarchy_row_flags::SELECTED);
        const bool disabled = has_flag(row.flags, hierarchy_row_flags::DISABLED);
        const bool prefab = has_flag(row.flags, hierarchy_row_flags::PREFAB_INSTANCE);
        if (mRenaming == row.entity) {
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::IsWindowAppearing() || !ImGui::IsAnyItemActive()) {
                ImGui::SetKeyboardFocusHere();
            }
            if (ImGui::InputText(
              "##rename",
              &mRenameBuffer,
              ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll
            )) {
                support::fire_command(
                  context,
                  "entity.rename",
                  core::json::object({{"entity", guid_text(scene, row.entity)}, {"name", mRenameBuffer}})
                );
                mRenaming = {};
            } else if (
              ImGui::IsKeyPressed(
                ImGuiKey_Escape
              ) || (ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit())
            ) {
                mRenaming = {};
            }
        } else {
            std::string label = std::string(row.nameId.view());
            if (label.empty()) {
                label = std::format("Entity {}", row.entity.index);
            }
            if (prefab) {
                label += "  [prefab]";
            }
            if (disabled) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));
            } else if (prefab) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
            }
            if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                select_row(context, row.entity, ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift);
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    support::fire_command(
                      context,
                      "view.focus",
                      core::json::object({{"entity", guid_text(scene, row.entity)}})
                    );
                }
            }
            if (disabled || prefab) {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGuiKey_F2)) {
                mRenaming = row.entity;
                mRenameBuffer = std::string(row.nameId.view());
            }
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                const std::string guid = guid_text(scene, row.entity);
                ImGui::SetDragDropPayload(ENTITY_PAYLOAD, guid.data(), guid.size());
                ImGui::TextUnformatted(label.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ENTITY_PAYLOAD); payload != nullptr) {
                    const std::string dragged(
                      static_cast<const char *>(payload->Data),
                      static_cast<usize>(payload->DataSize)
                    );
                    const std::string target = guid_text(scene, row.entity);
                    if (dragged != target) {
                        support::fire_command(
                          context,
                          "entity.set_parent",
                          core::json::object({{"entity", dragged}, {"parent", target}})
                        );
                        mCache.set_expanded(row.entity, true);
                    }
                }
                ImGui::EndDragDropTarget();
            }
            draw_context_menu(context, row.entity);
        }
        ImGui::Unindent(INDENT_PER_LEVEL * static_cast<f32>(row.depth) + 1.0f);
        ImGui::PopID();
    }

    void hierarchy_panel::draw_context_menu(editor_context &context, ecs::entity entity) {
        if (!ImGui::BeginPopupContextItem("entity_menu")) {
            return;
        }
        scene::scene &scene = context.scene();
        const std::string guid = guid_text(scene, entity);
        if (!context.selection().contains(entity)) {
            select_row(context, entity, false);
        }
        if (ImGui::MenuItem("Create Child")) {
            support::fire_command(context, "entity.create", core::json::object({{"name", "Entity"}, {"parent", guid}}));
            mCache.set_expanded(entity, true);
        }
        if (ImGui::MenuItem("Duplicate")) {
            support::fire_command(context, "entity.duplicate", core::json::object({{"entity", guid}}));
        }
        if (ImGui::MenuItem("Rename", "F2")) {
            mRenaming = entity;
            mRenameBuffer = std::string(scene.name_of(entity).view());
        }
        const bool enabled = scene.is_enabled(entity);
        if (ImGui::MenuItem(enabled ? "Disable" : "Enable")) {
            support::fire_command(
              context,
              "entity.set_enabled",
              core::json::object({{"entity", guid}, {"enabled", !enabled}})
            );
        }
        if (ImGui::MenuItem("Focus")) {
            support::fire_command(context, "view.focus", core::json::object({{"entity", guid}}));
        }
        if (!scene.tree().parent_of(entity).is_null() && ImGui::MenuItem("Move to Root")) {
            support::fire_command(
              context,
              "entity.set_parent",
              core::json::object({{"entity", guid}, {"parent", nullptr}})
            );
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete", "Del")) {
            core::json::document guids = core::json::document::array();
            for (const core::uuid &selected : context.selection().guids()) {
                guids.push_back(selected.to_string());
            }
            if (guids.root().size() == 0) {
                guids.push_back(guid);
            }
            support::fire_command(context, "entity.destroy_many", core::json::object({{"entities", guids}}));
        }
        ImGui::EndPopup();
    }

    void hierarchy_panel::select_row(editor_context &context, ecs::entity entity, bool additive) {
        const core::result<core::uuid> guid = entity_ref::guid_of(context.scene(), entity);
        if (!guid.has_value()) {
            return;
        }
        const std::array<core::uuid, 1> guids = {guid.value()};
        if (!additive) {
            context.selection().set(guids);
        } else if (context.selection().contains(guid.value())) {
            context.selection().remove(guids);
        } else {
            context.selection().add(guids);
        }
        context.selection().rebuild(context.scene());
    }
}
