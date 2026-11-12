#pragma once

#include "conjure/core/api.h"
#include "conjure/core/hash.h"
#include "conjure/core/types.h"

#include <concepts>
#include <format>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

// Errors.
//
// An error is a category (which module), a code (that module's enum value) and a message. Core
// owns the generic category (error_code). Every other module declares its own enum and binds it
// to a category once:
//
//   enum class ecs_error : u16 { ENTITY_NOT_ALIVE = 1, STRUCTURE_LOCKED, ... };
//   CONJURE_DECLARE_ERROR_CATEGORY(ecs_error, "ecs", ecs_error_name);   // in a public header
//
// where `std::string_view ecs_error_name(ecs_error) noexcept` returns the enumerator name. Then:
//
//   return core::make_error(ecs_error::ENTITY_NOT_ALIVE, "entity {} was destroyed", id);
//   if (failure.is(ecs_error::ENTITY_NOT_ALIVE)) { ... }

namespace conjure::core {

    /// <summary>
    /// Generic failures that any module may return. Module-specific conditions get their own enum.
    /// </summary>
    enum class error_code : u16 {
        NONE = 0,
        UNKNOWN,
        INVALID_ARGUMENT,
        INVALID_STATE,
        NOT_FOUND,
        ALREADY_EXISTS,
        OUT_OF_RANGE,
        OUT_OF_MEMORY,
        UNSUPPORTED,
        NOT_IMPLEMENTED,
        IO_FAILURE,
        PARSE_FAILURE,
        TIMEOUT,
        PERMISSION_DENIED,
        BACKEND_FAILURE,
        DEPENDENCY_FAILURE,
        CANCELLED,
        COUNT
    };

    /// <summary>
    /// Returns the stable, upper-case name of a generic error code (e.g. "NOT_FOUND").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(error_code code) noexcept;

    /// <summary>
    /// Function that names a code of one category. Returns "" for unknown values.
    /// </summary>
    using error_code_name_fn = std::string_view (*)(u32 code) noexcept;

    /// <summary>
    /// Binds an error enumeration to a category. Specialised through CONJURE_DECLARE_ERROR_CATEGORY.
    /// </summary>
    template <typename EnumType>
    struct error_category_traits;

    template <typename EnumType>
    concept error_enum = std::is_enum_v<EnumType> && requires {
        { error_category_traits<EnumType>::NAME } -> std::convertible_to<std::string_view>;
        { error_category_traits<EnumType>::ID } -> std::convertible_to<u32>;
        { error_category_traits<EnumType>::code_name(u32{}) } -> std::convertible_to<std::string_view>;
    };

    /// <summary>
    /// A recoverable failure: category, code and a human readable message. Engine code returns
    /// these through result<T> instead of throwing. A default-constructed error means "no error".
    /// </summary>
    class CONJURE_API error {
        private:
            u32 mCategory = 0;
            u32 mCode = 0;
            const char *mCategoryName = "";
            error_code_name_fn mCodeName = nullptr;
            std::string mMessage = {};

        public:
            error();
            explicit error(error_code code);
            error(error_code code, std::string message);
            error(u32 category, u32 code, const char *categoryName, error_code_name_fn codeName, std::string message);
            ~error();

            error(const error &) = default;
            error &operator=(const error &) = default;
            error(error &&) noexcept = default;
            error &operator=(error &&) noexcept = default;

        public:
            /// <summary>
            /// Builds an error from a module error enumerator.
            /// </summary>
            template <error_enum EnumType>
            [[nodiscard]] static error from(EnumType code, std::string message = {}) {
                using traits = error_category_traits<EnumType>;
                return error(
                  traits::ID,
                  static_cast<u32>(code),
                  traits::NAME.data(),
                  &traits::code_name,
                  std::move(message)
                );
            }

            /// <summary>
            /// FNV-1a 32 hash of the category name. A default-constructed error carries the core
            /// category (error_category_traits<error_code>::ID) with code NONE; test for "no error"
            /// with is(error_code::NONE), not with category() == 0.
            /// </summary>
            [[nodiscard]] u32 category() const noexcept {
                return mCategory;
            }

