#pragma once

#include "conjure/core/containers/detail.h"

#include "conjure/core/assert.h"
#include "conjure/core/types.h"

#include <algorithm>
#include <bit>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

// flat_hash_map / flat_hash_set: open-addressing hash containers.
//
//   flat_hash_map<string_id, u32> indexByName;
//   indexByName.try_emplace(name, index);
//   if (auto found = indexByName.find(name); found != indexByName.end()) { found->second = 0; }
//   for (auto &[key, value] : indexByName) { ... }
//
// Linear probing with backward-shift deletion (no tombstones), power-of-two capacity and a maximum
// load factor of 0.875. Probe sequences wrap around the end of the table and are never cut short:
// the table only rehashes when size() reaches capacity() * 0.875, so reserve() really does
// guarantee that the next insertions move nothing. Every slot stores 31 bits of the key's hash;
// a probe only compares keys whose stored hash matches, and erase recomputes an element's home
// bucket from it. Elements are stored in place as std::pair<const K, V> (K for the set).
//
// Inserting never moves an existing element unless it rehashes (only insertions rehash). Erasing
// shifts later elements of the same probe run one slot back, which invalidates pointers to those
// elements only. Iteration order is the slot order starting right after an empty "anchor" slot
// that erase never fills, so no probe run straddles the start of the sequence and erase(iterator)
// reliably returns the next unvisited element. An insertion that lands on the anchor moves it,
// so re-fetch end() after inserting. Storage comes from std::allocator through
// allocate_table/deallocate_table so the allocation point can be redirected to the memory area.

namespace conjure::core {

    namespace detail {

        template <typename K, typename V>
        struct pair_key_of {
            using key_type = K;

            [[nodiscard]] static const K &key(const std::pair<const K, V> &value) noexcept {
                return value.first;
            }
        };

        template <typename K>
        struct identity_key_of {
            using key_type = K;

            [[nodiscard]] static const K &key(const K &value) noexcept {
                return value;
            }
        };

        /// <summary>
        /// The table behind flat_hash_map and flat_hash_set. Value is the stored element, KeyOf
        /// extracts its key. Not thread-safe; concurrent const access is fine.
        /// </summary>
        template <typename Value, typename KeyOf, typename Hash, typename Eq>
        class hash_table {
            public:
                using key_type = typename KeyOf::key_type;
                using value_type = Value;

                template <bool IsConst>
                class basic_iterator;

                using iterator = basic_iterator<false>;
                using const_iterator = basic_iterator<true>;

                static constexpr f32 MAX_LOAD_FACTOR = 0.875f;
                static constexpr usize MIN_CAPACITY = 8;

                /// <summary>
                /// Largest number of buckets: the stored hash keeps 31 bits, so the home bucket is
                /// addressable for every capacity up to 2^31.
                /// </summary>
                static constexpr usize MAX_CAPACITY = usize{1} << 31;

            private:
                // Slot metadata: 0 = empty, otherwise OCCUPIED_BIT | (hash & HASH_MASK). The low
                // log2(capacity) bits of the stored hash are the home bucket, the remaining bits
                // filter key comparisons.
                static constexpr u32 OCCUPIED_BIT = 0x80000000u;
                static constexpr u32 HASH_MASK = 0x7FFFFFFFu;
                static constexpr usize NO_SLOT = std::numeric_limits<usize>::max();

                Value *mValues = nullptr;       // mCapacity slots, constructed where mMeta != 0
                u32 *mMeta = nullptr;           // mCapacity entries
                usize mCapacity = 0;            // power of two (0 before first insert)
                usize mMask = 0;                // mCapacity - 1 (0 before first insert)
                usize mSize = 0;
                usize mGrowAt = 0;              // size at which the next insert rehashes
                usize mAnchor = 0;              // an empty slot; iteration starts right after it
                Hash mHash = {};
                Eq mEqual = {};

            public:
                hash_table() = default;

                explicit hash_table(usize initialCapacity) {
                    reserve(initialCapacity);
                }

                hash_table(const hash_table &other) :
                    mHash(other.mHash),
                    mEqual(other.mEqual) {
                    clone_from(other);
                }

                hash_table(hash_table &&other) noexcept :
                    mValues(other.mValues),
                    mMeta(other.mMeta),
                    mCapacity(other.mCapacity),
                    mMask(other.mMask),
                    mSize(other.mSize),
                    mGrowAt(other.mGrowAt),
                    mAnchor(other.mAnchor),
                    mHash(std::move(other.mHash)),
                    mEqual(std::move(other.mEqual)) {
                    other.forget_table();
                }

