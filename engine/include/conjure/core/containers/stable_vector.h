#pragma once

#include "conjure/core/containers/detail.h"

#include "conjure/core/assert.h"
#include "conjure/core/types.h"

#include <compare>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

// stable_vector: a growable sequence whose elements never move.
//
//   stable_vector<asset_slot> slots;
//   asset_slot &slot = slots.emplace_back(...);   // &slot stays valid until pop_back/clear/destruction
//
// Storage is a list of fixed-size chunks; growing adds a chunk instead of reallocating, so
// references and pointers to elements survive every emplace_back. Random access is one division
// and one indirection. Chunks come from std::allocator through allocate_chunk/deallocate_chunk so
// the allocation point can be redirected to the memory area later.

namespace conjure::core {

    template <typename T, usize ChunkSize = 64>
    class stable_vector {
        private:
            static_assert(ChunkSize > 0, "stable_vector chunks must hold at least one element");
            static_assert(!std::is_const_v<T> && !std::is_reference_v<T>, "stable_vector elements must be objects");

        public:
            using value_type = T;
            using size_type = usize;
            using difference_type = isize;
            using reference = T &;
            using const_reference = const T &;
            using pointer = T *;
            using const_pointer = const T *;

            template <bool IsConst>
            class basic_iterator;

            using iterator = basic_iterator<false>;
            using const_iterator = basic_iterator<true>;

            /// <summary>
            /// Elements per chunk.
            /// </summary>
            static constexpr usize CHUNK_SIZE = ChunkSize;

        private:
            std::vector<T *> mChunks = {};   // every chunk holds CHUNK_SIZE uninitialised slots
            usize mSize = 0;

        public:
            stable_vector() = default;

            stable_vector(const stable_vector &other) {
                reserve(other.mSize);
                for (usize index = 0; index < other.mSize; ++index) {
                    emplace_back(other[index]);
                }
            }

            stable_vector(stable_vector &&other) noexcept :
                mChunks(std::move(other.mChunks)),
                mSize(other.mSize) {
                other.mChunks.clear();
                other.mSize = 0;
            }

            ~stable_vector() {
                clear();
                release_chunks();
            }

            stable_vector &operator=(const stable_vector &other) {
                if (this != &other) {
                    clear();
                    reserve(other.mSize);
                    for (usize index = 0; index < other.mSize; ++index) {
                        emplace_back(other[index]);
                    }
                }
                return *this;
            }

            stable_vector &operator=(stable_vector &&other) noexcept {
                if (this != &other) {
                    clear();
                    release_chunks();
                    mChunks = std::move(other.mChunks);
                    mSize = other.mSize;
                    other.mChunks.clear();
                    other.mSize = 0;
                }
                return *this;
            }

        public:
            /// <summary>
            /// Constructs an element at the end and returns a reference that stays valid until the
            /// element is destroyed (pop_back, clear, destruction). Never moves other elements.
            /// </summary>
            template <typename... Args>
            T &emplace_back(Args &&...args) {
                if (mSize == capacity()) {
                    mChunks.push_back(allocate_chunk());
                }
                T *slot = std::construct_at(slot_at(mSize), std::forward<Args>(args)...);
                ++mSize;
                return *slot;
            }

            T &push_back(const T &value) {
                return emplace_back(value);
            }

            T &push_back(T &&value) {
                return emplace_back(std::move(value));
            }

            /// <summary>
            /// Destroys the last element. Asserts when empty. Chunks are kept.
            /// </summary>
            void pop_back() noexcept {
                CONJURE_ASSERT(mSize > 0, "stable_vector::pop_back on an empty vector");
                if (mSize == 0) {
                    return;
                }
                --mSize;
                std::destroy_at(slot_at(mSize));
            }

            /// <summary>
            /// Destroys every element; chunks are kept so capacity() is unchanged.
            /// </summary>
            void clear() noexcept {
                for (usize index = mSize; index > 0; --index) {
                    std::destroy_at(slot_at(index - 1));
                }
                mSize = 0;
            }

