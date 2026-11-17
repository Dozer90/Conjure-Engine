#pragma once

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_loader.h"
#include "conjure/assets/asset_types.h"
#include "conjure/core/api.h"
#include "conjure/core/image.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/rhi/texture_format.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The CPU texture asset (docs/design/02_renderer.md 2.6, 4.2). Mips are generated on the CPU at
// import (box filter, sRGB-aware) and stored in the .ctex blob, so the GPU path is upload-only.
// Decoding goes through libjxl (JPEG XL) or stb_image (PNG, JPEG, TGA, BMP) in engine/src, chosen by the file's
// signature; the public API only sees bytes.

namespace conjure::renderer {

    inline constexpr std::string_view TEXTURE_ASSET_TYPE_NAME = "conjure.renderer.texture";
    inline constexpr std::string_view TEXTURE_IMPORTER_ID = "conjure.renderer.texture";
    inline constexpr u32 TEXTURE_IMPORTER_VERSION = 2;         // 2: JPEG XL sources, colour-managed
    inline constexpr u32 TEXTURE_BLOB_VERSION = 1;
    inline constexpr std::array<std::string_view, 6> TEXTURE_EXTENSIONS = {
      ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".jxl"
    };

    enum class texture_colour_space : u8 {
        SRGB = 0,
        LINEAR
    };

    enum class sampler_filter : u8 {
        LINEAR = 0,
        POINT
    };

    enum class texture_wrap : u8 {
        REPEAT = 0,
        CLAMP,
        MIRROR
    };

    /// <summary>
    /// One mip level, tightly packed rows of bytes_per_pixel(format) pixels.
    /// </summary>
    struct texture_mip {
        u32 width = 0;
        u32 height = 0;
        std::vector<std::byte> pixels = {};

        [[nodiscard]] bool operator==(const texture_mip &other) const noexcept = default;
    };

    /// <summary>
    /// Number of mip levels of a full chain down to 1x1.
    /// </summary>
    [[nodiscard]] CONJURE_API u32 mip_count_for(u32 width, u32 height) noexcept;

    class CONJURE_API texture_asset {
        private:
            rhi::texture_format mFormat = rhi::texture_format::RGBA8_SRGB;
            texture_colour_space mColourSpace = texture_colour_space::SRGB;
            sampler_filter mFilter = sampler_filter::LINEAR;
            texture_wrap mWrap = texture_wrap::REPEAT;
            std::vector<texture_mip> mMips = {};
            std::string mName = {};

        public:
            /// <summary>
            /// Largest width or height accepted (the D3D12 / Vulkan guaranteed limit).
            /// </summary>
            static constexpr u32 MAX_DIMENSION = 16384;

        public:
            texture_asset();
            ~texture_asset();

            texture_asset(const texture_asset &) = default;
            texture_asset &operator=(const texture_asset &) = default;
            texture_asset(texture_asset &&) noexcept = default;
            texture_asset &operator=(texture_asset &&) noexcept = default;

        public:
            /// <summary>
            /// Builds a texture from RGBA8 pixels (rows top to bottom, no padding). SRGB selects the
            /// RGBA8_SRGB format, LINEAR RGBA8_UNORM; with 'generateMips' the full chain is built with a
            /// box filter in linear space. Fails with INVALID_ARGUMENT for a zero size, a size above
            /// MAX_DIMENSION or a pixel array of the wrong length.
            /// </summary>
            [[nodiscard]] static core::result<texture_asset> from_rgba8(
              u32 width,
              u32 height,
              std::span<const std::byte> pixels,
              texture_colour_space colourSpace,
              bool generateMips = true
            );

            /// <summary>
            /// from_rgba8 over a core::image.
            /// </summary>
            [[nodiscard]] static core::result<texture_asset> from_image(
              const core::image &source,
              texture_colour_space colourSpace,
              bool generateMips = true
            );

