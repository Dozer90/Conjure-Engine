#include "conjure/core/image.h"

#include "core/system/system_internal.h"
#include "conjure/core/assert.h"

#include <stb_image.h>
#include <stb_image_write.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <system_error>

using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// Helpers

namespace {
    constexpr std::array<std::byte, 8> PNG_SIGNATURE = {
        std::byte{0x89}, std::byte{'P'}, std::byte{'N'}, std::byte{'G'},
        std::byte{0x0D}, std::byte{0x0A}, std::byte{0x1A}, std::byte{0x0A},
    };

    bool has_png_signature(std::span<const std::byte> data) noexcept {
        if (data.size() < PNG_SIGNATURE.size()) {
            return false;
        }
        return std::equal(PNG_SIGNATURE.begin(), PNG_SIGNATURE.end(), data.begin());
    }

    // stb_image_write hands out the encoded file in pieces; collect them.
    void append_encoded(void *context, void *data, int size) {
        if (size <= 0) {
            return;
        }
        std::vector<std::byte> *encoded = static_cast<std::vector<std::byte> *>(context);
        const std::byte *bytes = static_cast<const std::byte *>(data);
        encoded->insert(encoded->end(), bytes, bytes + size);
    }

    conjure::usize pixel_offset(conjure::u32 width, conjure::u32 x, conjure::u32 y) noexcept {
        return (static_cast<conjure::usize>(y) * width + x) * image::BYTES_PER_PIXEL;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

image::image() {}

image::image(conjure::u32 width, conjure::u32 height) :
    mWidth(width),
    mHeight(height) {
    CONJURE_ASSERT(
      width <= MAX_DIMENSION && height <= MAX_DIMENSION,
      "image dimensions {}x{} exceed {}",
      width,
      height,
      MAX_DIMENSION
    );
    if (width == 0 || height == 0 || width > MAX_DIMENSION || height > MAX_DIMENSION) {
        mWidth = 0;
        mHeight = 0;
        return;
    }
    mPixels.resize(static_cast<conjure::usize>(width) * height * BYTES_PER_PIXEL);
}

image::~image() {}


/*===========================================================================*/
/*===========================================================================*/
// Pixels

rgba8 image::pixel(conjure::u32 x, conjure::u32 y) const noexcept {
    CONJURE_ASSERT(x < mWidth && y < mHeight, "image::pixel({}, {}) is outside {}x{}", x, y, mWidth, mHeight);
    if (x >= mWidth || y >= mHeight) {
        return rgba8{};
    }

    const conjure::usize offset = pixel_offset(mWidth, x, y);
    rgba8 color = {};
    color.r = static_cast<conjure::u8>(mPixels[offset + 0]);
    color.g = static_cast<conjure::u8>(mPixels[offset + 1]);
    color.b = static_cast<conjure::u8>(mPixels[offset + 2]);
    color.a = static_cast<conjure::u8>(mPixels[offset + 3]);
    return color;
}

void image::set_pixel(conjure::u32 x, conjure::u32 y, rgba8 color) noexcept {
    CONJURE_ASSERT(
      x < mWidth && y < mHeight,
      "image::set_pixel({}, {}) is outside {}x{}",
      x,
      y,
      mWidth,
      mHeight
    );
    if (x >= mWidth || y >= mHeight) {
        return;
    }

    const conjure::usize offset = pixel_offset(mWidth, x, y);
    mPixels[offset + 0] = std::byte{color.r};
    mPixels[offset + 1] = std::byte{color.g};
    mPixels[offset + 2] = std::byte{color.b};
    mPixels[offset + 3] = std::byte{color.a};
}

void image::fill(rgba8 color) noexcept {
    for (conjure::usize offset = 0; offset < mPixels.size(); offset += BYTES_PER_PIXEL) {
        mPixels[offset + 0] = std::byte{color.r};
        mPixels[offset + 1] = std::byte{color.g};
        mPixels[offset + 2] = std::byte{color.b};
        mPixels[offset + 3] = std::byte{color.a};
    }
}

void image::flip_vertical() noexcept {
    const conjure::usize rowBytes = static_cast<conjure::usize>(mWidth) * BYTES_PER_PIXEL;
    conjure::u32 top = 0;
    conjure::u32 bottom = mHeight;
    while (top + 1 < bottom) {
        std::byte *topRow = mPixels.data() + static_cast<conjure::usize>(top) * rowBytes;
        std::byte *bottomRow = mPixels.data() + static_cast<conjure::usize>(bottom - 1) * rowBytes;
        std::swap_ranges(topRow, topRow + rowBytes, bottomRow);
        ++top;
        --bottom;
    }
}

conjure::f64 image::compare(const image &other) const noexcept {
    if (mWidth != other.mWidth || mHeight != other.mHeight) {
        return 1.0;
    }
    if (mPixels.empty()) {
        return 0.0;
    }

    conjure::u64 totalDifference = 0;
    for (conjure::usize index = 0; index < mPixels.size(); ++index) {
        const int left = static_cast<int>(mPixels[index]);
        const int right = static_cast<int>(other.mPixels[index]);
        totalDifference += static_cast<conjure::u64>(left > right ? left - right : right - left);
    }
    return static_cast<conjure::f64>(totalDifference) / (static_cast<conjure::f64>(mPixels.size()) * 255.0);
}

bool image::operator==(const image &other) const noexcept {
    return mWidth == other.mWidth && mHeight == other.mHeight && mPixels == other.mPixels;
}


/*===========================================================================*/
/*===========================================================================*/
// PNG

result<image> image::load_png(std::span<const std::byte> data) {
    if (data.empty()) {
        return error(error_code::INVALID_ARGUMENT, "image::load_png: no data");
    }
    if (data.size() > static_cast<conjure::usize>(std::numeric_limits<int>::max())) {
        return error(error_code::INVALID_ARGUMENT, "image::load_png: data exceeds 2 GiB");
    }
    if (!has_png_signature(data)) {
        return error(error_code::PARSE_FAILURE, "image::load_png: not a PNG file (bad signature)");
    }

    int width = 0;
    int height = 0;
    int channelsInFile = 0;
    stbi_uc *decoded = stbi_load_from_memory(
      reinterpret_cast<const stbi_uc *>(data.data()),
      static_cast<int>(data.size()),
      &width,
      &height,
      &channelsInFile,
      static_cast<int>(BYTES_PER_PIXEL)
    );
    if (decoded == nullptr) {
        const char *reason = stbi_failure_reason();
        return make_error(
          error_code::PARSE_FAILURE,
          "image::load_png: {}",
          reason != nullptr ? reason : "decode failed"
        );
    }

    image loaded(static_cast<conjure::u32>(width), static_cast<conjure::u32>(height));
    if (!loaded.is_empty()) {
        std::memcpy(loaded.mPixels.data(), decoded, loaded.mPixels.size());
    }
    stbi_image_free(decoded);
    return loaded;
}

result<image> image::load_file(const std::filesystem::path &path) {
    std::error_code ignored = {};
    if (!std::filesystem::is_regular_file(path, ignored)) {
        return make_error(error_code::NOT_FOUND, "image::load_file: '{}' does not exist", detail::path_to_utf8(path));
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        return make_error(error_code::IO_FAILURE, "image::load_file: cannot open '{}'", detail::path_to_utf8(path));
    }

    // file_size reports failure (the file vanished or became unreadable since the check above)
    // through the error code and a value of -1; feeding that into the vector would throw.
    std::error_code sizeError = {};
    const std::uintmax_t size = std::filesystem::file_size(path, sizeError);
    if (sizeError) {
        return make_error(
          error_code::IO_FAILURE,
          "image::load_file: cannot read the size of '{}': {}",
          detail::path_to_utf8(path),
          sizeError.message()
        );
    }

    std::vector<std::byte> contents(static_cast<conjure::usize>(size));
    if (!contents.empty()) {
        stream.read(reinterpret_cast<char *>(contents.data()), static_cast<std::streamsize>(contents.size()));
        if (!stream) {
            return make_error(error_code::IO_FAILURE, "image::load_file: cannot read '{}'", detail::path_to_utf8(path));
        }
    }
    return load_png(contents);
}

result<std::vector<std::byte>> image::encode_png() const {
    if (is_empty()) {
        return error(error_code::INVALID_STATE, "image::encode_png: the image is empty");
    }

    // stb_image_write sizes its filtered rows ((width * 4 + 1) * height bytes) and the zlib
    // buffer in int; beyond that the arithmetic inside the encoder overflows.
    if (mPixels.size() + mHeight > MAX_ENCODED_BYTES) {
        return make_error(
          error_code::UNSUPPORTED,
          "image::encode_png: {}x{} exceeds the encoder's {} byte limit",
          mWidth,
          mHeight,
          MAX_ENCODED_BYTES
        );
    }

    std::vector<std::byte> encoded = {};
    encoded.reserve(mPixels.size() / 4);
    const int written = stbi_write_png_to_func(
      &append_encoded,
      &encoded,
      static_cast<int>(mWidth),
      static_cast<int>(mHeight),
      static_cast<int>(BYTES_PER_PIXEL),
      mPixels.data(),
      static_cast<int>(mWidth * BYTES_PER_PIXEL)
    );
    if (written == 0 || encoded.empty()) {
        return error(error_code::BACKEND_FAILURE, "image::encode_png: stb_image_write failed");
    }
    return encoded;
}

result<void> image::save_png(const std::filesystem::path &path) const {
    CONJURE_TRY_ASSIGN(const std::vector<std::byte> encoded, encode_png());

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        return make_error(
          error_code::IO_FAILURE,
          "image::save_png: cannot open '{}' for writing",
          detail::path_to_utf8(path)
        );
    }
    stream.write(reinterpret_cast<const char *>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
    stream.flush();
    if (!stream) {
        return make_error(error_code::IO_FAILURE, "image::save_png: cannot write '{}'", detail::path_to_utf8(path));
    }
    return {};
}
