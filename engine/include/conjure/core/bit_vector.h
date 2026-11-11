#pragma once

#include "conjure/core/api.h"
#include "conjure/core/assert.h"
#include "conjure/core/types.h"

#include <iterator>
#include <ranges>
#include <span>
#include <vector>

namespace conjure::core {

    /// <summary>
    /// A dynamically sized set of bits packed into 64-bit words. Used for component masks,
    /// free-slot tracking and dirty flags. Bits past size() are always zero, so whole-word
    /// operations (count, any, equality, bitwise ops) need no masking at the call site.
    /// Not thread-safe.
    /// </summary>
    class CONJURE_API bit_vector {
        public:
            /// <summary>
            /// Returned by the find_* functions when no matching bit exists.
            /// </summary>
            static constexpr usize NPOS = static_cast<usize>(-1);

            /// <summary>
            /// Bits per storage word.
            /// </summary>
            static constexpr usize WORD_BITS = 64;

            /// <summary>
            /// Forward iterator over the indices of the set bits, ascending. Dereferencing yields
            /// the index by value, so it models std::forward_iterator (C++20) but only the legacy
            /// input iterator category, exactly like std::ranges::iota_view's iterator.
            /// </summary>
            class set_bit_iterator {
                public:
                    using iterator_concept = std::forward_iterator_tag;
                    using iterator_category = std::input_iterator_tag;
                    using value_type = usize;
                    using difference_type = isize;
                    using pointer = const usize *;
                    using reference = usize;

                private:
                    const bit_vector *mOwner = nullptr;
                    usize mIndex = NPOS;

                public:
                    constexpr set_bit_iterator() noexcept = default;

                    constexpr set_bit_iterator(const bit_vector *owner, usize index) noexcept :
                        mOwner(owner),
                        mIndex(index) {}

                public:
                    [[nodiscard]] usize operator*() const noexcept {
                        return mIndex;
                    }

                    set_bit_iterator &operator++() noexcept {
                        // NPOS + 1 wraps to 0 and would silently restart from the first bit.
                        CONJURE_ASSERT(mOwner != nullptr && mIndex != NPOS, "incrementing bit_vector end iterator");
                        if (mOwner == nullptr || mIndex == NPOS) {
                            return *this;
                        }
                        mIndex = mOwner->find_first_set(mIndex + 1);
                        return *this;
                    }

                    set_bit_iterator operator++(int) noexcept {
                        const set_bit_iterator previous = *this;
                        ++(*this);
                        return previous;
                    }

                    [[nodiscard]] bool operator==(const set_bit_iterator &other) const noexcept {
                        return mIndex == other.mIndex;
                    }
            };

            /// <summary>
            /// Range over the set bits: for (usize index : bits.set_bits()) { ... }. Its iterators
            /// refer to the bit_vector, not to the range object, so it is a borrowed range and
            /// std::ranges algorithms may take it by value: std::ranges::find(bits.set_bits(), 7).
            /// </summary>
            class set_bit_range {
                private:
                    const bit_vector *mOwner = nullptr;

                public:
                    constexpr explicit set_bit_range(const bit_vector *owner) noexcept :
                        mOwner(owner) {}

                public:
                    [[nodiscard]] set_bit_iterator begin() const noexcept {
                        return set_bit_iterator(mOwner, mOwner->find_first_set(0));
                    }

                    [[nodiscard]] set_bit_iterator end() const noexcept {
                        return set_bit_iterator(mOwner, NPOS);
                    }
            };

        private:
            std::vector<u64> mWords = {};
            usize mSize = 0;

        public:
            bit_vector() = default;

            /// <summary>
            /// Creates 'size' bits, all set to 'value'.
            /// </summary>
            explicit bit_vector(usize size, bool value = false);

            ~bit_vector() = default;

            bit_vector(const bit_vector &) = default;
            bit_vector &operator=(const bit_vector &) = default;
            bit_vector(bit_vector &&) noexcept = default;
            bit_vector &operator=(bit_vector &&) noexcept = default;

        public:
            /*-----------------------------------------------------------------------*/
            // Size

            /// <summary>
            /// Number of bits.
            /// </summary>
            [[nodiscard]] usize size() const noexcept {
                return mSize;
            }

            /// <summary>
            /// True when there are no bits at all.
            /// </summary>
            [[nodiscard]] bool empty() const noexcept {
                return mSize == 0;
            }

            /// <summary>
            /// Number of 64-bit words backing the bits.
            /// </summary>
            [[nodiscard]] usize word_count() const noexcept {
                return mWords.size();
            }

            /// <summary>
            /// Number of bits that fit without reallocating.
            /// </summary>
            [[nodiscard]] usize capacity() const noexcept {
                return mWords.capacity() * WORD_BITS;
            }

            /// <summary>
            /// Ensures room for 'bits' bits without changing size().
            /// </summary>
            void reserve(usize bits);

            /// <summary>
            /// Changes the number of bits. New bits take 'value'; removed bits are discarded.
            /// </summary>
            void resize(usize size, bool value = false);

            /// <summary>
            /// Appends one bit.
            /// </summary>
            void push_back(bool value);

            /// <summary>
            /// Removes every bit (size becomes 0). Storage is kept.
            /// </summary>
            void clear() noexcept;

            /*-----------------------------------------------------------------------*/
            // Single bits

