#pragma once

#include "conjure/core/memory/tracked_allocation.h"
#include "conjure/core/types.h"

#include <atomic>
#include <limits>
#include <new>
#include <span>
#include <type_traits>
#include <utility>

namespace conjure::core::memory {

    /// <summary>
    /// Per-frame scratch memory shared by every thread. One arena of arenaBytes is carved into
    /// thread blocks of threadBlockBytes with a single atomic bump; each thread then allocates inside
    /// its own block from a thread_local slot without any synchronisation. When a request does not
    /// fit the thread's block the thread grabs a new one (the remainder of the old block is
    /// abandoned); when the arena is exhausted, or a request is larger than a block, the allocation
    /// falls back to tracked_allocate and overflow_count() grows. Overflow blocks are released by
    /// reset().
    ///
    /// Thread safety: allocate() and the getters may be called from any thread concurrently and are
    /// lock-free (overflow uses a lock-free push; every counter the getters read is atomic). reset()
    /// must not run concurrently with allocate(): the engine resets at FRAME_BEGIN with no job in
    /// flight. reset() bumps the epoch, which invalidates every thread's cached block so a stale
    /// thread_local cursor is never reused.
    ///
    /// Implementation note: the thread_local slot is {owner, epoch, cursor, end, arenaExhausted}; a
    /// thread that alternates between two frame allocators re-acquires a block on every switch.
    /// Epochs are process-unique, so a slot left over from a destroyed allocator never matches a new
    /// one that happens to live at the same address.
    /// </summary>
    class frame_allocator {
        private:
            struct overflow_block;

            byte *mArena = nullptr;
            usize mCapacityBytes = 0;
            usize mThreadBlockBytes = 0;
            std::atomic<usize> mOffset = 0;                             // claimed arena bytes, never above capacity
            std::atomic<u64> mEpoch = 0;
            std::atomic<overflow_block *> mOverflowHead = nullptr;      // lock-free intrusive stack
            std::atomic<u32> mOverflowCount = 0;
            std::atomic<usize> mOverflowBytes = 0;
            std::atomic<usize> mHighWaterBytes = 0;                     // written by reset(), read anywhere

        public:
            /// <summary>
            /// 'threadBlockBytes' is rounded up to a multiple of CACHE_LINE_SIZE (at least one line) so
            /// blocks of different threads never share a cache line; 'arenaBytes' is rounded up to a
            /// whole number of blocks. An arena of 0 bytes is allowed: every allocation then overflows,
            /// which keeps the allocator usable when no frame budget is configured. An arena the heap
            /// cannot provide (too large for the address space or refused by the system allocator)
            /// degrades the same way: capacity_bytes() reports 0 and every allocation overflows.
            /// </summary>
            frame_allocator(usize arenaBytes, usize threadBlockBytes);
            ~frame_allocator();

            frame_allocator(const frame_allocator &) = delete;
            frame_allocator &operator=(const frame_allocator &) = delete;
            frame_allocator(frame_allocator &&) = delete;
            frame_allocator &operator=(frame_allocator &&) = delete;

        public:
            /// <summary>
            /// Returns 'bytes' bytes aligned to 'alignment' (a power of two) that live until the next
            /// reset(). Thread-safe and lock-free. Returns nullptr for a zero-byte request, for an
            /// alignment that is not a power of two (asserts) and when the heap fallback fails.
            /// </summary>
            [[nodiscard]] void *allocate(usize bytes, usize alignment = DEFAULT_ALIGNMENT) noexcept;

            /// <summary>
            /// Allocates uninitialised storage for 'count' elements of a trivially destructible T:
            /// nothing is constructed or destroyed, the caller initialises every element before reading
            /// it. Returns an empty span for a count of 0 or when allocation fails.
            /// </summary>
            template <typename T>
            [[nodiscard]] std::span<T> allocate_span(usize count) noexcept {
                static_assert(
                  std::is_trivially_destructible_v<T>,
                  "frame_allocator::allocate_span needs a trivially destructible type: reset() never runs destructors"
                );
                if (count == 0 || count > std::numeric_limits<usize>::max() / sizeof(T)) {
                    return {};
                }
                void *storage = allocate(count * sizeof(T), alignof(T));
                if (storage == nullptr) {
                    return {};
                }
                return std::span<T>(static_cast<T *>(storage), count);
            }

            /// <summary>
            /// Allocates and constructs a T that lives until the next reset(). T must be trivially
            /// destructible because reset() never runs destructors. Returns nullptr when allocation fails.
            /// </summary>
            template <typename T, typename... Args>
            [[nodiscard]] T *construct(Args &&...args) {
                static_assert(
                  std::is_trivially_destructible_v<T>,
                  "frame_allocator::construct requires a trivially destructible type: reset() never runs destructors"
                );
                void *storage = allocate(sizeof(T), alignof(T));
                if (storage == nullptr) {
                    return nullptr;
                }
                return ::new (storage) T(std::forward<Args>(args)...);
            }

            /// <summary>
            /// Starts a new frame: frees the overflow blocks, returns every thread block to the arena and
            /// bumps the epoch so cached thread blocks are abandoned. Not thread-safe: no allocate() may
            /// run concurrently.
            /// </summary>
            void reset() noexcept;

            /// <summary>
            /// The largest frame seen so far, including the current one: arena bytes claimed by thread
            /// blocks plus bytes served by the heap fallback. Size the arena from this.
            /// </summary>
            [[nodiscard]] usize high_water_bytes() const noexcept;

            /// <summary>
            /// Usable arena bytes (after rounding to whole blocks); 0 when the allocator is heap-only.
            /// </summary>
            [[nodiscard]] usize capacity_bytes() const noexcept {
                return mCapacityBytes;
            }

            /// <summary>
            /// Arena bytes claimed by thread blocks in the current frame.
            /// </summary>
            [[nodiscard]] usize used_bytes() const noexcept;

            /// <summary>
            /// Size of one thread block (after rounding).
            /// </summary>
            [[nodiscard]] usize thread_block_bytes() const noexcept {
                return mThreadBlockBytes;
            }

            /// <summary>
            /// Allocations served by the heap fallback in the current frame.
            /// </summary>
            [[nodiscard]] u32 overflow_count() const noexcept {
                return mOverflowCount.load(std::memory_order_relaxed);
            }

            /// <summary>
            /// Bytes served by the heap fallback in the current frame.
            /// </summary>
            [[nodiscard]] usize overflow_bytes() const noexcept {
                return mOverflowBytes.load(std::memory_order_relaxed);
            }

            /// <summary>
            /// Identifies the current frame. Changes on every reset() and is unique across every
            /// frame_allocator in the process.
            /// </summary>
            [[nodiscard]] u64 epoch() const noexcept {
                return mEpoch.load(std::memory_order_acquire);
            }

        private:
            [[nodiscard]] bool fits_in_fresh_block(usize bytes, usize alignment) const noexcept;
            [[nodiscard]] byte *acquire_block() noexcept;
            [[nodiscard]] void *allocate_overflow(usize bytes, usize alignment) noexcept;
            void free_overflow_blocks() noexcept;
    };
}
