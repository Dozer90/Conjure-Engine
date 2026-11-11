#pragma once

#include "conjure/editor/selection_set.h"

#include "conjure/core/api.h"
#include "conjure/core/bit_vector.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/ecs/entity.h"
#include "conjure/scene/scene.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

// The hierarchy panel's flat row list (docs/design/04_editor.md D11, 2.5): one pre-order walk per structural
// change, expansion state and name filter, drawn with a list clipper so 10,000 entities cost O(visible rows).

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
            core::bit_vector mExpanded = {};                // by entity index
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

            void set_expanded(ecs::entity entity, bool expanded);
            [[nodiscard]] bool is_expanded(ecs::entity entity) const noexcept;

            /// <summary>
            /// Expands every ancestor of the entity so its row is visible.
            /// </summary>
            void reveal(const scene::scene &target, ecs::entity entity);

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
