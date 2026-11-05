#pragma once

#include "conjure/core/containers/detail.h"

#include "conjure/core/assert.h"
#include "conjure/core/types.h"

#include <algorithm>
#include <compare>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

// small_vector: a std::vector with inline storage for the first N elements.
//
//   small_vector<entity, 4> children;    // no heap allocation until the fifth child
//   children.push_back(child);
//   std::span<const entity> view = children;
//
// Trivially copyable element types are relocated with memcpy; everything else is moved and
// destroyed element by element, so non-trivial types are safe. Heap storage comes from
// std::allocator through two private functions (allocate_storage/deallocate_storage) so the
// allocation point can be redirected to the memory area later.

namespace conjure::core {

    template <typename T, usize N>
    class small_vector {
        private:
            static_assert(N > 0, "small_vector needs at least one inline element; use std::vector otherwise");
            static_assert(!std::is_const_v<T> && !std::is_reference_v<T>, "small_vector elements must be objects");

        public:
            using value_type = T;
            using size_type = usize;
            using difference_type = isize;
            using reference = T &;
            using const_reference = const T &;
            using pointer = T *;
            using const_pointer = const T *;
            using iterator = T *;
            using const_iterator = const T *;
            using reverse_iterator = std::reverse_iterator<iterator>;
            using const_reverse_iterator = std::reverse_iterator<const_iterator>;

            /// <summary>
            /// Elements stored inside the object before the heap is used.
            /// </summary>
            static constexpr usize INLINE_CAPACITY = N;

            /// <summary>
            /// True when elements are relocated with memcpy instead of move + destroy.
            /// </summary>
            static constexpr bool IS_TRIVIALLY_RELOCATABLE = std::is_trivially_copyable_v<T>;

        private:
            /// <summary>
            /// Raw inline storage. A union keeps the natural alignment of T without an alignas
            /// specifier and never constructs the elements itself.
            /// </summary>
            union inline_storage {
                T elements[N];

                inline_storage() noexcept {}
                ~inline_storage() {}

                inline_storage(const inline_storage &) = delete;
                inline_storage &operator=(const inline_storage &) = delete;
            };

            inline_storage mInline = {};
            T *mData = nullptr;
            usize mSize = 0;
            usize mCapacity = N;

        public:
            small_vector() noexcept :
                mData(inline_data()) {}

            /// <summary>
            /// 'count' value-initialised elements.
            /// </summary>
            explicit small_vector(usize count) :
                mData(inline_data()) {
                resize(count);
            }

            /// <summary>
            /// 'count' copies of 'value'.
            /// </summary>
            small_vector(usize count, const T &value) :
                mData(inline_data()) {
                resize(count, value);
            }

            small_vector(std::initializer_list<T> values) :
                mData(inline_data()) {
                append_range(values.begin(), values.end());
            }

            template <std::input_iterator Iterator>
            small_vector(Iterator first, Iterator last) :
                mData(inline_data()) {
                append_range(first, last);
            }

            explicit small_vector(std::span<const T> values) :
                mData(inline_data()) {
                append_range(values.begin(), values.end());
            }

            small_vector(const small_vector &other) :
                mData(inline_data()) {
                append_range(other.begin(), other.end());
            }

            small_vector(small_vector &&other) noexcept :
                mData(inline_data()) {
                take_from(other);
            }

            ~small_vector() {
                destroy_range(mData, mSize);
                release_heap();
            }

            small_vector &operator=(const small_vector &other) {
                if (this != &other) {
                    assign_range(other.begin(), other.end());
                }
                return *this;
            }

            small_vector &operator=(small_vector &&other) noexcept {
                if (this != &other) {
                    clear();
                    release_heap();
                    mData = inline_data();
                    mCapacity = N;
                    take_from(other);
                }
                return *this;
            }

            small_vector &operator=(std::initializer_list<T> values) {
                assign_range(values.begin(), values.end());
                return *this;
            }

        public:
            /// <summary>
            /// Replaces the contents with 'count' copies of 'value'.
            /// </summary>
            void assign(usize count, const T &value) {
                clear();
                resize(count, value);
            }

            /// <summary>
            /// Replaces the contents with a copy of the range.
            /// </summary>
            template <std::input_iterator Iterator>
            void assign(Iterator first, Iterator last) {
                assign_range(first, last);
            }

