#pragma once

#include "conjure/renderer/limits.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_loader.h"
#include "conjure/assets/asset_types.h"
#include "conjure/core/api.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/math.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Image-based lighting environments (docs/design/02_renderer.md R13, 3.6, R34). The bake runs on
// the CPU inside the importer: equirect .hdr -> sky cubemap with a box-filtered mip chain -> SH9
// irradiance -> GGX prefiltered specular cubemap. It is deterministic (fixed Hammersley sequence,
// fixed evaluation order, bit-identical whether faces are baked inline or on jobs) because the
// importer output is a cache key. The cooked .cenv holds both cubemaps as RGBA16F plus the SH9.

namespace conjure::renderer {

    inline constexpr std::string_view ENVIRONMENT_ASSET_TYPE_NAME = "conjure.renderer.environment";
    inline constexpr std::string_view ENVIRONMENT_IMPORTER_ID = "conjure.renderer.environment";
    // 2: the cubemaps clamped to the half range. 3: the samples made finite and non-negative before the bake (NaN
    // and negative as 0, above MAX_ENVIRONMENT_RADIANCE as that), which changes the bake of such a source.
    inline constexpr u32 ENVIRONMENT_IMPORTER_VERSION = 3;
    inline constexpr u32 ENVIRONMENT_BLOB_VERSION = 1;
    inline constexpr std::array<std::string_view, 1> ENVIRONMENT_EXTENSIONS = {".hdr"};

    /// <summary>
    /// The brightest radiance the bake integrates (about 5.2e33): equirect_image::decode_hdr and
    /// equirect_image::sample read +inf and anything brighter as this value. It is the largest float divided by
    /// 2^16, so the bake's largest sum, the GGX prefilter's over at most 4096 samples, stays 16 times below the
    /// float limit; the 4-texel mip averages and the SH9 coefficients (at most 4 pi x 0.63 times this) stay further
    /// below. Real radiance is many orders of magnitude darker (the sun's disc is about 1.6e9 cd/m^2), so a bright
    /// sun reaches the SH9 irradiance and the prefiltered mips unclipped; only the stored cubemap texels are
    /// clamped, to the half range (bake_environment).
    /// </summary>
    inline constexpr f32 MAX_ENVIRONMENT_RADIANCE = std::numeric_limits<f32>::max() / 65536.0f;

    /// <summary>
    /// The grey radiance of both fallback environments: the loader's and the renderer's.
    /// </summary>
    inline constexpr f32 FALLBACK_ENVIRONMENT_RADIANCE = 0.25f;

    /// <summary>
    /// Nine spherical harmonics coefficients (bands 0..2) of RGB irradiance.
    /// </summary>
    using sh9 = std::array<math::vec3, 9>;

    /// <summary>
    /// A cubemap with a mip chain, RGBA16F texels stored as packed halves. Faces follow the +X, -X,
    /// +Y, -Y, +Z, -Z convention; texels are stored mip-major then face-major, rows top to bottom.
    /// </summary>
    struct CONJURE_API cubemap_data {
        u32 size = 0;                                   // edge of mip 0
        u32 mipCount = 0;
        std::vector<u16> texels = {};                   // 4 halves per texel

        /// <summary>
        /// Edge of a mip (at least 1).
        /// </summary>
        [[nodiscard]] u32 mip_size(u32 mip) const noexcept;

        /// <summary>
        /// Index of the first half of a face of a mip inside 'texels'.
        /// </summary>
        [[nodiscard]] usize face_offset(u32 mip, u32 face) const noexcept;

        /// <summary>
        /// Number of halves one face of a mip occupies.
        /// </summary>
        [[nodiscard]] usize face_length(u32 mip) const noexcept;

        /// <summary>
        /// The RGB radiance of a texel.
        /// </summary>
        [[nodiscard]] math::vec3 texel(u32 mip, u32 face, u32 x, u32 y) const noexcept;

        /// <summary>
        /// True when 'texels' has exactly the length the size and mip count imply.
        /// </summary>
        [[nodiscard]] bool is_consistent() const noexcept;

        [[nodiscard]] bool operator==(const cubemap_data &other) const noexcept = default;
    };

