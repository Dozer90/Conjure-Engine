#pragma once

#include "conjure/renderer/render_scene.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <span>
#include <vector>

// Frustum culling (docs/design/02_renderer.md 3.2): plane-vs-AABB with centre / extents, layer masks
// tested first, five planes (no far plane) for cascades so casters behind the camera still cast.
// Above PARALLEL_THRESHOLD instances the work is split into chunks of PARALLEL_GRAIN over the job
// system; each chunk appends to its own slice of a caller-owned scratch buffer and the slices are
// concatenated in order, so the result equals the serial one and a warm scratch never allocates.

namespace conjure::renderer {

    inline constexpr u32 CULLING_PARALLEL_THRESHOLD = 2048;
    inline constexpr u32 CULLING_PARALLEL_GRAIN = 1024;

    struct cull_options {
        u32 layerMask = 0xFFFFFFFFu;
        bool ignoreFarPlane = false;        // cascades: five planes
        bool shadowCastersOnly = false;     // skip instances that do not cast shadows
    };

    /// <summary>
    /// Per-caller scratch of the job-parallel path: one slice of CULLING_PARALLEL_GRAIN indices per
    /// chunk plus the count each chunk filled. Sized on first use for the instance count seen and
    /// kept across frames, so the renderer's steady-state culls allocate nothing (02_renderer.md
    /// "steady-state frames do not allocate").
    /// </summary>
    struct cull_scratch {
        std::vector<u32> chunkVisible = {};     // chunkCount * CULLING_PARALLEL_GRAIN slots
        std::vector<u32> chunkCounts = {};      // visible indices written per chunk
    };

    /// <summary>
    /// Indices of the instances that pass the layer mask and intersect the frustum, in input order.
    /// 'out' is cleared first and keeps its capacity. Runs on jobs when a system is given and the
    /// instance count exceeds the threshold; the parallel path uses 'scratch', which grows to the
    /// instance count once and is reused afterwards.
    /// </summary>
    void cull_instances(
      std::span<const mesh_instance> instances,
      const math::frustum &frustum,
      const cull_options &options,
      core::jobs::job_system *jobs,
      cull_scratch &scratch,
      std::vector<u32> &out
    );

    /// <summary>
    /// The same cull with a scratch of its own. The serial path (no job system, no workers, or a
    /// count at or below CULLING_PARALLEL_THRESHOLD) touches nothing but 'out'; the parallel path
    /// allocates its scratch per call, so hot paths pass a persistent cull_scratch instead.
    /// </summary>
    void cull_instances(
      std::span<const mesh_instance> instances,
      const math::frustum &frustum,
      const cull_options &options,
      core::jobs::job_system *jobs,
      std::vector<u32> &out
    );

    /// <summary>
    /// The test one instance undergoes: layer mask, caster filter and the plane test.
    /// </summary>
    [[nodiscard]] bool instance_visible(
      const mesh_instance &instance,
      const math::frustum &frustum,
      const cull_options &options
    ) noexcept;

    /// <summary>
    /// True when the box is not entirely behind any of the tested planes (p-vertex test).
    /// </summary>
    [[nodiscard]] bool box_intersects(
      const math::aabb &box,
      const math::frustum &frustum,
      bool ignoreFarPlane
    ) noexcept;
}
