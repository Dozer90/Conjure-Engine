#pragma once

#include "conjure/core/types.h"

#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace conjure::core {

    /// <summary>
    /// Broad classification of a recoverable failure. Specific context goes in the message.
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
    /// Returns the stable, upper-case name of an error code (e.g. "NOT_FOUND").
    /// </summary>
    [[nodiscard]] std::string_view to_string(error_code code) noexcept;

    /// <summary>
    /// A recoverable failure: a code plus a human readable message. Engine code returns these
    /// through result<T> instead of throwing.
    /// </summary>
    class error {
        private:
            error_code mCode = error_code::NONE;
            std::string mMessage = {};

        public:
            error();
            explicit error(error_code code);
            error(error_code code, std::string message);
            ~error();

            error(const error &) = default;
            error &operator=(const error &) = default;
            error(error &&) noexcept = default;
            error &operator=(error &&) noexcept = default;

        public:
            /// <summary>
            /// The classification of the failure.
            /// </summary>
            [[nodiscard]] error_code code() const noexcept {
                return mCode;
            }

            /// <summary>
            /// Human readable detail; may be empty.
            /// </summary>
            [[nodiscard]] const std::string &message() const noexcept {
                return mMessage;
            }

            /// <summary>
            /// "CODE: message" or just "CODE" when there is no message.
            /// </summary>
            [[nodiscard]] std::string to_string() const;

            [[nodiscard]] bool operator==(const error &other) const noexcept {
                return mCode == other.mCode && mMessage == other.mMessage;
            }
    };

    /// <summary>
    /// Builds an error with a std::format style message.
    /// </summary>
    template <typename... Args>
    [[nodiscard]] error make_error(error_code code, std::format_string<Args...> format, Args &&...args) {
        return error(code, std::format(format, std::forward<Args>(args)...));
    }
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