            /// <summary>
            /// Appends a copy of 'value'. 'value' may refer to an element of this vector.
            /// </summary>
            void push_back(const T &value) {
                emplace_back(value);
            }

            /// <summary>
            /// Appends by move.
            /// </summary>
            void push_back(T &&value) {
                emplace_back(std::move(value));
            }

            /// <summary>
            /// Constructs an element at the end and returns it. Arguments may refer to existing
            /// elements; they are read before any reallocation.
            /// </summary>
            template <typename... Args>
            T &emplace_back(Args &&...args) {
                if (mSize == mCapacity) {
                    return *emplace_with_growth(mSize, std::forward<Args>(args)...);
                }
                T *slot = std::construct_at(mData + mSize, std::forward<Args>(args)...);
                ++mSize;
                return *slot;
            }

            /// <summary>
            /// Destroys the last element. Asserts when empty.
            /// </summary>
            void pop_back() noexcept {
                CONJURE_ASSERT(mSize > 0, "small_vector::pop_back on an empty vector");
                if (mSize == 0) {
                    return;
                }
                --mSize;
                std::destroy_at(mData + mSize);
            }

            /// <summary>
            /// Inserts a copy of 'value' before 'position'. Returns an iterator to the new element.
            /// </summary>
            iterator insert(const_iterator position, const T &value) {
                return emplace(position, value);
            }

            iterator insert(const_iterator position, T &&value) {
                return emplace(position, std::move(value));
            }

            /// <summary>
            /// Inserts a copy of the range before 'position' (which may be end()). The range must
            /// not alias this vector. Returns an iterator to the first inserted element. Single-pass
            /// (input) iterators are collected into a temporary vector first.
            /// </summary>
            template <std::input_iterator Iterator>
            iterator insert(const_iterator position, Iterator first, Iterator last) {
                const usize index = index_of(position);
                CONJURE_ASSERT(index <= mSize, "small_vector::insert position out of range");
                if (index > mSize) {
                    return end();
                }
                if constexpr (!std::forward_iterator<Iterator>) {
                    small_vector collected(first, last);
                    return insert(
                      mData + index,
                      std::move_iterator<iterator>(collected.begin()),
                      std::move_iterator<iterator>(collected.end())
                    );
                } else {
                    const usize count = static_cast<usize>(std::distance(first, last));
                    if (count == 0) {
                        return mData + index;
                    }
                    open_gap(index, count);
                    T *destination = mData + index;
                    for (Iterator source = first; source != last; ++source, ++destination) {
                        std::construct_at(destination, *source);
                    }
                    mSize += count;
                    return mData + index;
                }
            }

            iterator insert(const_iterator position, std::initializer_list<T> values) {
                return insert(position, values.begin(), values.end());
            }

            /// <summary>
            /// Constructs an element before 'position' (which may be end()). Arguments may refer
            /// to existing elements. Requires a move-assignable T unless T is trivially copyable.
            /// </summary>
            template <typename... Args>
            iterator emplace(const_iterator position, Args &&...args) {
                const usize index = index_of(position);
                CONJURE_ASSERT(index <= mSize, "small_vector::emplace position out of range");
                if (index > mSize) {
                    return end();
                }
                if (index == mSize) {
                    emplace_back(std::forward<Args>(args)...);
                    return mData + index;
                }
                if (mSize == mCapacity) {
                    return emplace_with_growth(index, std::forward<Args>(args)...);
                }

                // Build the value first so arguments that alias elements are read before anything
                // moves.
                T value(std::forward<Args>(args)...);
                if constexpr (IS_TRIVIALLY_RELOCATABLE) {
                    std::memmove(
                      static_cast<void *>(mData + index + 1),
                      static_cast<const void *>(mData + index),
                      (mSize - index) * sizeof(T)
                    );
                    std::construct_at(mData + index, std::move(value));
                } else {
                    std::construct_at(mData + mSize, std::move(mData[mSize - 1]));
                    std::move_backward(mData + index, mData + mSize - 1, mData + mSize);
                    mData[index] = std::move(value);
                }
                ++mSize;
                return mData + index;
            }

            /// <summary>
            /// Erases the element at 'position'. Returns the iterator following it.
            /// </summary>
            iterator erase(const_iterator position) {
                return erase(position, position + 1);
            }

