#pragma once

#include "conjure/engine/engine_config.h"

#include "conjure/core/api.h"
#include "conjure/core/memory/frame_allocator.h"
#include "conjure/core/types.h"

#include <span>
#include <type_traits>
#include <utility>

// Per-frame scratch memory (docs/design/06_engine_loop.md 2.6, D6): one core::memory::frame_allocator
// with per-thread blocks, reset by the main thread at FRAME_BEGIN with no scheduler job in flight (every
// job of frame N-1 completed at FRAME_END, so the reset is safe by construction). Anything allocated here
// is dead at the next FRAME_BEGIN; an arena overflow falls back to the heap, is counted and warned about
// once per OVERFLOW_WARNING_INTERVAL frames with the high-water mark.

namespace conjure::engine {

    struct frame_memory_stats {
        usize highWaterBytes = 0;
        usize capacityBytes = 0;
        u32 overflowCount = 0;

        [[nodiscard]] bool operator==(const frame_memory_stats &) const = default;
    };

    class CONJURE_API frame_memory {
        public:
            /// <summary>
            /// Resets between two overflow warnings (one second at 60 Hz).
            /// </summary>
            static constexpr u64 OVERFLOW_WARNING_INTERVAL = 60;

        private:
            core::memory::frame_allocator mAllocator;      // no default: sized by the constructor
            u64 mEpoch = 0;
            u64 mResetCount = 0;
            u64 mLastWarningReset = 0;
            u32 mOverflowCount = 0;                        // overflows seen in the frame that was last reset
            bool mWarnedOnce = false;
            bool mPhaseInFlight = false;                   // a scheduler phase is running (reset() is refused)

        public:
            explicit frame_memory(const memory_settings &settings);
            ~frame_memory();

            frame_memory(const frame_memory &) = delete;
            frame_memory &operator=(const frame_memory &) = delete;

        public:
            /// <summary>
            /// Thread-safe bump from the calling thread's block; the arena overflow falls back to the heap.
            /// Returns nullptr for a zero size or an alignment that is not a power of two (asserted).
            /// </summary>
            [[nodiscard]] void *allocate(usize bytes, usize alignment = core::memory::DEFAULT_ALIGNMENT) noexcept;

            /// <summary>
            /// Uninitialised storage for 'count' trivially destructible elements; empty for 0 or on failure.
            /// </summary>
            template <typename ElementType>
            [[nodiscard]] std::span<ElementType> allocate_span(usize count) noexcept {
                return mAllocator.allocate_span<ElementType>(count);
            }

            /// <summary>
            /// Constructs a trivially destructible object that lives until the next reset(); nullptr on failure.
            /// </summary>
            template <typename ElementType, typename... Args>
            [[nodiscard]] ElementType *construct(Args &&...args) {
                static_assert(std::is_trivially_destructible_v<ElementType>, "frame memory never runs destructors");
                return mAllocator.construct<ElementType>(std::forward<Args>(args)...);
            }

            /// <summary>
            /// Main thread, at FRAME_BEGIN: returns every block, frees the overflow blocks, bumps the epoch
            /// and warns about overflows at most once per OVERFLOW_WARNING_INTERVAL resets. Must not run
            /// concurrently with allocate(): a call while a scheduler phase is in flight asserts and is
            /// refused (D6).
            /// </summary>
            void reset();

            /// <summary>
            /// The frame loop marks the scheduler phases it runs; reset() asserts that none is in flight.
            /// </summary>
            void set_phase_in_flight(bool inFlight) noexcept {
                mPhaseInFlight = inFlight;
            }

            [[nodiscard]] bool is_phase_in_flight() const noexcept {
                return mPhaseInFlight;
            }

            /// <summary>
            /// Identifies the current frame; changes on every reset().
            /// </summary>
            [[nodiscard]] u64 epoch() const noexcept {
                return mEpoch;
            }

            /// <summary>
            /// Number of reset() calls so far.
            /// </summary>
            [[nodiscard]] u64 reset_count() const noexcept {
                return mResetCount;
            }

            /// <summary>
            /// High-water mark (including the current frame), arena capacity and the overflow count of the
            /// current frame.
            /// </summary>
            [[nodiscard]] frame_memory_stats stats() const noexcept;

            /// <summary>
            /// Arena bytes claimed by thread blocks in the current frame.
            /// </summary>
            [[nodiscard]] usize used_bytes() const noexcept;

            /// <summary>
            /// The allocator underneath (for diagnostics and tests).
            /// </summary>
            [[nodiscard]] const core::memory::frame_allocator &allocator() const noexcept {
                return mAllocator;
            }
    };
}
