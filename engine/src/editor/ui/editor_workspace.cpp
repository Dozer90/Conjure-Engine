#include "conjure/editor/ui/editor_workspace.h"

#include "editor/ui/ui_support.h"

#include "conjure/editor/editor_module_consumer.h"
#include "conjure/editor/log_category.h"
#include "conjure/editor/play_controller.h"
#include "conjure/editor/ui/panels/ai_session_panel.h"
#include "conjure/editor/ui/panels/asset_browser_panel.h"
#include "conjure/editor/ui/panels/console_panel.h"
#include "conjure/editor/ui/panels/export_dialog.h"
#include "conjure/editor/ui/panels/hierarchy_panel.h"
#include "conjure/editor/ui/panels/history_panel.h"
#include "conjure/editor/ui/panels/inspector_panel.h"
#include "conjure/editor/ui/panels/profiler_panel.h"
#include "conjure/editor/ui/panels/toolbar_panel.h"
#include "conjure/editor/ui/panels/viewport_panel.h"
#include "conjure/editor/undo_stack.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <format>
#include <system_error>
#include <utility>

namespace conjure::editor::ui {

    namespace {
        constexpr const char *DOCKSPACE_ID = "conjure_editor_dockspace";
        constexpr const char *SAVE_AS_POPUP = "Save Scene As";
        constexpr const char *TRUST_POPUP = "Trust This Project?";
        // The width the trust question wraps its text at, in font sizes: a path and three sentences stay readable.
        constexpr f32 TRUST_POPUP_WRAP_EMS = 34.0f;

        struct builtin_panel {
            const char *name = "";
            const char *title = "";
            bool openByDefault = true;
        };

        constexpr std::array<builtin_panel, 10> BUILTIN_PANELS = {
            builtin_panel{"toolbar", "Toolbar", true},
            builtin_panel{"viewport", "Viewport", true},
            builtin_panel{"hierarchy", "Hierarchy", true},
            builtin_panel{"inspector", "Inspector", true},
            builtin_panel{"assets", "Assets", true},
            builtin_panel{"console", "Console", true},
            builtin_panel{"history", "History", false},
            builtin_panel{"profiler", "Profiler", false},
            builtin_panel{"ai_session", "AI Session", false},
            builtin_panel{"export", "Export", false},
        };

        /// <summary>
        /// The fixed-argument menu items: one table so draw_menu_bar and menu_requests() cannot disagree.
        /// </summary>
        struct menu_item {
            const char *command = "";
            const char *argsJson = "{}";
        };

        constexpr menu_item NEW_SCENE_ITEM = {"scene.new", R"({"name":"Untitled"})"};
        constexpr menu_item SAVE_SCENE_ITEM = {"scene.save", "{}"};
        constexpr menu_item SAVE_PROJECT_ITEM = {"project.save", "{}"};
        constexpr menu_item QUIT_ITEM = {"editor.quit", "{}"};
        constexpr menu_item UNDO_ITEM = {"history.undo", "{}"};
        constexpr menu_item REDO_ITEM = {"history.redo", "{}"};
        constexpr menu_item CREATE_ENTITY_ITEM = {"entity.create", R"({"name":"Entity"})"};
        constexpr menu_item PLAY_ITEM = {"play.start", "{}"};
        constexpr menu_item PAUSE_ITEM = {"play.pause", "{}"};
        constexpr menu_item RESUME_ITEM = {"play.resume", "{}"};
        constexpr menu_item STOP_ITEM = {"play.stop", "{}"};

        constexpr std::array<menu_item, 11> MENU_ITEMS = {
            NEW_SCENE_ITEM, SAVE_SCENE_ITEM, SAVE_PROJECT_ITEM, QUIT_ITEM, UNDO_ITEM, REDO_ITEM, CREATE_ENTITY_ITEM,
            PLAY_ITEM, PAUSE_ITEM, RESUME_ITEM, STOP_ITEM,
        };