            /// <summary>
            /// Erases [first, last). Returns the iterator following the last erased element.
            /// </summary>
            iterator erase(const_iterator first, const_iterator last) {
                const usize firstIndex = index_of(first);
                const usize lastIndex = index_of(last);
                CONJURE_ASSERT(firstIndex <= lastIndex && lastIndex <= mSize, "small_vector::erase range out of range");
                if (firstIndex >= lastIndex || lastIndex > mSize) {
                    return mData + std::min(firstIndex, mSize);
                }

                const usize count = lastIndex - firstIndex;
                if constexpr (IS_TRIVIALLY_RELOCATABLE) {
                    std::memmove(
                      static_cast<void *>(mData + firstIndex),
                      static_cast<const void *>(mData + lastIndex),
                      (mSize - lastIndex) * sizeof(T)
                    );
                } else {
                    std::move(mData + lastIndex, mData + mSize, mData + firstIndex);
                    destroy_range(mData + mSize - count, count);
                }
                mSize -= count;
                return mData + firstIndex;
            }

            /// <summary>
            /// Grows with value-initialised elements or shrinks by destroying the tail.
            /// </summary>
            void resize(usize count) {
                if (count < mSize) {
                    destroy_range(mData + count, mSize - count);
                    mSize = count;
                    return;
                }
                reserve(count);
                for (usize index = mSize; index < count; ++index) {
                    std::construct_at(mData + index);
                }
                mSize = count;
            }

            /// <summary>
            /// Grows with copies of 'value' or shrinks by destroying the tail.
            /// </summary>
            void resize(usize count, const T &value) {
                if (count < mSize) {
                    destroy_range(mData + count, mSize - count);
                    mSize = count;
                    return;
                }
                reserve(count);
                for (usize index = mSize; index < count; ++index) {
                    std::construct_at(mData + index, value);
                }
                mSize = count;
            }

            /// <summary>
            /// Makes room for at least 'count' elements without constructing any.
            /// </summary>
            void reserve(usize count) {
                if (count <= mCapacity) {
                    return;
                }
                reallocate(count);
            }

            /// <summary>
            /// Moves the elements back inline when they fit, or trims heap capacity to size().
            /// </summary>
            void shrink_to_fit() {
                if (is_inline()) {
                    return;
                }
                if (mSize <= N) {
                    T *heap = mData;
                    const usize heapCapacity = mCapacity;
                    relocate_range(heap, inline_data(), mSize);
                    deallocate_storage(heap, heapCapacity);
                    mData = inline_data();
                    mCapacity = N;
                    return;
                }
                if (mSize < mCapacity) {
                    reallocate(mSize);
                }
            }

            /// <summary>
            /// Destroys every element; keeps the current storage.
            /// </summary>
            void clear() noexcept {
                destroy_range(mData, mSize);
                mSize = 0;
            }

            /// <summary>
            /// Exchanges contents with another vector of the same type.
            /// </summary>
            void swap(small_vector &other) noexcept {
                if (this == &other) {
                    return;
                }
                small_vector temporary(std::move(other));
                other = std::move(*this);
                *this = std::move(temporary);
            }

        public:
            [[nodiscard]] T *data() noexcept {
                return mData;
            }

            [[nodiscard]] const T *data() const noexcept {
                return mData;
            }

            [[nodiscard]] usize size() const noexcept {
                return mSize;
            }

            [[nodiscard]] usize capacity() const noexcept {
                return mCapacity;
            }

            [[nodiscard]] bool empty() const noexcept {
                return mSize == 0;
            }

            [[nodiscard]] static constexpr usize max_size() noexcept {
                return static_cast<usize>(std::numeric_limits<isize>::max()) / sizeof(T);
            }

            /// <summary>
            /// True while the elements live inside the object (no heap allocation).
            /// </summary>
            [[nodiscard]] bool is_inline() const noexcept {
                return mData == inline_data();
            }

            [[nodiscard]] T &front() noexcept {
                CONJURE_ASSERT(mSize > 0, "small_vector::front on an empty vector");
                return mData[0];
            }

            [[nodiscard]] const T &front() const noexcept {
                CONJURE_ASSERT(mSize > 0, "small_vector::front on an empty vector");
                return mData[0];
            }

