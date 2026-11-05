#pragma once

#include "conjure/core/assert.h"
#include "conjure/core/error.h"
#include "conjure/core/platform_detect.h"

#include <cstdlib>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace conjure::core {

    /// <summary>
    /// Either a value or an error. The engine's replacement for exceptions. Check with
    /// has_value() (or operator bool) before calling value().
    /// </summary>
    namespace detail {
        /// <summary>
        /// Accessing the wrong alternative of a result is a programmer error. It asserts, and
        /// because continuing would dereference nothing, it terminates even when the assertion
        /// handler asks to continue.
        /// </summary>
        [[noreturn]] inline void wrong_result_alternative(const char *what) {
            CONJURE_ASSERT(false, "{}", what);
            CONJURE_DEBUG_BREAK();
            std::abort();
        }
    }

    template <typename ValueType>
    class [[nodiscard]] result {
        private:
            static_assert(!std::is_same_v<std::remove_cvref_t<ValueType>, error>, "result<error> is ambiguous");
            static_assert(!std::is_reference_v<ValueType>, "result cannot hold references");

            // No default initialiser: a result always holds exactly one alternative, set by the
            // constructor; a default-constructed result does not exist.
            std::variant<ValueType, error> mStorage;

        public:
            result(const ValueType &value) :
                mStorage(std::in_place_index<0>, value) {}

            result(ValueType &&value) noexcept(std::is_nothrow_move_constructible_v<ValueType>) :
                mStorage(std::in_place_index<0>, std::move(value)) {}

            result(error failure) noexcept :
                mStorage(std::in_place_index<1>, std::move(failure)) {}

            result(const result &) = default;
            result &operator=(const result &) = default;
            result(result &&) noexcept = default;
            result &operator=(result &&) noexcept = default;

            ~result() = default;

        public:
            /// <summary>
            /// True when this result holds a value rather than an error.
            /// </summary>
            [[nodiscard]] bool has_value() const noexcept {
                return mStorage.index() == 0;
            }

            [[nodiscard]] explicit operator bool() const noexcept {
                return has_value();
            }

            /// <summary>
            /// The held value. Calling this on an error result is a fatal programmer error.
            /// </summary>
            [[nodiscard]] ValueType &value() & noexcept {
                ValueType *held = std::get_if<0>(&mStorage);
                if (held == nullptr) [[unlikely]] {
                    detail::wrong_result_alternative("result::value() called on an error result");
                }
                return *held;
            }

            [[nodiscard]] const ValueType &value() const & noexcept {
                const ValueType *held = std::get_if<0>(&mStorage);
                if (held == nullptr) [[unlikely]] {
                    detail::wrong_result_alternative("result::value() called on an error result");
                }
                return *held;
            }

            [[nodiscard]] ValueType &&value() && noexcept {
                ValueType *held = std::get_if<0>(&mStorage);
                if (held == nullptr) [[unlikely]] {
                    detail::wrong_result_alternative("result::value() called on an error result");
                }
                return std::move(*held);
            }

            /// <summary>
            /// The held error. Calling this on a value result is a fatal programmer error.
            /// </summary>
            [[nodiscard]] const error &get_error() const & noexcept {
                const error *held = std::get_if<1>(&mStorage);
                if (held == nullptr) [[unlikely]] {
                    detail::wrong_result_alternative("result::get_error() called on a value result");
                }
                return *held;
            }

            [[nodiscard]] error &&get_error() && noexcept {
                error *held = std::get_if<1>(&mStorage);
                if (held == nullptr) [[unlikely]] {
                    detail::wrong_result_alternative("result::get_error() called on a value result");
                }
                return std::move(*held);
            }

            /// <summary>
            /// The value, or 'fallback' when this result holds an error.
            /// </summary>
            template <typename FallbackType>
            [[nodiscard]] ValueType value_or(FallbackType &&fallback) const & {
                if (has_value()) {
                    return *std::get_if<0>(&mStorage);
                }
                return static_cast<ValueType>(std::forward<FallbackType>(fallback));
            }

            template <typename FallbackType>
            [[nodiscard]] ValueType value_or(FallbackType &&fallback) && {
                if (has_value()) {
                    return std::move(*std::get_if<0>(&mStorage));
                }
                return static_cast<ValueType>(std::forward<FallbackType>(fallback));
            }

            [[nodiscard]] ValueType *operator->() noexcept {
                return &value();
            }

            [[nodiscard]] const ValueType *operator->() const noexcept {
                return &value();
            }

            [[nodiscard]] ValueType &operator*() & noexcept {
                return value();
            }

            [[nodiscard]] const ValueType &operator*() const & noexcept {
                return value();
            }

            [[nodiscard]] ValueType &&operator*() && noexcept {
                return std::move(*this).value();
            }
    };

    /// <summary>
    /// A result that carries no value: success, or an error.
    /// </summary>
    template <>
    class [[nodiscard]] result<void> {
        private:
            std::optional<error> mError = std::nullopt;

        public:
            result() noexcept = default;

            result(error failure) noexcept :
                mError(std::move(failure)) {}

            result(const result &) = default;
            result &operator=(const result &) = default;
            result(result &&) noexcept = default;
            result &operator=(result &&) noexcept = default;

            ~result() = default;

        public:
            /// <summary>
            /// A successful void result. Equivalent to result<void>{}.
            /// </summary>
            [[nodiscard]] static result success() noexcept {
                return result();
            }

            [[nodiscard]] bool has_value() const noexcept {
                return !mError.has_value();
            }

            [[nodiscard]] explicit operator bool() const noexcept {
                return has_value();
            }

            /// <summary>
            /// The held error. Calling this on a success result is a fatal programmer error.
            /// </summary>
            [[nodiscard]] const error &get_error() const & noexcept {
                if (!mError.has_value()) [[unlikely]] {
                    detail::wrong_result_alternative("result::get_error() called on a success result");
                }
                return *mError;
            }

            [[nodiscard]] error &&get_error() && noexcept {
                if (!mError.has_value()) [[unlikely]] {
                    detail::wrong_result_alternative("result::get_error() called on a success result");
                }
                return std::move(*mError);
            }
    };

    /// <summary>
    /// Convenience alias for operations that only report success or failure.
    /// </summary>
    using status = result<void>;
}

