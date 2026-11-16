#pragma once

#include "conjure/editor/editor_context.h"
#include "conjure/editor/ui/editor_panel.h"

#include "conjure/assets/asset_database.h"
#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <string>
#include <string_view>

// The asset browser: the database's records as a filterable table (path, type, status), import by path,
// reimport, delete, open scenes and instantiate prefabs; all through asset.*, scene.open and prefab.instantiate.
// Deleting the file behind the open scene asks first: a confirmation dialog closes the scene and deletes its
// file (code sweep F656); every other asset is deleted at once and comes back with undo.

namespace conjure::editor::ui {

    class CONJURE_API asset_browser_panel final : public editor_panel {
        private:
            std::string mFilter = "";
            std::string mImportPath = "";
            std::string mSelectedPath = "";
            std::string mPendingDeleteUuid = "";        // the open scene's asset, waiting for the confirmation
            std::string mPendingDeletePath = "";
            u32 mRowsDrawn = 0;
            bool mConfirmOpenRequested = false;         // the confirmation dialog opens on the next draw

        public:
            asset_browser_panel();
            ~asset_browser_panel() override;

        public:
            void draw_contents(editor_context &context) override;

            /// <summary>
            /// The Delete action of a row (tests call it directly). An asset is deleted at once through asset.delete.
            /// The file behind the open scene is not: it becomes the pending delete, and the next draw opens the
            /// confirmation dialog whose buttons are confirm_pending_delete() and cancel_pending_delete().
            /// </summary>
            void request_delete(editor_context &context, std::string_view uuid, std::string_view path);

            /// <summary>
            /// True while the confirmation for deleting the open scene waits for an answer.
            /// </summary>
            [[nodiscard]] bool has_pending_delete() const noexcept {
                return !mPendingDeleteUuid.empty();
            }

            /// <summary>
            /// The asset path the confirmation asks about, "" when none is pending.
            /// </summary>
            [[nodiscard]] const std::string &pending_delete_path() const noexcept {
                return mPendingDeletePath;
            }

            /// <summary>
            /// The dialog's "Close and Delete": one asset.delete with closeOpenScene, which deletes the scene's
            /// file and closes the scene (unsaved changes are dropped). Nothing happens when no delete is pending.
            /// While playing or paused the command refuses closeOpenScene, so nothing is sent and the delete stays
            /// pending: the dialog disables the button and asks to stop playing first.
            /// </summary>
            void confirm_pending_delete(editor_context &context);

            /// <summary>
            /// The dialog's "Cancel": forgets the pending delete; the scene and its file stay.
            /// </summary>
            void cancel_pending_delete() noexcept;

            [[nodiscard]] u32 rows_drawn() const noexcept {
                return mRowsDrawn;
            }

            [[nodiscard]] const std::string &selected_path() const noexcept {
                return mSelectedPath;
            }

        private:
            void draw_table(editor_context &context, const assets::asset_database &database);
            void draw_delete_confirmation(editor_context &context);
    };
}
