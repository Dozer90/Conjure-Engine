#include "renderer/import/image_loader.h"
#include "renderer/import/jpeg_xl_codec.h"

#include "conjure/renderer/jpeg_xl.h"
#include "conjure/renderer/renderer_error.h"
#include "conjure/renderer/texture_asset.h"

#include "conjure/core/error.h"
#include "conjure/core/result.h"

#include <stb_image.h>

#include <cstring>
#include <limits>
#include <memory>
#include <string_view>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    /// <summary>
    /// Releases stb's pixel allocation when the decoded data has been copied out.
    /// </summary>
    struct stb_deleter {
        void operator()(void *pixels) const noexcept {
            stbi_image_free(pixels);
        }
    };

    [[nodiscard]] core::error decode_failure() {
        const char *reason = stbi_failure_reason();
        return core::make_error(
          renderer_error::DECODE_FAILED,
          "stb_image: {}",
          reason != nullptr ? std::string_view(reason) : std::string_view("unknown error")
        );
    }

    [[nodiscard]] bool too_large(std::span<const std::byte> encoded) noexcept {
        return encoded.size() > static_cast<usize>(std::numeric_limits<int>::max());
    }

    [[nodiscard]] bool exceeds_texel_limit(int width, int height) noexcept {
        const bool tooWide = static_cast<u32>(width) > texture_asset::MAX_DIMENSION;
        const bool tooTall = static_cast<u32>(height) > texture_asset::MAX_DIMENSION;
        return tooWide || tooTall;
    }

    /// <summary>
    /// Fails with INVALID_ARGUMENT when the header alone already names a size above the texel limit,
    /// so an oversized image is refused before stb allocates and decodes it. A header stb cannot read
    /// passes: the decode that follows reports it.
    /// </summary>
    [[nodiscard]] core::result<void> check_header_size(std::span<const std::byte> encoded, std::string_view caller) {
        int width = 0;
        int height = 0;
        int channels = 0;
        const int known = stbi_info_from_memory(
          reinterpret_cast<const stbi_uc *>(encoded.data()),
          static_cast<int>(encoded.size()),
          &width,
          &height,
          &channels
        );
        if (known == 0 || !exceeds_texel_limit(width, height)) {
            return core::result<void>();
        }
        return core::make_error(
          core::error_code::INVALID_ARGUMENT,
          "{}: {}x{} exceeds the {} texel limit",
          caller,
          width,
          height,
          texture_asset::MAX_DIMENSION
        );
    }
}

core::result<detail::decoded_image> conjure::renderer::detail::decode_rgba8(
  std::span<const std::byte> encoded,
  texture_colour_space colourSpace,
  core::jobs::job_system *jobs
) {
    if (is_jpeg_xl(encoded)) {
        return decode_jpeg_xl_rgba8(encoded, colourSpace, jobs);
    }
    if (encoded.empty()) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "decode_rgba8: no data");
    }
    if (too_large(encoded)) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "decode_rgba8: file larger than 2 GiB");
    }
    CONJURE_TRY(check_header_size(encoded, "decode_rgba8"));

    int width = 0;
    int height = 0;
    int channels = 0;
    const std::unique_ptr<stbi_uc, stb_deleter> pixels(stbi_load_from_memory(
      reinterpret_cast<const stbi_uc *>(encoded.data()),
      static_cast<int>(encoded.size()),
      &width,
      &height,
      &channels,
      4
    ));
    if (pixels == nullptr || width <= 0 || height <= 0) {
        return decode_failure();
    }
    // The header check above covers every format stb can describe; this one stays as the backstop.
    if (exceeds_texel_limit(width, height)) {
        return core::make_error(
          core::error_code::INVALID_ARGUMENT,
          "decode_rgba8: {}x{} exceeds the {} texel limit",
          width,
          height,
          texture_asset::MAX_DIMENSION
        );
    }

    decoded_image result = {};
    result.width = static_cast<u32>(width);
    result.height = static_cast<u32>(height);
    result.rgba.resize(static_cast<usize>(result.width) * result.height * 4);
    std::memcpy(result.rgba.data(), pixels.get(), result.rgba.size());
    return result;
}

core::result<detail::decoded_hdr_image> conjure::renderer::detail::decode_hdr_rgb(
  std::span<const std::byte> encoded,
  core::jobs::job_system *jobs
) {
    if (is_jpeg_xl(encoded)) {
        return decode_jpeg_xl_rgb_float(encoded, jobs);
    }
    if (encoded.empty()) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "decode_hdr_rgb: no data");
    }
    if (too_large(encoded)) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "decode_hdr_rgb: file larger than 2 GiB");
    }
    CONJURE_TRY(check_header_size(encoded, "decode_hdr_rgb"));

    int width = 0;
    int height = 0;
    int channels = 0;
    const std::unique_ptr<float, stb_deleter> pixels(stbi_loadf_from_memory(
      reinterpret_cast<const stbi_uc *>(encoded.data()),
      static_cast<int>(encoded.size()),
      &width,
      &height,
      &channels,
      3
    ));
    if (pixels == nullptr || width <= 0 || height <= 0) {
        return decode_failure();
    }
    // The header check above covers every format stb can describe; this one stays as the backstop.
    if (exceeds_texel_limit(width, height)) {
        return core::make_error(
          core::error_code::INVALID_ARGUMENT,
          "decode_hdr_rgb: {}x{} exceeds the {} texel limit",
          width,
          height,
          texture_asset::MAX_DIMENSION
        );
    }

    decoded_hdr_image result = {};
    result.width = static_cast<u32>(width);
    result.height = static_cast<u32>(height);
    result.rgb.resize(static_cast<usize>(result.width) * result.height * 3);
    std::memcpy(result.rgb.data(), pixels.get(), result.rgb.size() * sizeof(f32));
    return result;
}
