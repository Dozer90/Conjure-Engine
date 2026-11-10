#pragma once

#include "conjure/renderer/environment_asset.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <vector>

// The CPU image-based-lighting bake (docs/design/02_renderer.md 3.6, R34). Everything here works
// on float cubemaps; environment_asset.cpp packs the result to halves. Faces are independent, so the
// face loops run through job_system::parallel_for when a job system is given and inline otherwise,
// with bit-identical output.

namespace conjure::renderer::detail {

    /// <summary>
    /// A float RGB cubemap with a mip chain, mip-major then face-major, rows top to bottom.
    /// </summary>
    struct cubemap_f32 {
        u32 size = 0;
        u32 mipCount = 0;
        std::vector<math::vec3> texels = {};

        [[nodiscard]] u32 mip_size(u32 mip) const noexcept;
        [[nodiscard]] usize face_offset(u32 mip, u32 face) const noexcept;
        [[nodiscard]] const math::vec3 &at(u32 mip, u32 face, u32 x, u32 y) const noexcept;
        [[nodiscard]] math::vec3 &at(u32 mip, u32 face, u32 x, u32 y) noexcept;

        /// <summary>
        /// Allocates a zeroed chain of 'mips' levels for the given edge.
        /// </summary>
        [[nodiscard]] static cubemap_f32 allocate(u32 edge, u32 mips);
    };

    /// <summary>
    /// Face index and [0, 1] face coordinates of a direction (the inverse of cubemap_texel_direction).
    /// </summary>
    void direction_to_face(const math::vec3 &direction, u32 &face, f32 &s, f32 &t) noexcept;

    /// <summary>
    /// Bilinear lookup inside the face a direction hits (no seam filtering), at one mip.
    /// </summary>
    [[nodiscard]] math::vec3 sample_cubemap(const cubemap_f32 &cubemap, u32 mip, const math::vec3 &direction) noexcept;

    /// <summary>
    /// Trilinear lookup between the two mips around 'lod' (clamped to the chain).
    /// </summary>
    [[nodiscard]] math::vec3 sample_cubemap_lod(
      const cubemap_f32 &cubemap,
      f32 lod,
      const math::vec3 &direction
    ) noexcept;

    /// <summary>
    /// Mip 0 of a cubemap from an equirect image (bilinear lookup per texel), faces on jobs.
    /// </summary>
    [[nodiscard]] cubemap_f32 equirect_to_cubemap(
      const equirect_image &source,
      u32 size,
      u32 mipCount,
      core::jobs::job_system *jobs
    );

    /// <summary>
    /// Fills mips 1.. of a cubemap with a 2x2 box filter of the previous level, faces on jobs.
    /// </summary>
    void build_mip_chain(cubemap_f32 &cubemap, core::jobs::job_system *jobs);

    /// <summary>
    /// Projects one mip of a cubemap onto the nine SH basis functions with solid-angle weights.
    /// </summary>
    [[nodiscard]] sh9 project_sh9(const cubemap_f32 &cubemap, u32 mip);

    /// <summary>
    /// GGX prefiltered chain: mip m has roughness m / (mipCount - 1); importance sampling with a
    /// Hammersley sequence of 'sampleCount' points and PDF-based source-mip selection, faces on jobs.
    /// </summary>
    [[nodiscard]] cubemap_f32 prefilter_ggx(
      const cubemap_f32 &sky,
      u32 size,
      u32 mipCount,
      u32 sampleCount,
      core::jobs::job_system *jobs
    );

    /// <summary>
    /// Packs a float chain into RGBA16F halves (alpha 1).
    /// </summary>
    [[nodiscard]] cubemap_data pack_cubemap(const cubemap_f32 &cubemap);

    /// <summary>
    /// The i-th point of the Hammersley sequence of 'count' points.
    /// </summary>
    [[nodiscard]] math::vec2 hammersley(u32 index, u32 count) noexcept;

    /// <summary>
    /// Number of mips a chain of edge 'size' has, capped at ENVIRONMENT_MIP_COUNT.
    /// </summary>
    [[nodiscard]] u32 environment_mip_count(u32 size) noexcept;
}