            /// <summary>
            /// Allocates chunks so that 'count' elements fit without further allocation.
            /// </summary>
            void reserve(usize count) {
                while (capacity() < count) {
                    mChunks.push_back(allocate_chunk());
                }
            }

            /// <summary>
            /// Frees chunks that hold no element.
            /// </summary>
            void shrink_to_fit() noexcept {
                const usize neededChunks = (mSize + ChunkSize - 1) / ChunkSize;
                while (mChunks.size() > neededChunks) {
                    deallocate_chunk(mChunks.back());
                    mChunks.pop_back();
                }
            }

        public:
            [[nodiscard]] usize size() const noexcept {
                return mSize;
            }

            [[nodiscard]] bool empty() const noexcept {
                return mSize == 0;
            }

            /// <summary>
            /// Elements that fit in the allocated chunks.
            /// </summary>
            [[nodiscard]] usize capacity() const noexcept {
                return mChunks.size() * ChunkSize;
            }

            [[nodiscard]] usize chunk_count() const noexcept {
                return mChunks.size();
            }

            [[nodiscard]] T &operator[](usize index) noexcept {
                CONJURE_ASSERT(index < mSize, "stable_vector index {} out of range (size {})", index, mSize);
                return *slot_at(index);
            }

            [[nodiscard]] const T &operator[](usize index) const noexcept {
                CONJURE_ASSERT(index < mSize, "stable_vector index {} out of range (size {})", index, mSize);
                return *slot_at(index);
            }

            /// <summary>
            /// Bounds-checked access in every configuration. An out-of-range index is a fatal
            /// programmer error (asserts, then terminates), never an exception.
            /// </summary>
            [[nodiscard]] T &at(usize index) noexcept {
                if (index >= mSize) [[unlikely]] {
                    detail::fatal_container_access("stable_vector::at: index out of range");
                }
                return *slot_at(index);
            }

            [[nodiscard]] const T &at(usize index) const noexcept {
                if (index >= mSize) [[unlikely]] {
                    detail::fatal_container_access("stable_vector::at: index out of range");
                }
                return *slot_at(index);
            }

            [[nodiscard]] T &front() noexcept {
                CONJURE_ASSERT(mSize > 0, "stable_vector::front on an empty vector");
                return *slot_at(0);
            }

            [[nodiscard]] const T &front() const noexcept {
                CONJURE_ASSERT(mSize > 0, "stable_vector::front on an empty vector");
                return *slot_at(0);
            }

            [[nodiscard]] T &back() noexcept {
                CONJURE_ASSERT(mSize > 0, "stable_vector::back on an empty vector");
                return *slot_at(mSize - 1);
            }

            [[nodiscard]] const T &back() const noexcept {
                CONJURE_ASSERT(mSize > 0, "stable_vector::back on an empty vector");
                return *slot_at(mSize - 1);
            }

            [[nodiscard]] iterator begin() noexcept {
                return iterator(this, 0);
            }

            [[nodiscard]] iterator end() noexcept {
                return iterator(this, mSize);
            }

            [[nodiscard]] const_iterator begin() const noexcept {
                return const_iterator(this, 0);
            }

            [[nodiscard]] const_iterator end() const noexcept {
                return const_iterator(this, mSize);
            }

            [[nodiscard]] const_iterator cbegin() const noexcept {
                return begin();
            }

            [[nodiscard]] const_iterator cend() const noexcept {
                return end();
            }

        private:
            [[nodiscard]] T *slot_at(usize index) const noexcept {
                return mChunks[index / ChunkSize] + (index % ChunkSize);
            }

            /*===================================================================================*/
            // Allocation points. Everything that touches the heap goes through these two.

            [[nodiscard]] static T *allocate_chunk() {
                return std::allocator<T>().allocate(ChunkSize);
            }

            static void deallocate_chunk(T *chunk) noexcept {
                std::allocator<T>().deallocate(chunk, ChunkSize);
            }

            void release_chunks() noexcept {
                for (T *chunk : mChunks) {
                    deallocate_chunk(chunk);
                }
                mChunks.clear();
            }

