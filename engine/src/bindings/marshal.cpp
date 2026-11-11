#include "conjure/bindings/marshal.h"

#include <algorithm>
#include <array>
#include <cstring>

using namespace conjure;
using namespace conjure::bindings;

/*===========================================================================*/
/*===========================================================================*/
// Layout

bool conjure::bindings::marshal::quat_storage_is_xyzw() noexcept {
    // Constructed (w, x, y, z); stored x, y, z, w when the bytes read 2, 3, 4, 1 (R58).
    const math::quat probe(1.0f, 2.0f, 3.0f, 4.0f);
    std::array<f32, 4> bytes = {};
    std::memcpy(bytes.data(), &probe, sizeof(bytes));
    return bytes[0] == 2.0f && bytes[1] == 3.0f && bytes[2] == 4.0f && bytes[3] == 1.0f;
}


/*===========================================================================*/
/*===========================================================================*/
// Strings and arrays

core::result<std::string_view> conjure::bindings::marshal::read_utf8(const char *utf8, int32_t length) noexcept {
    if (length < 0) {
        return core::make_error(bindings_error::INVALID_ARGUMENT, "negative string length");
    }
    if (length == 0) {
        return std::string_view{};
    }
    if (utf8 == nullptr) {
        return core::make_error(bindings_error::INVALID_ARGUMENT, "null string with a non-zero length");
    }
    return std::string_view(utf8, static_cast<usize>(length));
}

int32_t conjure::bindings::marshal::write_utf8(std::string_view text, char *utf8, int32_t capacity) noexcept {
    const int32_t needed = static_cast<int32_t>(std::min<usize>(text.size(), static_cast<usize>(INT32_MAX)));
    if (utf8 == nullptr || capacity <= 0) {
        return needed;
    }
    const usize written = std::min<usize>(text.size(), static_cast<usize>(capacity));
    if (written > 0) {
        std::memcpy(utf8, text.data(), written);
    }
    return needed;
}

int32_t conjure::bindings::marshal::write_entities(
  std::span<const ecs::entity> entities,
  uint64_t *out,
  int32_t capacity
) noexcept {
    const int32_t count = static_cast<int32_t>(std::min<usize>(entities.size(), static_cast<usize>(INT32_MAX)));
    if (out == nullptr || capacity <= 0) {
        return count;
    }
    const usize written = std::min<usize>(entities.size(), static_cast<usize>(capacity));
    for (usize index = 0; index < written; ++index) {
        out[index] = from_entity(entities[index]);
    }
    return count;
}
