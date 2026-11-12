#pragma once

#include "conjure/core/api.h"
#include "conjure/core/bit_vector.h"
#include "conjure/core/event.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/entity.h"
#include "conjure/scene/scene.h"

#include <span>
#include <vector>

// The selection (docs/design/04_editor.md D12, 2.5): UI state, never undoable. A sorted guid list plus a
// dense bit set by entity index so the hierarchy panel asks "is this row selected" in O(1), and the primary
// (active) entity: the one named last, as the inspector and the hierarchy's "Add" parent use it.

namespace conjure::editor {

    class CONJURE_API selection_set {
        private:
            std::vector<core::uuid> mGuids = {};            // sorted, unique
            core::uuid mPrimary = core::uuid::null();       // the guid named last by set() / add()
            core::bit_vector mByEntity = {};
            core::event<> mOnChanged = {};

        public:
            selection_set();
            ~selection_set();

            selection_set(const selection_set &) = delete;
            selection_set &operator=(const selection_set &) = delete;

        public:
            /// <summary>
            /// Replaces the selection. Duplicates and null guids are dropped; the last non-null guid in the
            /// caller's order becomes the primary. Emits on_changed when the resulting set or the primary differs
            /// from the current one.
            /// </summary>
            void set(std::span<const core::uuid> guids);

            /// <summary>
            /// Adds guids; the last non-null one (also one already selected) becomes the primary. Emits on_changed
            /// when anything new was added or the primary changed.
            /// </summary>
            void add(std::span<const core::uuid> guids);

            /// <summary>
            /// Removes guids; when the primary leaves, the lowest remaining guid takes its place. Emits
            /// on_changed when anything was removed.
            /// </summary>
            void remove(std::span<const core::uuid> guids);

            /// <summary>
            /// Empties the selection; emits on_changed when it was not empty.
            /// </summary>
            void clear();

            /// <summary>
            /// Rebuilds the entity bit set from the live scene (after structural changes or a restore) and
            /// drops guids no live entity carries (a dropped primary falls back to the lowest remaining guid).
            /// Emits on_changed when guids were dropped.
            /// </summary>
            void rebuild(const scene::scene &target);

            [[nodiscard]] bool contains(const core::uuid &guid) const noexcept;

            /// <summary>
            /// True when the entity's index is marked by the last rebuild().
            /// </summary>
            [[nodiscard]] bool contains(ecs::entity entity) const noexcept;

            [[nodiscard]] std::span<const core::uuid> guids() const noexcept {
                return mGuids;
            }

            [[nodiscard]] usize size() const noexcept {
                return mGuids.size();
            }

            [[nodiscard]] bool empty() const noexcept {
                return mGuids.empty();
            }

            /// <summary>
            /// The active entity: the guid the caller named last in the set() or add() call that last changed
            /// it; when that guid leaves the selection (remove(), rebuild()), the lowest remaining guid; the null
            /// uuid when the selection is empty.
            /// </summary>
            [[nodiscard]] core::uuid primary() const noexcept;

            [[nodiscard]] core::event<> &on_changed() noexcept {
                return mOnChanged;
            }

        private:
            /// <summary>
            /// After guids left the selection: keeps the primary while it is still selected, otherwise falls
            /// back to the lowest remaining guid (null when empty).
            /// </summary>
            void refresh_primary() noexcept;
    };
}