        [[nodiscard]] core::json::document menu_args(const menu_item &item) {
            core::result<core::json::document> parsed = core::json::document::parse(item.argsJson);
            // The table holds literals; a typo there is a programmer error caught by the menu test.
            return parsed.has_value() ? std::move(parsed).value() : core::json::document::object();
        }

        void fire_menu_item(editor_context &context, const menu_item &item) {
            support::fire_command(context, item.command, menu_args(item));
        }
    }

    editor_workspace::editor_workspace() = default;

    editor_workspace::~editor_workspace() = default;

    void editor_workspace::create_default_panels(editor_context &context) {
        (void) context;
        const property_drawer_registry *drawers = &mDrawers;
        for (const builtin_panel &builtin : BUILTIN_PANELS) {
            panel_descriptor descriptor = {};
            descriptor.name = builtin.name;
            descriptor.title = builtin.title;
            descriptor.owner = "editor";
            descriptor.openByDefault = builtin.openByDefault;
            const std::string name = builtin.name;
            descriptor.create = [name, drawers]() -> std::unique_ptr<editor_panel> {
                if (name == "toolbar") {
                    return std::make_unique<toolbar_panel>();
                }
                if (name == "viewport") {
                    return std::make_unique<viewport_panel>();
                }
                if (name == "hierarchy") {
                    return std::make_unique<hierarchy_panel>();
                }
                if (name == "inspector") {
                    return std::make_unique<inspector_panel>(*drawers);
                }
                if (name == "assets") {
                    return std::make_unique<asset_browser_panel>();
                }
                if (name == "console") {
                    return std::make_unique<console_panel>();
                }
                if (name == "history") {
                    return std::make_unique<history_panel>();
                }
                if (name == "profiler") {
                    return std::make_unique<profiler_panel>();
                }
                if (name == "ai_session") {
                    return std::make_unique<ai_session_panel>();
                }
                return std::make_unique<export_dialog>();
            };
            mPanels.add(std::move(descriptor));
        }
        for (const panel_descriptor &descriptor : mPanels.descriptors()) {
            if (descriptor.openByDefault) {
                (void) open_panel(descriptor.name);
            }
        }
    }

    bool editor_workspace::open_panel(std::string_view name) {
        if (editor_panel *existing = find_panel(name); existing != nullptr) {
            existing->set_open(true);
            return true;
        }
        const panel_descriptor *descriptor = mPanels.find(name);
        if (descriptor == nullptr || !descriptor->create) {
            return false;
        }
        panel_instance instance = {};
        instance.name = std::string(name);
        instance.panel = descriptor->create();
        if (instance.panel == nullptr) {
            return false;
        }
        instance.panel->set_open(true);
        mInstances.push_back(std::move(instance));
        return true;
    }

    bool editor_workspace::close_panel(std::string_view name) {
        const usize before = mInstances.size();
        std::erase_if(mInstances, [name](const panel_instance &entry) {
            return entry.name == name;
        });
        return mInstances.size() != before;
    }

    editor_panel *editor_workspace::find_panel(std::string_view name) noexcept {
        for (panel_instance &entry : mInstances) {
            if (entry.name == name) {
                return entry.panel.get();
            }
        }
        return nullptr;
    }

    void editor_workspace::sync_module_panels(editor_context &context) {
        // Drop the descriptors (and instances) of panels whose module left, add the newcomers.
        std::vector<std::string> wanted = {};
        for (const module_panel_entry &entry : context.module_consumer().panels()) {
            const std::string name = entry.owner + "." + entry.title;
            wanted.push_back(name);
            if (mPanels.find(name) == nullptr) {
                mPanels.add_module_panel(entry.owner, entry.desc);
                if (entry.desc.openByDefault) {
                    (void) open_panel(name);
                }
            }
        }
        std::vector<std::string> stale = {};
        for (const panel_descriptor &descriptor : mPanels.descriptors()) {
            if (descriptor.owner == "editor") {
                continue;
            }
            if (std::find(wanted.begin(), wanted.end(), descriptor.name) == wanted.end()) {
                stale.push_back(descriptor.name);
            }
        }
        for (const std::string &name : stale) {
            (void) close_panel(name);
            const panel_descriptor *descriptor = mPanels.find(name);
            if (descriptor != nullptr) {
                (void) mPanels.remove_owned_by(descriptor->owner);
            }
        }
    }

