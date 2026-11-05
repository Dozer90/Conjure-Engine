#include "conjure/core/containers/detail.h"
#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/core/containers/flat_map.h"
#include "conjure/core/containers/slot_key_format.h"
#include "conjure/core/containers/slot_map.h"
#include "conjure/core/containers/small_vector.h"
#include "conjure/core/containers/stable_vector.h"

#include "conjure/core/assert.h"
#include "conjure/core/platform_detect.h"

#include <charconv>
#include <cstdlib>
#include <string>
#include <string_view>
#include <system_error>

using namespace conjure;
using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// Fatal access

void conjure::core::detail::fatal_container_access(const char *message) {
    CONJURE_ASSERT(false, "{}", message);
    CONJURE_DEBUG_BREAK();
    std::abort();
}


/*===========================================================================*/
/*===========================================================================*/
// slot_key text form

namespace {
    bool parse_u32(std::string_view text, u32 &value) noexcept {
        if (text.empty()) {
            return false;
        }
        const char *first = text.data();
        const char *last = first + text.size();
        const std::from_chars_result parsed = std::from_chars(first, last, value);
        return parsed.ec == std::errc() && parsed.ptr == last;
    }
}

std::string conjure::core::to_string(slot_key key) {
    std::string text = std::to_string(key.index);
    text += ':';
    text += std::to_string(key.generation);
    return text;
}

std::optional<slot_key> conjure::core::parse_slot_key(std::string_view text) noexcept {
    const usize separator = text.find(':');
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }

    u32 index = 0;
    u32 generation = 0;
    if (!parse_u32(text.substr(0, separator), index) || !parse_u32(text.substr(separator + 1), generation)) {
        return std::nullopt;
    }
    return slot_key{index, generation};
}