            /// <summary>
            /// The category's registered name ("core", "ecs", ...).
            /// </summary>
            [[nodiscard]] std::string_view category_name() const noexcept {
                return mCategoryName;
            }

            /// <summary>
            /// The raw code within the category.
            /// </summary>
            [[nodiscard]] u32 code() const noexcept {
                return mCode;
            }

            /// <summary>
            /// The enumerator name of the code within its category, or "" when unknown.
            /// </summary>
            [[nodiscard]] std::string_view code_name() const noexcept;

            /// <summary>
            /// The generic code when this error belongs to the core category, otherwise NONE.
            /// </summary>
            [[nodiscard]] error_code generic_code() const noexcept;

            /// <summary>
            /// Human readable detail; may be empty.
            /// </summary>
            [[nodiscard]] const std::string &message() const noexcept {
                return mMessage;
            }

            /// <summary>
            /// True when this error carries exactly this code of this category.
            /// </summary>
            template <error_enum EnumType>
            [[nodiscard]] bool is(EnumType code) const noexcept {
                return mCategory == error_category_traits<EnumType>::ID && mCode == static_cast<u32>(code);
            }

            /// <summary>
            /// True when this error belongs to the category bound to the given enumeration.
            /// </summary>
            template <error_enum EnumType>
            [[nodiscard]] bool is_category() const noexcept {
                return mCategory == error_category_traits<EnumType>::ID;
            }

            /// <summary>
            /// "category.CODE: message", or "category.CODE" when there is no message.
            /// </summary>
            [[nodiscard]] std::string to_string() const;

            [[nodiscard]] bool operator==(const error &other) const noexcept {
                return mCategory == other.mCategory && mCode == other.mCode && mMessage == other.mMessage;
            }
    };

    /// <summary>
    /// Builds an error from an enumerator with a std::format style message.
    /// </summary>
    template <error_enum EnumType, typename... Args>
    [[nodiscard]] error make_error(EnumType code, std::format_string<Args...> format, Args &&...args) {
        return error::from(code, std::format(format, std::forward<Args>(args)...));
    }

    /// <summary>
    /// Builds an error from an enumerator with a fixed detail message (possibly empty).
    /// </summary>
    template <error_enum EnumType>
    [[nodiscard]] error make_error(EnumType code, std::string_view detail = {}) {
        return error::from(code, std::string(detail));
    }

    namespace detail {
        [[nodiscard]] CONJURE_API std::string_view core_error_code_name(u32 code) noexcept;
    }

    template <>
    struct error_category_traits<error_code> {
        static constexpr std::string_view NAME = "core";
        static constexpr u32 ID = hash::fnv1a32(NAME);

        [[nodiscard]] static std::string_view code_name(u32 code) noexcept {
            return detail::core_error_code_name(code);
        }
    };
}

// Binds a module error enumeration to a named category. `name_function` has the signature
// `std::string_view name_function(enum_type) noexcept`. Use at namespace scope in the module's
// public error header.
#define CONJURE_DECLARE_ERROR_CATEGORY(enum_type, category_name, name_function)                             \
    template <>                                                                                             \
    struct conjure::core::error_category_traits<enum_type> {                                                \
        static constexpr std::string_view NAME = category_name;                                             \
        static constexpr conjure::u32 ID = conjure::core::hash::fnv1a32(NAME);                              \
        [[nodiscard]] static std::string_view code_name(conjure::u32 code) noexcept {                       \
            return name_function(static_cast<enum_type>(code));                                             \
        }                                                                                                   \
    }

template <>
struct std::formatter<conjure::core::error_code> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(conjure::core::error_code code, FormatContext &context) const {
        return std::formatter<std::string_view>::format(conjure::core::to_string(code), context);
    }
};

template <>
struct std::formatter<conjure::core::error> : std::formatter<std::string> {
    template <typename FormatContext>
    auto format(const conjure::core::error &value, FormatContext &context) const {
        return std::formatter<std::string>::format(value.to_string(), context);
    }
};
