#include "conjure/renderer/debug_draw.h"

#include <algorithm>
#include <cmath>
#include <utility>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    constexpr math::vec4 AXIS_X_COLOUR = math::vec4(1.0f, 0.1f, 0.1f, 1.0f);
    constexpr math::vec4 AXIS_Y_COLOUR = math::vec4(0.1f, 1.0f, 0.1f, 1.0f);
    constexpr math::vec4 AXIS_Z_COLOUR = math::vec4(0.1f, 0.3f, 1.0f, 1.0f);

    /// <summary>
    /// The pairs of corner indices (aabb::corners order) that make the twelve box edges.
    /// </summary>
    constexpr std::array<std::pair<u32, u32>, 12> BOX_EDGES = {{
      {0, 1}, {1, 3}, {3, 2}, {2, 0}, {4, 5}, {5, 7}, {7, 6}, {6, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7},
    }};

    /// <summary>
    /// The pairs of frustum::corners indices (near 0..3, far 4..7, each bottom-left, bottom-right,
    /// top-right, top-left) that make the twelve frustum edges.
    /// </summary>
    constexpr std::array<std::pair<u32, u32>, 12> FRUSTUM_EDGES = {{
      {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7},
    }};

    [[nodiscard]] u8 to_unorm8(f32 value) noexcept {
        return static_cast<u8>(std::lround(math::saturate(value) * 255.0f));
    }
}

u32 conjure::renderer::pack_debug_colour(const math::vec4 &colour) noexcept {
    return static_cast<u32>(to_unorm8(colour.x)) | (static_cast<u32>(to_unorm8(colour.y)) << 8) |
      (static_cast<u32>(to_unorm8(colour.z)) << 16) | (static_cast<u32>(to_unorm8(colour.w)) << 24);
}

/*===========================================================================*/
/*===========================================================================*/
// debug_draw_buffer

debug_draw_buffer::debug_draw_buffer() {}

debug_draw_buffer::~debug_draw_buffer() {}

void debug_draw_buffer::line(const math::vec3 &a, const math::vec3 &b, const math::vec4 &colour, debug_depth depth) {
    push_line(a, b, pack_debug_colour(colour), depth);
}

void debug_draw_buffer::box(const math::aabb &box, const math::vec4 &colour, debug_depth depth) {
    if (!box.is_valid()) {
        return;
    }
    const std::array<math::vec3, 8> corners = box.corners();
    const u32 packed = pack_debug_colour(colour);
    for (const auto &[first, second] : BOX_EDGES) {
        push_line(corners[first], corners[second], packed, depth);
    }
}

void debug_draw_buffer::sphere(
  const math::vec3 &centre,
  f32 radius,
  const math::vec4 &colour,
  debug_depth depth,
  u32 segments
) {
    const u32 steps = std::max<u32>(segments, 3);
    const u32 packed = pack_debug_colour(colour);
    const f32 stepAngle = math::TWO_PI / static_cast<f32>(steps);
    for (u32 index = 0; index < steps; ++index) {
        const f32 angle0 = stepAngle * static_cast<f32>(index);
        const f32 angle1 = stepAngle * static_cast<f32>(index + 1);
        const f32 c0 = std::cos(angle0) * radius;
        const f32 s0 = std::sin(angle0) * radius;
        const f32 c1 = std::cos(angle1) * radius;
        const f32 s1 = std::sin(angle1) * radius;
        push_line(centre + math::vec3(c0, s0, 0.0f), centre + math::vec3(c1, s1, 0.0f), packed, depth);
        push_line(centre + math::vec3(c0, 0.0f, s0), centre + math::vec3(c1, 0.0f, s1), packed, depth);
        push_line(centre + math::vec3(0.0f, c0, s0), centre + math::vec3(0.0f, c1, s1), packed, depth);
    }
}

void debug_draw_buffer::frustum(const math::mat4 &clipFromWorld, const math::vec4 &colour, debug_depth depth) {
    const std::array<math::vec3, 8> corners = math::frustum::corners(glm::inverse(clipFromWorld));
    const u32 packed = pack_debug_colour(colour);
    for (const auto &[first, second] : FRUSTUM_EDGES) {
        push_line(corners[first], corners[second], packed, depth);
    }
}

void debug_draw_buffer::axes(const math::mat4 &worldFromLocal, f32 length, debug_depth depth) {
    const math::vec3 origin = math::vec3(worldFromLocal[3]);
    const math::vec3 axisX = math::safe_normalize(math::vec3(worldFromLocal[0]), math::vec3(1.0f, 0.0f, 0.0f));
    const math::vec3 axisY = math::safe_normalize(math::vec3(worldFromLocal[1]), math::vec3(0.0f, 1.0f, 0.0f));
    const math::vec3 axisZ = math::safe_normalize(math::vec3(worldFromLocal[2]), math::vec3(0.0f, 0.0f, 1.0f));
    push_line(origin, origin + axisX * length, pack_debug_colour(AXIS_X_COLOUR), depth);
    push_line(origin, origin + axisY * length, pack_debug_colour(AXIS_Y_COLOUR), depth);
    push_line(origin, origin + axisZ * length, pack_debug_colour(AXIS_Z_COLOUR), depth);
}