                ~hash_table() {
                    destroy_elements();
                    deallocate_table();
                }

                hash_table &operator=(const hash_table &other) {
                    if (this != &other) {
                        destroy_elements();
                        deallocate_table();
                        mHash = other.mHash;
                        mEqual = other.mEqual;
                        clone_from(other);
                    }
                    return *this;
                }

                hash_table &operator=(hash_table &&other) noexcept {
                    if (this != &other) {
                        destroy_elements();
                        deallocate_table();
                        mValues = other.mValues;
                        mMeta = other.mMeta;
                        mCapacity = other.mCapacity;
                        mMask = other.mMask;
                        mSize = other.mSize;
                        mGrowAt = other.mGrowAt;
                        mAnchor = other.mAnchor;
                        mHash = std::move(other.mHash);
                        mEqual = std::move(other.mEqual);
                        other.forget_table();
                    }
                    return *this;
                }

            public:
                [[nodiscard]] iterator find(const key_type &key) noexcept {
                    const usize index = find_index(key);
                    return index == NO_SLOT ? end() : iterator_at(index);
                }

                [[nodiscard]] const_iterator find(const key_type &key) const noexcept {
                    const usize index = find_index(key);
                    return index == NO_SLOT ? end() : iterator_at(index);
                }

                [[nodiscard]] bool contains(const key_type &key) const noexcept {
                    return find_index(key) != NO_SLOT;
                }

                /// <summary>
                /// Inserts when the key is absent. 'build' receives the uninitialised slot and must
                /// construct the value there. Returns the element and whether it was inserted.
                /// Arguments captured by 'build' must not refer to elements of this table.
                /// </summary>
                template <typename Builder>
                std::pair<iterator, bool> insert_with(const key_type &key, Builder &&build) {
                    const u64 hash = hash_of(key);
                    const usize existing = find_index(hash, key);
                    if (existing != NO_SLOT) {
                        return {iterator_at(existing), false};
                    }
                    if (mSize >= mGrowAt) {
                        rehash(mCapacity == 0 ? MIN_CAPACITY : mCapacity * 2);
                    }
                    return {iterator_at(insert_new(hash, std::forward<Builder>(build))), true};
                }

                std::pair<iterator, bool> insert(const value_type &value) {
                    return insert_with(KeyOf::key(value), [&value](value_type *slot) {
                        std::construct_at(slot, value);
                    });
                }

                std::pair<iterator, bool> insert(value_type &&value) {
                    return insert_with(KeyOf::key(value), [&value](value_type *slot) {
                        std::construct_at(slot, std::move(value));
                    });
                }

                /// <summary>
                /// Removes the element with 'key'. Returns false when absent. Never rehashes.
                /// </summary>
                bool erase(const key_type &key) noexcept {
                    const usize index = find_index(key);
                    if (index == NO_SLOT) {
                        return false;
                    }
                    erase_index(index);
                    return true;
                }

                /// <summary>
                /// Removes the element at 'position'. Returns the iterator following it, which is
                /// the next element an iteration from begin() has not visited yet: deletion only
                /// shifts elements towards the start of their probe run and no run crosses the
                /// anchor the iteration order starts at.
                /// </summary>
                iterator erase(const_iterator position) noexcept {
                    const usize index = position.mIndex;
                    const bool occupied = position.mMeta == mMeta && mMeta != nullptr && index <= mMask &&
                        is_occupied(index);
                    CONJURE_ASSERT(occupied, "flat_hash_map: erase of an end or foreign iterator");
                    if (!occupied) {
                        return end();
                    }
                    erase_index(index);
                    return iterator_at(index);
                }

                /// <summary>
                /// Makes room for 'count' elements without a rehash during the next insertions.
                /// reserve(0) does nothing, so an empty table stays unallocated.
                /// </summary>
                void reserve(usize count) {
                    if (count == 0) {
                        return;
                    }
                    const usize requiredCapacity = capacity_for(count);
                    if (requiredCapacity > mCapacity) {
                        rehash(requiredCapacity);
                    }
                }

                /// <summary>
                /// Destroys every element; keeps the table so capacity() is unchanged.
                /// </summary>
                void clear() noexcept {
                    destroy_elements();
                    if (mMeta != nullptr) {
                        std::memset(mMeta, 0, mCapacity * sizeof(u32));
                    }
                    mSize = 0;
                    mAnchor = 0;
                }

