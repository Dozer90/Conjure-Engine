#include "renderer/culling.h"

#include "conjure/core/containers/small_vector.h"
#include "conjure/core/jobs/parallel_for.h"

#include <algorithm>
#include <functional>

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
  std::vector<u32> &out
) {
    out.clear();
    const usize count = instances.size();
    if (count == 0) {
        return;
    }
    const bool parallel = jobs != nullptr && jobs->worker_count() > 0 && count > CULLING_PARALLEL_THRESHOLD;
    if (!parallel) {
        for (usize index = 0; index < count; ++index) {
            if (instance_visible(instances[index], frustum, options)) {
                out.push_back(static_cast<u32>(index));
            }
        }
        return;
    }

    // One list per chunk, filled by its own job, concatenated afterwards (no atomics).
    const usize chunkCount = core::jobs::range_count(count, CULLING_PARALLEL_GRAIN);
    std::vector<core::small_vector<u32, 64>> chunks(chunkCount);
    const std::function<void(usize, usize)> body = [&](usize begin, usize end) {
        core::small_vector<u32, 64> &list = chunks[begin / CULLING_PARALLEL_GRAIN];
        for (usize index = begin; index < end; ++index) {
            if (instance_visible(instances[index], frustum, options)) {
                list.push_back(static_cast<u32>(index));
            }
        }
    };
    jobs->parallel_for(count, CULLING_PARALLEL_GRAIN, body);
    usize total = 0;
    for (const core::small_vector<u32, 64> &list : chunks) {
        total += list.size();
    }
    out.reserve(total);
    for (const core::small_vector<u32, 64> &list : chunks) {
        out.insert(out.end(), list.begin(), list.end());
    }
}