// Propagates the error of a result-returning expression to the caller.
//
//   CONJURE_TRY(file.write(bytes));
//
// Limitations (by design, to stay within standard C++):
//   - The expression is bound by forwarding reference; passing a named lvalue result moves the
//     error out of it. Pass the call expression directly, or a temporary.
//   - Two uses on the same source line are not allowed (the helper name is built from __LINE__).
//   - A declaration containing an unparenthesised comma (std::pair<int, int> p) must use a type
//     alias.
#define CONJURE_TRY(expression)                                                                             \
    do {                                                                                                    \
        auto &&CONJURE_UNIQUE_NAME(conjureTryResult_) = (expression);                                      \
        if (!CONJURE_UNIQUE_NAME(conjureTryResult_).has_value()) {                                          \
            return ::conjure::core::error(std::move(CONJURE_UNIQUE_NAME(conjureTryResult_)).get_error()); \
        }                                                                                                   \
    } while (false)

// Assigns the value of a result-returning expression, or propagates its error.
//
//   CONJURE_TRY_ASSIGN(auto handle, open_file(path));
//
#define CONJURE_TRY_ASSIGN(declaration, expression)                                                         \
    auto &&CONJURE_UNIQUE_NAME(conjureTryAssign_) = (expression);                                          \
    if (!CONJURE_UNIQUE_NAME(conjureTryAssign_).has_value()) {                                              \
        return ::conjure::core::error(std::move(CONJURE_UNIQUE_NAME(conjureTryAssign_)).get_error());     \
    }                                                                                                       \
    declaration = std::move(CONJURE_UNIQUE_NAME(conjureTryAssign_)).value()
