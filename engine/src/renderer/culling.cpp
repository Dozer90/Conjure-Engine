#include "renderer/culling.h"

#include "conjure/core/jobs/parallel_for.h"

#include <algorithm>

using namespace conjure;
using namespace conjure::renderer;

bool conjure::renderer::box_intersects(
  const math::aabb &box,
  const math::frustum &frustum,
  bool ignoreFarPlane
) noexcept {
    if (!box.is_valid()) {
        return false;
    }
    const math::vec3 centre = box.center();
    const math::vec3 extents = box.extents();
    for (usize index = 0; index < math::FRUSTUM_PLANE_COUNT; ++index) {
        if (ignoreFarPlane && index == static_cast<usize>(math::frustum_plane::FAR_PLANE)) {
            continue;
        }
        const math::plane &plane = frustum.planes[index];
        const f32 radius = extents.x * math::absolute(plane.normal.x) + extents.y * math::absolute(plane.normal.y) +
          extents.z * math::absolute(plane.normal.z);
        if (plane.signed_distance(centre) < -radius) {
            return false;
        }
    }
    return true;
}

namespace {

    /// <summary>
    /// The job-parallel path is taken only with a job system that has workers and above the threshold;
    /// both overloads decide this the same way so the serial one never builds a scratch.
    /// </summary>
    [[nodiscard]] bool culls_in_parallel(const core::jobs::job_system *jobs, usize count) noexcept {
        return jobs != nullptr && jobs->worker_count() > 0 && count > CULLING_PARALLEL_THRESHOLD;
    }

    void cull_serial(
      std::span<const mesh_instance> instances,
      const math::frustum &frustum,
      const cull_options &options,
      std::vector<u32> &out
    ) {
        const usize count = instances.size();
        for (usize index = 0; index < count; ++index) {
            if (instance_visible(instances[index], frustum, options)) {
                out.push_back(static_cast<u32>(index));
            }
        }
    }
}

bool conjure::renderer::instance_visible(
  const mesh_instance &instance,
  const math::frustum &frustum,
  const cull_options &options
) noexcept {
    if ((instance.layerMask & options.layerMask) == 0) {
        return false;
    }
    if (options.shadowCastersOnly && !instance.castsShadows) {
        return false;
    }
    return box_intersects(instance.worldBounds, frustum, options.ignoreFarPlane);
}

void conjure::renderer::cull_instances(
  std::span<const mesh_instance> instances,
  const math::frustum &frustum,
  const cull_options &options,
  core::jobs::job_system *jobs,
  cull_scratch &scratch,
  std::vector<u32> &out
) {
    out.clear();
    const usize count = instances.size();
    if (!culls_in_parallel(jobs, count)) {
        cull_serial(instances, frustum, options, out);
        return;
    }

    // One slice per chunk, filled by its own job, concatenated afterwards (no atomics). A chunk can
    // never hold more visible indices than it has instances, so the slices never overflow and the
    // scratch only grows when the instance count exceeds every count seen before.
    const usize chunkCount = core::jobs::range_count(count, CULLING_PARALLEL_GRAIN);
    if (scratch.chunkVisible.size() < chunkCount * CULLING_PARALLEL_GRAIN) {
        scratch.chunkVisible.resize(chunkCount * CULLING_PARALLEL_GRAIN);
    }
    if (scratch.chunkCounts.size() < chunkCount) {
        scratch.chunkCounts.resize(chunkCount);
    }
    u32 *const slices = scratch.chunkVisible.data();
    u32 *const counts = scratch.chunkCounts.data();
    const auto body = [&instances, &frustum, &options, slices, counts](usize begin, usize end) {
        const usize chunk = begin / CULLING_PARALLEL_GRAIN;
        u32 *slice = slices + chunk * CULLING_PARALLEL_GRAIN;
        u32 written = 0;
        for (usize index = begin; index < end; ++index) {
            if (instance_visible(instances[index], frustum, options)) {
                slice[written] = static_cast<u32>(index);
                ++written;
            }
        }
        counts[chunk] = written;
    };
    // parallel_for_range wraps the body in a std::function that captures one reference, which every
    // standard library stores without a heap allocation.
    core::jobs::parallel_for_range(*jobs, count, CULLING_PARALLEL_GRAIN, body);

    usize total = 0;
    for (usize chunk = 0; chunk < chunkCount; ++chunk) {
        total += counts[chunk];
    }
    out.reserve(total);
    for (usize chunk = 0; chunk < chunkCount; ++chunk) {
        const u32 *slice = slices + chunk * CULLING_PARALLEL_GRAIN;
        out.insert(out.end(), slice, slice + counts[chunk]);
    }
}

void conjure::renderer::cull_instances(
  std::span<const mesh_instance> instances,
  const math::frustum &frustum,
  const cull_options &options,
  core::jobs::job_system *jobs,
  std::vector<u32> &out
) {
    if (!culls_in_parallel(jobs, instances.size())) {
        // No scratch on the serial path: even an empty std::vector costs a heap block under the MSVC
        // Debug runtime (iterator-debugging proxies), which the Debug allocation gates see.
        out.clear();
        cull_serial(instances, frustum, options, out);
        return;
    }
    cull_scratch scratch = {};
    cull_instances(instances, frustum, options, jobs, scratch, out);
}