    /// <summary>
    /// A linear RGB equirectangular image (the decoded .hdr or JPEG XL).
    /// </summary>
    struct CONJURE_API equirect_image {
        u32 width = 0;
        u32 height = 0;
        std::vector<f32> rgb = {};                      // 3 floats per pixel, rows top to bottom

        /// <summary>
        /// Decodes a Radiance .hdr file (stb_image) or a JPEG XL file (libjxl), recognised by its signature.
        /// A JPEG XL file of any sample type, float included, is converted from its colour encoding (Display P3,
        /// an ICC profile, ...) to linear light with sRGB primaries, keeping values above 1, its premultiplied
        /// alpha divided out, and its first frame is used; 'jobs' (optional) decodes it on the workers. PQ and
        /// HLG files are display light in nits and come out with 1.0 at 203 nits, the reference white of
        /// ITU-R BT.2408, where a .hdr or float file keeps its 1.0 (HLG through its system gamma for a display of
        /// the file's intensity target, 1000 nits unless stated). Every sample is then made one the bake can sum:
        /// NaN and negative values become 0 and anything above MAX_ENVIRONMENT_RADIANCE, +inf included, becomes
        /// that value; finite HDR radiance below it is kept as decoded, however bright. Fails with
        /// renderer_error::DECODE_FAILED when the decoder rejects the data and INVALID_ARGUMENT for empty data, a
        /// size above texture_asset::MAX_DIMENSION or a JPEG XL file over one of its decoding limits
        /// (texture_asset::decode).
        /// </summary>
        [[nodiscard]] static core::result<equirect_image> decode_hdr(
          std::span<const std::byte> encoded,
          core::jobs::job_system *jobs = nullptr
        );

        /// <summary>
        /// An image of one colour.
        /// </summary>
        [[nodiscard]] static equirect_image constant(u32 width, u32 height, const math::vec3 &colour);

        /// <summary>
        /// Bilinear lookup in a direction (y up; the image seam is at -Z). Each texel is read as decode_hdr leaves
        /// it (NaN and negative values as 0, anything above MAX_ENVIRONMENT_RADIANCE as that value), so an image
        /// filled by hand bakes to finite values too.
        /// </summary>
        [[nodiscard]] math::vec3 sample(const math::vec3 &direction) const noexcept;

        [[nodiscard]] bool is_valid() const noexcept {
            return width > 0 && height > 0 && rgb.size() == static_cast<usize>(width) * height * 3;
        }
    };

    /// <summary>
    /// Bake sizes; the defaults are the design's (R13). Tests use small sizes.
    /// </summary>
    struct CONJURE_API environment_bake_options {
        u32 skySize = 512;                              // power of two, >= 8
        u32 prefilteredSize = 128;                      // power of two, >= 8
        u32 sampleCount = 128;                          // GGX importance samples per texel

        [[nodiscard]] static core::result<environment_bake_options> from_json(const core::json::value &source);
        void to_json(core::json::writer &out) const;
        [[nodiscard]] core::json::document to_document() const;

        [[nodiscard]] bool operator==(const environment_bake_options &other) const noexcept = default;
    };

    class CONJURE_API environment_asset {
        private:
            cubemap_data mSky = {};
            cubemap_data mPrefiltered = {};
            sh9 mIrradiance = {};
            std::string mName = {};

        public:
            environment_asset();
            environment_asset(cubemap_data sky, cubemap_data prefiltered, const sh9 &irradiance, std::string name = {});
            ~environment_asset();

            environment_asset(const environment_asset &) = default;
            environment_asset &operator=(const environment_asset &) = default;
            environment_asset(environment_asset &&) noexcept = default;
            environment_asset &operator=(environment_asset &&) noexcept = default;

        public:
            /// <summary>
            /// Parses a .cenv blob. Fails with renderer_error::INVALID_ASSET naming the problem, including
            /// a cubemap whose mip count exceeds the chain of its size (bit_width(size) levels).
            /// </summary>
            [[nodiscard]] static core::result<environment_asset> parse_blob(std::span<const std::byte> bytes);

            [[nodiscard]] std::vector<std::byte> write_blob() const;

            /// <summary>
            /// A uniform grey environment (the fallback): constant sky and prefiltered chains of the
            /// given radiance, SH band 0 matching, tiny cubemaps.
            /// </summary>
            [[nodiscard]] static environment_asset uniform(const math::vec3 &radiance, u32 size = 8);

