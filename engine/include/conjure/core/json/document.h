#pragma once

#include "conjure/core/json/value.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <concepts>
#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

// JSON documents.
//
// A document owns a tree of JSON values and is the only way to create or change them. Values are
// read through `root()` and the `value` views it hands out; every mutation goes through the
// document so lifetimes stay simple: a view is valid until the document is mutated, moved or
// destroyed. Copying a document deep-copies the tree; moving is a pointer swap.
//
// Numbers: non-negative integers are stored unsigned, negative ones signed, reals as double.
// Non-finite reals (inf, nan) are not representable in JSON; the mutation API asserts and stores
// null instead so a document can always be serialised.
//
// Integer arguments of any width are accepted: the fixed-width overloads take the common types
// directly and a constrained template widens every other integral type (long, size_t, i8, ...) to
// i64 or u64 so calls stay unambiguous on every platform.
//
// Nesting: parse() rejects text nested deeper than MAX_NESTING_DEPTH because deep copies (copy
// constructor, from_value, set/push_back/set_pointer of a value) and merge_patch recurse once per
// level inside yyjson. Serialisation, the canonical form and equality walk with explicit stacks
// and handle any depth. The mutation API does not enforce the limit: a tree built deeper than
// that through set()/push_back() still serialises and compares, but copying or merging it is not
// protected against exhausting the stack.

namespace conjure::core::json {

    class document;
    class init_value;
    struct init_member;

    // The builders are friends of document and init_value. Every declaration of them, the friend
    // ones included, carries CONJURE_API: MSVC rejects a redeclaration that drops the attribute
    // (C4273, inconsistent dll linkage).
    [[nodiscard]] CONJURE_API document object(std::initializer_list<init_member> members);
    [[nodiscard]] CONJURE_API document array(std::initializer_list<init_value> items);

    /// <summary>
    /// Deepest container nesting a parsed document may have: "[]" is depth 1, "[[]]" depth 2, a
    /// scalar root depth 0. Sized so yyjson's recursive copy fits a 512 KiB thread stack in Debug.
    /// </summary>
    inline constexpr usize MAX_NESTING_DEPTH = 1024;

    namespace detail {
        /// <summary>
        /// i64 for signed integral types, u64 for unsigned ones; the storage width of the API.
        /// </summary>
        template <std::integral T>
        using widened_integer = std::conditional_t<std::is_signed_v<T>, i64, u64>;

        /// <summary>
        /// Integral types that are not bool and have no dedicated overload of their own.
        /// </summary>
        template <typename T>
        concept other_integer = std::integral<T> && !std::same_as<T, bool> && !std::same_as<T, i32> &&
          !std::same_as<T, u32> && !std::same_as<T, i64> && !std::same_as<T, u64>;
    }

    /// <summary>
    /// An owning, mutable JSON document.
    /// </summary>
    class CONJURE_API document {
        private:
            void *mHandle = nullptr;    // yyjson mutable document; opaque so yyjson stays out of public headers

        public:
            /// <summary>
            /// A document whose root is JSON null. The first set() turns the root into an object,
            /// the first push_back() into an array.
            /// </summary>
            document();
            ~document();

            document(const document &other);
            document &operator=(const document &other);

            /// <summary>
            /// Moving transfers the tree without copying. The moved-from document is empty: its
            /// root() is invalid, it serialises to "", every mutation is rejected (asserts or
            /// returns false) and it equals nothing; only assignment and destruction are meant for
            /// it. Copying it yields another empty document.
            /// </summary>
            document(document &&other) noexcept;
            document &operator=(document &&other) noexcept;

        public:
            /// <summary>
            /// A document whose root is an empty object.
            /// </summary>
            [[nodiscard]] static document object();

            /// <summary>
            /// A document whose root is an empty array.
            /// </summary>
            [[nodiscard]] static document array();

            /// <summary>
            /// A document whose root is a deep copy of 'source' (any node of any document). An
            /// invalid source yields a null root.
            /// </summary>
            [[nodiscard]] static document from_value(const value &source);

            /// <summary>
            /// Parses strict RFC 8259 JSON (no comments, trailing commas, inf/nan or BOM). Fails
            /// with PARSE_FAILURE; the message carries the 1-based line and column, or names the
            /// limit when containers nest deeper than MAX_NESTING_DEPTH.
            /// </summary>
            [[nodiscard]] static result<document> parse(std::string_view text);

            /// <summary>
            /// Reads a file and parses it. Fails with IO_FAILURE when the path is not a readable
            /// regular file and PARSE_FAILURE when its content is not JSON. A leading UTF-8 byte order
            /// mark is skipped (as file_system::read_text_file does); parse() itself rejects one.
            /// </summary>
            [[nodiscard]] static result<document> parse_file(const std::filesystem::path &path);