        public:
            /// <summary>
            /// Random-access iterator by index. Stays valid across emplace_back; invalidated only
            /// when the element it points at is destroyed. Ordering (<, <=>, -) is only defined
            /// between iterators of the same container and asserts otherwise; equality is defined
            /// for any pair and is false across containers.
            /// </summary>
            template <bool IsConst>
            class basic_iterator {
                public:
                    using owner_pointer = std::conditional_t<IsConst, const stable_vector *, stable_vector *>;
                    using iterator_category = std::random_access_iterator_tag;
                    using value_type = T;
                    using difference_type = isize;
                    using pointer = std::conditional_t<IsConst, const T *, T *>;
                    using reference = std::conditional_t<IsConst, const T &, T &>;

                private:
                    owner_pointer mOwner = nullptr;
                    usize mIndex = 0;

                public:
                    basic_iterator() noexcept = default;

                    basic_iterator(owner_pointer owner, usize index) noexcept :
                        mOwner(owner),
                        mIndex(index) {}

                    /// <summary>
                    /// Mutable iterators convert to const ones.
                    /// </summary>
                    template <bool OtherConst>
                        requires (IsConst && !OtherConst)
                    basic_iterator(const basic_iterator<OtherConst> &other) noexcept :
                        mOwner(other.owner()),
                        mIndex(other.index()) {}

                public:
                    [[nodiscard]] reference operator*() const noexcept {
                        return (*mOwner)[mIndex];
                    }

                    [[nodiscard]] pointer operator->() const noexcept {
                        return &(*mOwner)[mIndex];
                    }

                    [[nodiscard]] reference operator[](difference_type offset) const noexcept {
                        return *(*this + offset);
                    }

                    basic_iterator &operator++() noexcept {
                        ++mIndex;
                        return *this;
                    }

                    basic_iterator operator++(int) noexcept {
                        basic_iterator copy = *this;
                        ++mIndex;
                        return copy;
                    }

                    basic_iterator &operator--() noexcept {
                        --mIndex;
                        return *this;
                    }

                    basic_iterator operator--(int) noexcept {
                        basic_iterator copy = *this;
                        --mIndex;
                        return copy;
                    }

                    basic_iterator &operator+=(difference_type offset) noexcept {
                        mIndex = static_cast<usize>(static_cast<difference_type>(mIndex) + offset);
                        return *this;
                    }

                    basic_iterator &operator-=(difference_type offset) noexcept {
                        mIndex = static_cast<usize>(static_cast<difference_type>(mIndex) - offset);
                        return *this;
                    }

                    [[nodiscard]] friend basic_iterator operator+(basic_iterator left, difference_type offset) {
                        left += offset;
                        return left;
                    }

                    [[nodiscard]] friend basic_iterator operator+(difference_type offset, basic_iterator right) {
                        right += offset;
                        return right;
                    }

                    [[nodiscard]] friend basic_iterator operator-(basic_iterator left, difference_type offset) {
                        left -= offset;
                        return left;
                    }

                    [[nodiscard]] friend difference_type operator-(
                      const basic_iterator &left,
                      const basic_iterator &right
                    ) noexcept {
                        CONJURE_ASSERT(left.mOwner == right.mOwner, "stable_vector: iterators of different vectors");
                        return static_cast<difference_type>(left.mIndex) - static_cast<difference_type>(right.mIndex);
                    }

                    [[nodiscard]] bool operator==(const basic_iterator &other) const noexcept {
                        return mOwner == other.mOwner && mIndex == other.mIndex;
                    }

                    [[nodiscard]] std::strong_ordering operator<=>(const basic_iterator &other) const noexcept {
                        CONJURE_ASSERT(mOwner == other.mOwner, "stable_vector: iterators of different vectors");
                        return mIndex <=> other.mIndex;
                    }

                    [[nodiscard]] usize index() const noexcept {
                        return mIndex;
                    }

                    [[nodiscard]] owner_pointer owner() const noexcept {
                        return mOwner;
                    }
            };
    };
}