            /// <summary>
            /// Decodes an encoded image file (JPEG XL, PNG, JPEG, TGA, BMP), recognised by its signature. A JPEG
            /// XL file is colour-managed: SRGB converts its colour profile to sRGB, LINEAR keeps its samples as
            /// stored (data such as normals must not change). For SRGB a PQ or HLG file is taken to linear light
            /// with 1.0 at 203 nits (BT.2408's reference white), clipped at 1 and sRGB-encoded, so reference white
            /// is white and brighter highlights clip. Its orientation is applied, premultiplied alpha is divided
            /// out in the encoding the file stores, before any conversion, an animation keeps its first frame
            /// (with a warning), and colour and alpha deeper than 8 bits keep their high byte, as a 16-bit PNG
            /// does (8-bit colour beside a deeper alpha is rounded). The other formats ignore colour profiles.
            /// 'jobs' (optional) decodes a JPEG XL file on its workers; the result is the same bytes either way,
            /// and so is the memory limit below. Fails with renderer_error::DECODE_FAILED for
            /// anything the decoder rejects, naming a truncated file apart from a corrupt one, and with
            /// INVALID_ARGUMENT for empty data, an image larger than MAX_DIMENSION, or a JPEG XL file whose header
            /// asks for more than its limits allow: more than 16 extra channels or a layer larger than
            /// MAX_DIMENSION (both refused before any frame is decoded), or more decoding memory than the image's
            /// size allows.
            /// </summary>
            [[nodiscard]] static core::result<texture_asset> decode(
              std::span<const std::byte> encoded,
              texture_colour_space colourSpace,
              bool generateMips = true,
              core::jobs::job_system *jobs = nullptr
            );

            /// <summary>
            /// Reads and decodes a file. Fails with NOT_FOUND when the file does not exist, IO_FAILURE
            /// when it cannot be read, then as decode().
            /// </summary>
            [[nodiscard]] static core::result<texture_asset> load_from_file(
              const std::filesystem::path &path,
              texture_colour_space colourSpace,
              bool generateMips = true,
              core::jobs::job_system *jobs = nullptr
            );

            /// <summary>
            /// A 1x1 (or width x height) texture of one colour.
            /// </summary>
            [[nodiscard]] static texture_asset solid_colour(
              core::rgba8 colour,
              texture_colour_space colourSpace,
              u32 width = 1,
              u32 height = 1
            );

            /// <summary>
            /// A checkerboard of two colours with 'cellSize' pixel cells.
            /// </summary>
            [[nodiscard]] static texture_asset checker(
              core::rgba8 first,
              core::rgba8 second,
              u32 size = 16,
              u32 cellSize = 4,
              texture_colour_space colourSpace = texture_colour_space::SRGB
            );

            /// <summary>
            /// Parses a .ctex blob. Fails with renderer_error::INVALID_ASSET naming the problem.
            /// </summary>
            [[nodiscard]] static core::result<texture_asset> parse_blob(std::span<const std::byte> bytes);

            /// <summary>
            /// Serialises the texture as a .ctex blob (little-endian, deterministic).
            /// </summary>
            [[nodiscard]] std::vector<std::byte> write_blob() const;

            [[nodiscard]] rhi::texture_format format() const noexcept {
                return mFormat;
            }

            [[nodiscard]] texture_colour_space colour_space() const noexcept {
                return mColourSpace;
            }

            [[nodiscard]] sampler_filter filter() const noexcept {
                return mFilter;
            }

            [[nodiscard]] texture_wrap wrap() const noexcept {
                return mWrap;
            }

            void set_filter(sampler_filter filter) noexcept {
                mFilter = filter;
            }

            void set_wrap(texture_wrap wrap) noexcept {
                mWrap = wrap;
            }

            void set_name(std::string name) {
                mName = std::move(name);
            }

            [[nodiscard]] std::string_view name() const noexcept {
                return mName;
            }

