#include "conjure/editor/ui/panels/hierarchy_panel.h"

#include "editor/ui/ui_support.h"

#include "conjure/editor/entity_ref.h"
#include "conjure/editor/selection_set.h"

#include "conjure/scene/hierarchy.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace conjure::editor::ui {

    namespace {
        constexpr const char *ENTITY_PAYLOAD = "CONJURE_ENTITY";
        constexpr f32 INDENT_PER_LEVEL = 14.0f;

        [[nodiscard]] std::string guid_text(scene::scene &scene, ecs::entity entity) {
            const core::result<core::uuid> guid = entity_ref::guid_of(scene, entity);
            return guid.has_value() ? guid.value().to_string() : std::string("");
        }

        void destroy_selection(editor_context &context) {
            core::json::document guids = core::json::document::array();
            for (const core::uuid &guid : context.selection().guids()) {
                guids.push_back(guid.to_string());
            }
            support::fire_command(context, "entity.destroy_many", core::json::object({{"entities", guids}}));
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

        // Names are refreshed per visible row while drawing, but a rename can change which rows match the filter.
        if (context.revision() != mSeenRevision) {
            mSeenRevision = context.revision();
            if (!mCache.filter().empty()) {
                mCache.invalidate();
            }
        }

        // A primary selected outside the tree (a viewport pick, a protocol client) is revealed and scrolled to.
        const core::uuid primary = context.selection().primary();
        if (primary != mLastPrimary) {
            mLastPrimary = primary;
            const ecs::entity entity = primary.is_null() ? ecs::entity{} : context.scene().find_guid(primary);
            if (!mSelectedHere && !entity.is_null()) {
                mCache.reveal(context.scene(), entity);
                mScrollTarget = primary;
            }
        }
        // Always cleared: a tree click that left the primary unchanged must not suppress the next outside reveal.
        mSelectedHere = false;
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
        // Every row is one frame tall: the expand arrow, its placeholder or the rename field.
        const f32 rowHeight = ImGui::GetFrameHeightWithSpacing();
        if (!mScrollTarget.is_null()) {
            scroll_to_target(context, rowHeight);
        }
        const std::span<const hierarchy_row> rows = mCache.rows();
        ImGuiListClipper clipper = {};
        clipper.Begin(static_cast<int>(rows.size()), rowHeight);
        while (clipper.Step()) {
            // Refreshed in place: 'rows' stays valid and sees the new names.
            mCache.refresh_labels(
              context.scene(),
              static_cast<usize>(clipper.DisplayStart),
              static_cast<usize>(clipper.DisplayEnd)
            );
            for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
                draw_row(context, rows[static_cast<usize>(index)], static_cast<usize>(index));
                ++mRowsDrawn;
            }
        }
        // Dropping on the empty area below the rows reparents to the root.
        const ImVec2 available = ImGui::GetContentRegionAvail();
        ImGui::Dummy(ImVec2(std::max(available.x, 1.0f), std::max(available.y, ImGui::GetTextLineHeight())));
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
        // Keys of the tree, not of a text field: a Delete or F2 typed into the rename field edits the name.
        const bool treeHasKeys = mRenaming.is_null() && !ImGui::GetIO().WantTextInput && ImGui::IsWindowFocused();
        if (treeHasKeys && ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !context.selection().empty()) {
            destroy_selection(context);
        }
        if (treeHasKeys && ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
            const core::uuid primary = context.selection().primary();
            if (!primary.is_null()) {
                begin_rename(context, context.scene().find_guid(primary));
            }
        }
        ImGui::EndChild();
    }

    void hierarchy_panel::begin_rename(editor_context &context, ecs::entity entity) {
        if (!context.has_scene() || entity.is_null() || !context.scene().contains(entity)) {
            return;
        }
        scene::scene &scene = context.scene();
        mRenaming = entity;
        mRenameBuffer = std::string(scene.name_of(entity).view());
        mRenameFocusPending = true;
        mCache.reveal(scene, entity);
        const std::optional<core::uuid> guid = scene.guid_of(entity);
        if (guid.has_value()) {
            mScrollTarget = *guid;
        }
    }

    void hierarchy_panel::draw_row(editor_context &context, const hierarchy_row &row, usize index) {
        scene::scene &scene = context.scene();
        ImGui::PushID(static_cast<int>(index));
        ImGui::Indent(INDENT_PER_LEVEL * static_cast<f32>(row.depth) + 1.0f);
        const bool hasChildren = has_flag(row.flags, hierarchy_row_flags::HAS_CHILDREN);
        const bool expanded = has_flag(row.flags, hierarchy_row_flags::EXPANDED);
        if (hasChildren) {
            if (ImGui::ArrowButton("##expand", expanded ? ImGuiDir_Down : ImGuiDir_Right)) {
                mCache.set_expanded(scene, row.entity, !expanded);
            }
        } else {
            ImGui::Dummy(ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
        }
        ImGui::SameLine();

        const bool selected = has_flag(row.flags, hierarchy_row_flags::SELECTED);
        const bool disabled = has_flag(row.flags, hierarchy_row_flags::DISABLED);
        const bool prefab = has_flag(row.flags, hierarchy_row_flags::PREFAB_INSTANCE);
        if (mRenaming == row.entity) {
            draw_rename_field(context, row);
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
                const ImGuiIO &io = ImGui::GetIO();
                if (io.KeyShift && !mAnchor.is_null()) {
                    select_range(context, row.entity, io.KeyCtrl);
                } else {
                    select_row(context, row.entity, io.KeyCtrl);
                }
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
                        mCache.set_expanded(scene, row.entity, true);
                    }
                }
                ImGui::EndDragDropTarget();
            }
            draw_context_menu(context, row.entity);
        }
        ImGui::Unindent(INDENT_PER_LEVEL * static_cast<f32>(row.depth) + 1.0f);
        ImGui::PopID();
    }

    void hierarchy_panel::draw_rename_field(editor_context &context, const hierarchy_row &row) {
        scene::scene &scene = context.scene();
        ImGui::SetNextItemWidth(-1.0f);
        // Focused once: a click elsewhere must be able to take the focus away (and so end the rename).
        if (mRenameFocusPending) {
            ImGui::SetKeyboardFocusHere();
            mRenameFocusPending = false;
        }
        // Read before the field, which reverts its text on Escape and reports the deactivation as an edit.
        const bool escaped = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
        ImGui::InputText("##rename", &mRenameBuffer, ImGuiInputTextFlags_AutoSelectAll);
        if (ImGui::IsItemDeactivatedAfterEdit() && !escaped) {
            if (mRenameBuffer != std::string(scene.name_of(row.entity).view())) {
                support::fire_command(
                  context,
                  "entity.rename",
                  core::json::object({{"entity", guid_text(scene, row.entity)}, {"name", mRenameBuffer}})
                );
            }
            mRenaming = {};
        } else if (escaped || ImGui::IsItemDeactivated()) {
            mRenaming = {};
        }
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
            mCache.set_expanded(scene, entity, true);
        }
        if (ImGui::MenuItem("Duplicate")) {
            support::fire_command(context, "entity.duplicate", core::json::object({{"entity", guid}}));
        }
        if (ImGui::MenuItem("Rename", "F2")) {
            begin_rename(context, entity);
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

    void hierarchy_panel::scroll_to_target(editor_context &context, f32 rowHeight) {
        const ecs::entity target = context.scene().find_guid(mScrollTarget);
        mScrollTarget = {};
        if (target.is_null()) {
            return;
        }
        const std::span<const hierarchy_row> rows = mCache.rows();
        for (usize index = 0; index < rows.size(); ++index) {
            if (rows[index].entity != target) {
                continue;
            }
            // The clipper lays rows out from the top of the child at a uniform height.
            const f32 top = static_cast<f32>(index) * rowHeight;
            const f32 scrollY = ImGui::GetScrollY();
            const f32 visibleHeight = ImGui::GetWindowHeight();
            if (top < scrollY || top + rowHeight > scrollY + visibleHeight) {
                ImGui::SetScrollY(std::max(0.0f, top - visibleHeight * 0.5f));
            }
            return;
        }
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
        mAnchor = guid.value();
        mSelectedHere = true;
    }

    void hierarchy_panel::select_range(editor_context &context, ecs::entity entity, bool additive) {
        scene::scene &scene = context.scene();
        const ecs::entity anchor = scene.find_guid(mAnchor);
        const std::span<const hierarchy_row> rows = mCache.rows();
        const usize missing = rows.size();
        usize anchorIndex = missing;
        usize entityIndex = missing;
        for (usize index = 0; index < rows.size(); ++index) {
            if (rows[index].entity == anchor) {
                anchorIndex = index;
            }
            if (rows[index].entity == entity) {
                entityIndex = index;
            }
        }
        // An anchor that was deleted, filtered out or collapsed away starts a new range at the clicked row.
        if (anchor.is_null() || anchorIndex == missing || entityIndex == missing) {
            select_row(context, entity, additive);
            return;
        }
        const usize first = std::min(anchorIndex, entityIndex);
        const usize last = std::max(anchorIndex, entityIndex);
        std::vector<core::uuid> guids = {};
        guids.reserve(last - first + 1);
        for (usize index = first; index <= last; ++index) {
            const std::optional<core::uuid> guid = scene.guid_of(rows[index].entity);
            if (index != entityIndex && guid.has_value()) {
                guids.push_back(*guid);
            }
        }
        // Named last, the clicked row becomes the primary selection.
        const std::optional<core::uuid> clicked = scene.guid_of(entity);
        if (clicked.has_value()) {
            guids.push_back(*clicked);
        }
        if (additive) {
            context.selection().add(guids);
        } else {
            context.selection().set(guids);
        }
        context.selection().rebuild(scene);
        mSelectedHere = true;
    }
}