    void editor_workspace::update(editor_context &context) {
        sync_module_panels(context);
        for (panel_instance &entry : mInstances) {
            entry.panel->update(context);
        }
    }

    void editor_workspace::draw(editor_context &context) {
        if (!support::frame_is_open()) {
            return;
        }
        load_layout_once();
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::DockSpaceOverViewport(ImGui::GetID(DOCKSPACE_ID), viewport, ImGuiDockNodeFlags_PassthruCentralNode);
        draw_menu_bar(context);
        handle_global_shortcuts(context);
        for (panel_instance &entry : mInstances) {
            entry.panel->draw(context);
        }
        draw_save_as_modal(context);
        draw_trust_prompt(context);
        if (mShowDemo) {
            ImGui::ShowDemoWindow(&mShowDemo);
        }
        ++mFramesDrawn;
    }

    void editor_workspace::set_layout_file(std::filesystem::path path) {
        mLayoutFile = std::move(path);
        mLayoutLoaded = false;
    }

    void editor_workspace::save_layout() {
        if (mLayoutFile.empty() || ImGui::GetCurrentContext() == nullptr) {
            return;
        }
        std::error_code ignored = {};
        std::filesystem::create_directories(mLayoutFile.parent_path(), ignored);
        const std::string path = mLayoutFile.generic_string();
        ImGui::SaveIniSettingsToDisk(path.c_str());
    }

