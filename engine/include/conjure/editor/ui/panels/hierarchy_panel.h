#pragma once

#include "conjure/editor/editor_context.h"
#include "conjure/editor/ui/editor_panel.h"
#include "conjure/editor/ui/hierarchy_cache.h"

#include "conjure/core/api.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/entity.h"

#include <string>

// The scene tree (docs/design/04_editor.md D11): flat rows from hierarchy_cache drawn with a list clipper,
// click, ctrl-click (toggle) and shift-click (range from the last clicked row) selection, drag and drop
// reparenting (entity.set_parent, dropping below the rows moves to the root), a context menu for create,
// duplicate, rename, enable and destroy, inline rename (F2 or the menu; Enter or clicking away commits, Escape
// cancels) and a name filter. A selection made elsewhere (a viewport pick) expands its ancestors and scrolls
// its row into view.

namespace conjure::editor::ui {

    class CONJURE_API hierarchy_panel final : public editor_panel {
        private:
            hierarchy_cache mCache = {};
            std::string mRenameBuffer = "";
            ecs::entity mRenaming = {};
            core::uuid mAnchor = {};                    // the row a shift-click range starts from
            core::uuid mLastPrimary = {};               // the primary selection the last update saw
            core::uuid mScrollTarget = {};              // scroll this entity's row into view on the next draw
            u64 mSeenRevision = 0;
            u32 mRowsDrawn = 0;
            bool mRenameFocusPending = false;
            bool mSelectedHere = false;                 // the tree changed the selection since the last update

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

            /// <summary>
            /// Starts the inline rename of an entity's row (what F2 and the context menu's Rename do): reveals the
            /// row, fills the field with the current name and focuses it on the next draw. No-op for an entity
            /// the scene does not hold.
            /// </summary>
            void begin_rename(editor_context &context, ecs::entity entity);

            /// <summary>
            /// True while a row shows the inline rename field.
            /// </summary>
            [[nodiscard]] bool is_renaming() const noexcept {
                return !mRenaming.is_null();
            }

        private:
            void draw_row(editor_context &context, const hierarchy_row &row, usize index);
            void draw_rename_field(editor_context &context, const hierarchy_row &row);
            void draw_context_menu(editor_context &context, ecs::entity entity);
            void scroll_to_target(editor_context &context, f32 rowHeight);
            void select_row(editor_context &context, ecs::entity entity, bool additive);
            void select_range(editor_context &context, ecs::entity entity, bool additive);
    };
}