            /// <summary>
            /// A view of the root value. Valid for every document except a moved-from one.
            /// </summary>
            [[nodiscard]] value root() const noexcept;

            /*---------------------------------------------------------------*/
            // Object mutation. The root must be an object (or null, which becomes an object);
            // otherwise the call asserts and does nothing. An existing member is replaced in place
            // so member order is preserved; a new member is appended.

            /// <summary>
            /// Sets member 'key' to a scalar. Non-negative integers are stored unsigned, negative
            /// ones signed; non-finite reals assert and store null; a null C string stores null.
            /// </summary>
            void set(std::string_view key, bool item);
            void set(std::string_view key, i32 item);
            void set(std::string_view key, u32 item);
            void set(std::string_view key, i64 item);
            void set(std::string_view key, u64 item);
            void set(std::string_view key, f32 item);
            void set(std::string_view key, f64 item);
            void set(std::string_view key, std::string_view item);
            void set(std::string_view key, const char *item);
            void set(std::string_view key, std::nullptr_t);

            template <detail::other_integer T>
            void set(std::string_view key, T item) {
                set(key, static_cast<detail::widened_integer<T>>(item));
            }

            /// <summary>
            /// Deep-copies another document's tree under 'key'.
            /// </summary>
            void set(std::string_view key, const document &item);

            /// <summary>
            /// Deep-copies a value (from this or any other document) under 'key'.
            /// </summary>
            void set(std::string_view key, const value &item);

            /// <summary>
            /// Sets 'key' to JSON null (the member stays; use erase() to remove it).
            /// </summary>
            void set_null(std::string_view key);

            /// <summary>
            /// Removes every member named 'key'. Returns false when there was none.
            /// </summary>
            bool erase(std::string_view key);

            /*---------------------------------------------------------------*/
            // Array mutation. The root must be an array (or null, which becomes an array);
            // otherwise the call asserts and does nothing.

            /// <summary>
            /// Appends a scalar element. Non-negative integers are stored unsigned, negative ones
            /// signed; non-finite reals assert and store null; a null C string stores null.
            /// </summary>
            void push_back(bool item);
            void push_back(i32 item);
            void push_back(u32 item);
            void push_back(i64 item);
            void push_back(u64 item);
            void push_back(f32 item);
            void push_back(f64 item);
            void push_back(std::string_view item);
            void push_back(const char *item);
            void push_back(std::nullptr_t);

            template <detail::other_integer T>
            void push_back(T item) {
                push_back(static_cast<detail::widened_integer<T>>(item));
            }

            /// <summary>
            /// Appends a deep copy of another document's tree or of a value from any document.
            /// </summary>
            void push_back(const document &item);
            void push_back(const value &item);

            /// <summary>
            /// Removes the element at 'index'. Returns false when out of range.
            /// </summary>
            bool erase(usize index);

            /// <summary>
            /// Removes every member or element of a container root; any other root becomes null.
            /// </summary>
            void clear();

            /*---------------------------------------------------------------*/
            // JSON Pointer (RFC 6901) access.

            /// <summary>
            /// The value at a JSON Pointer such as "/a/b/0" ("" is the root), or an invalid value
            /// when the pointer is malformed or does not resolve.
            /// </summary>
            [[nodiscard]] value at_pointer(std::string_view pointer) const noexcept;

            /// <summary>
            /// Sets the value at a JSON Pointer, creating missing intermediate objects. In an
            /// array the last token replaces an existing index, while "-" or an index equal to the
            /// current size appends (RFC 6902 "add" semantics). Returns false for a malformed
            /// pointer or when an existing parent is a scalar or an array index is past the end.
            /// </summary>
            bool set_pointer(std::string_view pointer, bool item);
            bool set_pointer(std::string_view pointer, i32 item);
            bool set_pointer(std::string_view pointer, u32 item);
            bool set_pointer(std::string_view pointer, i64 item);
            bool set_pointer(std::string_view pointer, u64 item);
            bool set_pointer(std::string_view pointer, f32 item);
            bool set_pointer(std::string_view pointer, f64 item);
            bool set_pointer(std::string_view pointer, std::string_view item);
            bool set_pointer(std::string_view pointer, const char *item);
            bool set_pointer(std::string_view pointer, std::nullptr_t);
            bool set_pointer(std::string_view pointer, const document &item);
            bool set_pointer(std::string_view pointer, const value &item);

            template <detail::other_integer T>
            bool set_pointer(std::string_view pointer, T item) {
                return set_pointer(pointer, static_cast<detail::widened_integer<T>>(item));
            }

