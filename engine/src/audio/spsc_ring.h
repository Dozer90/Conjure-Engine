#pragma once

#include "conjure/core/types.h"

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <type_traits>

// Single-producer single-consumer ring of trivially copyable items (docs/design/12_audio.md 2.7).
// Head (consumer) and tail (producer) live on separate cache lines; try_push publishes with a
// release store of the tail and try_pop acquires it, so the item bytes are visible to the consumer.
// Neither side allocates, locks or blocks. Candidate for core/containers once a second user exists.

namespace conjure::audio {

    template <typename T, u32 Capacity>
    class spsc_ring {
        private:
            static_assert(std::is_trivially_copyable_v<T>, "ring items are copied without their own synchronisation");
            static_assert(Capacity >= 2 && std::has_single_bit(Capacity), "capacity must be a power of two");

            static constexpr u32 MASK = Capacity - 1;
            static constexpr usize INDEX_PADDING = CACHE_LINE_SIZE - sizeof(std::atomic<u32>);

            // Explicit padding keeps head and tail on their own cache lines without alignas (which
            // MSVC reports as C4324 "structure was padded").
            std::atomic<u32> mHead = 0;                       // next index the consumer reads
            [[maybe_unused]] std::array<byte, INDEX_PADDING> mHeadPadding = {};
            std::atomic<u32> mTail = 0;                       // next index the producer writes
            [[maybe_unused]] std::array<byte, INDEX_PADDING> mTailPadding = {};
            std::array<T, Capacity> mItems = {};

        public:
            spsc_ring() = default;
            ~spsc_ring() = default;

            spsc_ring(const spsc_ring &) = delete;
            spsc_ring &operator=(const spsc_ring &) = delete;

        public:
            /// <summary>
            /// Number of items the ring holds at most.
            /// </summary>
            [[nodiscard]] static constexpr u32 capacity() noexcept {
                return Capacity;
            }

            /// <summary>
            /// Producer: appends an item. Returns false when the ring is full.
            /// </summary>
            [[nodiscard]] bool try_push(const T &item) noexcept {
                const u32 tail = mTail.load(std::memory_order_relaxed);
                const u32 head = mHead.load(std::memory_order_acquire);
                if (tail - head >= Capacity) {
                    return false;
                }
                mItems[tail & MASK] = item;
                mTail.store(tail + 1, std::memory_order_release);
                return true;
            }

            /// <summary>
            /// Consumer: removes the oldest item into 'out'. Returns false when the ring is empty.
            /// </summary>
            [[nodiscard]] bool try_pop(T &out) noexcept {
                const u32 head = mHead.load(std::memory_order_relaxed);
                const u32 tail = mTail.load(std::memory_order_acquire);
                if (head == tail) {
                    return false;
                }
                out = mItems[head & MASK];
                mHead.store(head + 1, std::memory_order_release);
                return true;
            }

            /// <summary>
            /// Items currently queued. Exact only when called by one of the two sides while the
            /// other is idle; otherwise a snapshot.
            /// </summary>
            [[nodiscard]] u32 size() const noexcept {
                const u32 tail = mTail.load(std::memory_order_acquire);
                const u32 head = mHead.load(std::memory_order_acquire);
                return tail - head;
            }

            /// <summary>
            /// Free slots as seen by the producer.
            /// </summary>
            [[nodiscard]] u32 free_count() const noexcept {
                return Capacity - size();
            }

            /// <summary>
            /// True when nothing is queued.
            /// </summary>
            [[nodiscard]] bool empty() const noexcept {
                return size() == 0;
            }

            /// <summary>
            /// Drops every item. Only when both sides are idle (the device is stopped).
            /// </summary>
            void clear() noexcept {
                mHead.store(0, std::memory_order_relaxed);
                mTail.store(0, std::memory_order_relaxed);
            }
    };
}