                void swap(hash_table &other) noexcept {
                    std::swap(mValues, other.mValues);
                    std::swap(mMeta, other.mMeta);
                    std::swap(mCapacity, other.mCapacity);
                    std::swap(mMask, other.mMask);
                    std::swap(mSize, other.mSize);
                    std::swap(mGrowAt, other.mGrowAt);
                    std::swap(mAnchor, other.mAnchor);
                    std::swap(mHash, other.mHash);
                    std::swap(mEqual, other.mEqual);
                }

            public:
                [[nodiscard]] usize size() const noexcept {
                    return mSize;
                }

                [[nodiscard]] bool empty() const noexcept {
                    return mSize == 0;
                }

                /// <summary>
                /// Buckets (a power of two, 0 before the first insertion).
                /// </summary>
                [[nodiscard]] usize capacity() const noexcept {
                    return mCapacity;
                }

                /// <summary>
                /// size() / capacity(); 0 for an empty table.
                /// </summary>
                [[nodiscard]] f32 load_factor() const noexcept {
                    return mCapacity == 0 ? 0.0f : static_cast<f32>(mSize) / static_cast<f32>(mCapacity);
                }

                [[nodiscard]] const Hash &hash_function() const noexcept {
                    return mHash;
                }

                [[nodiscard]] const Eq &key_eq() const noexcept {
                    return mEqual;
                }

                [[nodiscard]] iterator begin() noexcept {
                    return iterator(mValues, mMeta, (mAnchor + 1) & mMask, mAnchor, mMask);
                }

                [[nodiscard]] const_iterator begin() const noexcept {
                    return const_iterator(mValues, mMeta, (mAnchor + 1) & mMask, mAnchor, mMask);
                }

                [[nodiscard]] iterator end() noexcept {
                    return iterator(mValues, mMeta, mAnchor, mAnchor, mMask);
                }

                [[nodiscard]] const_iterator end() const noexcept {
                    return const_iterator(mValues, mMeta, mAnchor, mAnchor, mMask);
                }

            private:
                [[nodiscard]] u64 hash_of(const key_type &key) const noexcept {
                    return mix_hash(static_cast<u64>(mHash(key)));
                }

                [[nodiscard]] usize home_of(u64 hash) const noexcept {
                    return static_cast<usize>(hash) & mMask;
                }

                /// <summary>
                /// What an occupied slot stores for 'hash'. Its low bits are the home bucket, so
                /// erase can recompute homes without re-hashing keys.
                /// </summary>
                [[nodiscard]] static u32 tag_of(u64 hash) noexcept {
                    return OCCUPIED_BIT | (static_cast<u32>(hash) & HASH_MASK);
                }

                [[nodiscard]] usize home_of_slot(usize index) const noexcept {
                    return static_cast<usize>(mMeta[index]) & mMask;
                }

                [[nodiscard]] bool is_occupied(usize index) const noexcept {
                    return mMeta[index] != 0;
                }

                /// <summary>
                /// Smallest power-of-two capacity (at least MIN_CAPACITY) whose grow threshold
                /// admits 'count' elements.
                /// </summary>
                [[nodiscard]] static usize capacity_for(usize count) noexcept {
                    usize capacity = MIN_CAPACITY;
                    while (grow_threshold(capacity) < count && capacity < MAX_CAPACITY) {
                        capacity *= 2;
                    }
                    return capacity;
                }

                [[nodiscard]] static usize grow_threshold(usize capacity) noexcept {
                    return capacity / 8 * 7;   // MAX_LOAD_FACTOR without floating point
                }

                [[nodiscard]] usize find_index(const key_type &key) const noexcept {
                    if (mCapacity == 0) {
                        return NO_SLOT;
                    }
                    return find_index(hash_of(key), key);
                }

                /// <summary>
                /// Slot of 'key' or NO_SLOT. The probe stops at the first empty slot (there always
                /// is one: the load factor is below one); keys are only compared when the stored
                /// hash matches.
                /// </summary>
                [[nodiscard]] usize find_index(u64 hash, const key_type &key) const noexcept {
                    if (mCapacity == 0) {
                        return NO_SLOT;
                    }
                    const u32 tag = tag_of(hash);
                    usize index = home_of(hash);
                    while (true) {
                        const u32 meta = mMeta[index];
                        if (meta == 0) {
                            return NO_SLOT;
                        }
                        if (meta == tag && mEqual(KeyOf::key(mValues[index]), key)) {
                            return index;
                        }
                        index = (index + 1) & mMask;
                    }
                }

