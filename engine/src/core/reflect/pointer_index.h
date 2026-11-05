#pragma once

#include "conjure/core/types.h"

#include <atomic>
#include <memory>
#include <vector>

// Private to the reflect area: the lock-free lookup table behind type_registry::find().
//
// Keys are non-zero 64-bit hashes (type ids), values are pointers that never move. Writers are
// serialised by the registry's mutex; readers run concurrently without any lock. The structure is a
// chain of open-addressing tables that only ever grows: a table never rehashes, so a reader probing
// it is never disturbed, and when the newest table is half full the next insertion links a larger
// one behind it. Erasing leaves a tombstone (the key with a null value) so probe sequences stay
// intact; re-inserting the same key reuses its slot.

namespace conjure::core::reflect::detail {

    class pointer_index {
        private:
            struct slot {
                std::atomic<u64> key = 0;
                std::atomic<const void *> value = nullptr;
            };

            struct table {
                usize capacity = 0;                         // power of two
                usize used = 0;                             // slots with a key (writer-only)
                std::unique_ptr<slot[]> slots = nullptr;
                std::atomic<table *> next = nullptr;        // the larger table linked after this one
            };

            static constexpr usize INITIAL_CAPACITY = 256;

            std::vector<std::unique_ptr<table>> mTables = {};   // owns every table in chain order
            table *mHead = nullptr;                             // first table (readers start here)
            table *mTail = nullptr;                             // newest table (insertions go here)

        public:
            pointer_index();
            ~pointer_index();

            pointer_index(const pointer_index &) = delete;
            pointer_index &operator=(const pointer_index &) = delete;

        public:
            /// <summary>
            /// The value stored under 'key', or nullptr when absent or erased. Lock-free; safe
            /// concurrently with insert() and erase().
            /// </summary>
            [[nodiscard]] const void *find(u64 key) const noexcept;

            /// <summary>
            /// Stores 'value' under 'key' (non-zero key, non-null value), replacing an erased or
            /// existing entry. Writers must be serialised by the caller.
            /// </summary>
            void insert(u64 key, const void *value);

            /// <summary>
            /// Marks 'key' as absent. Unknown keys are ignored. Writers must be serialised.
            /// </summary>
            void erase(u64 key) noexcept;

            /// <summary>
            /// Number of tables in the chain (1 until the first growth).
            /// </summary>
            [[nodiscard]] usize table_count() const noexcept {
                return mTables.size();
            }

        private:
            [[nodiscard]] static slot *find_slot(table &target, u64 key) noexcept;
            void add_table(usize capacity);
    };
}