            /*---------------------------------------------------------------*/
            // Whole-document operations.

            /// <summary>
            /// Applies an RFC 7386 merge patch: object members are merged recursively, a null
            /// member removes the target member, anything else replaces the target. Returns false
            /// only when memory runs out; the document is then unchanged.
            /// </summary>
            bool merge_patch(const value &patch);
            bool merge_patch(const document &patch);

            /// <summary>
            /// Serialises in member order: minified, or pretty with 4-space indentation. Returns an
            /// empty string (and logs a warning) only when a string holds invalid UTF-8.
            /// </summary>
            [[nodiscard]] std::string to_string(bool pretty = false) const;

            /// <summary>
            /// Canonical form for hashing and diffing: members sorted by key (byte order),
            /// minified, shortest round-trip reals, integers as plain digits. Identical on every
            /// platform for the same content, and identical to what `writer` produces when the
            /// same data is written with keys in sorted order. Returns an empty string (and logs a
            /// warning) only when a string holds invalid UTF-8.
            /// </summary>
            [[nodiscard]] std::string to_canonical_string() const;

            /// <summary>
            /// Writes to_string(pretty) to a file, creating parent directories. IO_FAILURE when the
            /// file cannot be written.
            /// </summary>
            [[nodiscard]] result<void> write_file(const std::filesystem::path &path, bool pretty = false) const;

            /// <summary>
            /// Structural equality of the two roots (see value::operator==).
            /// </summary>
            [[nodiscard]] bool operator==(const document &other) const;

        private:
            friend class init_value;
            friend CONJURE_API document object(std::initializer_list<init_member> members);
            friend CONJURE_API document array(std::initializer_list<init_value> items);

            explicit document(void *handle) noexcept;

            [[nodiscard]] void *root_node() const noexcept;
            [[nodiscard]] void *object_root();
            [[nodiscard]] void *array_root();
            void set_node(std::string_view key, void *node);
            void push_node(void *node);
            bool set_pointer_node(std::string_view pointer, void *node);
    };

    /// <summary>
    /// One argument of the object()/array() builders. Implicitly constructed from every scalar
    /// type the mutation API accepts, from a document (copied or moved) and from a value (copied).
    /// A null C string becomes JSON null, as in the mutation API. Intended only as a builder
    /// argument; do not store it.
    /// </summary>
    class CONJURE_API init_value {
        private:
            using storage = std::variant<std::nullptr_t, bool, i64, u64, f64, std::string, document, value>;

            // No default initialiser: every constructor sets exactly one alternative.
            storage mStorage;

        public:
            init_value(std::nullptr_t) noexcept;
            init_value(bool item) noexcept;
            init_value(i32 item) noexcept;
            init_value(u32 item) noexcept;
            init_value(i64 item) noexcept;
            init_value(u64 item) noexcept;
            init_value(f32 item) noexcept;
            init_value(f64 item) noexcept;
            init_value(const char *item);
            init_value(std::string_view item);
            init_value(const std::string &item);
            init_value(std::string &&item) noexcept;
            init_value(const document &item);
            init_value(document &&item) noexcept;
            init_value(const value &item) noexcept;
            ~init_value();

            template <detail::other_integer T>
            init_value(T item) noexcept :
                init_value(static_cast<detail::widened_integer<T>>(item)) {}

            init_value(const init_value &) = default;
            init_value &operator=(const init_value &) = default;
            init_value(init_value &&) noexcept = default;
            init_value &operator=(init_value &&) noexcept = default;

        public:
            /// <summary>
            /// Builds a one-value document from the stored alternative.
            /// </summary>
            [[nodiscard]] document to_document() const;

        private:
            friend CONJURE_API document object(std::initializer_list<init_member> members);
            friend CONJURE_API document array(std::initializer_list<init_value> items);

            // Creates the node straight inside 'target' (an opaque yyjson document) so the builders
            // allocate once per element; only document and value alternatives are deep-copied.
            [[nodiscard]] void *make_node(void *target) const;
    };

    /// <summary>
    /// One `{"key", value}` entry of the object() builder.
    /// </summary>
    struct init_member {
        std::string_view key = {};
        init_value item = nullptr;
    };

    /// <summary>
    /// Builds an object document: `json::object({{"a", 1}, {"b", "x"}, {"c", json::array({1, 2})}})`.
    /// </summary>
    [[nodiscard]] CONJURE_API document object(std::initializer_list<init_member> members);

    /// <summary>
    /// Builds an array document: `json::array({1, 2.5, "three", nullptr})`.
    /// </summary>
    [[nodiscard]] CONJURE_API document array(std::initializer_list<init_value> items);
}
