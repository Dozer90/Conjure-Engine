#include "conjure/core/string_utils.h"

#include <array>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <format>
#include <limits>
#include <version>

using namespace conjure::core;
using namespace conjure::core::str;

/*===========================================================================*/
/*===========================================================================*/
// Case conversion

std::string conjure::core::str::to_lower(std::string_view text) {
    std::string result(text);
    to_lower_in_place(result);
    return result;
}

std::string conjure::core::str::to_upper(std::string_view text) {
    std::string result(text);
    to_upper_in_place(result);
    return result;
}

void conjure::core::str::to_lower_in_place(std::string &text) noexcept {
    for (char &character : text) {
        character = to_lower(character);
    }
}

void conjure::core::str::to_upper_in_place(std::string &text) noexcept {
    for (char &character : text) {
        character = to_upper(character);
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Splitting, joining, replacing

std::vector<std::string_view> conjure::core::str::split(std::string_view text, char delimiter, bool keepEmpty) {
    return split(text, std::string_view(&delimiter, 1), keepEmpty);
}

std::vector<std::string_view> conjure::core::str::split(
  std::string_view text,
  std::string_view delimiter,
  bool keepEmpty
) {
    std::vector<std::string_view> pieces = {};
    if (delimiter.empty()) {
        pieces.push_back(text);
        return pieces;
    }

    conjure::usize start = 0;
    while (true) {
        const conjure::usize found = text.find(delimiter, start);
        const std::string_view piece =
          found == std::string_view::npos ? text.substr(start) : text.substr(start, found - start);
        if (keepEmpty || !piece.empty()) {
            pieces.push_back(piece);
        }
        if (found == std::string_view::npos) {
            return pieces;
        }
        start = found + delimiter.size();
    }
}

std::vector<std::string_view> conjure::core::str::split_lines(std::string_view text) {
    std::vector<std::string_view> lines = {};
    conjure::usize start = 0;
    for (conjure::usize index = 0; index < text.size(); ++index) {
        const char character = text[index];
        if (character != '\n' && character != '\r') {
            continue;
        }
        lines.push_back(text.substr(start, index - start));
        if (character == '\r' && index + 1 < text.size() && text[index + 1] == '\n') {
            ++index;
        }
        start = index + 1;
    }
    if (start < text.size()) {
        lines.push_back(text.substr(start));
    }
    return lines;
}

std::string conjure::core::str::replace_all(std::string_view text, std::string_view from, std::string_view to) {
    if (from.empty()) {
        return std::string(text);
    }

    std::string result = {};
    conjure::usize start = 0;
    while (true) {
        const conjure::usize found = text.find(from, start);
        if (found == std::string_view::npos) {
            result.append(text.substr(start));
            return result;
        }
        result.append(text.substr(start, found - start));
        result.append(to);
        start = found + from.size();
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Parsing

namespace {
    [[nodiscard]] int digit_value(char character) noexcept {
        if (character >= '0' && character <= '9') {
            return character - '0';
        }
        if (character >= 'a' && character <= 'z') {
            return 10 + (character - 'a');
        }
        if (character >= 'A' && character <= 'Z') {
            return 10 + (character - 'A');
        }
        return -1;
    }

    // Strips an optional '+'. A second sign character afterwards is malformed, so the caller
    // sees it as a non-digit and fails.
    [[nodiscard]] std::string_view strip_plus(std::string_view text) noexcept {
        if (text.size() > 1 && text.front() == '+' && text[1] != '+' && text[1] != '-') {
            return text.substr(1);
        }
        return text;
    }
}

bool conjure::core::str::detail::parse_unsigned_digits(
  std::string_view text,
  int base,
  bool allowNegative,
  conjure::u64 limitPositive,
  conjure::u64 limitNegative,
  conjure::u64 &magnitude,
  bool &negative
) noexcept {
    magnitude = 0;
    negative = false;
    if (base < 2 || base > 36 || text.empty()) {
        return false;
    }

    if (text.front() == '+') {
        text.remove_prefix(1);
    } else if (text.front() == '-') {
        if (!allowNegative) {
            return false;
        }
        negative = true;
        text.remove_prefix(1);
    }

    if (base == 16 && text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        text.remove_prefix(2);
    }
    if (text.empty()) {
        return false;
    }

    const conjure::u64 limit = negative ? limitNegative : limitPositive;
    const conjure::u64 unsignedBase = static_cast<conjure::u64>(base);
    for (const char character : text) {
        const int digit = digit_value(character);
        if (digit < 0 || digit >= base) {
            return false;
        }
        const conjure::u64 digitValue = static_cast<conjure::u64>(digit);
        if (magnitude > (limit - digitValue) / unsignedBase) {
            return false;
        }
        magnitude = magnitude * unsignedBase + digitValue;
    }
    return true;
}

std::optional<conjure::i64> conjure::core::str::parse_int(std::string_view text, int base) noexcept {
    return parse_integer<conjure::i64>(text, base);
}

std::optional<conjure::u64> conjure::core::str::parse_uint(std::string_view text, int base) noexcept {
    return parse_integer<conjure::u64>(text, base);
}

std::optional<conjure::f64> conjure::core::str::parse_double(std::string_view text) noexcept {
    text = strip_plus(text);
    if (text.empty() || is_space(text.front())) {
        return std::nullopt;
    }
    // Hexadecimal floats are rejected everywhere so both implementations below agree.
    if (contains(text, 'x') || contains(text, 'X')) {
        return std::nullopt;
    }

#if defined(__cpp_lib_to_chars)
    conjure::f64 value = 0.0;
    const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc() || parsed.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
#else
    // Standard libraries without floating-point from_chars (older libc++) fall back to strtod,
    // which needs a terminated buffer. The engine never changes the C locale, so '.' is the
    // decimal point.
    const std::string terminated(text);
    char *end = nullptr;
    errno = 0;
    const conjure::f64 value = std::strtod(terminated.c_str(), &end);
    if (end != terminated.c_str() + terminated.size()) {
        return std::nullopt;
    }
    // ERANGE covers both overflow (infinite result) and underflow. strtod reports a denormal
    // result as ERANGE too, but from_chars accepts denormals and rejects only underflow to zero,
    // so match it: reject when the value collapsed to zero or infinity.
    if (errno == ERANGE && (std::isinf(value) || value == 0.0)) {
        return std::nullopt;
    }
    return value;
#endif
}

std::optional<conjure::f32> conjure::core::str::parse_float(std::string_view text) noexcept {
    const std::optional<conjure::f64> wide = parse_double(text);
    if (!wide.has_value()) {
        return std::nullopt;
    }
    // Decide before narrowing: converting a finite f64 beyond the f32 range is undefined
    // behaviour, not a well-defined infinity. Values past F32_MAX but closer to it than half an
    // ulp (2^103) are what IEEE round-to-nearest would give F32_MAX, so the shortest decimal
    // spelling of F32_MAX ("3.4028235e38") still round-trips; the midpoint and beyond overflow.
    constexpr conjure::f32 F32_MAX = std::numeric_limits<conjure::f32>::max();
    constexpr conjure::f64 F32_HALF_ULP = 0x1p103;
    constexpr conjure::f64 F32_OVERFLOW = static_cast<conjure::f64>(F32_MAX) + F32_HALF_ULP;
    const conjure::f64 magnitude = std::fabs(*wide);
    if (std::isfinite(*wide) && magnitude > static_cast<conjure::f64>(F32_MAX)) {
        if (magnitude >= F32_OVERFLOW) {
            return std::nullopt;
        }
        return *wide < 0.0 ? -F32_MAX : F32_MAX;
    }
    return static_cast<conjure::f32>(*wide);
}

std::optional<bool> conjure::core::str::parse_bool(std::string_view text) noexcept {
    if (iequals(text, "true") || iequals(text, "yes") || iequals(text, "on") || text == "1") {
        return true;
    }
    if (iequals(text, "false") || iequals(text, "no") || iequals(text, "off") || text == "0") {
        return false;
    }
    return std::nullopt;
}


/*===========================================================================*/
/*===========================================================================*/
// Formatting

std::string conjure::core::str::format_bytes(conjure::u64 bytes) {
    constexpr std::array<std::string_view, 7> UNITS = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
    constexpr conjure::f64 STEP = 1024.0;

    if (bytes < 1024) {
        return std::format("{} B", bytes);
    }

    conjure::f64 value = static_cast<conjure::f64>(bytes) / STEP;
    conjure::usize unit = 1;
    // Advance while the value, rounded to one decimal, would still print as 1024.0 or more.
    while (unit + 1 < UNITS.size() && std::round(value * 10.0) / 10.0 >= STEP) {
        value /= STEP;
        ++unit;
    }
    return std::format("{:.1f} {}", value, UNITS[unit]);
}


/*===========================================================================*/
/*===========================================================================*/
// Unicode

namespace {
    constexpr char32_t MAX_CODE_POINT = 0x10FFFF;
    constexpr char32_t HIGH_SURROGATE_FIRST = 0xD800;
    constexpr char32_t LOW_SURROGATE_FIRST = 0xDC00;
    constexpr char32_t SURROGATE_LAST = 0xDFFF;
    constexpr char32_t SUPPLEMENTARY_FIRST = 0x10000;

    [[nodiscard]] constexpr bool is_surrogate(char32_t codePoint) noexcept {
        return codePoint >= HIGH_SURROGATE_FIRST && codePoint <= SURROGATE_LAST;
    }

    [[nodiscard]] constexpr bool is_scalar_value(char32_t codePoint) noexcept {
        return codePoint <= MAX_CODE_POINT && !is_surrogate(codePoint);
    }

    [[nodiscard]] constexpr bool in_range(conjure::u8 value, conjure::u8 low, conjure::u8 high) noexcept {
        return value >= low && value <= high;
    }

    // Decodes one code point starting at 'position' and advances it past the bytes consumed.
    // Returns false for a malformed sequence; the position then skips the maximal invalid
    // subpart (one byte for a bad lead byte) so that one U+FFFD is written per error.
    [[nodiscard]] bool decode_utf8(std::string_view text, conjure::usize &position, char32_t &codePoint) noexcept {
        const conjure::u8 lead = static_cast<conjure::u8>(text[position]);
        if (lead < 0x80) {
            codePoint = lead;
            ++position;
            return true;
        }

        conjure::usize length = 0;
        conjure::u8 secondLow = 0x80;
        conjure::u8 secondHigh = 0xBF;
        if (in_range(lead, 0xC2, 0xDF)) {
            length = 2;
            codePoint = lead & 0x1Fu;
        } else if (in_range(lead, 0xE0, 0xEF)) {
            length = 3;
            codePoint = lead & 0x0Fu;
            if (lead == 0xE0) {
                secondLow = 0xA0;   // shorter forms of U+0080..U+07FF are overlong
            } else if (lead == 0xED) {
                secondHigh = 0x9F;  // U+D800..U+DFFF are surrogates
            }
        } else if (in_range(lead, 0xF0, 0xF4)) {
            length = 4;
            codePoint = lead & 0x07u;
            if (lead == 0xF0) {
                secondLow = 0x90;   // overlong forms of U+0800..U+FFFF
            } else if (lead == 0xF4) {
                secondHigh = 0x8F;  // beyond U+10FFFF
            }
        } else {
            // Stray continuation byte, overlong 2-byte lead (C0/C1) or lead beyond F4.
            codePoint = REPLACEMENT_CHARACTER;
            ++position;
            return false;
        }

        conjure::usize consumed = 1;
        for (conjure::usize index = 1; index < length; ++index) {
            if (position + index >= text.size()) {
                break;
            }
            const conjure::u8 continuation = static_cast<conjure::u8>(text[position + index]);
            const conjure::u8 low = index == 1 ? secondLow : conjure::u8{0x80};
            const conjure::u8 high = index == 1 ? secondHigh : conjure::u8{0xBF};
            if (!in_range(continuation, low, high)) {
                break;
            }
            codePoint = (codePoint << 6) | (continuation & 0x3Fu);
            ++consumed;
        }

        position += consumed;
        if (consumed != length) {
            codePoint = REPLACEMENT_CHARACTER;
            return false;
        }
        return true;
    }

    void append_utf8(std::string &output, char32_t codePoint) {
        if (codePoint < 0x80) {
            output.push_back(static_cast<char>(codePoint));
        } else if (codePoint < 0x800) {
            output.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
            output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else if (codePoint < SUPPLEMENTARY_FIRST) {
            output.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
            output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else {
            output.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
            output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
    }

    template <typename CharType>
    void append_utf16(std::basic_string<CharType> &output, char32_t codePoint) {
        if (codePoint < SUPPLEMENTARY_FIRST) {
            output.push_back(static_cast<CharType>(codePoint));
            return;
        }
        const char32_t offset = codePoint - SUPPLEMENTARY_FIRST;
        output.push_back(static_cast<CharType>(HIGH_SURROGATE_FIRST + (offset >> 10)));
        output.push_back(static_cast<CharType>(LOW_SURROGATE_FIRST + (offset & 0x3FF)));
    }

    // Decodes one UTF-16 code unit sequence; unpaired surrogates become U+FFFD.
    template <typename CharType>
    [[nodiscard]] char32_t decode_utf16(std::basic_string_view<CharType> text, conjure::usize &position) noexcept {
        const char32_t unit = static_cast<char32_t>(static_cast<conjure::u16>(text[position]));
        ++position;
        if (!is_surrogate(unit)) {
            return unit;
        }
        if (unit >= LOW_SURROGATE_FIRST || position >= text.size()) {
            return REPLACEMENT_CHARACTER;
        }
        const char32_t next = static_cast<char32_t>(static_cast<conjure::u16>(text[position]));
        if (next < LOW_SURROGATE_FIRST || next > SURROGATE_LAST) {
            return REPLACEMENT_CHARACTER;
        }
        ++position;
        return SUPPLEMENTARY_FIRST + ((unit - HIGH_SURROGATE_FIRST) << 10) + (next - LOW_SURROGATE_FIRST);
    }

    template <typename CharType>
    std::basic_string<CharType> utf8_to_utf16(std::string_view utf8) {
        std::basic_string<CharType> output = {};
        output.reserve(utf8.size());
        conjure::usize position = 0;
        while (position < utf8.size()) {
            char32_t codePoint = 0;
            // Malformed input already yields U+FFFD in codePoint; validity is irrelevant here.
            static_cast<void>(decode_utf8(utf8, position, codePoint));
            append_utf16(output, codePoint);
        }
        return output;
    }

    template <typename CharType>
    std::string utf16_to_utf8(std::basic_string_view<CharType> utf16) {
        std::string output = {};
        output.reserve(utf16.size());
        conjure::usize position = 0;
        while (position < utf16.size()) {
            append_utf8(output, decode_utf16(utf16, position));
        }
        return output;
    }

    // The UTF-32 conversions are templates as well so that to_wide/from_wide can select either
    // encoding with `if constexpr` without the unused branch leaving helpers unreferenced.
    template <typename CharType>
    std::basic_string<CharType> utf8_to_utf32(std::string_view utf8) {
        std::basic_string<CharType> output = {};
        output.reserve(utf8.size());
        conjure::usize position = 0;
        while (position < utf8.size()) {
            char32_t codePoint = 0;
            static_cast<void>(decode_utf8(utf8, position, codePoint));
            output.push_back(static_cast<CharType>(codePoint));
        }
        return output;
    }

    template <typename CharType>
    std::string utf32_to_utf8(std::basic_string_view<CharType> utf32) {
        std::string output = {};
        output.reserve(utf32.size());
        for (const CharType unit : utf32) {
            // A signed 32-bit wchar_t that holds a negative value wraps to a huge code point and is
            // rejected like any other value past U+10FFFF.
            const char32_t codePoint = static_cast<char32_t>(unit);
            append_utf8(output, is_scalar_value(codePoint) ? codePoint : REPLACEMENT_CHARACTER);
        }
        return output;
    }
}

std::u16string conjure::core::str::to_utf16(std::string_view utf8) {
    return utf8_to_utf16<char16_t>(utf8);
}

std::string conjure::core::str::from_utf16(std::u16string_view utf16) {
    return utf16_to_utf8<char16_t>(utf16);
}

std::u32string conjure::core::str::to_utf32(std::string_view utf8) {
    return utf8_to_utf32<char32_t>(utf8);
}

std::string conjure::core::str::from_utf32(std::u32string_view utf32) {
    return utf32_to_utf8<char32_t>(utf32);
}

std::wstring conjure::core::str::to_wide(std::string_view utf8) {
    if constexpr (sizeof(wchar_t) == 2) {
        return utf8_to_utf16<wchar_t>(utf8);
    } else {
        return utf8_to_utf32<wchar_t>(utf8);
    }
}

std::string conjure::core::str::from_wide(std::wstring_view wide) {
    if constexpr (sizeof(wchar_t) == 2) {
        return utf16_to_utf8<wchar_t>(wide);
    } else {
        return utf32_to_utf8<wchar_t>(wide);
    }
}

bool conjure::core::str::is_valid_utf8(std::string_view text) noexcept {
    conjure::usize position = 0;
    while (position < text.size()) {
        char32_t codePoint = 0;
        if (!decode_utf8(text, position, codePoint)) {
            return false;
        }
    }
    return true;
}
