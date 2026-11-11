#pragma once

#include "conjure/editor/editor_context.h"
#include "conjure/editor/ui/editor_panel.h"
#include "conjure/editor/ui/property_drawer.h"

#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/types.h"

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The docking workspace (docs/design/04_editor.md 4.2, D12): the full-window dockspace with a pass-through
// central node (the scene renders underneath), the main menu bar, the panel instances and the layout file
// <project>/.conjure/editor_layout.ini.

namespace conjure::editor::ui {

    /// <summary>
    /// A command the menu bar fires with a fixed argument set.
    /// </summary>
    struct menu_request {
        std::string command = "";
        core::json::document args = core::json::document::object();
    };

    class CONJURE_API editor_workspace {
        private:
            struct panel_instance {
                std::string name = "";
                std::unique_ptr<editor_panel> panel = nullptr;
            };

            panel_registry mPanels = {};
            property_drawer_registry mDrawers = {};
            std::vector<panel_instance> mInstances = {};
            std::filesystem::path mLayoutFile = {};
            u32 mFramesDrawn = 0;
            bool mLayoutLoaded = false;
            bool mShowDemo = false;

        public:
            editor_workspace();
            ~editor_workspace();

            editor_workspace(const editor_workspace &) = delete;
            editor_workspace &operator=(const editor_workspace &) = delete;

        public:
            /// <summary>
            /// Registers the built-in panels and instantiates the ones open by default.
            /// </summary>
            void create_default_panels(editor_context &context);

            /// <summary>
            /// Instantiates a registered panel (no-op when already open). False when unknown.
            /// </summary>
            bool open_panel(std::string_view name);

            /// <summary>
            /// Destroys a panel instance. False when not open.
            /// </summary>
            bool close_panel(std::string_view name);

            [[nodiscard]] editor_panel *find_panel(std::string_view name) noexcept;

            /// <summary>
            /// Adds the panels of loaded modules to the registry (and removes those of unloaded ones).
            /// </summary>
            void sync_module_panels(editor_context &context);

            /// <summary>
            /// Per-frame updates of every panel instance (open or closed).
            /// </summary>
            void update(editor_context &context);

            /// <summary>
            /// Draws the dockspace, the menu bar and every open panel. Call inside an ImGui frame.
            /// </summary>
            void draw(editor_context &context);

            /// <summary>
            /// The layout file; loaded on the first draw when it exists, saved by save_layout().
            /// </summary>
            void set_layout_file(std::filesystem::path path);

            [[nodiscard]] const std::filesystem::path &layout_file() const noexcept {
                return mLayoutFile;
            }

            /// <summary>
            /// Writes the ImGui ini settings to the layout file (IO failures are logged).
            /// </summary>
            void save_layout();

            [[nodiscard]] panel_registry &panels() noexcept {
                return mPanels;
            }

            [[nodiscard]] property_drawer_registry &drawers() noexcept {
                return mDrawers;
            }

            [[nodiscard]] usize open_panel_count() const noexcept {
                return mInstances.size();
            }

            [[nodiscard]] u32 frames_drawn() const noexcept {
                return mFramesDrawn;
            }

            /// <summary>
            /// Every request the File, Edit and Play menus fire with fixed arguments, as the menu sends them
            /// (Delete Selection builds its entity list at click time and is not listed). Tests validate them
            /// against the command registry's schemas.
            /// </summary>
            [[nodiscard]] static std::vector<menu_request> menu_requests();

        private:
            void draw_menu_bar(editor_context &context);
            void load_layout_once();
    };
}