                /// <summary>
                /// Places a value whose key is known to be absent in the first empty slot of its
                /// probe sequence. The caller has made sure the table is below its grow threshold.
                /// </summary>
                template <typename Builder>
                usize insert_new(u64 hash, Builder &&build) {
                    usize index = home_of(hash);
                    while (is_occupied(index)) {
                        index = (index + 1) & mMask;
                    }

                    build(mValues + index);
                    mMeta[index] = tag_of(hash);
                    ++mSize;
                    if (index == mAnchor) {
                        mAnchor = next_empty_after(index);
                    }
                    return index;
                }

                /// <summary>
                /// The first empty slot after 'index' in probe order. Exists because the table is
                /// never full.
                /// </summary>
                [[nodiscard]] usize next_empty_after(usize index) const noexcept {
                    usize candidate = (index + 1) & mMask;
                    while (is_occupied(candidate)) {
                        candidate = (candidate + 1) & mMask;
                    }
                    return candidate;
                }

                /// <summary>
                /// Backward-shift deletion: later elements of the run whose home is at or before the
                /// hole move into it, so no tombstone is needed and lookups stay short after many
                /// erases. The anchor is empty and therefore never part of the run.
                /// </summary>
                void erase_index(usize index) noexcept {
                    std::destroy_at(mValues + index);
                    mMeta[index] = 0;
                    --mSize;

                    usize hole = index;
                    for (usize next = (index + 1) & mMask; is_occupied(next); next = (next + 1) & mMask) {
                        const usize distance = (next - home_of_slot(next)) & mMask;
                        const usize shift = (next - hole) & mMask;
                        if (distance < shift) {
                            continue;   // its home lies after the hole; it must stay
                        }
                        relocate(mValues + next, mValues + hole);
                        mMeta[hole] = mMeta[next];
                        mMeta[next] = 0;
                        hole = next;
                    }
                }

                void rehash(usize newCapacity) {
                    CONJURE_ASSERT(std::has_single_bit(newCapacity), "flat_hash_map capacity must be a power of two");
                    CONJURE_FATAL_ASSERT(newCapacity <= MAX_CAPACITY, "flat_hash_map: capacity limit reached");
                    Value *oldValues = mValues;
                    u32 *oldMeta = mMeta;
                    const usize oldCapacity = mCapacity;
                    const usize oldSize = mSize;

                    allocate_table(newCapacity);
                    mSize = 0;
                    for (usize index = 0; index < oldCapacity; ++index) {
                        if (oldMeta[index] == 0) {
                            continue;
                        }
                        Value *source = oldValues + index;
                        insert_new(hash_of(KeyOf::key(*source)), [source](Value *slot) {
                            std::construct_at(slot, std::move(*source));
                        });
                        std::destroy_at(source);
                    }
                    CONJURE_ASSERT(mSize == oldSize, "flat_hash_map lost elements during rehash");
                    deallocate_table(oldValues, oldMeta, oldCapacity);
                }

                /// <summary>
                /// Copies another table slot for slot, which keeps its iteration order.
                /// </summary>
                void clone_from(const hash_table &other) {
                    if (other.mCapacity == 0) {
                        forget_table();
                        return;
                    }
                    allocate_table(other.mCapacity);
                    for (usize index = 0; index < other.mCapacity; ++index) {
                        if (other.is_occupied(index)) {
                            std::construct_at(mValues + index, other.mValues[index]);
                            mMeta[index] = other.mMeta[index];
                        }
                    }
                    mSize = other.mSize;
                    mAnchor = other.mAnchor;
                }

                static void relocate(Value *source, Value *destination) noexcept {
                    if constexpr (std::is_trivially_copyable_v<Value>) {
                        std::memcpy(static_cast<void *>(destination), static_cast<const void *>(source), sizeof(Value));
                    } else {
                        std::construct_at(destination, std::move(*source));
                        std::destroy_at(source);
                    }
                }

                void destroy_elements() noexcept {
                    if constexpr (!std::is_trivially_destructible_v<Value>) {
                        for (usize index = 0; index < mCapacity; ++index) {
                            if (is_occupied(index)) {
                                std::destroy_at(mValues + index);
                            }
                        }
                    }
                }

                [[nodiscard]] iterator iterator_at(usize index) noexcept {
                    return iterator(mValues, mMeta, index, mAnchor, mMask);
                }

                [[nodiscard]] const_iterator iterator_at(usize index) const noexcept {
                    return const_iterator(mValues, mMeta, index, mAnchor, mMask);
                }

