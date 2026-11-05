#pragma once

#include "conjure/core/assert.h"
#include "conjure/core/types.h"

#include <compare>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <limits>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// slot_map: generational handles over dense storage.
//
//   slot_map<particle> particles;
//   const slot_key key = particles.emplace(position, velocity);
//   if (particle *found = particles.find(key)) { ... }      // nullptr once the key is stale
//   particles.erase(key);
//   for (auto &[key, particle] : particles) { ... }          // live elements, dense storage order
//
// Lookup is O(1), allocation-free and rejects stale keys: every slot carries a generation that is
// bumped when its element is erased. Elements live in a dense array of {key, value} entries
// (swap-remove on erase) so iteration is cache friendly; element addresses are stable only until
// the next insert or erase. The text form of slot_key (to_string, parse, std::formatter) lives in
// slot_key_format.h so this header stays light.

namespace conjure::core {

    /// <summary>
    /// Default key of a slot_map: a slot index and the generation of that slot when the key was
    /// issued. The all-zero key is null; generation 0 is never issued.
    /// </summary>
    struct slot_key {
        u32 index = 0;
        u32 generation = 0;

        /// <summary>
        /// True for every key a slot_map has issued; false for the null key.
        /// </summary>
        [[nodiscard]] constexpr bool is_valid() const noexcept {
            return generation != 0;
        }

        /// <summary>
        /// Packs the key into one integer (generation in the high 32 bits). Used for ImTextureID
        /// style opaque handles and serialisation.
        /// </summary>
        [[nodiscard]] constexpr u64 to_u64() const noexcept {
            return (static_cast<u64>(generation) << 32) | static_cast<u64>(index);
        }

        /// <summary>
        /// Inverse of to_u64().
        /// </summary>
        [[nodiscard]] static constexpr slot_key from_u64(u64 packed) noexcept {
            return slot_key{static_cast<u32>(packed & 0xFFFFFFFFull), static_cast<u32>(packed >> 32)};
        }

        /// <summary>
        /// The all-zero key. Means "none".
        /// </summary>
        [[nodiscard]] static constexpr slot_key null() noexcept {
            return slot_key{};
        }

        [[nodiscard]] constexpr auto operator<=>(const slot_key &) const noexcept = default;
        [[nodiscard]] constexpr bool operator==(const slot_key &) const noexcept = default;
    };

    static_assert(sizeof(slot_key) == 8, "slot_key must pack into 64 bits");
    static_assert(std::is_trivially_copyable_v<slot_key>, "slot_key must be trivially copyable");

    /// <summary>
    /// Requirements on a custom key type: trivially copyable, default constructible (the null
    /// key), with assignable unsigned 'index' and 'generation' fields of at most 32 bits each.
    /// Narrower fields are allowed: a u16 generation retires a slot after 65535 reuses.
    /// </summary>
    template <typename Key>
    concept slot_key_type = std::is_trivially_copyable_v<Key> && std::is_default_constructible_v<Key> &&
        std::unsigned_integral<std::remove_cvref_t<decltype(Key::index)>> &&
        std::unsigned_integral<std::remove_cvref_t<decltype(Key::generation)>> &&
        sizeof(Key::index) <= sizeof(u32) && sizeof(Key::generation) <= sizeof(u32) &&
        requires(
          Key key,
          std::remove_cvref_t<decltype(Key::index)> index,
          std::remove_cvref_t<decltype(Key::generation)> generation
        ) {
            key.index = index;
            key.generation = generation;
        };

    static_assert(slot_key_type<slot_key>);

    /// <summary>
    /// What a slot_map stores and what its iterators point at: the key and the element. Supports
    /// structured bindings (auto &[key, value] binds straight into the map's storage) and
    /// key()/value() accessors. The key is read-only; changing it would corrupt the map.
    /// </summary>
    template <typename Key, typename T>
    class slot_map_entry {
        private:
            Key mKey = {};
            T mValue;   // no default initialiser: T may have no default constructor; every
                        // constructor below initialises it

        public:
            template <typename... Args>
                requires std::is_constructible_v<T, Args &&...>
            explicit slot_map_entry(const Key &key, Args &&...args) :
                mKey(key),
                mValue(std::forward<Args>(args)...) {}

        public:
            /// <summary>
            /// The key that addresses this element.
            /// </summary>
            [[nodiscard]] constexpr const Key &key() const noexcept {
                return mKey;
            }

