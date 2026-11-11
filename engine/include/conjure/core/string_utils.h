#pragma once

#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <concepts>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// ASCII-oriented string helpers used throughout the engine for identifiers, paths, config values
// and log output. Case conversion and classification are ASCII only on purpose: engine identifiers
// are ASCII, and locale-aware behaviour would make builds non-deterministic. UTF-8 is treated as
// opaque bytes everywhere except the explicit UTF-8 <-> UTF-16/UTF-32 conversions.

namespace conjure::core::str {

    /// <summary>
    /// The characters trim() removes by default: space, tab, newline, carriage return, form feed
    /// and vertical tab.
    /// </summary>
    inline constexpr std::string_view WHITESPACE = " \t\n\r\f\v";

    /*-----------------------------------------------------------------------*/
    // Character classification (ASCII)

    /// <summary>
    /// True for the characters in WHITESPACE.
    /// </summary>
    [[nodiscard]] constexpr bool is_space(char character) noexcept {
        return character == ' ' || character == '\t' || character == '\n' || character == '\r' || character == '\f' ||
          character == '\v';
    }

    /// <summary>
    /// True for '0'..'9'.
    /// </summary>
    [[nodiscard]] constexpr bool is_digit(char character) noexcept {
        return character >= '0' && character <= '9';
    }

    /// <summary>
    /// True for 'a'..'z' and 'A'..'Z'.
    /// </summary>
    [[nodiscard]] constexpr bool is_alpha(char character) noexcept {
        return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
    }

    /// <summary>
    /// True for ASCII letters and digits.
    /// </summary>
    [[nodiscard]] constexpr bool is_alnum(char character) noexcept {
        return is_alpha(character) || is_digit(character);
    }

    /// <summary>
    /// Lower-cases one ASCII letter; every other byte is returned unchanged.
    /// </summary>
    [[nodiscard]] constexpr char to_lower(char character) noexcept {
        if (character >= 'A' && character <= 'Z') {
            return static_cast<char>(character + ('a' - 'A'));
        }
        return character;
    }

    /// <summary>
    /// Upper-cases one ASCII letter; every other byte is returned unchanged.
    /// </summary>
    [[nodiscard]] constexpr char to_upper(char character) noexcept {
        if (character >= 'a' && character <= 'z') {
            return static_cast<char>(character - ('a' - 'A'));
        }
        return character;
    }

    /*-----------------------------------------------------------------------*/
    // Case conversion

    /// <summary>
    /// Copy of 'text' with ASCII letters lower-cased. Other bytes (including UTF-8) are untouched.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string to_lower(std::string_view text);

    /// <summary>
    /// Copy of 'text' with ASCII letters upper-cased. Other bytes (including UTF-8) are untouched.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string to_upper(std::string_view text);

    /// <summary>
    /// Lower-cases the ASCII letters of 'text' in place.
    /// </summary>
    CONJURE_API void to_lower_in_place(std::string &text) noexcept;

    /// <summary>
    /// Upper-cases the ASCII letters of 'text' in place.
    /// </summary>
    CONJURE_API void to_upper_in_place(std::string &text) noexcept;

    /*-----------------------------------------------------------------------*/
    // Trimming

    /// <summary>
    /// 'text' without leading and trailing characters from 'characters'. Returns a view into 'text'.
    /// </summary>
    [[nodiscard]] constexpr std::string_view trim(
      std::string_view text,
      std::string_view characters = WHITESPACE
    ) noexcept {
        const usize first = text.find_first_not_of(characters);
        if (first == std::string_view::npos) {
            return std::string_view();
        }
        const usize last = text.find_last_not_of(characters);
        return text.substr(first, last - first + 1);
    }

    /// <summary>
    /// 'text' without leading characters from 'characters'. Returns a view into 'text'.
    /// </summary>
    [[nodiscard]] constexpr std::string_view trim_left(
      std::string_view text,
      std::string_view characters = WHITESPACE
    ) noexcept {
        const usize first = text.find_first_not_of(characters);
        if (first == std::string_view::npos) {
            return std::string_view();
        }
        return text.substr(first);
    }