            [[nodiscard]] u32 width() const noexcept;
            [[nodiscard]] u32 height() const noexcept;

            [[nodiscard]] u32 mip_count() const noexcept {
                return static_cast<u32>(mMips.size());
            }

            [[nodiscard]] std::span<const texture_mip> mips() const noexcept {
                return mMips;
            }

            /// <summary>
            /// Bytes of every mip together.
            /// </summary>
            [[nodiscard]] u64 gpu_byte_size() const noexcept;

            /// <summary>
            /// The pixel of mip 0 at (x, y) as RGBA8 (the 8-bit formats only; other formats read opaque
            /// black). Out of range asserts and reads black.
            /// </summary>
            [[nodiscard]] core::rgba8 pixel(u32 x, u32 y) const noexcept;

            [[nodiscard]] bool operator==(const texture_asset &other) const noexcept;
    };

    /// <summary>
    /// Importer settings, mirrored by the JSON object
    /// { "colourSpace": "srgb" | "linear", "filter": "linear" | "point", "wrap": "repeat" | "clamp" | "mirror",
    /// "generateMips": true }.
    /// </summary>
    struct CONJURE_API texture_import_settings {
        texture_colour_space colourSpace = texture_colour_space::SRGB;
        sampler_filter filter = sampler_filter::LINEAR;
        texture_wrap wrap = texture_wrap::REPEAT;
        bool generateMips = true;

        /// <summary>
        /// Reads the settings object; missing keys keep their defaults, an invalid value yields the
        /// defaults. Fails with INVALID_ARGUMENT for an unknown enumerator name or a non-object.
        /// </summary>
        [[nodiscard]] static core::result<texture_import_settings> from_json(const core::json::value &source);

        void to_json(core::json::writer &out) const;
        [[nodiscard]] core::json::document to_document() const;

        [[nodiscard]] bool operator==(const texture_import_settings &other) const noexcept = default;
    };

}

CONJURE_DECLARE_ASSET_TYPE(conjure::renderer::texture_asset, "conjure.renderer.texture");

namespace conjure::renderer {

    /// <summary>
    /// The texture importer: .png .jpg .jpeg .tga .bmp .jxl -> one .ctex blob (decode + CPU mips). The file's
    /// signature picks the decoder, so a misnamed file still imports.
    /// </summary>
    class CONJURE_API texture_importer final : public assets::asset_importer {
        private:
            core::jobs::job_system *mJobs = nullptr;    // optional; JPEG XL decodes inline without it

        public:
            texture_importer();
            explicit texture_importer(core::jobs::job_system *jobs);
            ~texture_importer() override;

        public:
            [[nodiscard]] std::string_view id() const noexcept override;
            [[nodiscard]] u32 version() const noexcept override;
            [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override;
            [[nodiscard]] core::json::document default_settings() const override;
            [[nodiscard]] core::result<assets::import_output> import(assets::import_context &context) const override;
    };

    /// <summary>
    /// The conjure.renderer.texture loader; the fallback is a 1x1 white texture.
    /// </summary>
    class CONJURE_API texture_loader final : public assets::typed_asset_loader<texture_asset, texture_asset> {
        public:
            texture_loader();
            ~texture_loader() override;

        protected:
            [[nodiscard]] core::result<texture_asset> load_cpu_typed(
              const assets::load_context &context,
              std::span<const std::byte> payloadBytes
            ) const override;

            [[nodiscard]] core::result<texture_asset> finalise_typed(
              const assets::finalise_context &context,
              texture_asset &&intermediate
            ) const override;

            [[nodiscard]] std::optional<texture_asset> create_fallback_typed() const override;
    };

    [[nodiscard]] CONJURE_API std::string_view to_string(texture_colour_space value) noexcept;
    [[nodiscard]] CONJURE_API std::string_view to_string(sampler_filter value) noexcept;
    [[nodiscard]] CONJURE_API std::string_view to_string(texture_wrap value) noexcept;
}