            [[nodiscard]] T &back() noexcept {
                CONJURE_ASSERT(mSize > 0, "small_vector::back on an empty vector");
                return mData[mSize - 1];
            }

            [[nodiscard]] const T &back() const noexcept {
                CONJURE_ASSERT(mSize > 0, "small_vector::back on an empty vector");
                return mData[mSize - 1];
            }

            [[nodiscard]] T &operator[](usize index) noexcept {
                CONJURE_ASSERT(index < mSize, "small_vector index {} out of range (size {})", index, mSize);
                return mData[index];
            }

            [[nodiscard]] const T &operator[](usize index) const noexcept {
                CONJURE_ASSERT(index < mSize, "small_vector index {} out of range (size {})", index, mSize);
                return mData[index];
            }

            /// <summary>
            /// Bounds-checked access in every configuration. An out-of-range index is a fatal
            /// programmer error (asserts, then terminates), never an exception.
            /// </summary>
            [[nodiscard]] T &at(usize index) noexcept {
                if (index >= mSize) [[unlikely]] {
                    detail::fatal_container_access("small_vector::at: index out of range");
                }
                return mData[index];
            }

            [[nodiscard]] const T &at(usize index) const noexcept {
                if (index >= mSize) [[unlikely]] {
                    detail::fatal_container_access("small_vector::at: index out of range");
                }
                return mData[index];
            }

            [[nodiscard]] iterator begin() noexcept {
                return mData;
            }

            [[nodiscard]] const_iterator begin() const noexcept {
                return mData;
            }

            [[nodiscard]] const_iterator cbegin() const noexcept {
                return mData;
            }

            [[nodiscard]] iterator end() noexcept {
                return mData + mSize;
            }

            [[nodiscard]] const_iterator end() const noexcept {
                return mData + mSize;
            }

            [[nodiscard]] const_iterator cend() const noexcept {
                return mData + mSize;
            }

            [[nodiscard]] reverse_iterator rbegin() noexcept {
                return reverse_iterator(end());
            }

            [[nodiscard]] const_reverse_iterator rbegin() const noexcept {
                return const_reverse_iterator(end());
            }

            [[nodiscard]] reverse_iterator rend() noexcept {
                return reverse_iterator(begin());
            }

            [[nodiscard]] const_reverse_iterator rend() const noexcept {
                return const_reverse_iterator(begin());
            }

            /// <summary>
            /// Views the elements as a span.
            /// </summary>
            [[nodiscard]] operator std::span<T>() noexcept {
                return std::span<T>(mData, mSize);
            }

            [[nodiscard]] operator std::span<const T>() const noexcept {
                return std::span<const T>(mData, mSize);
            }

            [[nodiscard]] bool operator==(const small_vector &other) const {
                return mSize == other.mSize && std::equal(begin(), end(), other.begin());
            }

            /// <summary>
            /// Lexicographical ordering like std::vector's: uses T's operator<=> when it has one
            /// and falls back to operator< (weak ordering) otherwise. Absent for element types
            /// that are not even less-than comparable.
            /// </summary>
            [[nodiscard]] auto operator<=>(const small_vector &other) const
                requires detail::synth_three_way_comparable<T> {
                using ordering = detail::synth_three_way_result<T>;
                const usize common = std::min(mSize, other.mSize);
                for (usize index = 0; index < common; ++index) {
                    const ordering order = detail::synth_three_way(mData[index], other.mData[index]);
                    if (order != 0) {
                        return order;
                    }
                }
                return ordering(mSize <=> other.mSize);
            }

        private:
            [[nodiscard]] T *inline_data() noexcept {
                return mInline.elements;
            }

            [[nodiscard]] const T *inline_data() const noexcept {
                return mInline.elements;
            }

            [[nodiscard]] usize index_of(const_iterator position) const noexcept {
                return static_cast<usize>(position - mData);
            }

            [[nodiscard]] usize grown_capacity(usize required) const noexcept {
                return std::max(required, mCapacity * 2);
            }

            /*===================================================================================*/
            // Allocation points. Everything that touches the heap goes through these two.

            [[nodiscard]] static T *allocate_storage(usize count) {
                return std::allocator<T>().allocate(count);
            }

            static void deallocate_storage(T *storage, usize count) noexcept {
                std::allocator<T>().deallocate(storage, count);
            }

