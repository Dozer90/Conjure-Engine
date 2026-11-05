#pragma once

#include "conjure/core/containers/detail.h"

#include "conjure/core/assert.h"
#include "conjure/core/types.h"

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// flat_map: a sorted vector of key/value pairs with binary search.
//
//   flat_map<string_id, module *> modules;
//   modules.insert_or_assign(id, pointer);
//   if (auto found = modules.find(id); found != modules.end()) { ... }
//   for (const auto &[key, value] : modules) { ... }          // ascending key order
//
// Lookup is O(log n), insertion and erase O(n). It beats hash maps for small maps and whenever a
// deterministic iteration order matters (serialisation, module load order). Elements are
// std::pair<K, V>: do not modify a key through an iterator. With a transparent comparator
// (std::less<>) every lookup (find, contains, count, lower_bound, upper_bound, at, erase(key))
// also accepts any type the comparator can order against K, e.g. std::string_view for std::string.

namespace conjure::core {

    template <typename K, typename V, typename Compare = std::less<K>>
    class flat_map {
        public:
            using key_type = K;
            using mapped_type = V;
            using value_type = std::pair<K, V>;
            using key_compare = Compare;
            using size_type = usize;
            using difference_type = isize;
            using reference = value_type &;
            using const_reference = const value_type &;
            using storage_type = std::vector<value_type>;
            using iterator = typename storage_type::iterator;
            using const_iterator = typename storage_type::const_iterator;
            using reverse_iterator = typename storage_type::reverse_iterator;
            using const_reverse_iterator = typename storage_type::const_reverse_iterator;

        private:
            storage_type mEntries = {};   // sorted by key, unique keys
            Compare mCompare = {};

        public:
            flat_map() = default;
            ~flat_map() = default;

            explicit flat_map(Compare compare) :
                mCompare(std::move(compare)) {}

            /// <summary>
            /// Builds the map from pairs; for duplicate keys the first one wins.
            /// </summary>
            flat_map(std::initializer_list<value_type> values) {
                reserve(values.size());
                for (const value_type &value : values) {
                    insert(value);
                }
            }

            flat_map(const flat_map &) = default;
            flat_map &operator=(const flat_map &) = default;
            flat_map(flat_map &&) noexcept = default;
            flat_map &operator=(flat_map &&) noexcept = default;

        public:
            /// <summary>
            /// The element with key 'key', or end().
            /// </summary>
            [[nodiscard]] iterator find(const K &key) {
                return find_in(mEntries, key);
            }

            [[nodiscard]] const_iterator find(const K &key) const {
                return find_in(mEntries, key);
            }

            /// <summary>
            /// Heterogeneous lookup, available when Compare declares is_transparent (e.g. finding
            /// by std::string_view in a std::string keyed map).
            /// </summary>
            template <typename KeyLike>
                requires detail::transparent_compare<Compare>
            [[nodiscard]] iterator find(const KeyLike &key) {
                return find_in(mEntries, key);
            }

            template <typename KeyLike>
                requires detail::transparent_compare<Compare>
            [[nodiscard]] const_iterator find(const KeyLike &key) const {
                return find_in(mEntries, key);
            }

            [[nodiscard]] bool contains(const K &key) const {
                return find(key) != mEntries.end();
            }

            template <typename KeyLike>
                requires detail::transparent_compare<Compare>
            [[nodiscard]] bool contains(const KeyLike &key) const {
                return find(key) != mEntries.end();
            }

            /// <summary>
            /// 0 or 1.
            /// </summary>
            [[nodiscard]] usize count(const K &key) const {
                return contains(key) ? 1 : 0;
            }

            template <typename KeyLike>
                requires detail::transparent_compare<Compare>
            [[nodiscard]] usize count(const KeyLike &key) const {
                return contains(key) ? 1 : 0;
            }

            /// <summary>
            /// First element whose key is not less than 'key'.
            /// </summary>
            [[nodiscard]] iterator lower_bound(const K &key) {
                return lower_bound_in(mEntries, key);
            }

            [[nodiscard]] const_iterator lower_bound(const K &key) const {
                return lower_bound_in(mEntries, key);
            }

            template <typename KeyLike>
                requires detail::transparent_compare<Compare>
            [[nodiscard]] iterator lower_bound(const KeyLike &key) {
                return lower_bound_in(mEntries, key);
            }

