#pragma once

#include "conjure/editor/selection_set.h"

#include "conjure/core/api.h"
#include "conjure/core/bit_vector.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/entity.h"
#include "conjure/scene/scene.h"

#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

// The hierarchy panel's flat row list (docs/design/04_editor.md D11, 2.5): one pre-order walk per structural
// change, expansion state (by scene guid) and name filter, drawn with a list clipper so 10,000 entities cost
// O(visible rows). Names and enabled state, which change without a structural change, are refreshed for the
// visible rows only (refresh_labels).

namespace conjure::editor::ui {

    enum class hierarchy_row_flags : u8 {
        NONE = 0,
        HAS_CHILDREN = 1 << 0,
        EXPANDED = 1 << 1,
        SELECTED = 1 << 2,
        PREFAB_INSTANCE = 1 << 3,
        DISABLED = 1 << 4
    };
    CONJURE_ENUM_FLAGS(hierarchy_row_flags);

    struct hierarchy_row {
        ecs::entity entity = {};
        core::string_id nameId = {};
        u16 depth = 0;
        hierarchy_row_flags flags = hierarchy_row_flags::NONE;
        u8 padding = 0;
    };

    static_assert(sizeof(hierarchy_row) <= 24, "hierarchy_row stays compact");

    class CONJURE_API hierarchy_cache {
        private:
            std::vector<hierarchy_row> mRows = {};
            std::unordered_set<core::uuid> mExpanded = {};  // by scene guid (D5): survives a play/stop restore
                                                            // and never leaks onto a recycled entity index
            core::bit_vector mMatches = {};                 // filter matches by entity index
            std::string mFilter = "";
            u64 mStructureVersion = 0;
            u32 mRebuilds = 0;
            bool mDirty = true;

        public:
            hierarchy_cache();
            ~hierarchy_cache();

            hierarchy_cache(const hierarchy_cache &) = delete;
            hierarchy_cache &operator=(const hierarchy_cache &) = delete;

        public:
            /// <summary>
            /// Rebuilds when the scene's structure version, the expansion set or the filter changed since the
            /// last call. Returns true when a rebuild happened.
            /// </summary>
            bool refresh_if_dirty(const scene::scene &target, const selection_set &selection);

            /// <summary>
            /// Forces a rebuild at the next refresh (selection changes).
            /// </summary>
            void invalidate() noexcept {
                mDirty = true;
            }

            /// <summary>
            /// Case-insensitive substring filter on names; rows whose subtree holds a match are kept.
            /// </summary>
            void set_filter(std::string_view filter);

            [[nodiscard]] const std::string &filter() const noexcept {
                return mFilter;
            }

            /// <summary>
            /// Expands or collapses an entity's row. The state is keyed by the entity's guid, so it follows the
            /// entity through a snapshot restore and is never inherited by an entity that reuses the index. A
            /// null entity or one without a guid is ignored.
            /// </summary>
            void set_expanded(const scene::scene &target, ecs::entity entity, bool expanded);

            /// <summary>
            /// True when the entity's guid is marked expanded (false for a null entity).
            /// </summary>
            [[nodiscard]] bool is_expanded(const scene::scene &target, ecs::entity entity) const;

            /// <summary>
            /// Expands every ancestor of the entity so its row is visible.
            /// </summary>
            void reveal(const scene::scene &target, ecs::entity entity);

            /// <summary>
            /// Re-reads the names and enabled state of the rows already built in [first, last) (O(range), no
            /// rebuild): renames and enable toggles do not change the structure version. Rows of entities that
            /// no longer exist are left as they are; 'last' is clamped to the row count.
            /// </summary>
            void refresh_labels(const scene::scene &target, usize first, usize last) noexcept;

            [[nodiscard]] std::span<const hierarchy_row> rows() const noexcept {
                return mRows;
            }

            [[nodiscard]] u32 rebuild_count() const noexcept {
                return mRebuilds;
            }

            [[nodiscard]] u64 structure_version() const noexcept {
                return mStructureVersion;
            }

        private:
            void rebuild(const scene::scene &target, const selection_set &selection);
            void compute_matches(const scene::scene &target);
            [[nodiscard]] bool subtree_matches(const scene::scene &target, ecs::entity entity) const;
    };
}
