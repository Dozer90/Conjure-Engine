#pragma once

#include "conjure/core/api.h"
#include "conjure/core/containers/slot_map.h"

#include <format>
#include <optional>
#include <string>
#include <string_view>

// Text form of slot_key: to_string, parse_slot_key and the std::formatter specialisation. Kept
// apart from slot_map.h so code that only stores and looks up handles does not pay for <format>
// and the string headers.
//
//   CONJURE_LOG_INFO(LOG_ASSETS, "loaded {}", key);        // "12:3"
//   if (auto parsed = parse_slot_key(text)) { ... }

namespace conjure::core {

    /// <summary>
    /// "index:generation" in decimal, e.g. "12:3". The null key prints as "0:0".
    /// </summary>
    [[nodiscard]] CONJURE_API std::string to_string(slot_key key);

    /// <summary>
    /// Parses the to_string() form. Returns nullopt for anything else.
    /// </summary>
    [[nodiscard]] CONJURE_API std::optional<slot_key> parse_slot_key(std::string_view text) noexcept;
}

template <>
struct std::formatter<conjure::core::slot_key> : std::formatter<std::string> {
    template <typename FormatContext>
    auto format(const conjure::core::slot_key &key, FormatContext &context) const {
        return std::formatter<std::string>::format(conjure::core::to_string(key), context);
    }
};