    /// <summary>
    /// 'text' without trailing characters from 'characters'. Returns a view into 'text'.
    /// </summary>
    [[nodiscard]] constexpr std::string_view trim_right(
      std::string_view text,
      std::string_view characters = WHITESPACE
    ) noexcept {
        const usize last = text.find_last_not_of(characters);
        if (last == std::string_view::npos) {
            return std::string_view();
        }
        return text.substr(0, last + 1);
    }

    /*-----------------------------------------------------------------------*/
    // Comparison and searching

    /// <summary>
    /// True when 'text' begins with 'prefix' (byte-wise). Every string starts with "".
    /// </summary>
    [[nodiscard]] constexpr bool starts_with(std::string_view text, std::string_view prefix) noexcept {
        return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
    }

    /// <summary>
    /// True when 'text' ends with 'suffix' (byte-wise). Every string ends with "".
    /// </summary>
    [[nodiscard]] constexpr bool ends_with(std::string_view text, std::string_view suffix) noexcept {
        return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
    }

    /// <summary>
    /// True when the two strings are equal ignoring ASCII case.
    /// </summary>
    [[nodiscard]] constexpr bool iequals(std::string_view left, std::string_view right) noexcept {
        if (left.size() != right.size()) {
            return false;
        }
        for (usize index = 0; index < left.size(); ++index) {
            if (to_lower(left[index]) != to_lower(right[index])) {
                return false;
            }
        }
        return true;
    }

    /// <summary>
    /// starts_with() ignoring ASCII case.
    /// </summary>
    [[nodiscard]] constexpr bool istarts_with(std::string_view text, std::string_view prefix) noexcept {
        return text.size() >= prefix.size() && iequals(text.substr(0, prefix.size()), prefix);
    }

    /// <summary>
    /// ends_with() ignoring ASCII case.
    /// </summary>
    [[nodiscard]] constexpr bool iends_with(std::string_view text, std::string_view suffix) noexcept {
        return text.size() >= suffix.size() && iequals(text.substr(text.size() - suffix.size()), suffix);
    }

    /// <summary>
    /// True when 'needle' occurs in 'text'. Every string contains "".
    /// </summary>
    [[nodiscard]] constexpr bool contains(std::string_view text, std::string_view needle) noexcept {
        return text.find(needle) != std::string_view::npos;
    }

    /// <summary>
    /// True when 'character' occurs in 'text'.
    /// </summary>
    [[nodiscard]] constexpr bool contains(std::string_view text, char character) noexcept {
        return text.find(character) != std::string_view::npos;
    }

    /*-----------------------------------------------------------------------*/
    // Splitting, joining, replacing

    /// <summary>
    /// Splits 'text' at every 'delimiter'. The pieces are views into 'text'. With 'keepEmpty'
    /// the result has exactly one more piece than there are delimiters (so "" yields [""] and
    /// "a,,b" yields ["a", "", "b"]); without it empty pieces are dropped.
    /// </summary>
    [[nodiscard]] CONJURE_API std::vector<std::string_view> split(
      std::string_view text,
      char delimiter,
      bool keepEmpty = true
    );

    /// <summary>
    /// Splits 'text' at every occurrence of the multi-character 'delimiter'. An empty delimiter
    /// yields the whole text as a single piece. Same 'keepEmpty' rules as the char overload.
    /// </summary>
    [[nodiscard]] CONJURE_API std::vector<std::string_view> split(
      std::string_view text,
      std::string_view delimiter,
      bool keepEmpty = true
    );

    /// <summary>
    /// Splits 'text' into lines at "\n", "\r\n" or a lone "\r". Line terminators are not part of
    /// the pieces; a trailing terminator does not produce an extra empty line.
    /// </summary>
    [[nodiscard]] CONJURE_API std::vector<std::string_view> split_lines(std::string_view text);