            template <typename KeyLike>
                requires detail::transparent_compare<Compare>
            [[nodiscard]] const_iterator lower_bound(const KeyLike &key) const {
                return lower_bound_in(mEntries, key);
            }

            /// <summary>
            /// First element whose key is greater than 'key'.
            /// </summary>
            [[nodiscard]] iterator upper_bound(const K &key) {
                return upper_bound_in(mEntries, key);
            }

            [[nodiscard]] const_iterator upper_bound(const K &key) const {
                return upper_bound_in(mEntries, key);
            }

            template <typename KeyLike>
                requires detail::transparent_compare<Compare>
            [[nodiscard]] iterator upper_bound(const KeyLike &key) {
                return upper_bound_in(mEntries, key);
            }

            template <typename KeyLike>
                requires detail::transparent_compare<Compare>
            [[nodiscard]] const_iterator upper_bound(const KeyLike &key) const {
                return upper_bound_in(mEntries, key);
            }

            /// <summary>
            /// Inserts when the key is absent. Returns the element and whether it was inserted.
            /// </summary>
            std::pair<iterator, bool> insert(const value_type &value) {
                iterator position = lower_bound(value.first);
                if (position != mEntries.end() && !mCompare(value.first, position->first)) {
                    return {position, false};
                }
                return {mEntries.insert(position, value), true};
            }

            std::pair<iterator, bool> insert(value_type &&value) {
                iterator position = lower_bound(value.first);
                if (position != mEntries.end() && !mCompare(value.first, position->first)) {
                    return {position, false};
                }
                return {mEntries.insert(position, std::move(value)), true};
            }

            /// <summary>
            /// Inserts every element of the range that is not already present.
            /// </summary>
            template <std::input_iterator Iterator>
            void insert(Iterator first, Iterator last) {
                for (Iterator source = first; source != last; ++source) {
                    insert(*source);
                }
            }

            /// <summary>
            /// Constructs a pair from the arguments and inserts it when its key is absent.
            /// </summary>
            template <typename... Args>
            std::pair<iterator, bool> emplace(Args &&...args) {
                return insert(value_type(std::forward<Args>(args)...));
            }

            /// <summary>
            /// Inserts {key, V(args...)} when 'key' is absent; the value is only constructed in
            /// that case. Returns the element and whether it was inserted.
            /// </summary>
            template <typename... Args>
            std::pair<iterator, bool> try_emplace(const K &key, Args &&...args) {
                iterator position = lower_bound(key);
                if (position != mEntries.end() && !mCompare(key, position->first)) {
                    return {position, false};
                }
                position = mEntries.emplace(
                  position,
                  std::piecewise_construct,
                  std::forward_as_tuple(key),
                  std::forward_as_tuple(std::forward<Args>(args)...)
                );
                return {position, true};
            }