                /*===============================================================================*/
                // Allocation points. Everything that touches the heap goes through these two.

                void allocate_table(usize capacity) {
                    mCapacity = capacity;
                    mMask = capacity - 1;
                    mGrowAt = grow_threshold(capacity);
                    mAnchor = 0;
                    mValues = std::allocator<Value>().allocate(capacity);
                    mMeta = std::allocator<u32>().allocate(capacity);
                    std::memset(mMeta, 0, capacity * sizeof(u32));
                }

                static void deallocate_table(Value *values, u32 *meta, usize capacity) noexcept {
                    if (values != nullptr) {
                        std::allocator<Value>().deallocate(values, capacity);
                    }
                    if (meta != nullptr) {
                        std::allocator<u32>().deallocate(meta, capacity);
                    }
                }

                void deallocate_table() noexcept {
                    deallocate_table(mValues, mMeta, mCapacity);
                    forget_table();
                }

                void forget_table() noexcept {
                    mValues = nullptr;
                    mMeta = nullptr;
                    mCapacity = 0;
                    mMask = 0;
                    mSize = 0;
                    mGrowAt = 0;
                    mAnchor = 0;
                }

            public:
                /// <summary>
                /// Forward iterator over occupied slots. Walks the table cyclically from the slot
                /// after the anchor and stops at the anchor, so it visits every element exactly
                /// once even when a probe run wraps around the end of the table.
                /// </summary>
                template <bool IsConst>
                class basic_iterator {
                    friend class hash_table;

                    public:
                        using iterator_category = std::forward_iterator_tag;
                        using value_type = Value;
                        using difference_type = isize;
                        using pointer = std::conditional_t<IsConst, const Value *, Value *>;
                        using reference = std::conditional_t<IsConst, const Value &, Value &>;

                    private:
                        pointer mValues = nullptr;
                        const u32 *mMeta = nullptr;
                        usize mIndex = 0;
                        usize mAnchor = 0;
                        usize mMask = 0;

                    public:
                        basic_iterator() noexcept = default;

                        basic_iterator(
                          pointer values,
                          const u32 *meta,
                          usize index,
                          usize anchor,
                          usize mask
                        ) noexcept :
                            mValues(values),
                            mMeta(meta),
                            mIndex(index),
                            mAnchor(anchor),
                            mMask(mask) {
                            skip_empty();
                        }

                        /// <summary>
                        /// Mutable iterators convert to const ones.
                        /// </summary>
                        template <bool OtherConst>
                            requires (IsConst && !OtherConst)
                        basic_iterator(const basic_iterator<OtherConst> &other) noexcept :
                            mValues(other.mValues),
                            mMeta(other.mMeta),
                            mIndex(other.mIndex),
                            mAnchor(other.mAnchor),
                            mMask(other.mMask) {}

                    public:
                        [[nodiscard]] reference operator*() const noexcept {
                            return mValues[mIndex];
                        }

                        [[nodiscard]] pointer operator->() const noexcept {
                            return mValues + mIndex;
                        }

                        basic_iterator &operator++() noexcept {
                            mIndex = (mIndex + 1) & mMask;
                            skip_empty();
                            return *this;
                        }

                        basic_iterator operator++(int) noexcept {
                            basic_iterator copy = *this;
                            ++(*this);
                            return copy;
                        }

                        [[nodiscard]] bool operator==(const basic_iterator &other) const noexcept {
                            return mIndex == other.mIndex && mMeta == other.mMeta;
                        }

                    private:
                        void skip_empty() noexcept {
                            while (mIndex != mAnchor && mMeta[mIndex] == 0) {
                                mIndex = (mIndex + 1) & mMask;
                            }
                        }

                        template <bool>
                        friend class basic_iterator;
                };
        };
    }

    /// <summary>
    /// Open-addressing hash map storing std::pair<const K, V> in place. Iteration order is the
    /// slot order (preserved by copies) and changes on rehash. Not thread-safe; concurrent const
    /// access is fine.
    ///
    /// Rehashing happens only when an insertion would push size() above capacity() *
    /// MAX_LOAD_FACTOR, so after reserve(n) the next n insertions move nothing. Pointers and
    /// references to elements are otherwise invalidated only by erasing an element of the same
    /// probe run (backward shift). Re-fetch end() after inserting: an insertion can move the slot
    /// the iteration order starts at. Arguments passed to an inserting call must not refer to
    /// elements of the same map.
    /// </summary>
    template <typename K, typename V, typename Hash = std::hash<K>, typename Eq = std::equal_to<K>>
    class flat_hash_map {
        public:
            using key_type = K;
            using mapped_type = V;
            using value_type = std::pair<const K, V>;
            using hasher = Hash;
            using key_equal = Eq;
            using size_type = usize;
            using difference_type = isize;
            using reference = value_type &;
            using const_reference = const value_type &;