            /// <summary>
            /// The element itself.
            /// </summary>
            [[nodiscard]] constexpr T &value() noexcept {
                return mValue;
            }

            [[nodiscard]] constexpr const T &value() const noexcept {
                return mValue;
            }

            /// <summary>
            /// Tuple protocol for structured bindings: get<0>() is the key, get<1>() the element.
            /// </summary>
            template <usize Index>
            [[nodiscard]] constexpr decltype(auto) get() noexcept {
                static_assert(Index < 2, "slot_map_entry has two members: key and value");
                if constexpr (Index == 0) {
                    return key();
                } else {
                    return value();
                }
            }

            template <usize Index>
            [[nodiscard]] constexpr decltype(auto) get() const noexcept {
                static_assert(Index < 2, "slot_map_entry has two members: key and value");
                if constexpr (Index == 0) {
                    return key();
                } else {
                    return value();
                }
            }
    };

    /// <summary>
    /// Dense storage addressed by generational keys. Insertion returns a key that stays valid
    /// until that element is erased; afterwards find() returns nullptr for it, even when the slot
    /// has been reused. Not thread-safe; concurrent const access is fine.
    ///
    /// T must be move-constructible and move-assignable (swap-remove). Pointers returned by find()
    /// and references and iterators obtained during iteration are invalidated by insert, emplace,
    /// erase, clear and reserve.
    /// </summary>
    template <typename T, slot_key_type Key = slot_key>
    class slot_map {
        public:
            using value_type = T;
            using key_type = Key;
            using size_type = usize;
            using index_type = std::remove_cvref_t<decltype(Key::index)>;
            using generation_type = std::remove_cvref_t<decltype(Key::generation)>;
            using entry = slot_map_entry<Key, T>;
            using iterator = entry *;
            using const_iterator = const entry *;

            /// <summary>
            /// Largest number of slots the key's index type can address.
            /// </summary>
            static constexpr usize MAX_SLOT_COUNT = static_cast<usize>(
              std::numeric_limits<index_type>::max() < std::numeric_limits<u32>::max()
                ? static_cast<u64>(std::numeric_limits<index_type>::max()) + 1
                : static_cast<u64>(std::numeric_limits<u32>::max())
            );

        private:
            static constexpr u32 INVALID_INDEX = std::numeric_limits<u32>::max();

            /// <summary>
            /// One slot of the indirection table. A slot is live (denseIndex valid), free (on the
            /// free list, generation = the one issued next) or retired (generation wrapped).
            /// </summary>
            struct slot_entry {
                u32 denseIndex = INVALID_INDEX;
                u32 nextFree = INVALID_INDEX;
                u32 generation = 0;
            };

            std::vector<entry> mEntries = {};         // dense {key, value} storage
            std::vector<slot_entry> mSlots = {};      // indirection table indexed by key.index
            u32 mFreeHead = INVALID_INDEX;
            u32 mFreeCount = 0;
            u32 mRetiredCount = 0;

        public:
            slot_map() = default;
            ~slot_map() = default;

            slot_map(const slot_map &) = default;
            slot_map &operator=(const slot_map &) = default;
            slot_map(slot_map &&) noexcept = default;
            slot_map &operator=(slot_map &&) noexcept = default;

        public:
            /// <summary>
            /// Constructs an element in place and returns its key. Returns the null key (and
            /// asserts) when the key's index type cannot address another slot.
            /// </summary>
            template <typename... Args>
            [[nodiscard]] Key emplace(Args &&...args) {
                u32 slotIndex = INVALID_INDEX;
                if (mFreeHead != INVALID_INDEX) {
                    slotIndex = mFreeHead;
                    mFreeHead = mSlots[slotIndex].nextFree;
                    --mFreeCount;
                } else {
                    if (mSlots.size() >= MAX_SLOT_COUNT) {
                        CONJURE_ASSERT(false, "slot_map: key index type exhausted ({} slots)", mSlots.size());
                        return Key{};
                    }
                    slotIndex = static_cast<u32>(mSlots.size());
                    mSlots.push_back(slot_entry{});
                }

                slot_entry &slot = mSlots[slotIndex];
                slot.nextFree = INVALID_INDEX;
                if (slot.generation == 0) {
                    slot.generation = 1;   // generation 0 is reserved for the null key
                }

                const u32 denseIndex = static_cast<u32>(mEntries.size());
                const Key key = make_key(slotIndex, slot.generation);
                mEntries.emplace_back(key, std::forward<Args>(args)...);
                slot.denseIndex = denseIndex;
                return key;
            }

