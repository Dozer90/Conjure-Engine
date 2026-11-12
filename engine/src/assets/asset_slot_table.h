#pragma once

#include "conjure/assets/asset_payload.h"
#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_slot.h"
#include "conjure/assets/asset_types.h"

#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/core/containers/stable_vector.h"
#include "conjure/core/error.h"
#include "conjure/core/jobs/job_handle.h"
#include "conjure/core/types.h"

#include <optional>
#include <vector>

// The manager's slot storage (docs/design/05_assets.md 2.2): the public, lock-free asset_slot
// per uuid plus the main-thread-only slot_data behind it. Both live in stable_vectors so every
// pointer handed out stays valid until the manager is destroyed.

namespace conjure::assets::detail {

    /// <summary>
    /// Main-thread state of one slot.
    /// </summary>
    struct slot_data {
        asset_payload payload = make_null_payload();                 // the published payload
        asset_payload cpuOutput = make_null_payload();               // between completion and finalise
        std::vector<asset_id> dependencyIds = {};                    // from the last completed blob
        std::vector<asset_handle> dependencyHandles = {};            // held while FINALISING/LOADED
        std::vector<asset_handle> pendingDependencyHandles = {};     // for the load in flight
        std::optional<core::error> lastError = std::nullopt;
        core::jobs::job_handle job = {};                             // kept until its completion drains
        asset_load_priority priority = asset_load_priority::NORMAL;
        u64 sequence = 0;                                            // FIFO order within a priority
        u64 payloadBytes = 0;                                        // cooked size of the published payload
        u64 pendingPayloadBytes = 0;                                 // cooked size of the load in flight
        u32 generation = 0;                                          // identifies the load in flight
        bool queued = false;                                         // has a live entry in a queue
        bool jobOutstanding = false;                                 // a job was scheduled and has not drained
        bool awaitingFinalise = false;                               // stage done: dependencyIds are authoritative
        bool inFailedList = false;                                   // listed in the manager's failed slots
        bool pinned = false;
        bool isReload = false;
        bool dependencyFailureWarned = false;
        bool notFoundLogged = false;
    };

    /// <summary>
    /// Slots by uuid. Creation is main-thread only; lookups by pointer are lock-free.
    /// </summary>
    class asset_slot_table {
        private:
            core::stable_vector<asset_slot> mSlots = {};
            core::stable_vector<slot_data> mData = {};
            core::flat_hash_map<asset_id, u32> mIndexById = {};

        public:
            asset_slot_table();
            ~asset_slot_table();

            asset_slot_table(const asset_slot_table &) = delete;
            asset_slot_table &operator=(const asset_slot_table &) = delete;

        public:
            [[nodiscard]] asset_slot *find(const asset_id &id) noexcept;
            [[nodiscard]] const asset_slot *find(const asset_id &id) const noexcept;

            /// <summary>
            /// The slot of a uuid, created on first use. The null uuid never gets a slot.
            /// </summary>
            [[nodiscard]] asset_slot *find_or_create(const asset_id &id, bool &outCreated);

            [[nodiscard]] slot_data &data(const asset_slot &slot) noexcept;
            [[nodiscard]] const slot_data &data(const asset_slot &slot) const noexcept;

            [[nodiscard]] asset_slot &at(u32 index) noexcept;
            [[nodiscard]] const asset_slot &at(u32 index) const noexcept;

            [[nodiscard]] u32 size() const noexcept {
                return static_cast<u32>(mSlots.size());
            }
    };
}
