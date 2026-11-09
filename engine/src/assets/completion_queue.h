#pragma once

#include "conjure/assets/asset_payload.h"
#include "conjure/assets/asset_types.h"
#include "conjure/assets/cooked_blob.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <mutex>
#include <utility>
#include <vector>

// Hand-over from load jobs to the main thread: one mutex-guarded vector that update() swaps out
// once per frame (docs/design/05_assets.md 3.3, 5).

namespace conjure::assets::detail {

    /// <summary>
    /// What a load job produced for one slot.
    /// </summary>
    struct load_completion {
        u32 slotIndex = 0;
        u32 generation = 0;                                 // must equal the slot's to be accepted
        cooked_blob_header header = {};
        std::vector<asset_id> dependencies = {};
        core::result<asset_payload> output = make_null_payload();
    };

    /// <summary>
    /// Thread-safe queue of completions.
    /// </summary>
    class completion_queue {
        private:
            std::mutex mMutex = {};
            std::vector<load_completion> mItems = {};

        public:
            completion_queue() = default;
            ~completion_queue() = default;

            completion_queue(const completion_queue &) = delete;
            completion_queue &operator=(const completion_queue &) = delete;

        public:
            /// <summary>
            /// Any thread.
            /// </summary>
            void push(load_completion &&completion) {
                const std::scoped_lock lock(mMutex);
                mItems.push_back(std::move(completion));
            }

            /// <summary>
            /// Moves every queued completion into 'out' (appending) in arrival order.
            /// </summary>
            void drain(std::vector<load_completion> &out) {
                std::vector<load_completion> taken = {};
                {
                    const std::scoped_lock lock(mMutex);
                    taken.swap(mItems);
                }
                for (load_completion &item : taken) {
                    out.push_back(std::move(item));
                }
            }

            [[nodiscard]] usize size() {
                const std::scoped_lock lock(mMutex);
                return mItems.size();
            }
    };
}
