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
// system; each chunk appends to its own list and the lists are concatenated in order, so the result
// equals the serial one.

namespace conjure::renderer {

    inline constexpr u32 CULLING_PARALLEL_THRESHOLD = 2048;
    inline constexpr u32 CULLING_PARALLEL_GRAIN = 1024;

    struct cull_options {
        u32 layerMask = 0xFFFFFFFFu;
        bool ignoreFarPlane = false;        // cascades: five planes
        bool shadowCastersOnly = false;     // skip instances that do not cast shadows
    };

    /// <summary>
    /// Indices of the instances that pass the layer mask and intersect the frustum, in input order.
    /// 'out' is cleared first and keeps its capacity. Runs on jobs when a system is given and the
    /// instance count exceeds the threshold.
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