        private:
            using table_type = detail::hash_table<value_type, detail::pair_key_of<K, V>, Hash, Eq>;

        public:
            using iterator = typename table_type::iterator;
            using const_iterator = typename table_type::const_iterator;

            static constexpr f32 MAX_LOAD_FACTOR = table_type::MAX_LOAD_FACTOR;
            static constexpr usize MAX_CAPACITY = table_type::MAX_CAPACITY;

        private:
            table_type mTable = {};

        public:
            flat_hash_map() = default;
            ~flat_hash_map() = default;

            /// <summary>
            /// Pre-sizes the table for 'initialCapacity' elements; 0 allocates nothing.
            /// </summary>
            explicit flat_hash_map(usize initialCapacity) :
                mTable(initialCapacity) {}

            /// <summary>
            /// Builds the map from pairs; for duplicate keys the first one wins.
            /// </summary>
            flat_hash_map(std::initializer_list<value_type> values) :
                mTable(values.size()) {
                for (const value_type &value : values) {
                    mTable.insert(value);
                }
            }

            flat_hash_map(const flat_hash_map &) = default;
            flat_hash_map &operator=(const flat_hash_map &) = default;
            flat_hash_map(flat_hash_map &&) noexcept = default;
            flat_hash_map &operator=(flat_hash_map &&) noexcept = default;

        public:
            /// <summary>
            /// The element with key 'key', or end().
            /// </summary>
            [[nodiscard]] iterator find(const K &key) noexcept {
                return mTable.find(key);
            }

            [[nodiscard]] const_iterator find(const K &key) const noexcept {
                return mTable.find(key);
            }

            [[nodiscard]] bool contains(const K &key) const noexcept {
                return mTable.contains(key);
            }

            /// <summary>
            /// 0 or 1.
            /// </summary>
            [[nodiscard]] usize count(const K &key) const noexcept {
                return mTable.contains(key) ? 1 : 0;
            }

            /// <summary>
            /// Inserts when the key is absent. Returns the element and whether it was inserted.
            /// </summary>
            std::pair<iterator, bool> insert(const value_type &value) {
                return mTable.insert(value);
            }

            std::pair<iterator, bool> insert(value_type &&value) {
                return mTable.insert(std::move(value));
            }

            /// <summary>
            /// Inserts every element of the range that is not already present.
            /// </summary>
            template <std::input_iterator Iterator>
            void insert(Iterator first, Iterator last) {
                for (Iterator source = first; source != last; ++source) {
                    mTable.insert(*source);
                }
            }

            /// <summary>
            /// Constructs a pair from the arguments and inserts it when its key is absent.
            /// </summary>
            template <typename... Args>
            std::pair<iterator, bool> emplace(Args &&...args) {
                return mTable.insert(value_type(std::forward<Args>(args)...));
            }

            /// <summary>
            /// Inserts {key, V(args...)} when 'key' is absent; the value is only constructed in
            /// that case. Returns the element and whether it was inserted.
            /// </summary>
            template <typename... Args>
            std::pair<iterator, bool> try_emplace(const K &key, Args &&...args) {
                return mTable.insert_with(key, [&key, &args...](value_type *slot) {
                    std::construct_at(
                      slot,
                      std::piecewise_construct,
                      std::forward_as_tuple(key),
                      std::forward_as_tuple(std::forward<Args>(args)...)
                    );
                });
            }

            template <typename... Args>
            std::pair<iterator, bool> try_emplace(K &&key, Args &&...args) {
                return mTable.insert_with(key, [&key, &args...](value_type *slot) {
                    std::construct_at(
                      slot,
                      std::piecewise_construct,
                      std::forward_as_tuple(std::move(key)),
                      std::forward_as_tuple(std::forward<Args>(args)...)
                    );
                });
            }

            /// <summary>
            /// Inserts or overwrites. Returns the element and true when it was newly inserted.
            /// </summary>
            template <typename M>
            std::pair<iterator, bool> insert_or_assign(const K &key, M &&value) {
                std::pair<iterator, bool> inserted = try_emplace(key, std::forward<M>(value));
                if (!inserted.second) {
                    inserted.first->second = std::forward<M>(value);
                }
                return inserted;
            }

