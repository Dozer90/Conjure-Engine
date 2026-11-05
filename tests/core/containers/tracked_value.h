#pragma once

// Element types shared by the container tests.
//
//   tracked_value       counts constructions, destructions, copies and moves; not trivially copyable
//   leak_check          fails the test when live tracked_value instances differ from the baseline
//   point               trivially copyable aggregate (exercises the memcpy relocation paths)
//   wide_value          over-aligned element (exercises alignment of raw storage)


#include "conjure/testing/doctest.h"

#include "conjure/core/types.h"

#include <compare>
#include <cstddef>
#include <functional>

namespace conjure::testing {

    /// <summary>
    /// An int wrapper with a non-trivial copy constructor that keeps process-wide counters.
    /// Reset the counters at the start of a test with tracked_value::reset().
    /// </summary>
    class tracked_value {
        private:
            static inline i64 sLiveCount = 0;
            static inline i64 sConstructedCount = 0;
            static inline i64 sDestroyedCount = 0;
            static inline i64 sCopyCount = 0;
            static inline i64 sMoveCount = 0;

            int mValue = 0;

        public:
            tracked_value() noexcept {
                ++sLiveCount;
                ++sConstructedCount;
            }

            explicit tracked_value(int value) noexcept :
                mValue(value) {
                ++sLiveCount;
                ++sConstructedCount;
            }

            tracked_value(const tracked_value &other) noexcept :
                mValue(other.mValue) {
                ++sLiveCount;
                ++sConstructedCount;
                ++sCopyCount;
            }

            tracked_value(tracked_value &&other) noexcept :
                mValue(other.mValue) {
                other.mValue = -1;
                ++sLiveCount;
                ++sConstructedCount;
                ++sMoveCount;
            }

            ~tracked_value() {
                --sLiveCount;
                ++sDestroyedCount;
            }

            tracked_value &operator=(const tracked_value &other) noexcept {
                mValue = other.mValue;
                ++sCopyCount;
                return *this;
            }

            tracked_value &operator=(tracked_value &&other) noexcept {
                mValue = other.mValue;
                other.mValue = -1;
                ++sMoveCount;
                return *this;
            }

        public:
            [[nodiscard]] int value() const noexcept {
                return mValue;
            }

            [[nodiscard]] bool operator==(const tracked_value &other) const noexcept {
                return mValue == other.mValue;
            }

            [[nodiscard]] auto operator<=>(const tracked_value &other) const noexcept {
                return mValue <=> other.mValue;
            }

            [[nodiscard]] static i64 live_count() noexcept {
                return sLiveCount;
            }

            [[nodiscard]] static i64 constructed_count() noexcept {
                return sConstructedCount;
            }

            [[nodiscard]] static i64 destroyed_count() noexcept {
                return sDestroyedCount;
            }

            [[nodiscard]] static i64 copy_count() noexcept {
                return sCopyCount;
            }

            [[nodiscard]] static i64 move_count() noexcept {
                return sMoveCount;
            }

            static void reset() noexcept {
                sLiveCount = 0;
                sConstructedCount = 0;
                sDestroyedCount = 0;
                sCopyCount = 0;
                sMoveCount = 0;
            }
    };

    /// <summary>
    /// Records the number of live tracked values at construction and checks that the same number
    /// is alive at destruction.
    /// </summary>
    class leak_check {
        private:
            i64 mBaseline = 0;

        public:
            leak_check() noexcept :
                mBaseline(tracked_value::live_count()) {}

            ~leak_check() {
                CHECK(tracked_value::live_count() == mBaseline);
            }

            leak_check(const leak_check &) = delete;
            leak_check &operator=(const leak_check &) = delete;
    };

    /// <summary>
    /// Trivially copyable element.
    /// </summary>
    struct point {
        int x = 0;
        int y = 0;

        [[nodiscard]] constexpr bool operator==(const point &) const noexcept = default;
        [[nodiscard]] constexpr auto operator<=>(const point &) const noexcept = default;
    };

    /// <summary>
    /// Element with 32-byte alignment. Exactly 32 bytes so no compiler has to pad it.
    /// </summary>
    struct alignas(32) wide_value {
        u64 payload = 0;
        u64 filler[3] = {};

        [[nodiscard]] constexpr bool operator==(const wide_value &) const noexcept = default;
    };

    /// <summary>
    /// True when 'pointer' is aligned to 'alignment' bytes.
    /// </summary>
    [[nodiscard]] inline bool is_aligned(const void *pointer, usize alignment) noexcept {
        return (reinterpret_cast<std::uintptr_t>(pointer) % alignment) == 0;
    }
}

template <>
struct std::hash<conjure::testing::tracked_value> {
    [[nodiscard]] std::size_t operator()(const conjure::testing::tracked_value &value) const noexcept {
        return std::hash<int>()(value.value());
    }
};
