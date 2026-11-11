#pragma once

#include "conjure/editor/editor_context.h"
#include "conjure/editor/ui/editor_panel.h"
#include "conjure/editor/ui/hierarchy_cache.h"

#include "conjure/core/api.h"
#include "conjure/core/types.h"
#include "conjure/ecs/entity.h"

#include <string>

// The scene tree (docs/design/04_editor.md D11): flat rows from hierarchy_cache drawn with a list clipper,
// click and shift-click selection, drag and drop reparenting (entity.set_parent), a context menu for create,
// duplicate, rename, enable and destroy, and a name filter.

namespace conjure::editor::ui {

    class CONJURE_API hierarchy_panel final : public editor_panel {
        private:
            hierarchy_cache mCache = {};
            std::string mRenameBuffer = "";
            ecs::entity mRenaming = {};
            u32 mRowsDrawn = 0;

        public:
            hierarchy_panel();
            ~hierarchy_panel() override;

        public:
            void update(editor_context &context) override;
            void draw_contents(editor_context &context) override;

            [[nodiscard]] hierarchy_cache &cache() noexcept {
                return mCache;
            }

            /// <summary>
            /// Rows drawn by the last draw_contents (the clipper's visible range).
            /// </summary>
            [[nodiscard]] u32 rows_drawn() const noexcept {
                return mRowsDrawn;
            }

        private:
            void draw_row(editor_context &context, const hierarchy_row &row, usize index);
            void draw_context_menu(editor_context &context, ecs::entity entity);
            void select_row(editor_context &context, ecs::entity entity, bool additive);
    };
}
