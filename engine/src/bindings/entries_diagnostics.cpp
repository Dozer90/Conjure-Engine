#include "bindings/entry_support.h"

#include "conjure/core/math/aabb.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/platform_detect.h"
#include "conjure/engine/diagnostics/check_registry.h"
#include "conjure/engine/engine_time.h"
#include "conjure/engine/frame_stats.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/renderer/debug_draw.h"
#endif

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>

// diagnostics_* (docs/design/15_bindings.md 2.3, B14, 3.5): self-verifying checks into the engine's
// check_registry, the last-error protocol of NativeCheck.ThrowIfFailed, debug drawing through the engine's
// renderer::debug_draw (a silent success when none is published), the latest frame_stats sample and the call
// counters.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::bindings::detail;

namespace {

    [[nodiscard]] bool finite_seconds(native_function_id id, float seconds, int32_t &status) noexcept {
        if (!std::isfinite(seconds) || seconds < 0.0f) {
            status = fail_entry(id, bindings_error::INVALID_ARGUMENT, "seconds must be finite and >= 0");
            return false;
        }
        return true;
    }

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    [[nodiscard]] renderer::debug_draw *drawer() noexcept {
        return active_context().debugDraw;
    }
#endif
}

int32_t conjure::bindings::entries::diagnostics_check(
  const char *utf8Name,
  int32_t nameLength,
  int32_t passed,
  const char *utf8Detail,
  int32_t detailLength
) {
    constexpr native_function_id ID = native_function_id::diagnostics_check;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    const core::result<std::string_view> name = marshal::read_utf8(utf8Name, nameLength);
    if (!name.has_value()) {
        return fail_entry(ID, name.get_error());
    }
    const core::result<std::string_view> detail = marshal::read_utf8(utf8Detail, detailLength);
    if (!detail.has_value()) {
        return fail_entry(ID, detail.get_error());
    }
    if (name->empty()) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "a check needs a name");
    }
    if (active_context().checks == nullptr) {
        return unavailable(ID, "check registry");
    }
    const u64 frame = active_context().time != nullptr ? active_context().time->frameIndex : 0;
    active_context().checks->report(*name, passed != 0, *detail, frame);
    return 0;
}

int32_t conjure::bindings::entries::diagnostics_last_error(int32_t *outStatus, char *utf8, int32_t capacity) {
    constexpr native_function_id ID = native_function_id::diagnostics_last_error;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (capacity < 0) {
        // Reported without touching the last error: the caller is asking about a previous failure.
        return to_status(bindings_error::INVALID_ARGUMENT);
    }
    if (outStatus != nullptr) {
        *outStatus = to_status(last_error_code());
    }
    return marshal::write_utf8(last_error_message(), utf8, capacity);
}

int32_t conjure::bindings::entries::diagnostics_draw_line(
  const conjure_vec3 *a,
  const conjure_vec3 *b,
  uint32_t rgba,
  float seconds
) {
    constexpr native_function_id ID = native_function_id::diagnostics_draw_line;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (a == nullptr || b == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "a / b");
    }
    int32_t status = 0;
    if (!finite_seconds(ID, seconds, status)) {
        return status;
    }
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    if (drawer() != nullptr) {
        drawer()->line(marshal::to_vec3(*a), marshal::to_vec3(*b), marshal::rgba_to_color(rgba), seconds);
    }
#else
    CONJURE_UNUSED(rgba);
#endif
    return 0;
}

int32_t conjure::bindings::entries::diagnostics_draw_box(
  const conjure_vec3 *center,
  const conjure_vec3 *halfExtents,
  const conjure_quat_xyzw *rotation,
  uint32_t rgba,
  float seconds
) {
    constexpr native_function_id ID = native_function_id::diagnostics_draw_box;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (center == nullptr || halfExtents == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "center / halfExtents");
    }
    int32_t status = 0;
    if (!finite_seconds(ID, seconds, status)) {
        return status;
    }
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    if (drawer() == nullptr) {
        return 0;
    }
    // Twelve edges of the oriented box: the renderer's aabb box has no rotation, so the corners are rotated here.
    const math::vec3 origin = marshal::to_vec3(*center);
    const math::vec3 extents = marshal::to_vec3(*halfExtents);
    const math::quat orientation =
      rotation == nullptr ? math::quat(1.0f, 0.0f, 0.0f, 0.0f) : marshal::to_quat(*rotation);
    std::array<math::vec3, 8> corners = {};
    for (usize index = 0; index < corners.size(); ++index) {
        const math::vec3 sign(
          (index & 1u) != 0 ? 1.0f : -1.0f,
          (index & 2u) != 0 ? 1.0f : -1.0f,
          (index & 4u) != 0 ? 1.0f : -1.0f
        );
        corners[index] = origin + orientation * (sign * extents);
    }
    constexpr std::array<std::array<u8, 2>, 12> EDGES = {{
      {{0, 1}}, {{2, 3}}, {{4, 5}}, {{6, 7}},
      {{0, 2}}, {{1, 3}}, {{4, 6}}, {{5, 7}},
      {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}},
    }};
    // One scratch buffer for every call: clear() keeps its capacity, so a box a frame stops allocating here. A
    // static is safe because the guard above keeps every entry on the main thread.
    static renderer::debug_draw_buffer sBoxScratch = {};
    sBoxScratch.clear();
    const math::vec4 colour = marshal::rgba_to_color(rgba);
    for (const std::array<u8, 2> &edge : EDGES) {
        sBoxScratch.line(corners[edge[0]], corners[edge[1]], colour);
    }
    drawer()->submit(sBoxScratch, seconds);
