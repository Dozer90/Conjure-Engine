#pragma once

#include "conjure/core/api.h"
#include "conjure/core/error.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"

#include <concepts>
#include <cstddef>
#include <format>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// JSON values.
//
// A value is a non-owning view into a document. Reading never asserts: indexing a missing key, an
// out-of-range element or a non-container yields an *invalid* value whose accessors return their
// fallback, so chains such as `doc.root()["window"]["width"].as_i64(1280)` are safe. A view stays
// valid until the owning document is mutated, moved or destroyed.
//
// Typed conversion goes through `value::to<T>()`, which forwards to the customisation point
// `json_converter<T>`. This header specialises it for bool, the fixed-width integers, f32/f64,
// std::string, uuid and std::vector<T>; other areas (reflect) add their own types.

namespace conjure::core::json {

    class document;
    class value;
    struct member;

    /// <summary>
    /// The six JSON value kinds. Invalid values report NULL_VALUE.
    /// </summary>
    enum class json_type : u8 {
        NULL_VALUE,
        BOOL,
        NUMBER,
        STRING,
        ARRAY,
        OBJECT
    };

    /// <summary>
    /// Returns the lower-case name of a type ("object"), used in error messages.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(json_type type) noexcept;

    /// <summary>
    /// Customisation point for value::to&lt;T&gt;(). Specialise it for a type and provide
    /// `static result&lt;T&gt; from_json(const value &amp;source)`. Conversion errors use the
    /// generic core codes: NOT_FOUND for an invalid (missing) value, INVALID_ARGUMENT for a type
    /// mismatch, OUT_OF_RANGE for a number outside the target range and PARSE_FAILURE for a
    /// string that does not parse as the target.
    /// </summary>
    template <typename T>
    struct json_converter;

    /// <summary>
    /// True when json_converter is specialised for T.
    /// </summary>
    template <typename T>
    concept json_convertible = requires(const value &source) {
        { json_converter<T>::from_json(source) } -> std::same_as<result<T>>;
    };

    namespace detail {
        struct value_access;
    }

    /// <summary>
    /// A non-owning view of one JSON value inside a document.
    /// </summary>
    class CONJURE_API value {
        private:
            void *mNode = nullptr;              // yyjson node; opaque so yyjson never leaks into public headers
            const document *mOwner = nullptr;   // informational: the document the node belongs to

            friend struct detail::value_access;

            constexpr value(void *node, const document *owner) noexcept :
                mNode(node),
                mOwner(owner) {}

        public:
            /// <summary>
            /// Forward iterator over the elements of an array. Dereferencing yields a value view.
            /// </summary>
            class CONJURE_API array_iterator {
                private:
                    void *mNode = nullptr;
                    usize mRemaining = 0;
                    const document *mOwner = nullptr;

                    friend class value;

                    constexpr array_iterator(void *node, usize remaining, const document *owner) noexcept :
                        mNode(node),
                        mRemaining(remaining),
                        mOwner(owner) {}

                public:
                    using iterator_category = std::forward_iterator_tag;
                    using value_type = value;
                    using difference_type = isize;
                    using reference = value;
                    using pointer = void;

                    constexpr array_iterator() noexcept = default;

                public:
                    [[nodiscard]] value operator*() const noexcept;
                    array_iterator &operator++() noexcept;
                    array_iterator operator++(int) noexcept;

                    [[nodiscard]] bool operator==(const array_iterator &other) const noexcept {
                        return mRemaining == other.mRemaining && (mRemaining == 0 || mNode == other.mNode);
                    }
            };

            /// <summary>
            /// Forward iterator over the members of an object. Dereferencing yields a member
            /// (key view plus value view).
            /// </summary>
            class CONJURE_API object_iterator {
                private:
                    void *mKey = nullptr;
                    usize mRemaining = 0;
                    const document *mOwner = nullptr;

                    friend class value;

                    constexpr object_iterator(void *key, usize remaining, const document *owner) noexcept :
                        mKey(key),
                        mRemaining(remaining),
                        mOwner(owner) {}

                public:
                    using iterator_category = std::forward_iterator_tag;
                    using value_type = member;
                    using difference_type = isize;
                    using reference = member;
                    using pointer = void;

                    constexpr object_iterator() noexcept = default;

                public:
                    [[nodiscard]] member operator*() const noexcept;
                    object_iterator &operator++() noexcept;
                    object_iterator operator++(int) noexcept;

                    [[nodiscard]] bool operator==(const object_iterator &other) const noexcept {
                        return mRemaining == other.mRemaining && (mRemaining == 0 || mKey == other.mKey);
                    }
            };

