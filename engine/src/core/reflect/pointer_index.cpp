#include "pointer_index.h"

#include "conjure/core/assert.h"
#include "conjure/core/types.h"

#include <atomic>
#include <memory>
#include <utility>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::reflect::detail;

namespace {
    /// <summary>
    /// Spreads the key over the table: FNV output is already uniform, so folding the high bits in
    /// is enough to keep clustered low bits from probing in lockstep.
    /// </summary>
    constexpr usize home_slot(u64 key, usize capacity) noexcept {
        return static_cast<usize>(key ^ (key >> 29)) & (capacity - 1);
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

pointer_index::pointer_index() {
    add_table(INITIAL_CAPACITY);
}

pointer_index::~pointer_index() {}

/*===========================================================================*/
/*===========================================================================*/
// Public methods

const void *pointer_index::find(u64 key) const noexcept {
    if (key == 0) {
        return nullptr;
    }

    for (const table *current = mHead; current != nullptr; current = current->next.load(std::memory_order_acquire)) {
        const usize mask = current->capacity - 1;
        usize index = home_slot(key, current->capacity);
        for (usize probe = 0; probe < current->capacity; ++probe) {
            const slot &candidate = current->slots[index];
            const u64 storedKey = candidate.key.load(std::memory_order_acquire);
            if (storedKey == 0) {
                // The probe chain of this table ends here; the key may live in a later table.
                break;
            }
            if (storedKey == key) {
                return candidate.value.load(std::memory_order_acquire);
            }
            index = (index + 1) & mask;
        }
    }
    return nullptr;
}

void pointer_index::insert(u64 key, const void *value) {
    CONJURE_ASSERT(key != 0, "pointer_index: key 0 is reserved for empty slots");
    CONJURE_ASSERT(value != nullptr, "pointer_index: a null value means 'erased'; use erase()");
    if (key == 0 || value == nullptr) {
        return;
    }

    // Re-insertion (after erase or a replacement) reuses the existing slot in whichever table has it.
    for (table *current = mHead; current != nullptr; current = current->next.load(std::memory_order_relaxed)) {
        slot *existing = find_slot(*current, key);
        if (existing != nullptr) {
            existing->value.store(value, std::memory_order_release);
            return;
        }
    }

    if (mTail->used * 2 >= mTail->capacity) {
        add_table(mTail->capacity * 2);
    }

    const usize mask = mTail->capacity - 1;
    usize index = home_slot(key, mTail->capacity);
    while (mTail->slots[index].key.load(std::memory_order_relaxed) != 0) {
        index = (index + 1) & mask;
    }

    // Publish the value before the key: a reader that observes the key then sees the value.
    slot &target = mTail->slots[index];
    target.value.store(value, std::memory_order_relaxed);
    target.key.store(key, std::memory_order_release);
    ++mTail->used;
}

void pointer_index::erase(u64 key) noexcept {
    for (table *current = mHead; current != nullptr; current = current->next.load(std::memory_order_relaxed)) {
        slot *existing = find_slot(*current, key);
        if (existing != nullptr) {
            existing->value.store(nullptr, std::memory_order_release);
            return;
        }
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Private methods

pointer_index::slot *pointer_index::find_slot(table &target, u64 key) noexcept {
    if (key == 0) {
        return nullptr;
    }
    const usize mask = target.capacity - 1;
    usize index = home_slot(key, target.capacity);
    for (usize probe = 0; probe < target.capacity; ++probe) {
        slot &candidate = target.slots[index];
        const u64 storedKey = candidate.key.load(std::memory_order_relaxed);
        if (storedKey == 0) {
            return nullptr;
        }
        if (storedKey == key) {
            return &candidate;
        }
        index = (index + 1) & mask;
    }
    return nullptr;
}

void pointer_index::add_table(usize capacity) {
    std::unique_ptr<table> fresh = std::make_unique<table>();
    fresh->capacity = capacity;
    fresh->slots = std::make_unique<slot[]>(capacity);

    table *raw = fresh.get();
    mTables.push_back(std::move(fresh));
    if (mHead == nullptr) {
        mHead = raw;
    } else {
        mTail->next.store(raw, std::memory_order_release);
    }
    mTail = raw;
}