    /// <summary>
    /// Concatenates the pieces with 'separator' between them. Accepts any range whose elements
    /// convert to std::string_view (std::vector<std::string>, std::span<std::string_view>, ...).
    /// </summary>
    template <typename Range>
        requires std::convertible_to<std::ranges::range_value_t<Range>, std::string_view>
    [[nodiscard]] std::string join(const Range &pieces, std::string_view separator) {
        std::string result = {};
        bool first = true;
        for (const auto &piece : pieces) {
            if (!first) {
                result.append(separator);
            }
            result.append(std::string_view(piece));
            first = false;
        }
        return result;
    }

    /// <summary>
    /// Copy of 'text' with every non-overlapping occurrence of 'from' replaced by 'to', scanning
    /// left to right. An empty 'from' returns 'text' unchanged.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string replace_all(
      std::string_view text,
      std::string_view from,
      std::string_view to
    );

    /*-----------------------------------------------------------------------*/
    // Parsing

    /// <summary>
    /// Parses the whole of 'text' as an integer of type T in 'base' (2..36). Accepts an optional
    /// leading '+' or (for signed types) '-', and for base 16 an optional "0x"/"0X" prefix. No
    /// whitespace, no trailing characters, no digits outside the base; out-of-range values fail.
    /// </summary>
    template <std::integral T>
        requires(!std::same_as<T, bool>)
    [[nodiscard]] std::optional<T> parse_integer(std::string_view text, int base = 10) noexcept;

    /// <summary>
    /// parse_integer<i64>.
    /// </summary>
    [[nodiscard]] CONJURE_API std::optional<i64> parse_int(std::string_view text, int base = 10) noexcept;

    /// <summary>
    /// parse_integer<u64>. A leading '-' is rejected.
    /// </summary>
    [[nodiscard]] CONJURE_API std::optional<u64> parse_uint(std::string_view text, int base = 10) noexcept;

    /// <summary>
    /// Parses the whole of 'text' as a decimal floating-point number: optional sign, digits,
    /// optional fraction and exponent ("1", "-2.5", "1e-3", ".5", "7."), plus "inf"/"nan" in any
    /// case. Hexadecimal floats, whitespace and trailing characters are rejected.
    /// </summary>
    [[nodiscard]] CONJURE_API std::optional<f64> parse_double(std::string_view text) noexcept;

    /// <summary>
    /// parse_double() narrowed to f32. Finite values that overflow f32 are rejected.
    /// </summary>
    [[nodiscard]] CONJURE_API std::optional<f32> parse_float(std::string_view text) noexcept;

    /// <summary>
    /// Parses "true"/"false", "yes"/"no", "on"/"off" and "1"/"0", ignoring ASCII case.
    /// </summary>
    [[nodiscard]] CONJURE_API std::optional<bool> parse_bool(std::string_view text) noexcept;

    /*-----------------------------------------------------------------------*/
    // Formatting

    /// <summary>
    /// Human readable byte count with binary units: "512 B", "1.0 KiB", "1.5 MiB", "2.0 GiB".
    /// Values below 1 KiB print as whole bytes; larger values print one decimal.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string format_bytes(u64 bytes);

    /*-----------------------------------------------------------------------*/
    // Unicode

    /// <summary>
    /// The replacement character written for malformed input during conversions.
    /// </summary>
    inline constexpr char32_t REPLACEMENT_CHARACTER = 0xFFFD;

    /// <summary>
    /// Decodes UTF-8 to UTF-16. Malformed bytes (truncated sequences, overlong forms, encoded
    /// surrogates, code points above U+10FFFF) each become U+FFFD; the function never fails.
    /// </summary>
    [[nodiscard]] CONJURE_API std::u16string to_utf16(std::string_view utf8);

    /// <summary>
    /// Encodes UTF-16 as UTF-8. Unpaired surrogates become U+FFFD; the function never fails.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string from_utf16(std::u16string_view utf16);

    /// <summary>
    /// Decodes UTF-8 to UTF-32 (one code point per element). Same error handling as to_utf16().
    /// </summary>
    [[nodiscard]] CONJURE_API std::u32string to_utf32(std::string_view utf8);

    /// <summary>
    /// Encodes UTF-32 as UTF-8. Surrogate code points and values above U+10FFFF become U+FFFD;
    /// the function never fails.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string from_utf32(std::u32string_view utf32);

    /// <summary>
    /// UTF-8 to the platform wide string: UTF-16 where wchar_t is 16 bits (Windows paths and
    /// Win32 calls), UTF-32 where it is 32 bits. Same error handling as to_utf16()/to_utf32().
    /// </summary>
    [[nodiscard]] CONJURE_API std::wstring to_wide(std::string_view utf8);

    /// <summary>
    /// Platform wide string to UTF-8. Inverse of to_wide(); unpaired surrogates and invalid
    /// UTF-32 code points become U+FFFD, as in from_utf16()/from_utf32().
    /// </summary>
    [[nodiscard]] CONJURE_API std::string from_wide(std::wstring_view wide);

    /// <summary>
    /// True when 'text' is a valid UTF-8 byte sequence (no overlong forms, surrogates or code
    /// points above U+10FFFF). The empty string is valid.
    /// </summary>
    [[nodiscard]] CONJURE_API bool is_valid_utf8(std::string_view text) noexcept;

    /*-----------------------------------------------------------------------*/
    // Identifiers

    /// <summary>
    /// True when 'text' is a C-style identifier: non-empty, starts with an ASCII letter or '_',
    /// and continues with ASCII letters, digits or '_'.
    /// </summary>
    [[nodiscard]] constexpr bool is_valid_identifier(std::string_view text) noexcept {
        if (text.empty()) {
            return false;
        }
        if (!is_alpha(text.front()) && text.front() != '_') {
            return false;
        }
        for (const char character : text) {
            if (!is_alnum(character) && character != '_') {
                return false;
            }
        }
        return true;
    }

    /*-----------------------------------------------------------------------*/
    // Template definitions

    namespace detail {
        /// <summary>
        /// Shared body of parse_integer: strips the sign and prefix, then converts the digits.
        /// 'negative' is only accepted when 'allowNegative' is set. Returns false on any error.
        /// </summary>
        [[nodiscard]] CONJURE_API bool parse_unsigned_digits(
          std::string_view text,
          int base,
          bool allowNegative,
          u64 limitPositive,
          u64 limitNegative,
          u64 &magnitude,
          bool &negative
        ) noexcept;
    }

    template <std::integral T>
        requires(!std::same_as<T, bool>)
    std::optional<T> parse_integer(std::string_view text, int base) noexcept {
        using unsigned_type = std::make_unsigned_t<T>;
        constexpr u64 LIMIT_POSITIVE = static_cast<u64>(static_cast<unsigned_type>(std::numeric_limits<T>::max()));
        constexpr u64 LIMIT_NEGATIVE =
          std::is_signed_v<T> ? static_cast<u64>(static_cast<unsigned_type>(std::numeric_limits<T>::max())) + 1u : 0u;

        u64 magnitude = 0;
        bool negative = false;
        const bool parsed = detail::parse_unsigned_digits(
          text,
          base,
          std::is_signed_v<T>,
          LIMIT_POSITIVE,
          LIMIT_NEGATIVE,
          magnitude,
          negative
        );
        if (!parsed) {
            return std::nullopt;
        }

        if (!negative) {
            return static_cast<T>(magnitude);
        }
        // Two's complement negate in the unsigned domain, so the minimum value does not overflow.
        return static_cast<T>(static_cast<unsigned_type>(0u - static_cast<unsigned_type>(magnitude)));
    }
}