            template <typename M>
            std::pair<iterator, bool> insert_or_assign(K &&key, M &&value) {
                std::pair<iterator, bool> inserted = try_emplace(std::move(key), std::forward<M>(value));
                if (!inserted.second) {
                    inserted.first->second = std::forward<M>(value);
                }
                return inserted;
            }

            /// <summary>
            /// The value for 'key', default-constructed and inserted when absent.
            /// </summary>
            [[nodiscard]] V &operator[](const K &key) {
                return try_emplace(key).first->second;
            }

            [[nodiscard]] V &operator[](K &&key) {
                return try_emplace(std::move(key)).first->second;
            }

            /// <summary>
            /// The value for 'key'. The key must be present: a missing key is a fatal programmer
            /// error (asserts, then terminates), never an exception.
            /// </summary>
            [[nodiscard]] V &at(const K &key) noexcept {
                iterator found = mTable.find(key);
                if (found == mTable.end()) [[unlikely]] {
                    detail::fatal_container_access("flat_hash_map::at: key not found");
                }
                return found->second;
            }

            [[nodiscard]] const V &at(const K &key) const noexcept {
                const_iterator found = mTable.find(key);
                if (found == mTable.end()) [[unlikely]] {
                    detail::fatal_container_access("flat_hash_map::at: key not found");
                }
                return found->second;
            }

            /// <summary>
            /// Removes the element with 'key'. Returns false when absent. Never rehashes.
            /// </summary>
            bool erase(const K &key) noexcept {
                return mTable.erase(key);
            }

            /// <summary>
            /// Removes the element at 'position'. Returns the iterator following it.
            /// </summary>
            iterator erase(const_iterator position) noexcept {
                return mTable.erase(position);
            }

            /// <summary>
            /// Makes room for 'count' elements without a rehash during the next insertions.
            /// reserve(0) does nothing, so an empty map stays unallocated.
            /// </summary>
            void reserve(usize count) {
                mTable.reserve(count);
            }

            /// <summary>
            /// Destroys every element; keeps the table so capacity() is unchanged.
            /// </summary>
            void clear() noexcept {
                mTable.clear();
            }

            void swap(flat_hash_map &other) noexcept {
                mTable.swap(other.mTable);
            }

        public:
            [[nodiscard]] usize size() const noexcept {
                return mTable.size();
            }

            [[nodiscard]] bool empty() const noexcept {
                return mTable.empty();
            }

            /// <summary>
            /// Buckets (a power of two, 0 before the first insertion). The table rehashes only
            /// when size() would exceed capacity() * MAX_LOAD_FACTOR.
            /// </summary>
            [[nodiscard]] usize capacity() const noexcept {
                return mTable.capacity();
            }

            [[nodiscard]] f32 load_factor() const noexcept {
                return mTable.load_factor();
            }

            [[nodiscard]] const Hash &hash_function() const noexcept {
                return mTable.hash_function();
            }

            [[nodiscard]] const Eq &key_eq() const noexcept {
                return mTable.key_eq();
            }

            [[nodiscard]] iterator begin() noexcept {
                return mTable.begin();
            }

            [[nodiscard]] const_iterator begin() const noexcept {
                return mTable.begin();
            }

            [[nodiscard]] const_iterator cbegin() const noexcept {
                return mTable.begin();
            }

            [[nodiscard]] iterator end() noexcept {
                return mTable.end();
            }

            [[nodiscard]] const_iterator end() const noexcept {
                return mTable.end();
            }

            [[nodiscard]] const_iterator cend() const noexcept {
                return mTable.end();
            }

            /// <summary>
            /// Same keys with equal values, regardless of slot order.
            /// </summary>
            [[nodiscard]] bool operator==(const flat_hash_map &other) const {
                if (size() != other.size()) {
                    return false;
                }
                for (const value_type &entry : *this) {
                    const_iterator found = other.find(entry.first);
                    if (found == other.end() || !(found->second == entry.second)) {
                        return false;
                    }
                }
                return true;
            }
    };

    /// <summary>
    /// Open-addressing hash set with the same table (and the same rehash and invalidation rules)
    /// as flat_hash_map. Elements are immutable through iterators. Not thread-safe; concurrent
    /// const access is fine.
    /// </summary>
    template <typename K, typename Hash = std::hash<K>, typename Eq = std::equal_to<K>>
    class flat_hash_set {
        public:
            using key_type = K;
            using value_type = K;
            using hasher = Hash;
            using key_equal = Eq;
            using size_type = usize;
            using difference_type = isize;
            using reference = const K &;
            using const_reference = const K &;

