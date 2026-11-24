#pragma once

#include "conjure/scripting/native_api.h"

#include "conjure/core/api.h"
#include "conjure/core/containers/small_vector.h"
#include "conjure/core/enum_flags.h"
#include "conjure/core/json/document.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"

#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

// The script component and its store (docs/design/03_scripting.md D6, 2.2, R29).
//
// script_component is 4 bytes: a generational handle into the script_store the runtime owns. A set holds
// up to MAX_SCRIPTS_PER_ENTITY entries (class hash, managed slot, serialised JSON fields, flags). The
// component registers with custom copy/destruct hooks (deep copy of the set, so archives, duplicates and
// snapshots never alias a store entry) and memcpy relocation; its serializer hooks write and read the
// script set as JSON through the active runtime.

namespace conjure::scripting {

    class script_runtime;

    /// <summary>
    /// The null store handle: zero-filled component memory refers to no set.
    /// </summary>
    inline constexpr u32 INVALID_SET_HANDLE = 0;

    /// <summary>
    /// Managed slot of an entry without a live instance (edit mode, missing class, disabled runtime).
    /// </summary>
    inline constexpr i32 INVALID_SLOT = -1;

    /// <summary>
    /// Scripts one entity may carry.
    /// </summary>
    inline constexpr u32 MAX_SCRIPTS_PER_ENTITY = 8;

    /// <summary>
    /// Registered component name.
    /// </summary>
    inline constexpr std::string_view SCRIPT_COMPONENT_NAME = "conjure.scripting.script_component";

    enum class script_entry_flags : u8 {
        NONE = 0,
        STARTED = 1 << 0,           // OnStart was attempted (or the entry was created ALREADY_STARTED)
        FAULTED = 1 << 1,           // a callback was disabled (three consecutive faults, or OnCreate/OnReload threw)
        CLASS_MISSING = 1 << 2,     // no class for the hash in the active registry; JSON preserved
        CREATED = 1 << 3,           // OnCreate returned this session (not threw); a reload then runs OnReload instead
        INACTIVE = 1 << 4           // the live instance was told its entity is inactive; runtime only, never saved
    };
    CONJURE_ENUM_FLAGS(script_entry_flags);

    struct script_component {
        u32 setHandle = INVALID_SET_HANDLE;

        [[nodiscard]] bool operator==(const script_component &) const noexcept = default;
    };

    static_assert(sizeof(script_component) == 4, "script_component is a 4-byte handle (D6)");
    static_assert(std::is_trivially_copyable_v<script_component>, "script_component relocates with memcpy (D6)");

    struct script_entry {
        u64 classHash = 0;
        i32 managedSlot = INVALID_SLOT;
        core::json::document fields = core::json::document::object();
        script_entry_flags flags = script_entry_flags::NONE;

        /// <summary>
        /// True when a managed instance exists for the entry.
        /// </summary>
        [[nodiscard]] bool is_live() const noexcept {
            return managedSlot != INVALID_SLOT;
        }

        [[nodiscard]] bool has_flag(script_entry_flags flag) const noexcept {
            return conjure::has_flag(flags, flag);
        }
    };

    struct script_set {
        ecs::entity owner = {};
        core::small_vector<script_entry, 2> entries = {};
    };

    /// <summary>
    /// Public handle to one script of one entity: the set handle plus the entry's index in the set.
    /// </summary>
    struct script_entry_id {
        u32 setHandle = INVALID_SET_HANDLE;
        u8 entryIndex = 0;

        [[nodiscard]] bool is_valid() const noexcept {
            return setHandle != INVALID_SET_HANDLE;
        }

        [[nodiscard]] bool operator==(const script_entry_id &) const noexcept = default;
    };

    /// <summary>
    /// Generational storage of script sets plus the managed slot -> entry reverse map. Handles pack a
    /// 20-bit index and a 12-bit generation (1..4095, 0 never issued) into the component's 32 bits.
    /// </summary>
    class CONJURE_API script_store {
        public:
            static constexpr u32 INDEX_BITS = 20;
            static constexpr u32 MAX_SETS = 1u << INDEX_BITS;
            static constexpr u32 GENERATION_MASK = 0xFFFu;

        private:
            struct set_slot {
                script_set set = {};
                u32 generation = 1;
                bool live = false;
            };

            std::vector<set_slot> mSlots = {};
            std::vector<u32> mFreeList = {};
            std::vector<script_entry_id> mSlotToEntry = {};     // indexed by managed slot
            usize mMappedCount = 0;                             // valid records of mSlotToEntry
            usize mLiveCount = 0;

        public:
            script_store();
            ~script_store();

            script_store(const script_store &) = delete;
            script_store &operator=(const script_store &) = delete;
            script_store(script_store &&) noexcept = default;
            script_store &operator=(script_store &&) noexcept = default;