    void editor_workspace::draw_menu_bar(editor_context &context) {
        if (!ImGui::BeginMainMenuBar()) {
            return;
        }
        if (ImGui::BeginMenu("File")) {
            const bool hasProject = context.has_project();
            const bool editing = context.play().is_editing();
            // scene.new, scene.save and scene.save_as are editing-state commands: while playing the scene holds
            // the play session's state.
            const bool canSaveScene = context.has_scene() && editing;
            if (ImGui::MenuItem("New Scene", nullptr, false, hasProject && editing)) {
                fire_menu_item(context, NEW_SCENE_ITEM);
            }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, canSaveScene)) {
                request_save_scene(context);
            }
            if (ImGui::MenuItem("Save Scene As...", nullptr, false, canSaveScene)) {
                request_save_scene_as(context);
            }
            if (ImGui::MenuItem("Save Project", nullptr, false, hasProject)) {
                fire_menu_item(context, SAVE_PROJECT_ITEM);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Export...", nullptr, false, hasProject)) {
                (void) open_panel("export");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit")) {
                fire_menu_item(context, QUIT_ITEM);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, context.undo().can_undo() && !context.undo().frozen())) {
                fire_menu_item(context, UNDO_ITEM);
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, context.undo().can_redo() && !context.undo().frozen())) {
                fire_menu_item(context, REDO_ITEM);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Create Entity", nullptr, false, context.has_scene())) {
                fire_menu_item(context, CREATE_ENTITY_ITEM);
            }
            if (ImGui::MenuItem(
              "Delete Selection",
              "Del",
              false,
              context.has_scene() && !context.selection().empty()
            )) {
                core::json::document guids = core::json::document::array();
                for (const core::uuid &guid : context.selection().guids()) {
                    guids.push_back(guid.to_string());
                }
                support::fire_command(context, "entity.destroy_many", core::json::object({{"entities", guids}}));
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            for (const panel_descriptor &descriptor : mPanels.descriptors()) {
                editor_panel *instance = find_panel(descriptor.name);
                bool open = instance != nullptr && instance->is_open();
                if (ImGui::MenuItem(descriptor.title.c_str(), nullptr, &open)) {
                    if (open) {
                        (void) open_panel(descriptor.name);
                    } else if (instance != nullptr) {
                        instance->set_open(false);
                    }
                }
            }
            ImGui::Separator();
            ImGui::MenuItem("ImGui Demo", nullptr, &mShowDemo);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Play")) {
            const play_state state = context.play().state();
            if (ImGui::MenuItem("Play", "F5", false, state == play_state::EDITING && context.has_scene())) {
                fire_menu_item(context, PLAY_ITEM);
            }
            if (ImGui::MenuItem("Pause", nullptr, false, state == play_state::PLAYING)) {
                fire_menu_item(context, PAUSE_ITEM);
            }
            if (ImGui::MenuItem("Resume", nullptr, false, state == play_state::PAUSED)) {
                fire_menu_item(context, RESUME_ITEM);
            }
            if (ImGui::MenuItem("Stop", "Shift+F5", false, state != play_state::EDITING)) {
                fire_menu_item(context, STOP_ITEM);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    std::vector<menu_request> editor_workspace::menu_requests() {
        std::vector<menu_request> requests = {};
        requests.reserve(MENU_ITEMS.size());
        for (const menu_item &item : MENU_ITEMS) {
            menu_request request = {};
            request.command = item.command;
            request.args = menu_args(item);
            requests.push_back(std::move(request));
        }
        return requests;
    }

    void editor_workspace::request_save_scene(editor_context &context) {
        if (!context.has_scene() || !context.play().is_editing()) {
            return;
        }
        if (context.scene_path().empty()) {
            request_save_scene_as(context);
            return;
        }
        fire_menu_item(context, SAVE_SCENE_ITEM);
    }

    void editor_workspace::request_save_scene_as(editor_context &context) {
        // A second request while the dialog is up (Ctrl+S in the dialog) must not reset what the user typed.
        if (!context.has_scene() || !context.play().is_editing() || mSaveAsPending) {
            return;
        }
        const std::string_view name = context.scene().name();
        if (!context.scene_path().empty()) {
            mSaveAsPath = context.scene_path();
        } else if (name.empty()) {
            mSaveAsPath = "scenes/untitled";
        } else {
            mSaveAsPath = std::format("scenes/{}", name);
        }
        mSaveAsError.clear();
        mSaveAsPending = true;
        mSaveAsOpenRequested = true;
    }

    bool editor_workspace::save_scene_as(editor_context &context, std::string_view path) {
        const core::result<core::json::document> saved = support::run_command(
          context,
          "scene.save_as",
          core::json::object({{"path", path}})
        );
        if (!saved.has_value()) {
            mSaveAsError = support::error_text(saved.get_error());
            return false;
        }
        mSaveAsError.clear();
        mSaveAsPending = false;
        mSaveAsOpenRequested = false;
        return true;
    }

    void editor_workspace::handle_global_shortcuts(editor_context &context) {
        const play_state state = context.play().state();
        const bool hasScene = context.has_scene();
        const bool undoFrozen = context.undo().frozen();
        // Every shortcut is submitted each frame, pressed or not: ImGui honours a route the frame after it is claimed.
        const bool undoPressed = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal);
        const bool redoPressed = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal);
        const bool savePressed = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal);
        const bool playPressed = ImGui::Shortcut(ImGuiKey_F5, ImGuiInputFlags_RouteGlobal);
        const bool stopPressed = ImGui::Shortcut(ImGuiMod_Shift | ImGuiKey_F5, ImGuiInputFlags_RouteGlobal);
        if (undoPressed && context.undo().can_undo() && !undoFrozen) {
            fire_menu_item(context, UNDO_ITEM);
        }
        if (redoPressed && context.undo().can_redo() && !undoFrozen) {
            fire_menu_item(context, REDO_ITEM);
        }
        if (savePressed && hasScene) {
            request_save_scene(context);
        }
        if (playPressed && hasScene && state == play_state::EDITING) {
            fire_menu_item(context, PLAY_ITEM);
        }
        if (stopPressed && state != play_state::EDITING) {
            fire_menu_item(context, STOP_ITEM);
        }
    }

    void editor_workspace::draw_save_as_modal(editor_context &context) {
        // The menu item that asks for the dialog runs inside the File menu's popup, so the modal opens here, at the
        // workspace's own ID stack level, on the next draw.
        if (mSaveAsOpenRequested) {
            ImGui::OpenPopup(SAVE_AS_POPUP);
            mSaveAsOpenRequested = false;
        }
        if (!ImGui::BeginPopupModal(SAVE_AS_POPUP, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (!ImGui::IsPopupOpen(SAVE_AS_POPUP)) {
                mSaveAsPending = false;
            }
            return;
        }
        // Saved through save_scene_as() from elsewhere, or the scene went away while the dialog was up.
        if (!mSaveAsPending || !context.has_scene()) {
            mSaveAsPending = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        support::hint_text("Relative to assets://; .scene.json is added.");
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::SetNextItemWidth(360.0f);
        const bool entered = ImGui::InputText(
          "Path",
          &mSaveAsPath,
          ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll
        );
        const bool clicked = ImGui::Button("Save");
        if ((entered || clicked) && save_scene_as(context, mSaveAsPath)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            mSaveAsPending = false;
            ImGui::CloseCurrentPopup();
        }
        if (!mSaveAsError.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::TextWrapped("%s", mSaveAsError.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::EndPopup();
    }

    bool editor_workspace::answer_trust_prompt(editor_context &context, bool trust) {
        if (!context.trust_prompt().has_value()) {
            return false;
        }
        if (!trust) {
            context.dismiss_trust_prompt();
            mTrustPromptError.clear();
            return true;
        }
        const std::string path = context.trust_prompt()->generic_string();
        const core::result<core::json::document> trusted = support::run_command(
          context,
          "project.trust",
          core::json::object({{"path", path}})
        );
        if (!trusted.has_value()) {
            mTrustPromptError = support::error_text(trusted.get_error());
            return false;
        }
        // project.trust closed the question. The open is a command like any other: a refusal (the editor is
        // playing) is logged, and the answer that was given stands.
        mTrustPromptError.clear();
        return support::run_command(context, "project.open", core::json::object({{"path", path}})).has_value();
    }

    void editor_workspace::draw_trust_prompt(editor_context &context) {
        const bool asked = context.trust_prompt().has_value();
        if (asked && !ImGui::IsPopupOpen(TRUST_POPUP)) {
            ImGui::OpenPopup(TRUST_POPUP);
        }
        if (!ImGui::BeginPopupModal(TRUST_POPUP, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            return;
        }
        if (!asked) {
            // Answered from elsewhere: a protocol client sent project.trust, or the project was opened.
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        const std::string path = context.trust_prompt()->generic_string();
        const project_trust *trust = context.trust();
        const bool remembered = trust != nullptr && !trust->store_file().empty();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * TRUST_POPUP_WRAP_EMS);
        ImGui::TextUnformatted("Do you trust the authors of this project?");
        ImGui::Spacing();
        ImGui::TextWrapped("%s", path.c_str());
        ImGui::Spacing();
        ImGui::TextWrapped(
          "Opening a project runs code from its folder with your rights on this computer: its C# scripts, its "
          "native modules and the build files of both. Trust it only when you know where it came from."
        );
        support::hint_text(
          remembered
            ? "The answer is remembered for this folder, for you, on this computer."
            : "No trust store is in use: the answer holds until the editor exits."
        );
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        if (ImGui::Button("Trust and Open")) {
            (void) answer_trust_prompt(context, true);
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't Open") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            (void) answer_trust_prompt(context, false);
        }
        if (!mTrustPromptError.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::TextWrapped("%s", mTrustPromptError.c_str());
            ImGui::PopStyleColor();
        }
        if (!context.trust_prompt().has_value()) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    void editor_workspace::load_layout_once() {
        if (mLayoutLoaded) {
            return;
        }
        mLayoutLoaded = true;
        if (mLayoutFile.empty()) {
            return;
        }
        std::error_code ignored = {};
        if (!std::filesystem::is_regular_file(mLayoutFile, ignored)) {
            return;
        }
        const std::string path = mLayoutFile.generic_string();
        ImGui::LoadIniSettingsFromDisk(path.c_str());
    }
}