            template <typename... Args>
            std::pair<iterator, bool> try_emplace(K &&key, Args &&...args) {
                iterator position = lower_bound(key);
                if (position != mEntries.end() && !mCompare(key, position->first)) {
                    return {position, false};
                }
                position = mEntries.emplace(
                  position,
                  std::piecewise_construct,
                  std::forward_as_tuple(std::move(key)),
                  std::forward_as_tuple(std::forward<Args>(args)...)
                );
                return {position, true};
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
            [[nodiscard]] V &at(const K &key) {
                iterator found = find(key);
                if (found == mEntries.end()) [[unlikely]] {
                    detail::fatal_container_access("flat_map::at: key not found");
                }
                return found->second;
            }

            [[nodiscard]] const V &at(const K &key) const {
                const_iterator found = find(key);
                if (found == mEntries.end()) [[unlikely]] {
                    detail::fatal_container_access("flat_map::at: key not found");
                }
                return found->second;
            }

            template <typename KeyLike>
                requires detail::transparent_compare<Compare>
            [[nodiscard]] V &at(const KeyLike &key) {
                iterator found = find(key);
                if (found == mEntries.end()) [[unlikely]] {
                    detail::fatal_container_access("flat_map::at: key not found");
                }
                return found->second;
            }

            template <typename KeyLike>
                requires detail::transparent_compare<Compare>
            [[nodiscard]] const V &at(const KeyLike &key) const {
                const_iterator found = find(key);
                if (found == mEntries.end()) [[unlikely]] {
                    detail::fatal_container_access("flat_map::at: key not found");
                }
                return found->second;
            }

            /// <summary>
            /// Removes the element with 'key'. Returns false when absent.
            /// </summary>
            bool erase(const K &key) {
                iterator found = find(key);
                if (found == mEntries.end()) {
                    return false;
                }
                mEntries.erase(found);
                return true;
            }

            template <typename KeyLike>
                requires detail::transparent_compare<Compare>
            bool erase(const KeyLike &key) {
                iterator found = find(key);
                if (found == mEntries.end()) {
                    return false;
                }
                mEntries.erase(found);
                return true;
            }

            /// <summary>
            /// Removes the element at 'position'. Returns the iterator following it.
            /// </summary>
            iterator erase(const_iterator position) {
                return mEntries.erase(position);
            }

            iterator erase(const_iterator first, const_iterator last) {
                return mEntries.erase(first, last);
            }

            void reserve(usize count) {
                mEntries.reserve(count);
            }

            void clear() noexcept {
                mEntries.clear();
            }

            void shrink_to_fit() {
                mEntries.shrink_to_fit();
            }

            void swap(flat_map &other) noexcept {
                mEntries.swap(other.mEntries);
                std::swap(mCompare, other.mCompare);
            }

        public:
            [[nodiscard]] usize size() const noexcept {
                return mEntries.size();
            }

            [[nodiscard]] bool empty() const noexcept {
                return mEntries.empty();
            }

            [[nodiscard]] usize capacity() const noexcept {
                return mEntries.capacity();
            }

            [[nodiscard]] key_compare key_comp() const {
                return mCompare;
            }

            /// <summary>
            /// The sorted pairs as a contiguous read-only view.
            /// </summary>
            [[nodiscard]] std::span<const value_type> entries() const noexcept {
                return std::span<const value_type>(mEntries.data(), mEntries.size());
            }

            [[nodiscard]] iterator begin() noexcept {
                return mEntries.begin();
            }

            [[nodiscard]] const_iterator begin() const noexcept {
                return mEntries.begin();
            }

            [[nodiscard]] const_iterator cbegin() const noexcept {
                return mEntries.cbegin();
            }

            [[nodiscard]] iterator end() noexcept {
                return mEntries.end();
            }

            [[nodiscard]] const_iterator end() const noexcept {
                return mEntries.end();
            }

            [[nodiscard]] const_iterator cend() const noexcept {
                return mEntries.cend();
            }

            [[nodiscard]] reverse_iterator rbegin() noexcept {
                return mEntries.rbegin();
            }

            [[nodiscard]] const_reverse_iterator rbegin() const noexcept {
                return mEntries.rbegin();
            }

            [[nodiscard]] reverse_iterator rend() noexcept {
                return mEntries.rend();
            }

            [[nodiscard]] const_reverse_iterator rend() const noexcept {
                return mEntries.rend();
            }

            [[nodiscard]] bool operator==(const flat_map &other) const {
                return mEntries == other.mEntries;
            }

        private:
            template <typename Storage, typename KeyLike>
            [[nodiscard]] auto lower_bound_in(Storage &storage, const KeyLike &key) const {
                const auto entryBeforeKey = [this](const value_type &left, const KeyLike &right) {
                    return mCompare(left.first, right);
                };
                return std::lower_bound(storage.begin(), storage.end(), key, entryBeforeKey);
            }

            template <typename Storage, typename KeyLike>
            [[nodiscard]] auto upper_bound_in(Storage &storage, const KeyLike &key) const {
                const auto keyBeforeEntry = [this](const KeyLike &left, const value_type &right) {
                    return mCompare(left, right.first);
                };
                return std::upper_bound(storage.begin(), storage.end(), key, keyBeforeEntry);
            }

            template <typename Storage, typename KeyLike>
            [[nodiscard]] auto find_in(Storage &storage, const KeyLike &key) const {
                auto position = lower_bound_in(storage, key);
                if (position != storage.end() && !mCompare(key, position->first)) {
                    return position;
                }
                return storage.end();
            }
    };

    template <typename K, typename V, typename Compare>
    void swap(flat_map<K, V, Compare> &left, flat_map<K, V, Compare> &right) noexcept {
        left.swap(right);
    }
}