            /// <summary>
            /// The bit at 'index'. Asserts on an out-of-range index and returns false.
            /// </summary>
            [[nodiscard]] bool test(usize index) const noexcept {
                CONJURE_ASSERT(index < mSize, "bit_vector index {} out of range (size {})", index, mSize);
                if (index >= mSize) {
                    return false;
                }
                return ((mWords[index / WORD_BITS] >> (index % WORD_BITS)) & 1u) != 0;
            }

            [[nodiscard]] bool operator[](usize index) const noexcept {
                return test(index);
            }

            /// <summary>
            /// Sets the bit at 'index' to 1. Asserts on an out-of-range index and does nothing.
            /// </summary>
            void set(usize index) noexcept {
                CONJURE_ASSERT(index < mSize, "bit_vector index {} out of range (size {})", index, mSize);
                if (index >= mSize) {
                    return;
                }
                mWords[index / WORD_BITS] |= (u64{1} << (index % WORD_BITS));
            }

            /// <summary>
            /// Sets the bit at 'index' to 'value'.
            /// </summary>
            void set(usize index, bool value) noexcept {
                if (value) {
                    set(index);
                } else {
                    reset(index);
                }
            }

            /// <summary>
            /// Clears the bit at 'index'. Asserts on an out-of-range index and does nothing.
            /// </summary>
            void reset(usize index) noexcept {
                CONJURE_ASSERT(index < mSize, "bit_vector index {} out of range (size {})", index, mSize);
                if (index >= mSize) {
                    return;
                }
                mWords[index / WORD_BITS] &= ~(u64{1} << (index % WORD_BITS));
            }

            /// <summary>
            /// Inverts the bit at 'index'. Asserts on an out-of-range index and does nothing.
            /// </summary>
            void flip(usize index) noexcept {
                CONJURE_ASSERT(index < mSize, "bit_vector index {} out of range (size {})", index, mSize);
                if (index >= mSize) {
                    return;
                }
                mWords[index / WORD_BITS] ^= (u64{1} << (index % WORD_BITS));
            }

            /*-----------------------------------------------------------------------*/
            // All bits

            /// <summary>
            /// Sets every bit.
            /// </summary>
            void set_all() noexcept;

            /// <summary>
            /// Clears every bit (size is unchanged).
            /// </summary>
            void reset_all() noexcept;

            /// <summary>
            /// Inverts every bit.
            /// </summary>
            void flip_all() noexcept;

            /// <summary>
            /// True when at least one bit is set.
            /// </summary>
            [[nodiscard]] bool any() const noexcept;

            /// <summary>
            /// True when no bit is set (always true for an empty vector).
            /// </summary>
            [[nodiscard]] bool none() const noexcept {
                return !any();
            }

            /// <summary>
            /// True when every bit is set (always true for an empty vector).
            /// </summary>
            [[nodiscard]] bool all() const noexcept;

            /// <summary>
            /// Number of set bits.
            /// </summary>
            [[nodiscard]] usize count() const noexcept;

            /*-----------------------------------------------------------------------*/
            // Searching

            /// <summary>
            /// Index of the first set bit at or after 'from', or NPOS.
            /// </summary>
            [[nodiscard]] usize find_first_set(usize from = 0) const noexcept;

            /// <summary>
            /// Index of the first clear bit at or after 'from', or NPOS.
            /// </summary>
            [[nodiscard]] usize find_first_clear(usize from = 0) const noexcept;

            /// <summary>
            /// Index of the last set bit, or NPOS.
            /// </summary>
            [[nodiscard]] usize find_last_set() const noexcept;

            /// <summary>
            /// Range over the indices of the set bits in ascending order. The vector must not be
            /// modified while iterating.
            /// </summary>
            [[nodiscard]] set_bit_range set_bits() const noexcept {
                return set_bit_range(this);
            }

            /*-----------------------------------------------------------------------*/
            // Bitwise operations (both operands must have the same size; asserted)

            bit_vector &operator&=(const bit_vector &other) noexcept;
            bit_vector &operator|=(const bit_vector &other) noexcept;
            bit_vector &operator^=(const bit_vector &other) noexcept;

            /// <summary>
            /// Clears every bit that is set in 'other' (this & ~other).
            /// </summary>
            bit_vector &remove(const bit_vector &other) noexcept;

            /// <summary>
            /// True when every set bit of this vector is also set in 'other'.
            /// </summary>
            [[nodiscard]] bool is_subset_of(const bit_vector &other) const noexcept;

            /// <summary>
            /// True when the two vectors share at least one set bit.
            /// </summary>
            [[nodiscard]] bool intersects(const bit_vector &other) const noexcept;

            [[nodiscard]] bool operator==(const bit_vector &other) const noexcept {
                return mSize == other.mSize && mWords == other.mWords;
            }

            /*-----------------------------------------------------------------------*/
            // Raw access

            /// <summary>
            /// The packed words, least significant bit first. Bits past size() are zero.
            /// </summary>
            [[nodiscard]] std::span<const u64> words() const noexcept {
                return mWords;
            }

        private:
            void clear_trailing_bits() noexcept;
    };

    [[nodiscard]] CONJURE_API bit_vector operator&(const bit_vector &left, const bit_vector &right);
    [[nodiscard]] CONJURE_API bit_vector operator|(const bit_vector &left, const bit_vector &right);
    [[nodiscard]] CONJURE_API bit_vector operator^(const bit_vector &left, const bit_vector &right);
    [[nodiscard]] CONJURE_API bit_vector operator~(const bit_vector &value);
}

// set_bit_range only carries a pointer to its bit_vector; iterators outlive the range object safely.
template <>
inline constexpr bool std::ranges::enable_borrowed_range<conjure::core::bit_vector::set_bit_range> = true;
