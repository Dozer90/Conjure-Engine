#include "conjure/rhi/texture_format.h"

#include "rhi/texture_format_mapping.h"

#include "conjure/core/error.h"
#include "conjure/core/string_utils.h"

#include <array>
#include <utility>

using namespace conjure;
using namespace conjure::rhi;

namespace {
    constexpr std::array<std::string_view, static_cast<usize>(texture_format::COUNT)> FORMAT_NAMES = {
        "UNKNOWN",
        "R8_UNORM",
        "RG8_UNORM",
        "RGBA8_UNORM",
        "RGBA8_SRGB",
        "BGRA8_SRGB",
        "RG16_SNORM",
        "RGBA16_SNORM",
        "R16_FLOAT",
        "RG16_FLOAT",
        "RGBA16_FLOAT",
        "R32_FLOAT",
        "RG32_FLOAT",
        "RGB32_FLOAT",
        "RGBA32_FLOAT",
        "R32_UINT",
        "R11G11B10_FLOAT",
        "D32_FLOAT",
        "BC1_SRGB",
        "BC5_UNORM",
        "BC7_SRGB",
    };
}

std::string_view conjure::rhi::to_string(texture_format format) noexcept {
    const usize index = static_cast<usize>(format);
    if (index >= FORMAT_NAMES.size()) {
        return FORMAT_NAMES[0];
    }
    return FORMAT_NAMES[index];
}

core::result<texture_format> conjure::rhi::parse_texture_format(std::string_view name) noexcept {
    const std::string_view trimmed = core::str::trim(name);
    for (usize index = 0; index < FORMAT_NAMES.size(); ++index) {
        if (core::str::iequals(trimmed, FORMAT_NAMES[index])) {
            return static_cast<texture_format>(index);
        }
    }
    return core::make_error(core::error_code::INVALID_ARGUMENT, "unknown texture format '{}'", name);
}

// The NVRHI mapping is a pair of constexpr switches in texture_format_mapping.h; the round trip of
// every enumerator is proven here at compile time so no runtime test has to include NVRHI.
namespace {
    constexpr bool round_trips(texture_format format) noexcept {
        return from_nvrhi_format(to_nvrhi_format(format)) == format;
    }

    template <usize... Indices>
    constexpr bool all_round_trip(std::index_sequence<Indices...>) noexcept {
        return (round_trips(static_cast<texture_format>(Indices)) && ...);
    }

    static_assert(
      all_round_trip(std::make_index_sequence<static_cast<usize>(texture_format::COUNT)>()),
      "every texture_format must map to a distinct NVRHI format and back"
    );
    static_assert(to_nvrhi_format(texture_format::COUNT) == nvrhi::Format::UNKNOWN, "COUNT maps to UNKNOWN");
    static_assert(
      from_nvrhi_format(nvrhi::Format::RGBA8_UINT) == texture_format::UNKNOWN,
      "NVRHI formats the engine does not use map to UNKNOWN"
    );
}
