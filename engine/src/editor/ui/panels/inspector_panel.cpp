#include "conjure/editor/ui/panels/inspector_panel.h"

#include "editor/core/component_access.h"
#include "editor/ui/ui_support.h"

#include "conjure/editor/entity_ref.h"
#include "conjure/editor/selection_set.h"

#include "conjure/scene/hierarchy.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace conjure::editor::ui {

    namespace {
        /// <summary>
        /// The short protocol name of a registered component ("local_transform" of "conjure.scene.local_transform"):
        /// the unique-suffix form the type resolver accepts.
        /// </summary>
        [[nodiscard]] std::string short_name(std::string_view registered) {
            const usize dot = registered.rfind('.');
            return std::string(dot == std::string_view::npos ? registered : registered.substr(dot + 1));
        }
    }

    inspector_panel::inspector_panel(const property_drawer_registry &drawers) :
        editor_panel("Inspector"),
        mDrawers(&drawers) {}

    inspector_panel::~inspector_panel() = default;

    void inspector_panel::draw_contents(editor_context &context) {
        mComponentsDrawn = 0;
        if (!context.has_scene()) {
            support::hint_text("No scene open.");
            return;
        }
        const core::uuid primary = context.selection().primary();
        if (primary.is_null()) {
            support::hint_text("Nothing selected.");
            return;
        }
        scene::scene &scene = context.scene();
        const ecs::entity entity = scene.find_guid(primary);
        if (entity.is_null()) {
            support::hint_text("The selected entity no longer exists.");
            return;
        }
        if (context.selection().size() > 1) {
            const std::string note = std::format(
              "{} entities selected; showing the first.",
              context.selection().size()
            );
            ImGui::TextDisabled("%s", note.c_str());
        }
        draw_entity_header(context, primary);
        ImGui::Separator();
        const std::vector<ecs::component_id> components = access::components_of(scene.world(), entity);
        for (const ecs::component_id id : components) {
            const std::string registered = access::component_name(context.components(), id);
            ImGui::PushID(registered.c_str());
            draw_component(context, primary, registered);
            ImGui::PopID();
            ++mComponentsDrawn;
        }
        ImGui::Separator();
        draw_add_component(context, primary);
    }

    void inspector_panel::draw_entity_header(editor_context &context, const core::uuid &guid) {
        scene::scene &scene = context.scene();
        const ecs::entity entity = scene.find_guid(guid);
        const std::string guidText = guid.to_string();
        const std::string currentName = std::string(scene.name_of(entity).view());
        if (!ImGui::IsAnyItemActive() || mNameBuffer.empty()) {
            mNameBuffer = currentName;
        }
        ImGui::SetNextItemWidth(-90.0f);
        if (ImGui::InputText(
          "##name",
          &mNameBuffer,
          ImGuiInputTextFlags_EnterReturnsTrue
        ) && mNameBuffer != currentName) {
            support::fire_command(
              context,
              "entity.rename",
              core::json::object({{"entity", guidText}, {"name", mNameBuffer}})
            );
        }
        ImGui::SameLine();
        bool enabled = scene.is_enabled(entity);
        if (ImGui::Checkbox("Enabled", &enabled)) {
            support::fire_command(
              context,
              "entity.set_enabled",
              core::json::object({{"entity", guidText}, {"enabled", enabled}})
            );
        }
        ImGui::TextDisabled("guid %s", guidText.c_str());
        const std::string path = entity_ref::path_of(scene, entity);
        ImGui::TextDisabled("path %s", path.c_str());
        const ecs::entity parent = scene.tree().parent_of(entity);
        if (!parent.is_null()) {
            const std::string parentName = std::string(scene.name_of(parent).view());
            ImGui::TextDisabled("parent %s", parentName.c_str());
        }
    }

    void inspector_panel::draw_component(editor_context &context, const core::uuid &guid, std::string_view typeName) {
        scene::scene &scene = context.scene();
        const ecs::entity entity = scene.find_guid(guid);
        const core::result<ecs::component_id> id = access::resolve_component_type(context.components(), typeName);
        if (!id.has_value() || entity.is_null()) {
            return;
        }
        const std::string shortName = short_name(typeName);
        const std::string header = std::format("{}###{}", shortName, typeName);
        const bool open = ImGui::CollapsingHeader(
          header.c_str(),
          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap
        );
        const bool removable = id.value() != scene.ids().localTransform;
        if (ImGui::BeginPopupContextItem("component_menu")) {
            if (ImGui::MenuItem("Remove", nullptr, false, removable)) {
                support::fire_command(
                  context,
                  "component.remove",
                  core::json::object({{"entity", guid.to_string()}, {"type", shortName}})
                );
            }
            if (ImGui::MenuItem("Copy as JSON")) {
                const core::result<core::json::document> value = access::read_component(scene, entity, id.value());
                if (value.has_value()) {
                    ImGui::SetClipboardText(value.value().to_string(true).c_str());
                }
            }
            ImGui::EndPopup();
        }
        if (!open) {
            return;
        }
        core::result<core::json::document> value = access::read_component(scene, entity, id.value());
        if (!value.has_value()) {
            ImGui::TextDisabled("%s", support::error_text(value.get_error()).c_str());
            return;
        }
        const ecs::component_info &info = context.components().info(id.value());
        // Edits during play are allowed on purpose: the dispatcher executes them against the live scene without
        // recording them (play.stop restores the snapshot) and warns once with UNRECORDED_PLAY_EDIT.
        const bool readOnly = false;
        bool changed = false;
        if (info.reflection != nullptr) {
            changed = mDrawers->draw_object(*info.reflection, value.value(), readOnly);
        } else if (value.value().root().is_object() && value.value().root().size() > 0) {
            changed = mDrawers->draw_json(shortName, value.value(), readOnly);
        } else {
            ImGui::TextDisabled("(no editable data)");
        }
        if (changed) {
            const std::string guidText = guid.to_string();
            (void) support::run_command(
              context,
              "component.set",
              core::json::object({{"entity", guidText}, {"path", shortName}, {"value", value.value()}}),
              support::coalesce_key(guidText, shortName)
            );
        }
    }

    void inspector_panel::draw_add_component(editor_context &context, const core::uuid &guid) {
        if (!ImGui::Button("Add Component")) {
            if (ImGui::BeginPopup("add_component")) {
                ImGui::EndPopup();
            }
        } else {
            ImGui::OpenPopup("add_component");
            mAddComponentFilter.clear();
        }
        if (!ImGui::BeginPopup("add_component")) {
            return;
        }
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint("##component_filter", "type name", &mAddComponentFilter);
        scene::scene &scene = context.scene();
        const ecs::entity entity = scene.find_guid(guid);
        std::vector<std::string> names = {};
        for (const ecs::component_info &info : context.components().types()) {
            if (!access::is_editable(info) || info.is_tag()) {
                continue;
            }
            if (!entity.is_null() && scene.world().has_raw(entity, info.id)) {
                continue;
            }
            const std::string name = std::string(info.name.view());
            if (!mAddComponentFilter.empty() && name.find(mAddComponentFilter) == std::string::npos) {
                continue;
            }
            names.push_back(name);
        }
        std::sort(names.begin(), names.end());
        if (ImGui::BeginChild("component_list", ImVec2(260.0f, 220.0f))) {
            for (const std::string &name : names) {
                if (ImGui::Selectable(name.c_str())) {
                    support::fire_command(
                      context,
                      "component.add",
                      core::json::object({{"entity", guid.to_string()}, {"type", name}})
                    );
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }
}