#else
    CONJURE_UNUSED(rotation);
    CONJURE_UNUSED(rgba);
#endif
    return 0;
}

int32_t conjure::bindings::entries::diagnostics_draw_sphere(
  const conjure_vec3 *center,
  float radius,
  uint32_t rgba,
  float seconds
) {
    constexpr native_function_id ID = native_function_id::diagnostics_draw_sphere;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (center == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "center");
    }
    if (!std::isfinite(radius) || radius < 0.0f) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "radius must be finite and >= 0");
    }
    int32_t status = 0;
    if (!finite_seconds(ID, seconds, status)) {
        return status;
    }
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    if (drawer() != nullptr) {
        drawer()->sphere(marshal::to_vec3(*center), radius, marshal::rgba_to_color(rgba), seconds);
    }
#else
    CONJURE_UNUSED(rgba);
#endif
    return 0;
}

int32_t conjure::bindings::entries::diagnostics_draw_text(
  const conjure_vec3 *position,
  const char *utf8,
  int32_t length,
  uint32_t rgba,
  float seconds
) {
    constexpr native_function_id ID = native_function_id::diagnostics_draw_text;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (position == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "position");
    }
    const core::result<std::string_view> text = marshal::read_utf8(utf8, length);
    if (!text.has_value()) {
        return fail_entry(ID, text.get_error());
    }
    int32_t status = 0;
    if (!finite_seconds(ID, seconds, status)) {
        return status;
    }
    // World-space debug text has no renderer in v1 (15_bindings.md 11): the call is validated and counted
    // so feature_test's DebugDrawProbe reaches it, and it draws nothing.
    CONJURE_UNUSED(rgba);
    return 0;
}

int32_t conjure::bindings::entries::diagnostics_frame_stats(conjure_frame_stats_block *out) {
    constexpr native_function_id ID = native_function_id::diagnostics_frame_stats;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (out == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "out");
    }
    if (active_context().stats == nullptr) {
        return unavailable(ID, "frame stats");
    }
    const engine::frame_sample &sample = active_context().stats->latest();
    *out = conjure_frame_stats_block{};
    out->frame_ms = sample.frameMilliseconds;
    static_assert(ecs::SYSTEM_PHASE_COUNT == 10, "conjure_frame_stats_block::phase_ms holds one entry per phase");
    for (usize phase = 0; phase < ecs::SYSTEM_PHASE_COUNT; ++phase) {
        out->phase_ms[phase] = sample.phaseMilliseconds[phase];
    }
    out->submit_ms = sample.submitMilliseconds;
    out->fixed_steps = sample.fixedSteps;
    out->jobs_executed = sample.jobsExecuted;
    out->frame_memory_high_water = static_cast<uint64_t>(sample.frameMemoryHighWater);
    out->managed_allocated_bytes = sample.managedAllocatedBytes;
    out->managed_heap_bytes = sample.managedHeapBytes;
    out->managed_collections = sample.managedCollections;
    out->managed_full_collections = sample.managedFullCollections;
    return 0;
}

int32_t conjure::bindings::entries::diagnostics_call_count(int32_t functionIndex) {
    constexpr native_function_id ID = native_function_id::diagnostics_call_count;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (functionIndex < 0 || static_cast<u32>(functionIndex) >= NATIVE_FUNCTION_COUNT) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "functionIndex");
    }
#if CONJURE_DISTRIBUTION
    return fail_entry(ID, bindings_error::NOT_SUPPORTED, "call counters are compiled out of Distribution");
#else
    const u32 count = call_count(static_cast<native_function_id>(functionIndex));
    return static_cast<int32_t>(std::min<u32>(count, static_cast<u32>(INT32_MAX)));
#endif
}