void debug_draw_buffer::grid(
  const math::vec3 &centre,
  u32 halfLines,
  f32 spacing,
  const math::vec4 &colour,
  debug_depth depth
) {
    const u32 packed = pack_debug_colour(colour);
    const f32 extent = spacing * static_cast<f32>(halfLines);
    for (u32 index = 0; index <= halfLines * 2; ++index) {
        const f32 offset = spacing * static_cast<f32>(index) - extent;
        push_line(
          centre + math::vec3(offset, 0.0f, -extent),
          centre + math::vec3(offset, 0.0f, extent),
          packed,
          depth
        );
        push_line(
          centre + math::vec3(-extent, 0.0f, offset),
          centre + math::vec3(extent, 0.0f, offset),
          packed,
          depth
        );
    }
}

void debug_draw_buffer::append(const debug_draw_buffer &other) {
    for (usize index = 0; index + 1 < other.mDepthTested.size(); index += 2) {
        push_line(
          other.mDepthTested[index].position,
          other.mDepthTested[index + 1].position,
          other.mDepthTested[index].colour,
          debug_depth::TESTED
        );
    }
    for (usize index = 0; index + 1 < other.mOverlay.size(); index += 2) {
        push_line(
          other.mOverlay[index].position,
          other.mOverlay[index + 1].position,
          other.mOverlay[index].colour,
          debug_depth::OVERLAY
        );
    }
    // What the source already refused (the facade's immediate list) still counts for the frame.
    mDropped += other.mDropped;
}

void debug_draw_buffer::clear() noexcept {
    mDepthTested.clear();
    mOverlay.clear();
    mDropped = 0;
}

void debug_draw_buffer::push_line(const math::vec3 &a, const math::vec3 &b, u32 packed, debug_depth depth) {
    std::vector<debug_vertex> &list = depth == debug_depth::TESTED ? mDepthTested : mOverlay;
    if (list.size() + 2 > MAX_DEBUG_VERTICES) {
        mDropped += 2;
        return;
    }
    debug_vertex first = {};
    first.position = a;
    first.colour = packed;
    debug_vertex second = {};
    second.position = b;
    second.colour = packed;
    list.push_back(first);
    list.push_back(second);
}

/*===========================================================================*/
/*===========================================================================*/
// debug_draw

debug_draw::debug_draw() {}

debug_draw::~debug_draw() {}

void debug_draw::line(
  const math::vec3 &a,
  const math::vec3 &b,
  const math::vec4 &colour,
  f32 durationSeconds,
  debug_depth depth
) {
    // Immediate shapes go straight into the frame list, without a temporary buffer per call.
    if (durationSeconds <= 0.0f) {
        const std::scoped_lock lock(mMutex);
        mImmediate.line(a, b, colour, depth);
        return;
    }
    debug_draw_buffer shape = {};
    shape.line(a, b, colour, depth);
    add_timed(std::move(shape), durationSeconds);
}

void debug_draw::box(const math::aabb &box, const math::vec4 &colour, f32 durationSeconds, debug_depth depth) {
    if (durationSeconds <= 0.0f) {
        const std::scoped_lock lock(mMutex);
        mImmediate.box(box, colour, depth);
        return;
    }
    debug_draw_buffer shape = {};
    shape.box(box, colour, depth);
    add_timed(std::move(shape), durationSeconds);
}

void debug_draw::sphere(
  const math::vec3 &centre,
  f32 radius,
  const math::vec4 &colour,
  f32 durationSeconds,
  debug_depth depth
) {
    if (durationSeconds <= 0.0f) {
        const std::scoped_lock lock(mMutex);
        mImmediate.sphere(centre, radius, colour, depth);
        return;
    }
    debug_draw_buffer shape = {};
    shape.sphere(centre, radius, colour, depth);
    add_timed(std::move(shape), durationSeconds);
}

void debug_draw::axes(const math::mat4 &worldFromLocal, f32 length, f32 durationSeconds, debug_depth depth) {
    if (durationSeconds <= 0.0f) {
        const std::scoped_lock lock(mMutex);
        mImmediate.axes(worldFromLocal, length, depth);
        return;
    }
    debug_draw_buffer shape = {};
    shape.axes(worldFromLocal, length, depth);
    add_timed(std::move(shape), durationSeconds);
}

void debug_draw::submit(const debug_draw_buffer &shape, f32 durationSeconds) {
    if (durationSeconds <= 0.0f) {
        const std::scoped_lock lock(mMutex);
        mImmediate.append(shape);
        return;
    }
    add_timed(debug_draw_buffer(shape), durationSeconds);
}

void debug_draw::add_timed(debug_draw_buffer &&shape, f32 durationSeconds) {
    // The entry (and the copy of its vertices) is built before the lock is taken.
    timed_entry entry = {};
    entry.shape = std::move(shape);
    entry.remainingSeconds = durationSeconds;
    const std::scoped_lock lock(mMutex);
    mTimed.push_back(std::move(entry));
}

void debug_draw::tick(f32 deltaSeconds) {
    const std::scoped_lock lock(mMutex);
    for (timed_entry &entry : mTimed) {
        entry.remainingSeconds -= deltaSeconds;
    }
    std::erase_if(mTimed, [](const timed_entry &entry) {
        return entry.remainingSeconds <= 0.0f;
    });
}

void debug_draw::append_to(debug_draw_buffer &target) {
    const std::scoped_lock lock(mMutex);
    target.append(mImmediate);
    mImmediate.clear();
    for (const timed_entry &entry : mTimed) {
        target.append(entry.shape);
    }
}

usize debug_draw::timed_entry_count() const {
    const std::scoped_lock lock(mMutex);
    return mTimed.size();
}

void debug_draw::clear() {
    const std::scoped_lock lock(mMutex);
    mImmediate.clear();
    mTimed.clear();
}
