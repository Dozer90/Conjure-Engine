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

#include <imgui.h>

#include <algorithm>
#include <system_error>
#include <utility>

namespace conjure::editor::ui {

    namespace {
        constexpr const char *DOCKSPACE_ID = "conjure_editor_dockspace";

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
        for (panel_instance &entry : mInstances) {
            entry.panel->draw(context);
        }
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
            if (ImGui::MenuItem("New Scene", nullptr, false, hasProject && context.play().is_editing())) {
                fire_menu_item(context, NEW_SCENE_ITEM);
            }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, context.has_scene() && !context.scene_path().empty())) {
                fire_menu_item(context, SAVE_SCENE_ITEM);
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
