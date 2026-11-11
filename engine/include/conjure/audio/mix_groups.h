#pragma once

#include "conjure/audio/audio_types.h"

#include "conjure/core/api.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#include <compare>
#include <type_traits>

// Mix groups (docs/design/12_audio.md 2.4): a gain tree rooted at MASTER_GROUP. The main thread
// owns the tree and sends each group's effective gain (the product up the chain) to the mixer.

namespace conjure::audio {

    /// <summary>
    /// Handle of a mix group: slot index plus the generation issued when the slot was last created.
    /// remove_group() bumps the generation so stale ids are rejected.
    /// </summary>
    struct group_id {
        u8 index = 0;
        u8 generation = 0;

        /// <summary>
        /// True for every id the engine issued; false for the null id.
        /// </summary>
        [[nodiscard]] constexpr bool is_valid() const noexcept {
            return generation != 0;
        }

        [[nodiscard]] constexpr auto operator<=>(const group_id &) const noexcept = default;
        [[nodiscard]] constexpr bool operator==(const group_id &) const noexcept = default;
    };

    static_assert(sizeof(group_id) == 2, "group_id is two bytes");
    static_assert(std::is_trivially_copyable_v<group_id>, "group_id must be trivially copyable");

    /// <summary>
    /// The root group, always present, never removable.
    /// </summary>
    inline constexpr group_id MASTER_GROUP = {0, 1};

    /// <summary>
    /// What create_group() needs. The parent is given by id, or by name through parentName when
    /// groups are declared in settings before any id exists (a null parentName means "use parent").
    /// </summary>
    struct group_desc {
        core::string_id name = {};
        group_id parent = MASTER_GROUP;
        core::string_id parentName = {};
        f32 gain = 1.0f;                    // [0, 2]

        [[nodiscard]] bool operator==(const group_desc &other) const noexcept = default;
    };

    /// <summary>
    /// Main-thread view of a group.
    /// </summary>
    struct group_info {
        group_id id = {};
        core::string_id name = {};
        group_id parent = {};
        f32 gain = 1.0f;
        f32 effectiveGain = 1.0f;           // gain * parent's effectiveGain
        bool paused = false;

        [[nodiscard]] bool operator==(const group_info &other) const noexcept = default;
    };

    /// <summary>
    /// Clamp rule of A15 for group gains: [0, 2], NaN becomes 1.
    /// </summary>
    [[nodiscard]] CONJURE_API f32 clamp_group_gain(f32 gain) noexcept;
}