            /// <summary>
            /// Moves an element in and returns its key.
            /// </summary>
            [[nodiscard]] Key insert(T &&value) {
                return emplace(std::move(value));
            }

            /// <summary>
            /// Copies an element in and returns its key.
            /// </summary>
            [[nodiscard]] Key insert(const T &value) {
                return emplace(value);
            }

            /// <summary>
            /// Destroys the element addressed by 'key'. Returns false for null, stale or unknown
            /// keys. The slot's generation is bumped so every outstanding copy of the key becomes
            /// stale; a generation that would wrap to 0 retires the slot for good.
            /// </summary>
            bool erase(Key key) {
                const slot_entry *slot = live_slot(key);
                if (slot == nullptr) {
                    return false;
                }
                erase_at(slot->denseIndex);
                return true;
            }

            /// <summary>
            /// Erases the element an iterator points at. Returns the iterator to the element that
            /// now occupies that storage position (or end()), so erase-while-iterating works.
            /// The iterator must point at an element of this map; end() and iterators of other
            /// maps assert and leave the map unchanged.
            /// </summary>
            iterator erase(const_iterator position) {
                const bool valid = position >= mEntries.data() && position < mEntries.data() + mEntries.size();
                CONJURE_ASSERT(valid, "slot_map: erase of an end or foreign iterator");
                if (!valid) {
                    return end();
                }
                const usize denseIndex = static_cast<usize>(position - mEntries.data());
                erase_at(static_cast<u32>(denseIndex));
                return mEntries.data() + denseIndex;
            }

            /// <summary>
            /// The element addressed by 'key', or nullptr when the key is null, stale or unknown.
            /// Allocation-free, O(1).
            /// </summary>
            [[nodiscard]] T *find(Key key) noexcept {
                const slot_entry *slot = live_slot(key);
                return slot == nullptr ? nullptr : &mEntries[slot->denseIndex].value();
            }

            [[nodiscard]] const T *find(Key key) const noexcept {
                const slot_entry *slot = live_slot(key);
                return slot == nullptr ? nullptr : &mEntries[slot->denseIndex].value();
            }

            /// <summary>
            /// True when 'key' addresses a live element.
            /// </summary>
            [[nodiscard]] bool contains(Key key) const noexcept {
                return live_slot(key) != nullptr;
            }

            /// <summary>
            /// Key of the element at dense position 'denseIndex' (the order iteration uses).
            /// Asserts and returns the null key when the position is out of range.
            /// </summary>
            [[nodiscard]] Key key_at(usize denseIndex) const noexcept {
                CONJURE_ASSERT(denseIndex < mEntries.size(), "slot_map: dense index {} out of range", denseIndex);
                if (denseIndex >= mEntries.size()) {
                    return Key{};
                }
                return mEntries[denseIndex].key();
            }

            /// <summary>
            /// Number of live elements.
            /// </summary>
            [[nodiscard]] usize size() const noexcept {
                return mEntries.size();
            }

            [[nodiscard]] bool empty() const noexcept {
                return mEntries.empty();
            }

            /// <summary>
            /// Elements the dense storage can hold without reallocating.
            /// </summary>
            [[nodiscard]] usize capacity() const noexcept {
                return mEntries.capacity();
            }

            /// <summary>
            /// Slots in the indirection table (live + free + retired).
            /// </summary>
            [[nodiscard]] usize slot_count() const noexcept {
                return mSlots.size();
            }

            /// <summary>
            /// Slots on the free list, reused by the next insertions.
            /// </summary>
            [[nodiscard]] usize free_slot_count() const noexcept {
                return mFreeCount;
            }

            /// <summary>
            /// Slots whose generation wrapped; they are never reused.
            /// </summary>
            [[nodiscard]] usize retired_slot_count() const noexcept {
                return mRetiredCount;
            }

            /// <summary>
            /// Reserves dense storage and slots for 'count' elements.
            /// </summary>
            void reserve(usize count) {
                mEntries.reserve(count);
                mSlots.reserve(count);
            }

