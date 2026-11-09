#pragma once

#include "conjure/assets/asset_types.h"

#include "conjure/core/types.h"

#include <atomic>

// The per-uuid slot every asset_ref and asset_handle points at (docs/design/05_assets.md 2.2).
// Slots are allocated once per uuid by the asset_manager, live in a core::stable_vector and are
// never freed before the manager shuts down, so a pointer to one stays valid for the manager's
// lifetime. Everything here is read from any thread; only the manager writes, on the main thread.

namespace conjure::assets {

    /// <summary>
    /// Lock-free view of one asset's residency. Read-only outside the assets module.
    /// </summary>
    struct asset_slot {
        std::atomic<void *> object = nullptr;                      // published payload; nullptr unless LOADED
        std::atomic<u32> version = 0;                              // bumped on every publish and every unload
        std::atomic<asset_state> state = asset_state::UNLOADED;
        std::atomic<i32> strongRefs = 0;                           // live asset_handle count
        std::atomic<asset_type_id> type = asset_type_id::INVALID;  // set at bind; checked against blobs
        asset_id id = {};
        u32 index = 0;                                             // into the manager's slot_data array
    };
}