            void release_heap() noexcept {
                if (!is_inline()) {
                    deallocate_storage(mData, mCapacity);
                }
            }

            /*===================================================================================*/
            // Element lifetime helpers.

            static void destroy_range(T *first, usize count) noexcept {
                std::destroy_n(first, count);
            }

            /// <summary>
            /// Moves 'count' elements from 'source' to uninitialised 'destination' and ends the
            /// lifetime of the sources. The ranges must not overlap.
            /// </summary>
            static void relocate_range(T *source, T *destination, usize count) noexcept {
                if (count == 0) {
                    return;
                }
                if constexpr (IS_TRIVIALLY_RELOCATABLE) {
                    std::memcpy(static_cast<void *>(destination), static_cast<const void *>(source), count * sizeof(T));
                } else {
                    for (usize index = 0; index < count; ++index) {
                        std::construct_at(destination + index, std::move(source[index]));
                        std::destroy_at(source + index);
                    }
                }
            }

            void reallocate(usize newCapacity) {
                T *storage = allocate_storage(newCapacity);
                relocate_range(mData, storage, mSize);
                release_heap();
                mData = storage;
                mCapacity = newCapacity;
            }

            /// <summary>
            /// Reallocates leaving an uninitialised hole at 'index' and constructs the new element
            /// there first, so arguments aliasing old elements are still intact when read.
            /// </summary>
            template <typename... Args>
            T *emplace_with_growth(usize index, Args &&...args) {
                const usize newCapacity = grown_capacity(mSize + 1);
                T *storage = allocate_storage(newCapacity);
                T *slot = std::construct_at(storage + index, std::forward<Args>(args)...);
                relocate_range(mData, storage, index);
                relocate_range(mData + index, storage + index + 1, mSize - index);
                release_heap();
                mData = storage;
                mCapacity = newCapacity;
                ++mSize;
                return slot;
            }

            /// <summary>
            /// Shifts [index, size) right by 'count' leaving uninitialised storage in the gap.
            /// mSize is not updated; the caller fills the gap and adds 'count'.
            /// </summary>
            void open_gap(usize index, usize count) {
                const usize required = mSize + count;
                if (required > mCapacity) {
                    const usize newCapacity = grown_capacity(required);
                    T *storage = allocate_storage(newCapacity);
                    relocate_range(mData, storage, index);
                    relocate_range(mData + index, storage + index + count, mSize - index);
                    release_heap();
                    mData = storage;
                    mCapacity = newCapacity;
                    return;
                }

                if constexpr (IS_TRIVIALLY_RELOCATABLE) {
                    std::memmove(
                      static_cast<void *>(mData + index + count),
                      static_cast<const void *>(mData + index),
                      (mSize - index) * sizeof(T)
                    );
                } else {
                    // Relocate back to front so overlapping moves never read a dead element.
                    for (usize offset = mSize; offset > index; --offset) {
                        std::construct_at(mData + offset - 1 + count, std::move(mData[offset - 1]));
                        std::destroy_at(mData + offset - 1);
                    }
                }
            }

            template <typename Iterator>
            void append_range(Iterator first, Iterator last) {
                if constexpr (std::forward_iterator<Iterator>) {
                    reserve(mSize + static_cast<usize>(std::distance(first, last)));
                }
                for (Iterator source = first; source != last; ++source) {
                    emplace_back(*source);
                }
            }

            template <typename Iterator>
            void assign_range(Iterator first, Iterator last) {
                clear();
                append_range(first, last);
            }

            /// <summary>
            /// Steals the heap buffer of 'other' or relocates its inline elements. 'other' ends up
            /// empty and inline. This vector must be empty and inline.
            /// </summary>
            void take_from(small_vector &other) noexcept {
                if (other.is_inline()) {
                    relocate_range(other.mData, mData, other.mSize);
                    mSize = other.mSize;
                } else {
                    mData = other.mData;
                    mCapacity = other.mCapacity;
                    mSize = other.mSize;
                    other.mData = other.inline_data();
                    other.mCapacity = N;
                }
                other.mSize = 0;
            }
    };

    template <typename T, usize N>
    void swap(small_vector<T, N> &left, small_vector<T, N> &right) noexcept {
        left.swap(right);
    }
}