            /// <summary>
            /// Erases every element. Every outstanding key becomes stale (generations are bumped);
            /// slots and capacity are kept for reuse.
            /// </summary>
            void clear() {
                // Walk the slots backwards so the free list hands out the lowest index first.
                for (usize slotIndex = mSlots.size(); slotIndex > 0; --slotIndex) {
                    if (mSlots[slotIndex - 1].denseIndex != INVALID_INDEX) {
                        release_slot(static_cast<u32>(slotIndex - 1));
                    }
                }
                mEntries.clear();
            }

            /// <summary>
            /// The live {key, value} entries in dense storage order (the order iteration uses).
            /// </summary>
            [[nodiscard]] std::span<entry> entries() noexcept {
                return std::span<entry>(mEntries.data(), mEntries.size());
            }

            [[nodiscard]] std::span<const entry> entries() const noexcept {
                return std::span<const entry>(mEntries.data(), mEntries.size());
            }

            [[nodiscard]] iterator begin() noexcept {
                return mEntries.data();
            }

            [[nodiscard]] iterator end() noexcept {
                return mEntries.data() + mEntries.size();
            }

            [[nodiscard]] const_iterator begin() const noexcept {
                return mEntries.data();
            }

            [[nodiscard]] const_iterator end() const noexcept {
                return mEntries.data() + mEntries.size();
            }

            [[nodiscard]] const_iterator cbegin() const noexcept {
                return begin();
            }

            [[nodiscard]] const_iterator cend() const noexcept {
                return end();
            }

        private:
            /// <summary>
            /// Builds a key field by field so key types that declare their members in any order
            /// (or add more) work.
            /// </summary>
            [[nodiscard]] static Key make_key(u32 slotIndex, u32 generation) noexcept {
                Key key = {};
                key.index = static_cast<index_type>(slotIndex);
                key.generation = static_cast<generation_type>(generation);
                return key;
            }

            [[nodiscard]] const slot_entry *live_slot(Key key) const noexcept {
                if (key.generation == 0) {
                    return nullptr;
                }
                const usize slotIndex = static_cast<usize>(key.index);
                if (slotIndex >= mSlots.size()) {
                    return nullptr;
                }
                const slot_entry &slot = mSlots[slotIndex];
                if (slot.denseIndex == INVALID_INDEX || slot.generation != static_cast<u32>(key.generation)) {
                    return nullptr;
                }
                return &slot;
            }

            void erase_at(u32 denseIndex) {
                const u32 slotIndex = static_cast<u32>(mEntries[denseIndex].key().index);
                const u32 lastIndex = static_cast<u32>(mEntries.size() - 1);
                if (denseIndex != lastIndex) {
                    mEntries[denseIndex] = std::move(mEntries[lastIndex]);
                    mSlots[static_cast<usize>(mEntries[denseIndex].key().index)].denseIndex = denseIndex;
                }
                mEntries.pop_back();
                release_slot(slotIndex);
            }

            /// <summary>
            /// Marks a slot free (or retired when its generation wraps). The caller has already
            /// removed the element from the dense storage.
            /// </summary>
            void release_slot(u32 slotIndex) noexcept {
                slot_entry &slot = mSlots[slotIndex];
                slot.denseIndex = INVALID_INDEX;

                const generation_type next = static_cast<generation_type>(
                  static_cast<generation_type>(slot.generation) + static_cast<generation_type>(1)
                );
                if (next == 0) {
                    slot.generation = 0;
                    slot.nextFree = INVALID_INDEX;
                    ++mRetiredCount;
                    return;
                }

                slot.generation = static_cast<u32>(next);
                slot.nextFree = mFreeHead;
                mFreeHead = slotIndex;
                ++mFreeCount;
            }
    };
}

template <typename Key, typename T>
struct std::tuple_size<conjure::core::slot_map_entry<Key, T>> : std::integral_constant<std::size_t, 2> {};

template <typename Key, typename T>
struct std::tuple_element<0, conjure::core::slot_map_entry<Key, T>> {
    using type = const Key;
};

template <typename Key, typename T>
struct std::tuple_element<1, conjure::core::slot_map_entry<Key, T>> {
    using type = T;
};

template <>
struct std::hash<conjure::core::slot_key> {
    [[nodiscard]] std::size_t operator()(const conjure::core::slot_key &key) const noexcept {
        // u64 and size_t are the same width on every supported (64-bit only) target.
        return key.to_u64();
    }
};
