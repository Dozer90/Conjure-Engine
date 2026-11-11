#pragma once

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <filesystem>
#include <span>
#include <vector>

// RGBA8 images with PNG loading and saving. This is the small image type core needs for golden
// tests, icons and screenshots; the renderer owns the real texture formats.

namespace conjure::core {

    /// <summary>
    /// One pixel: red, green, blue, alpha, 8 bits each. The default is opaque black.
    /// </summary>
    struct rgba8 {
        u8 r = 0;
        u8 g = 0;
        u8 b = 0;
        u8 a = 255;

        [[nodiscard]] constexpr bool operator==(const rgba8 &) const noexcept = default;
    };

    /// <summary>
    /// A row-major RGBA8 image, 4 bytes per pixel, rows top to bottom and no row padding. Value
    /// type: copies are deep.
    /// </summary>
    class CONJURE_API image {
        private:
            u32 mWidth = 0;
            u32 mHeight = 0;
            std::vector<std::byte> mPixels = {};

        public:
            /// <summary>
            /// Size of one pixel in bytes.
            /// </summary>
            static constexpr u32 BYTES_PER_PIXEL = 4;

            /// <summary>
            /// Largest width or height accepted; the PNG codec rejects anything larger.
            /// </summary>
            static constexpr u32 MAX_DIMENSION = 1u << 24;

            /// <summary>
            /// Largest value of width * height * BYTES_PER_PIXEL + height that encode_png()
            /// accepts (the PNG encoder works in 32-bit signed arithmetic).
            /// </summary>
            static constexpr usize MAX_ENCODED_BYTES = 0x7FFFFFFF;

        public:
            image();

            /// <summary>
            /// A zero-filled image (transparent black). A zero width or height yields the empty
            /// image. Dimensions above MAX_DIMENSION are a programmer error and yield the empty
            /// image too.
            /// </summary>
            image(u32 width, u32 height);

            ~image();

            image(const image &) = default;
            image &operator=(const image &) = default;
            image(image &&) noexcept = default;
            image &operator=(image &&) noexcept = default;

        public:
            /// <summary>
            /// Width in pixels; 0 for the empty image.
            /// </summary>
            [[nodiscard]] u32 width() const noexcept {
                return mWidth;
            }

            /// <summary>
            /// Height in pixels; 0 for the empty image.
            /// </summary>
            [[nodiscard]] u32 height() const noexcept {
                return mHeight;
            }

            /// <summary>
            /// True when the image has no pixels.
            /// </summary>
            [[nodiscard]] bool is_empty() const noexcept {
                return mPixels.empty();
            }

            /// <summary>
            /// The pixel bytes, width * height * 4 of them: for pixel (x, y) the red byte is at
            /// index (y * width + x) * 4, followed by green, blue and alpha.
            /// </summary>
            [[nodiscard]] std::span<std::byte> pixels() noexcept {
                return mPixels;
            }

            [[nodiscard]] std::span<const std::byte> pixels() const noexcept {
                return mPixels;
            }

            /// <summary>
            /// The pixel at (x, y). Out-of-range coordinates assert and return a default rgba8.
            /// </summary>
            [[nodiscard]] rgba8 pixel(u32 x, u32 y) const noexcept;

            /// <summary>
            /// Sets the pixel at (x, y). Out-of-range coordinates assert and change nothing.
            /// </summary>
            void set_pixel(u32 x, u32 y, rgba8 color) noexcept;

            /// <summary>
            /// Sets every pixel to one colour.
            /// </summary>
            void fill(rgba8 color) noexcept;

            /// <summary>
            /// Decodes a PNG held in memory. Grey, grey + alpha and RGB files are expanded to
            /// RGBA8 (alpha 255 when absent). Fails with INVALID_ARGUMENT for empty data and
            /// PARSE_FAILURE for anything that is not a well-formed PNG.
            /// </summary>
            [[nodiscard]] static result<image> load_png(std::span<const std::byte> data);

            /// <summary>
            /// Reads a PNG file. Fails with NOT_FOUND when the file does not exist, IO_FAILURE
            /// when it cannot be read and PARSE_FAILURE when it is not a PNG.
            /// </summary>
            [[nodiscard]] static result<image> load_file(const std::filesystem::path &path);

            /// <summary>
            /// Encodes the image as PNG. Fails with INVALID_STATE for an empty image and
            /// UNSUPPORTED for one whose pixel data (plus one filter byte per row) exceeds
            /// MAX_ENCODED_BYTES, the limit of the encoder's 32-bit arithmetic.
            /// </summary>
            [[nodiscard]] result<std::vector<std::byte>> encode_png() const;

            /// <summary>
            /// Writes the image as a PNG file, replacing any existing file. Same failures as
            /// encode_png() plus IO_FAILURE when the file cannot be written.
            /// </summary>
            [[nodiscard]] result<void> save_png(const std::filesystem::path &path) const;

            /// <summary>
            /// Reverses the order of the rows in place (top becomes bottom).
            /// </summary>
            void flip_vertical() noexcept;

            /// <summary>
            /// Mean absolute difference over every channel of every pixel, normalised to
            /// 0 (identical) .. 1 (every byte differs by 255). Images of different dimensions
            /// compare as 1. Two empty images compare as 0. Used by golden-image tests.
            /// </summary>
            [[nodiscard]] f64 compare(const image &other) const noexcept;

            /// <summary>
            /// True when dimensions and every pixel byte are equal.
            /// </summary>
            [[nodiscard]] bool operator==(const image &other) const noexcept;
    };
}
