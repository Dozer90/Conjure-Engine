#include "conjure/core/reflect/field_info.h"

#include "conjure/core/string_utils.h"
#include "conjure/core/types.h"

#include <array>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::reflect;

namespace {
    constexpr std::array<std::string_view, static_cast<usize>(field_kind::COUNT)> KIND_NAMES = {
        "BOOL",
        "I8",
        "I16",
        "I32",
        "I64",
        "U8",
        "U16",
        "U32",
        "U64",
        "F32",
        "F64",
        "STRING",
        "STRING_ID",
        "UUID",
        "VEC2",
        "VEC3",
        "VEC4",
        "QUAT",
        "MAT4",
        "TRANSFORM",
        "ENUM",
        "STRUCT",
        "ARRAY",
        "ENTITY",
        "ASSET_REF",
    };

    /// <summary>
    /// True when 'value' has exactly one bit set (a single flag).
    /// </summary>
    constexpr bool is_single_bit(u64 value) noexcept {
        return value != 0 && (value & (value - 1)) == 0;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Free functions

std::string_view conjure::core::reflect::to_string(field_kind kind) noexcept {
    const usize index = static_cast<usize>(kind);
    if (index >= KIND_NAMES.size()) {
        return "UNKNOWN";
    }
    return KIND_NAMES[index];
}

std::optional<field_kind> conjure::core::reflect::parse_field_kind(std::string_view name) noexcept {
    for (usize index = 0; index < KIND_NAMES.size(); ++index) {
        if (KIND_NAMES[index] == name) {
            return static_cast<field_kind>(index);
        }
    }
    return std::nullopt;
}

/*===========================================================================*/
/*===========================================================================*/
// enum_info

std::optional<i64> enum_info::find_value(std::string_view enumeratorName) const noexcept {
    for (const std::pair<std::string, i64> &entry : values) {
        if (entry.first == enumeratorName) {
            return entry.second;
        }
    }
    return std::nullopt;
}

const std::string *enum_info::find_name(i64 value) const noexcept {
    for (const std::pair<std::string, i64> &entry : values) {
        if (entry.second == value) {
            return &entry.first;
        }
    }
    return nullptr;
}

std::optional<std::string> enum_info::format_value(i64 value) const {
    // An exact enumerator always wins, for plain enums and for named flag combinations alike.
    const std::string *exact = find_name(value);
    if (exact != nullptr) {
        return *exact;
    }
    if (!isFlags) {
        return std::nullopt;
    }

    // Decompose a flag set into its single-bit enumerators; every bit must be covered.
    u64 remaining = static_cast<u64>(value);
    if (remaining == 0) {
        return std::string();
    }

    std::string text = {};
    for (const std::pair<std::string, i64> &entry : values) {
        const u64 bits = static_cast<u64>(entry.second);
        if (!is_single_bit(bits) || (remaining & bits) == 0) {
            continue;
        }
        if (!text.empty()) {
            text.push_back('|');
        }
        text.append(entry.first);
        remaining &= ~bits;
    }
    if (remaining != 0) {
        return std::nullopt;
    }
    return text;
}

std::optional<i64> enum_info::parse_value(std::string_view text) const {
    if (!isFlags) {
        return find_value(text);
    }

    u64 combined = 0;
    for (const std::string_view piece : str::split(text, '|')) {
        const std::string_view trimmed = str::trim(piece);
        if (trimmed.empty()) {
            continue;
        }
        const std::optional<i64> bits = find_value(trimmed);
        if (!bits.has_value()) {
            return std::nullopt;
        }
        combined |= static_cast<u64>(*bits);
    }
    return static_cast<i64>(combined);
}

bool enum_info::is_representable(i64 value) const noexcept {
    if (!is_valid_enum_size(underlyingSize)) {
        return false;
    }
    if (underlyingSize == 8) {
        return true;
    }
    const u32 bits = underlyingSize * 8;
    if (isSigned) {
        const i64 lowest = -(i64{1} << (bits - 1));
        const i64 highest = (i64{1} << (bits - 1)) - 1;
        return value >= lowest && value <= highest;
    }
    const i64 highest = static_cast<i64>((u64{1} << bits) - 1);
    return value >= 0 && value <= highest;
}