            /// <summary>
            /// Range returned by members(): `for (auto [key, item] : object.members())`.
            /// </summary>
            class object_range {
                private:
                    object_iterator mBegin = {};

                    friend class value;

                    constexpr explicit object_range(object_iterator first) noexcept :
                        mBegin(first) {}

                public:
                    constexpr object_range() noexcept = default;

                public:
                    [[nodiscard]] object_iterator begin() const noexcept {
                        return mBegin;
                    }

                    [[nodiscard]] object_iterator end() const noexcept {
                        return object_iterator();
                    }
            };

        public:
            /// <summary>
            /// An invalid value: not part of any document, every accessor returns its fallback.
            /// </summary>
            constexpr value() noexcept = default;

        public:
            /// <summary>
            /// False for a default-constructed value or the result of a failed lookup.
            /// </summary>
            [[nodiscard]] bool is_valid() const noexcept {
                return mNode != nullptr;
            }

            /// <summary>
            /// The document this view points into, or nullptr when invalid. Informational only;
            /// stale once the document has been moved.
            /// </summary>
            [[nodiscard]] const document *owner() const noexcept {
                return mOwner;
            }

            /// <summary>
            /// The kind of this value. Invalid values report NULL_VALUE.
            /// </summary>
            [[nodiscard]] json_type type() const noexcept;

            /// <summary>
            /// True for a JSON null and for an invalid value. Use is_valid() to tell them apart.
            /// </summary>
            [[nodiscard]] bool is_null() const noexcept;
            [[nodiscard]] bool is_bool() const noexcept;
            [[nodiscard]] bool is_number() const noexcept;
            [[nodiscard]] bool is_string() const noexcept;
            [[nodiscard]] bool is_array() const noexcept;
            [[nodiscard]] bool is_object() const noexcept;

            /// <summary>
            /// True for a number written without fraction or exponent (fits i64 or u64). `3.0`
            /// is a number but not an integer.
            /// </summary>
            [[nodiscard]] bool is_integer() const noexcept;

            /// <summary>
            /// The boolean, or 'fallback' when this is not a bool.
            /// </summary>
            [[nodiscard]] bool as_bool(bool fallback = false) const noexcept;

            /// <summary>
            /// The number as i64. Integers outside the i64 range and non-integral reals yield
            /// 'fallback'; integral reals convert.
            /// </summary>
            [[nodiscard]] i64 as_i64(i64 fallback = 0) const noexcept;

            /// <summary>
            /// The number as u64. Negative values and non-integral reals yield 'fallback'.
            /// </summary>
            [[nodiscard]] u64 as_u64(u64 fallback = 0) const noexcept;

            /// <summary>
            /// Any number as f64 (large integers round), or 'fallback' when this is not a number.
            /// </summary>
            [[nodiscard]] f64 as_f64(f64 fallback = 0.0) const noexcept;

            /// <summary>
            /// Any number as f32 (rounded), or 'fallback' when this is not a number or its
            /// magnitude exceeds the f32 range (the cast would be undefined behaviour).
            /// </summary>
            [[nodiscard]] f32 as_f32(f32 fallback = 0.0f) const noexcept;

            /// <summary>
            /// The string bytes (UTF-8, may contain embedded zeros), or 'fallback' when this is
            /// not a string. The view lives as long as the document.
            /// </summary>
            [[nodiscard]] std::string_view as_string(std::string_view fallback = {}) const noexcept;

            /// <summary>
            /// Number of elements (array) or members (object); 0 for anything else.
            /// </summary>
            [[nodiscard]] usize size() const noexcept;

            /// <summary>
            /// Element 'index' of an array, or an invalid value when out of range or not an array.
            /// </summary>
            [[nodiscard]] value operator[](usize index) const noexcept;

            /// <summary>
            /// The first member named 'key' of an object, or an invalid value when missing or
            /// not an object. Never asserts.
            /// </summary>
            [[nodiscard]] value operator[](std::string_view key) const noexcept;

            /// <summary>
            /// True when this is an object with a member named 'key'.
            /// </summary>
            [[nodiscard]] bool contains(std::string_view key) const noexcept;

            /// <summary>
            /// The member named 'key', or nullopt when missing or not an object.
            /// </summary>
            [[nodiscard]] std::optional<value> get(std::string_view key) const noexcept;

            /// <summary>
            /// Iteration over array elements: `for (value item : array)`. Empty for non-arrays.
            /// </summary>
            [[nodiscard]] array_iterator begin() const noexcept;
            [[nodiscard]] array_iterator end() const noexcept;

