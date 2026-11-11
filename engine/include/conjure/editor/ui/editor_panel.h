#pragma once

#include "conjure/editor/editor_context.h"
#include "conjure/editor/panel_desc.h"

#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Panels (docs/design/04_editor.md 4.2): one ImGui window each, drawn by the workspace every frame while
// open. Panels own only UI state and submit command_requests through the context's dispatcher. The registry
// lists the built-in panels and the ones loaded modules provide (panel_desc records).

namespace conjure::editor::ui {

    class CONJURE_API editor_panel {
        private:
            std::string mTitle = "";
            bool mOpen = true;

        public:
            explicit editor_panel(std::string title, bool open = true);
            virtual ~editor_panel();

            editor_panel(const editor_panel &) = delete;
            editor_panel &operator=(const editor_panel &) = delete;

        public:
            [[nodiscard]] std::string_view title() const noexcept {
                return mTitle;
            }

            [[nodiscard]] bool is_open() const noexcept {
                return mOpen;
            }

            void set_open(bool open) noexcept {
                mOpen = open;
            }

            /// <summary>
            /// Begins the ImGui window (with the close button bound to the open flag), draws the body and
            /// ends it. Does nothing while closed.
            /// </summary>
            void draw(editor_context &context);

            /// <summary>
            /// The body between ImGui::Begin and ImGui::End.
            /// </summary>
            virtual void draw_contents(editor_context &context) = 0;

            /// <summary>
            /// Called once per frame before drawing, open or not (caches, polling).
            /// </summary>
            virtual void update(editor_context &context);
    };

    /// <summary>
    /// Creates a panel instance.
    /// </summary>
    using panel_factory = std::function<std::unique_ptr<editor_panel>()>;

    struct panel_descriptor {
        std::string name = "";                              // "hierarchy"
        std::string title = "";                             // "Hierarchy"
        std::string owner = "editor";                       // module name for module panels
        panel_factory create = {};
        bool openByDefault = true;
    };

    class CONJURE_API panel_registry {
        private:
            std::vector<panel_descriptor> mDescriptors = {};

        public:
            panel_registry();
            ~panel_registry();

            panel_registry(const panel_registry &) = delete;
            panel_registry &operator=(const panel_registry &) = delete;

        public:
            /// <summary>
            /// Adds a descriptor; a duplicate name replaces the previous one.
            /// </summary>
            void add(panel_descriptor descriptor);

            /// <summary>
            /// Wraps a module panel record into a descriptor named "<owner>/<title>".
            /// </summary>
            void add_module_panel(std::string_view owner, const panel_desc &desc);

            /// <summary>
            /// Removes every descriptor of an owner and returns how many.
            /// </summary>
            usize remove_owned_by(std::string_view owner);

            [[nodiscard]] const panel_descriptor *find(std::string_view name) const noexcept;

            [[nodiscard]] std::span<const panel_descriptor> descriptors() const noexcept {
                return mDescriptors;
            }

            [[nodiscard]] usize size() const noexcept {
                return mDescriptors.size();
            }
    };
}