            [[nodiscard]] const cubemap_data &sky() const noexcept {
                return mSky;
            }

            [[nodiscard]] const cubemap_data &prefiltered() const noexcept {
                return mPrefiltered;
            }

            [[nodiscard]] const sh9 &irradiance() const noexcept {
                return mIrradiance;
            }

            [[nodiscard]] std::string_view name() const noexcept {
                return mName;
            }

            void set_name(std::string name) {
                mName = std::move(name);
            }

            /// <summary>
            /// Bytes of both cubemaps together.
            /// </summary>
            [[nodiscard]] u64 gpu_byte_size() const noexcept;

            [[nodiscard]] bool operator==(const environment_asset &other) const noexcept;
    };

    /// <summary>
    /// Bakes an environment on the CPU: cubemap, mips, SH9, GGX prefilter. Faces are baked through
    /// jobs->parallel_for when a job system is given and inline otherwise; the output is bit-identical
    /// either way. The mip chain, the SH9 projection and the prefilter run on 32-bit floats of the image's
    /// radiance as equirect_image::sample reads it, so a sun far brighter than a half holds still lights the
    /// irradiance and the rough mips fully; only the stored texels of both cubemaps are clamped to the finite
    /// half range [0, 65504] (NaN reads as 0), so a bright sun never packs to infinity. Fails with
    /// INVALID_ARGUMENT for an invalid image or options (sizes not powers of two or below 8, zero samples).
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<environment_asset> bake_environment(
      const equirect_image &source,
      const environment_bake_options &options,
      core::jobs::job_system *jobs
    );

    /// <summary>
    /// Evaluates SH9 irradiance (already convolved with the cosine lobe) in a direction.
    /// </summary>
    [[nodiscard]] CONJURE_API math::vec3 evaluate_sh9_irradiance(
      const sh9 &coefficients,
      const math::vec3 &normal
    ) noexcept;

    /// <summary>
    /// The direction through the centre of a cubemap texel, for a face index in the +X..-Z order
    /// and texel coordinates in [0, size).
    /// </summary>
    [[nodiscard]] CONJURE_API math::vec3 cubemap_texel_direction(u32 face, u32 x, u32 y, u32 size) noexcept;

}

CONJURE_DECLARE_ASSET_TYPE(conjure::renderer::environment_asset, "conjure.renderer.environment");

namespace conjure::renderer {

    /// <summary>
    /// The .hdr importer: decodes the equirect image and bakes it (settings: environment_bake_options). A JPEG XL
    /// equirect (float, PQ or HLG; equirect_image::decode_hdr) bakes too when its .meta names this importer; the .jxl
    /// extension itself belongs to the texture importer.
    /// </summary>
    class CONJURE_API environment_importer final : public assets::asset_importer {
        private:
            core::jobs::job_system *mJobs = nullptr;    // optional; faces bake inline without it

        public:
            explicit environment_importer(core::jobs::job_system *jobs = nullptr);
            ~environment_importer() override;

        public:
            [[nodiscard]] std::string_view id() const noexcept override;
            [[nodiscard]] u32 version() const noexcept override;
            [[nodiscard]] std::span<const std::string_view> extensions() const noexcept override;
            [[nodiscard]] core::json::document default_settings() const override;
            [[nodiscard]] core::result<assets::import_output> import(assets::import_context &context) const override;
    };

    /// <summary>
    /// The conjure.renderer.environment loader; the fallback is the uniform grey environment of
    /// FALLBACK_ENVIRONMENT_RADIANCE, the same grey the renderer lights a scene without an environment by.
    /// </summary>
    class CONJURE_API environment_loader final :
        public assets::typed_asset_loader<environment_asset, environment_asset> {
        public:
            environment_loader();
            ~environment_loader() override;

        protected:
            [[nodiscard]] core::result<environment_asset> load_cpu_typed(
              const assets::load_context &context,
              std::span<const std::byte> payloadBytes
            ) const override;

            [[nodiscard]] core::result<environment_asset> finalise_typed(
              const assets::finalise_context &context,
              environment_asset &&intermediate
            ) const override;

            [[nodiscard]] std::optional<environment_asset> create_fallback_typed() const override;
    };
}
