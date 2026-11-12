#pragma once

#include "conjure/renderer/limits.h"

#include "conjure/core/api.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <mutex>
#include <span>
#include <type_traits>
#include <vector>

// Debug lines (docs/design/02_renderer.md 3.10). debug_draw_buffer collects line vertices into a
// depth-tested list and an overlay list; the renderer uploads both once per frame. debug_draw is the
// facade the engine owns: it keeps timed entries alive across frames, is guarded by a mutex because
// physics and scripting may call it from jobs, and appends everything to the write scene each frame.

namespace conjure::renderer {

    /// <summary>
    /// One line vertex as the GPU consumes it (gpu_debug_vertex): position and an RGBA8 colour.
    /// </summary>
    struct debug_vertex {
        math::vec3 position = math::vec3(0.0f);
        u32 colour = 0xFFFFFFFFu;               // RGBA8, red in the low byte

        [[nodiscard]] bool operator==(const debug_vertex &other) const noexcept = default;
    };

    static_assert(sizeof(debug_vertex) == 16, "debug_vertex mirrors gpu_debug_vertex");
    static_assert(std::is_trivially_copyable_v<debug_vertex>, "debug_vertex is uploaded verbatim");

    /// <summary>
    /// Packs a linear colour with components in [0, 1] into RGBA8 (red in the low byte).
    /// </summary>
    [[nodiscard]] CONJURE_API u32 pack_debug_colour(const math::vec4 &colour) noexcept;

    /// <summary>
    /// How a debug line meets the scene: hidden behind geometry (TESTED) or drawn on top (OVERLAY). A
    /// typed argument, so it can never be confused with the facade's duration in the same position.
    /// </summary>
    enum class debug_depth : u8 {
        TESTED,
        OVERLAY
    };

    /// <summary>
    /// Line primitives collected for one frame. Each shape call appends pairs of vertices to the
    /// depth-tested or the overlay list; the lists are capped at MAX_DEBUG_VERTICES each (excess is
    /// dropped and counted).
    /// </summary>
    class CONJURE_API debug_draw_buffer {
        private:
            std::vector<debug_vertex> mDepthTested = {};
            std::vector<debug_vertex> mOverlay = {};
            u32 mDropped = 0;

        public:
            debug_draw_buffer();
            ~debug_draw_buffer();

            debug_draw_buffer(const debug_draw_buffer &) = default;
            debug_draw_buffer &operator=(const debug_draw_buffer &) = default;
            debug_draw_buffer(debug_draw_buffer &&) noexcept = default;
            debug_draw_buffer &operator=(debug_draw_buffer &&) noexcept = default;

        public:
            /// <summary>
            /// One line from a to b.
            /// </summary>
            void line(
              const math::vec3 &a,
              const math::vec3 &b,
              const math::vec4 &colour,
              debug_depth depth = debug_depth::TESTED
            );

            /// <summary>
            /// The twelve edges of a box (24 vertices). An invalid box adds nothing.
            /// </summary>
            void box(const math::aabb &box, const math::vec4 &colour, debug_depth depth = debug_depth::TESTED);

            /// <summary>
            /// Three great circles of 'segments' lines each.
            /// </summary>
            void sphere(
              const math::vec3 &centre,
              f32 radius,
              const math::vec4 &colour,
              debug_depth depth = debug_depth::TESTED,
              u32 segments = 24
            );

            /// <summary>
            /// The twelve edges of the view volume of a clip-from-world matrix (24 vertices).
            /// </summary>
            void frustum(
              const math::mat4 &clipFromWorld,
              const math::vec4 &colour,
              debug_depth depth = debug_depth::TESTED
            );

            /// <summary>
            /// Three axis lines (red X, green Y, blue Z) of 'length' from the origin of a transform.
            /// </summary>
            void axes(const math::mat4 &worldFromLocal, f32 length = 1.0f, debug_depth depth = debug_depth::TESTED);

            /// <summary>
            /// A grid in the XZ plane centred on 'centre': (2 * halfLines + 1) lines along each axis,
            /// 'spacing' apart.
            /// </summary>
            void grid(
              const math::vec3 &centre,
              u32 halfLines,
              f32 spacing,
              const math::vec4 &colour,
              debug_depth depth = debug_depth::TESTED
            );

