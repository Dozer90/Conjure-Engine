#pragma once

#include "conjure/core/string_id.h"

#include <type_traits>

// Module-provided panels (docs/design/04_editor.md D13, R14): a trivially copyable extension record. The
// editor's panel registry wraps the draw function into an editor_panel drawn inside the workspace with the
// one ImGui context the engine exports (07_modules.md D1); the runtime ignores the record.

namespace conjure::editor {

    /// <summary>
    /// Draws the panel body. Called every frame the panel is open, between ImGui::Begin and ImGui::End of
    /// the panel window, with the current ImGui context as argument (ImGui::GetCurrentContext()).
    /// </summary>
    using panel_draw_fn = void (*)(void *imguiContext, void *userData);

    struct panel_desc {
        static constexpr core::string_id EXTENSION_POINT{"editor.panel"};

        const char *title = nullptr;            // window title, unique per module
        panel_draw_fn draw = nullptr;
        void *userData = nullptr;
        bool openByDefault = true;
    };

    static_assert(std::is_trivially_copyable_v<panel_desc>, "panel_desc crosses the module ABI");
}