        private:
            using table_type = detail::hash_table<K, detail::identity_key_of<K>, Hash, Eq>;

        public:
            using iterator = typename table_type::const_iterator;
            using const_iterator = typename table_type::const_iterator;

            static constexpr f32 MAX_LOAD_FACTOR = table_type::MAX_LOAD_FACTOR;
            static constexpr usize MAX_CAPACITY = table_type::MAX_CAPACITY;

        private:
            table_type mTable = {};

        public:
            flat_hash_set() = default;
            ~flat_hash_set() = default;

            /// <summary>
            /// Pre-sizes the table for 'initialCapacity' elements; 0 allocates nothing.
            /// </summary>
            explicit flat_hash_set(usize initialCapacity) :
                mTable(initialCapacity) {}

            flat_hash_set(std::initializer_list<K> values) :
                mTable(values.size()) {
                for (const K &value : values) {
                    mTable.insert(value);
                }
            }

            flat_hash_set(const flat_hash_set &) = default;
            flat_hash_set &operator=(const flat_hash_set &) = default;
            flat_hash_set(flat_hash_set &&) noexcept = default;
            flat_hash_set &operator=(flat_hash_set &&) noexcept = default;

        public:
            [[nodiscard]] const_iterator find(const K &key) const noexcept {
                return mTable.find(key);
            }

            [[nodiscard]] bool contains(const K &key) const noexcept {
                return mTable.contains(key);
            }

            [[nodiscard]] usize count(const K &key) const noexcept {
                return mTable.contains(key) ? 1 : 0;
            }

            /// <summary>
            /// Inserts when absent. Returns the element and whether it was inserted.
            /// </summary>
            std::pair<const_iterator, bool> insert(const K &value) {
                return mTable.insert(value);
            }

            std::pair<const_iterator, bool> insert(K &&value) {
                return mTable.insert(std::move(value));
            }

            template <std::input_iterator Iterator>
            void insert(Iterator first, Iterator last) {
                for (Iterator source = first; source != last; ++source) {
                    mTable.insert(*source);
                }
            }

            template <typename... Args>
            std::pair<const_iterator, bool> emplace(Args &&...args) {
                return mTable.insert(K(std::forward<Args>(args)...));
            }

            bool erase(const K &key) noexcept {
                return mTable.erase(key);
            }

            const_iterator erase(const_iterator position) noexcept {
                return mTable.erase(position);
            }

            /// <summary>
            /// Makes room for 'count' elements without a rehash during the next insertions.
            /// reserve(0) does nothing, so an empty set stays unallocated.
            /// </summary>
            void reserve(usize count) {
                mTable.reserve(count);
            }

            void clear() noexcept {
                mTable.clear();
            }

            void swap(flat_hash_set &other) noexcept {
                mTable.swap(other.mTable);
            }

        public:
            [[nodiscard]] usize size() const noexcept {
                return mTable.size();
            }

            [[nodiscard]] bool empty() const noexcept {
                return mTable.empty();
            }

            [[nodiscard]] usize capacity() const noexcept {
                return mTable.capacity();
            }

            [[nodiscard]] f32 load_factor() const noexcept {
                return mTable.load_factor();
            }

            [[nodiscard]] const_iterator begin() const noexcept {
                return mTable.begin();
            }

            [[nodiscard]] const_iterator cbegin() const noexcept {
                return mTable.begin();
            }

            [[nodiscard]] const_iterator end() const noexcept {
                return mTable.end();
            }

            [[nodiscard]] const_iterator cend() const noexcept {
                return mTable.end();
            }

            /// <summary>
            /// Same elements, regardless of slot order.
            /// </summary>
            [[nodiscard]] bool operator==(const flat_hash_set &other) const {
                if (size() != other.size()) {
                    return false;
                }
                for (const K &value : *this) {
                    if (!other.contains(value)) {
                        return false;
                    }
                }
                return true;
            }
    };

    template <typename K, typename V, typename Hash, typename Eq>
    void swap(flat_hash_map<K, V, Hash, Eq> &left, flat_hash_map<K, V, Hash, Eq> &right) noexcept {
        left.swap(right);
    }

    template <typename K, typename Hash, typename Eq>
    void swap(flat_hash_set<K, Hash, Eq> &left, flat_hash_set<K, Hash, Eq> &right) noexcept {
        left.swap(right);
    }
}
