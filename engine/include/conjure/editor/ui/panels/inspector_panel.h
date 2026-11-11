#pragma once

#include "conjure/editor/editor_context.h"
#include "conjure/editor/ui/editor_panel.h"
#include "conjure/editor/ui/property_drawer.h"

#include "conjure/core/api.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"

#include <string>
#include <string_view>

// The inspector (docs/design/04_editor.md D11, 3.5): one component.get document per component of the primary
// selection per frame, drawn through the property drawers; edits go back through component.set with a
// coalesce key per field so a drag is one undo entry. Add and remove component live in the panel's menus.

namespace conjure::editor::ui {

    class CONJURE_API inspector_panel final : public editor_panel {
        private:
            const property_drawer_registry *mDrawers = nullptr;
            std::string mAddComponentFilter = "";
            std::string mNameBuffer = "";
            u32 mComponentsDrawn = 0;

        public:
            explicit inspector_panel(const property_drawer_registry &drawers);
            ~inspector_panel() override;

        public:
            void draw_contents(editor_context &context) override;

            [[nodiscard]] u32 components_drawn() const noexcept {
                return mComponentsDrawn;
            }

        private:
            void draw_entity_header(editor_context &context, const core::uuid &guid);
            void draw_component(editor_context &context, const core::uuid &guid, std::string_view typeName);
            void draw_add_component(editor_context &context, const core::uuid &guid);
    };
}