            /// <summary>
            /// Iteration over object members in document order. Empty for non-objects.
            /// </summary>
            [[nodiscard]] object_range members() const noexcept;

            /// <summary>
            /// Converts through json_converter&lt;T&gt;; see that template for the error codes.
            /// </summary>
            template <json_convertible T>
            [[nodiscard]] result<T> to() const {
                return json_converter<T>::from_json(*this);
            }

            /// <summary>
            /// Structural equality, defined as "same canonical text": objects compare regardless
            /// of member order (duplicate keys pair up in order); integers compare mathematically;
            /// reals compare bit for bit (so -0.0 differs from 0.0); an integer never equals a
            /// real. Two invalid values are equal; an invalid value never equals a valid one.
            /// Walks both trees with an explicit stack (one small allocation), so any nesting
            /// depth is safe.
            /// </summary>
            [[nodiscard]] bool operator==(const value &other) const;
    };

    /// <summary>
    /// One key/value pair produced by iterating an object.
    /// </summary>
    struct member {
        std::string_view key = {};
        value item = {};
    };

    namespace detail {
        /// <summary>
        /// Lets the document and the iterators build views; not for general use.
        /// </summary>
        struct value_access {
            [[nodiscard]] static value make(void *node, const document *owner) noexcept {
                return value(node, owner);
            }

            [[nodiscard]] static void *node(const value &source) noexcept {
                return source.mNode;
            }
        };
    }

    /*-----------------------------------------------------------------------*/
    // Built-in converters

    template <>
    struct CONJURE_API json_converter<bool> {
        [[nodiscard]] static result<bool> from_json(const value &source);
    };

    template <>
    struct CONJURE_API json_converter<i8> {
        [[nodiscard]] static result<i8> from_json(const value &source);
    };

    template <>
    struct CONJURE_API json_converter<i16> {
        [[nodiscard]] static result<i16> from_json(const value &source);
    };

    template <>
    struct CONJURE_API json_converter<i32> {
        [[nodiscard]] static result<i32> from_json(const value &source);
    };

    template <>
    struct CONJURE_API json_converter<i64> {
        [[nodiscard]] static result<i64> from_json(const value &source);
    };

    template <>
    struct CONJURE_API json_converter<u8> {
        [[nodiscard]] static result<u8> from_json(const value &source);
    };

    template <>
    struct CONJURE_API json_converter<u16> {
        [[nodiscard]] static result<u16> from_json(const value &source);
    };

    template <>
    struct CONJURE_API json_converter<u32> {
        [[nodiscard]] static result<u32> from_json(const value &source);
    };

    template <>
    struct CONJURE_API json_converter<u64> {
        [[nodiscard]] static result<u64> from_json(const value &source);
    };

    template <>
    struct CONJURE_API json_converter<f32> {
        [[nodiscard]] static result<f32> from_json(const value &source);
    };

    template <>
    struct CONJURE_API json_converter<f64> {
        [[nodiscard]] static result<f64> from_json(const value &source);
    };

    template <>
    struct CONJURE_API json_converter<std::string> {
        [[nodiscard]] static result<std::string> from_json(const value &source);
    };

    template <>
    struct CONJURE_API json_converter<uuid> {
        [[nodiscard]] static result<uuid> from_json(const value &source);
    };

    /// <summary>
    /// Converts a JSON array element by element. The first failing element stops the conversion
    /// and its error is returned with the element index prepended to the message.
    /// </summary>
    template <json_convertible T>
    struct json_converter<std::vector<T>> {
        [[nodiscard]] static result<std::vector<T>> from_json(const value &source) {
            if (!source.is_valid()) {
                return error(error_code::NOT_FOUND, "value is missing");
            }
            if (!source.is_array()) {
                const std::string_view found = to_string(source.type());
                return error(error_code::INVALID_ARGUMENT, std::format("expected array, found {}", found));
            }

            std::vector<T> items = {};
            items.reserve(source.size());

            usize index = 0;
            for (const value item : source) {
                result<T> converted = json_converter<T>::from_json(item);
                if (!converted.has_value()) {
                    const error &failure = converted.get_error();
                    return error(failure.generic_code(), std::format("element {}: {}", index, failure.message()));
                }
                items.push_back(std::move(converted).value());
                ++index;
            }
            return items;
        }
    };
}

template <>
struct std::formatter<conjure::core::json::json_type> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(conjure::core::json::json_type type, FormatContext &context) const {
        return std::formatter<std::string_view>::format(conjure::core::json::to_string(type), context);
    }
};