            /// <summary>
            /// Appends every vertex of another buffer and adds its dropped count to this one's.
            /// </summary>
            void append(const debug_draw_buffer &other);

            /// <summary>
            /// Empties both lists and resets the dropped counter; capacity is kept.
            /// </summary>
            void clear() noexcept;

            [[nodiscard]] std::span<const debug_vertex> depth_tested() const noexcept {
                return mDepthTested;
            }

            [[nodiscard]] std::span<const debug_vertex> overlay() const noexcept {
                return mOverlay;
            }

            /// <summary>
            /// Vertices refused because a list was at MAX_DEBUG_VERTICES.
            /// </summary>
            [[nodiscard]] u32 dropped_count() const noexcept {
                return mDropped;
            }

            [[nodiscard]] bool empty() const noexcept {
                return mDepthTested.empty() && mOverlay.empty();
            }

        private:
            void push_line(const math::vec3 &a, const math::vec3 &b, u32 packed, debug_depth depth);
    };

    /// <summary>
    /// The engine-owned facade: immediate shapes (drawn for one frame) and timed shapes (kept for
    /// durationSeconds). Thread-safe; append_to() and tick() run on the main thread each frame.
    /// Immediate shapes are written straight into the frame list under the mutex; only timed shapes
    /// build a buffer of their own (outside the lock).
    /// </summary>
    class CONJURE_API debug_draw {
        private:
            struct timed_entry {
                debug_draw_buffer shape = {};
                f32 remainingSeconds = 0.0f;
            };

            mutable std::mutex mMutex = {};
            debug_draw_buffer mImmediate = {};
            std::vector<timed_entry> mTimed = {};

        public:
            debug_draw();
            ~debug_draw();

            debug_draw(const debug_draw &) = delete;
            debug_draw &operator=(const debug_draw &) = delete;

        public:
            /// <summary>
            /// A line that lives this frame (durationSeconds 0) or for durationSeconds.
            /// </summary>
            void line(
              const math::vec3 &a,
              const math::vec3 &b,
              const math::vec4 &colour,
              f32 durationSeconds = 0.0f,
              debug_depth depth = debug_depth::TESTED
            );

            /// <summary>
            /// A box (debug_draw_buffer::box) that lives this frame (durationSeconds 0) or for durationSeconds.
            /// </summary>
            void box(
              const math::aabb &box,
              const math::vec4 &colour,
              f32 durationSeconds = 0.0f,
              debug_depth depth = debug_depth::TESTED
            );

            /// <summary>
            /// A sphere (debug_draw_buffer::sphere) that lives this frame (durationSeconds 0) or for
            /// durationSeconds.
            /// </summary>
            void sphere(
              const math::vec3 &centre,
              f32 radius,
              const math::vec4 &colour,
              f32 durationSeconds = 0.0f,
              debug_depth depth = debug_depth::TESTED
            );

            /// <summary>
            /// Axis lines (debug_draw_buffer::axes) that live this frame (durationSeconds 0) or for
            /// durationSeconds.
            /// </summary>
            void axes(
              const math::mat4 &worldFromLocal,
              f32 length = 1.0f,
              f32 durationSeconds = 0.0f,
              debug_depth depth = debug_depth::TESTED
            );

            /// <summary>
            /// Adds a prepared buffer as an immediate (durationSeconds 0) or timed entry.
            /// </summary>
            void submit(const debug_draw_buffer &shape, f32 durationSeconds = 0.0f);

            /// <summary>
            /// Ages the timed entries and drops the expired ones.
            /// </summary>
            void tick(f32 deltaSeconds);

            /// <summary>
            /// Appends the immediate shapes and every live timed shape to 'target' and clears the
            /// immediate list.
            /// </summary>
            void append_to(debug_draw_buffer &target);

            /// <summary>
            /// Number of timed entries still alive.
            /// </summary>
            [[nodiscard]] usize timed_entry_count() const;

            /// <summary>
            /// Drops every immediate and timed entry.
            /// </summary>
            void clear();

        private:
            void add_timed(debug_draw_buffer &&shape, f32 durationSeconds);
    };
}
