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
// central node (the scene renders underneath), the main menu bar, the global shortcuts (Ctrl+Z, Ctrl+Y, Ctrl+S,
// F5, Shift+F5, handled here so they work whichever panels are open), the Save Scene As dialog, the project trust
// question (decision D27: "Do you trust the authors of this project?", shown while the context's trust_prompt()
// names a project), the panel instances and the layout file <project>/.conjure/editor_layout.ini.

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
            std::string mSaveAsPath = "";
            std::string mSaveAsError = "";
            std::string mTrustPromptError = "";         // why the last "Trust and Open" recorded no answer
            u32 mFramesDrawn = 0;
            bool mLayoutLoaded = false;
            bool mShowDemo = false;
            bool mSaveAsPending = false;                // the Save Scene As dialog is requested or open
            bool mSaveAsOpenRequested = false;          // open the dialog's popup on the next draw

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
            /// (Delete Selection builds its entity list at click time and Save Scene As takes the path typed in
            /// its dialog, so neither is listed). Tests validate them against the command registry's schemas.
            /// </summary>
            [[nodiscard]] static std::vector<menu_request> menu_requests();

            /// <summary>
            /// What File > Save Scene and Ctrl+S do: scene.save when the scene has a path, otherwise the Save
            /// Scene As dialog (a scene made with New Scene has no path yet). No-op without a scene and while
            /// playing or paused: saving is an editing-state command, the playing scene is not the one to keep.
            /// </summary>
            void request_save_scene(editor_context &context);

            /// <summary>
            /// Opens the Save Scene As dialog on the next draw, its path field set to the scene's path, or to
            /// "scenes/<scene name>" ("scenes/untitled" for an unnamed scene) when it has none. No-op without a
            /// scene, while playing or paused, or while the dialog is already pending.
            /// </summary>
            void request_save_scene_as(editor_context &context);

            /// <summary>
            /// Saves the scene through scene.save_as (the path is relative to assets:// unless it names a
            /// scheme; .scene.json is added). True on success, which also closes the dialog; on failure the
            /// error text is kept for the dialog (save_as_error()) and false is returned.
            /// </summary>
            bool save_scene_as(editor_context &context, std::string_view path);

            /// <summary>
            /// True from request_save_scene_as() until the dialog saves, is cancelled or loses its scene.
            /// </summary>
            [[nodiscard]] bool save_as_pending() const noexcept {
                return mSaveAsPending;
            }

            /// <summary>
            /// The path in the Save Scene As dialog's field.
            /// </summary>
            [[nodiscard]] const std::string &save_as_path() const noexcept {
                return mSaveAsPath;
            }

            /// <summary>
            /// The error of the last failed save_scene_as() ("" after a success or a new request).
            /// </summary>
            [[nodiscard]] const std::string &save_as_error() const noexcept {
                return mSaveAsError;
            }

            /// <summary>
            /// Answers the question the context's trust_prompt() asks; false, and nothing happens, when it asks
            /// none. With 'trust' (the "Trust and Open" button) the project is trusted through project.trust
            /// and then opened through project.open, the same two requests a protocol client sends: true when
            /// both were accepted. A project.trust that fails leaves the question open and its error in
            /// trust_prompt_error(); a project.open that fails afterwards (playing, say) is logged like any
            /// menu command, and the project stays trusted. Without 'trust' ("Don't Open") the question is
            /// closed, nothing is recorded and the editor stays as it is: true.
            /// </summary>
            bool answer_trust_prompt(editor_context &context, bool trust);

            /// <summary>
            /// The error of the last "Trust and Open" whose project.trust failed ("" otherwise).
            /// </summary>
            [[nodiscard]] const std::string &trust_prompt_error() const noexcept {
                return mTrustPromptError;
            }

        private:
            void draw_menu_bar(editor_context &context);
            void handle_global_shortcuts(editor_context &context);
            void draw_save_as_modal(editor_context &context);
            void draw_trust_prompt(editor_context &context);
            void load_layout_once();
    };
}
