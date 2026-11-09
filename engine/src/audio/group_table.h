#pragma once

#include "conjure/audio/mix_groups.h"

#include "conjure/core/containers/small_vector.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#include <array>
#include <optional>
#include <span>
#include <vector>

// Main-thread owner of the group tree (docs/design/12_audio.md 2.4, 3.5): MAX_GROUPS slots with
// generations, MASTER_GROUP always live in slot 0, effective gains recomputed on every change. The
// engine turns the returned change lists into SET_GROUP_GAIN commands.

namespace conjure::audio {

    /// <summary>
    /// Slot indices whose effective gain changed, in depth-first order.
    /// </summary>
    using group_change_list = core::small_vector<u8, MAX_GROUPS>;

    /// <summary>
    /// What remove() leaves behind: where children and voices moved, and whose gains changed.
    /// </summary>
    struct group_removal {
        u8 parentIndex = 0;
        group_change_list changedGains = {};
    };

    class group_table {
        private:
            struct slot {
                bool live = false;
                u8 generation = 0;
                core::string_id name = {};
                u8 parentIndex = 0;
                f32 gain = 1.0f;
                f32 effectiveGain = 1.0f;
                bool paused = false;
            };

            std::array<slot, MAX_GROUPS> mSlots = {};
            std::vector<group_info> mInfos = {};
            u32 mLiveCount = 0;

        public:
            group_table();
            ~group_table();

            group_table(const group_table &) = delete;
            group_table &operator=(const group_table &) = delete;

        public:
            /// <summary>
            /// Creates a group. The parent is desc.parentName when non-null, else desc.parent.
            /// Errors: GROUP_LIMIT_REACHED, DUPLICATE_GROUP (null or taken name), INVALID_GROUP
            /// (unknown parent), GROUP_CYCLE (depth above MAX_GROUP_DEPTH).
            /// </summary>
            [[nodiscard]] core::result<group_id> create(const group_desc &desc);

            /// <summary>
            /// Retires a group: children are reparented to its parent (their effective gains
            /// recomputed) and the slot's generation is bumped. INVALID_GROUP for master or a stale id.
            /// </summary>
            [[nodiscard]] core::result<group_removal> remove(group_id id);

            /// <summary>
            /// True for MASTER_GROUP and every id of a live slot with a matching generation.
            /// </summary>
            [[nodiscard]] bool is_valid(group_id id) const noexcept;

            /// <summary>
            /// MASTER_GROUP for the null name, the group's id, or the null id when unknown.
            /// </summary>
            [[nodiscard]] group_id find(core::string_id name) const noexcept;

            /// <summary>
            /// The current id of a live slot; the null id for a free slot.
            /// </summary>
            [[nodiscard]] group_id id_of(u8 index) const noexcept;

            /// <summary>
            /// Stores a (clamped) gain and recomputes the subtree. Returns the slots whose effective
            /// gain changed; empty for an invalid id or an unchanged gain.
            /// </summary>
            [[nodiscard]] group_change_list set_gain(group_id id, f32 gain);

            /// <summary>
            /// Flags the group paused; no-op for an invalid id. Returns whether the flag changed.
            /// </summary>
            bool set_paused(group_id id, bool paused) noexcept;

            [[nodiscard]] f32 effective_gain(u8 index) const noexcept;
            [[nodiscard]] bool paused(u8 index) const noexcept;
            [[nodiscard]] std::optional<group_info> info(group_id id) const noexcept;

            /// <summary>
            /// Every live group, master first, in slot order.
            /// </summary>
            [[nodiscard]] std::span<const group_info> infos() const noexcept {
                return mInfos;
            }

            [[nodiscard]] u32 live_count() const noexcept {
                return mLiveCount;
            }

        private:
            [[nodiscard]] const slot *resolve(group_id id) const noexcept;
            [[nodiscard]] u32 depth_of(u8 index) const noexcept;
            void recompute_subtree(u8 rootIndex, group_change_list &changed);
            void rebuild_infos();
    };
}
