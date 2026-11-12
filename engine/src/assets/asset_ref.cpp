#include "conjure/assets/asset_ref.h"
#include "conjure/assets/log_category.h"

#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/core/log.h"

#include <atomic>
#include <mutex>
#include <utility>

using namespace conjure;
using namespace conjure::assets;

/*===========================================================================*/
/*===========================================================================*/
// Type mismatch reporting

namespace {
    struct mismatch_key {
        asset_id id = {};
        asset_type_id requested = asset_type_id::INVALID;

        [[nodiscard]] bool operator==(const mismatch_key &) const = default;
    };

    struct mismatch_key_hash {
        [[nodiscard]] usize operator()(const mismatch_key &key) const noexcept {
            return key.id.hash() ^ (static_cast<u64>(key.requested) * 0x9E3779B97F4A7C15ull);
        }
    };

    std::mutex &mismatch_mutex() {
        static std::mutex sMutex = {};
        return sMutex;
    }

    core::flat_hash_set<mismatch_key, mismatch_key_hash> &reported_mismatches() {
        static core::flat_hash_set<mismatch_key, mismatch_key_hash> sReported = {};
        return sReported;
    }
}

void conjure::assets::detail::report_type_mismatch(const asset_id &id, asset_type_id requested, asset_type_id actual) {
    bool first = false;
    {
        const std::scoped_lock lock(mismatch_mutex());
        first = reported_mismatches().insert(mismatch_key{id, requested}).second;
    }
    if (!first) {
        return;
    }
    CONJURE_LOG_ERROR(
      LOG_ASSETS,
      "asset {} is of type {:#x} but was accessed as type {:#x}",
      id,
      static_cast<u64>(actual),
      static_cast<u64>(requested)
    );
}

void conjure::assets::detail::forget_type_mismatches() noexcept {
    const std::scoped_lock lock(mismatch_mutex());
    reported_mismatches().clear();
}

/*===========================================================================*/
/*===========================================================================*/
// asset_handle

asset_handle::asset_handle(asset_slot *slot) noexcept :
    mSlot(slot) {
    if (mSlot != nullptr) {
        mSlot->strongRefs.fetch_add(1, std::memory_order_acq_rel);
    }
}

asset_handle::~asset_handle() {
    reset();
}

asset_handle::asset_handle(const asset_handle &other) noexcept :
    mSlot(other.mSlot) {
    if (mSlot != nullptr) {
        mSlot->strongRefs.fetch_add(1, std::memory_order_acq_rel);
    }
}

asset_handle &asset_handle::operator=(const asset_handle &other) noexcept {
    if (this == &other || mSlot == other.mSlot) {
        return *this;
    }
    reset();
    mSlot = other.mSlot;
    if (mSlot != nullptr) {
        mSlot->strongRefs.fetch_add(1, std::memory_order_acq_rel);
    }
    return *this;
}

asset_handle::asset_handle(asset_handle &&other) noexcept :
    mSlot(std::exchange(other.mSlot, nullptr)) {}

asset_handle &asset_handle::operator=(asset_handle &&other) noexcept {
    if (this == &other) {
        return *this;
    }
    reset();
    mSlot = std::exchange(other.mSlot, nullptr);
    return *this;
}

asset_id asset_handle::id() const noexcept {
    if (mSlot == nullptr) {
        return asset_id();
    }
    return mSlot->id;
}

asset_type_id asset_handle::type() const noexcept {
    if (mSlot == nullptr) {
        return asset_type_id::INVALID;
    }
    return mSlot->type.load(std::memory_order_acquire);
}

asset_state asset_handle::state() const noexcept {
    if (mSlot == nullptr) {
        return asset_state::UNLOADED;
    }
    return mSlot->state.load(std::memory_order_acquire);
}

bool asset_handle::is_terminal() const noexcept {
    if (mSlot == nullptr) {
        return true;
    }
    return assets::is_terminal(mSlot->state.load(std::memory_order_acquire));
}

u32 asset_handle::version() const noexcept {
    if (mSlot == nullptr) {
        return 0;
    }
    return mSlot->version.load(std::memory_order_acquire);
}

void asset_handle::reset() noexcept {
    if (mSlot == nullptr) {
        return;
    }
    mSlot->strongRefs.fetch_sub(1, std::memory_order_acq_rel);
    mSlot = nullptr;
}
