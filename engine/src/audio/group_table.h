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
// generations, MASTER_GROUP always live in slot 0, effective gains and effective paused flags (a
// group's own flag or any ancestor's, A17) recomputed on every change. The engine turns the returned
// change lists into SET_GROUP_GAIN and SET_GROUP_PAUSED commands.

namespace conjure::audio {

    /// <summary>
    /// Slot indices whose effective gain or effective paused flag changed, in depth-first order.
    /// </summary>
    using group_change_list = core::small_vector<u8, MAX_GROUPS>;

    /// <summary>
    /// What remove() leaves behind: where children and voices moved, whose effective gains changed
    /// and whose effective paused flags changed (the retired slot itself is in neither list).
    /// </summary>
    struct group_removal {
        u8 parentIndex = 0;
        group_change_list changedGains = {};
        group_change_list changedPaused = {};
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
                bool effectivePaused = false;
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
            /// Creates a group. The parent is desc.parentName when non-null, else desc.parent. The
            /// group starts with its own paused flag clear and its parent's effective paused flag.
            /// Errors: GROUP_LIMIT_REACHED, DUPLICATE_GROUP (null or taken name), INVALID_GROUP
            /// (unknown parent), GROUP_CYCLE (depth above MAX_GROUP_DEPTH).
            /// </summary>
            [[nodiscard]] core::result<group_id> create(const group_desc &desc);

            /// <summary>
            /// Retires a group: children are reparented to its parent (their effective gains and
            /// effective paused flags recomputed, so a pause the retired group held goes with it) and
            /// the id becomes stale. INVALID_GROUP for master or a stale id.
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
            /// Stores the group's own paused flag and recomputes the subtree. Returns the slots whose
            /// effective paused flag changed; empty for an invalid id, an unchanged flag, or a group
            /// that an ancestor already holds paused (its own flag still changes).
            /// </summary>
            [[nodiscard]] group_change_list set_paused(group_id id, bool paused);

            /// <summary>
            /// The product of the gains from the slot up to master; 1 for a free slot.
            /// </summary>
            [[nodiscard]] f32 effective_gain(u8 index) const noexcept;

            /// <summary>
            /// True when the slot or any of its ancestors is flagged paused, which is what the mixer
            /// is told; false for a free slot.
            /// </summary>
            [[nodiscard]] bool effective_paused(u8 index) const noexcept;

            /// <summary>
            /// The main-thread view of a live group (own and effective gain, own and effective paused
            /// flag); nullopt for a stale id.
            /// </summary>
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
            void recompute_subtree(u8 rootIndex, group_change_list &changedGains, group_change_list &changedPaused);
            void rebuild_infos();
    };
}