        public:
            /// <summary>
            /// Creates an empty set and returns its handle; INVALID_SET_HANDLE (and an assert) when MAX_SETS
            /// sets are live.
            /// </summary>
            [[nodiscard]] u32 create_set(ecs::entity owner);

            /// <summary>
            /// Deep-copies a set into a new one: same class hashes and JSON, no managed slots, flags reset to
            /// CLASS_MISSING where it was set. INVALID_SET_HANDLE for an invalid source.
            /// </summary>
            [[nodiscard]] u32 clone_set(u32 handle, ecs::entity owner = {});

            /// <summary>
            /// Frees a set and unmaps its live slots. False for a null, stale or unknown handle.
            /// </summary>
            bool free_set(u32 handle);

            [[nodiscard]] script_set *find(u32 handle) noexcept;
            [[nodiscard]] const script_set *find(u32 handle) const noexcept;

            [[nodiscard]] bool contains(u32 handle) const noexcept {
                return find(handle) != nullptr;
            }

            /// <summary>
            /// Appends an entry; an invalid id when the set is unknown or holds MAX_SCRIPTS_PER_ENTITY.
            /// </summary>
            [[nodiscard]] script_entry_id add_entry(u32 handle, script_entry entry);

            /// <summary>
            /// Removes an entry (later entries shift down; the reverse map follows). False when unknown.
            /// </summary>
            bool remove_entry(script_entry_id id);

            [[nodiscard]] script_entry *find_entry(script_entry_id id) noexcept;
            [[nodiscard]] const script_entry *find_entry(script_entry_id id) const noexcept;

            /// <summary>
            /// Binds a managed slot to an entry (and writes the entry's managedSlot).
            /// </summary>
            void map_slot(i32 slot, script_entry_id id);

            /// <summary>
            /// Clears the mapping of a slot (and the entry's managedSlot when it still points at the slot).
            /// </summary>
            void unmap_slot(i32 slot);

            /// <summary>
            /// The entry a managed slot belongs to, or nullopt.
            /// </summary>
            [[nodiscard]] std::optional<script_entry_id> resolve_slot(i32 slot) const noexcept;

            /// <summary>
            /// Number of mapped slots: a counter the mapping functions keep, so stats() costs nothing.
            /// </summary>
            [[nodiscard]] usize mapped_slot_count() const noexcept {
                return mMappedCount;
            }

            /// <summary>
            /// Live sets.
            /// </summary>
            [[nodiscard]] usize size() const noexcept {
                return mLiveCount;
            }

            /// <summary>
            /// Invokes callable(u32 handle, script_set &) for every live set.
            /// </summary>
            void for_each_set(const std::function<void(u32, script_set &)> &callable);

            /// <summary>
            /// Frees every set and clears the reverse map.
            /// </summary>
            void clear();

            /// <summary>
            /// Handle arithmetic.
            /// </summary>
            [[nodiscard]] static constexpr u32 make_handle(u32 index, u32 generation) noexcept {
                return (generation & GENERATION_MASK) << INDEX_BITS | (index & (MAX_SETS - 1));
            }

            [[nodiscard]] static constexpr u32 handle_index(u32 handle) noexcept {
                return handle & (MAX_SETS - 1);
            }

            [[nodiscard]] static constexpr u32 handle_generation(u32 handle) noexcept {
                return handle >> INDEX_BITS;
            }

        private:
            [[nodiscard]] set_slot *live_slot(u32 handle) noexcept;
            [[nodiscard]] const set_slot *live_slot(u32 handle) const noexcept;
    };

    /// <summary>
    /// The lifecycle hooks the component registers: copy deep-copies the set, destruct frees it; both go
    /// through script_runtime::active() and do nothing when no runtime is alive.
    /// </summary>
    [[nodiscard]] CONJURE_API ecs::component_ops script_component_ops() noexcept;

    /// <summary>
    /// The serializer hooks of the component: {"scripts":[{"class","hash","fields"}]} through the active
    /// runtime; remapEntities rewrites ENTITY fields of every entry with the class schema. Without a live
    /// runtime writeJson writes {"scripts":[]} for a component without a set and fails with INVALID_STATE for
    /// one with a set (its scripts would be lost), readJson fails with INVALID_STATE, remapEntities does nothing.
    /// </summary>
    [[nodiscard]] CONJURE_API ecs::component_serializer script_component_serializer() noexcept;

    /// <summary>
    /// Registers script_component (name SCRIPT_COMPONENT_NAME, 4 bytes, custom copy/destruct, memcpy
    /// relocate) and returns its id. Re-registration returns the existing id.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<ecs::component_id> register_script_component(
      ecs::component_registry &registry
    );
}

CONJURE_COMPONENT(conjure::scripting::script_component, "conjure.scripting.script_component");
