#pragma once

#include "conjure/core/enum_flags.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/types.h"

#include <string_view>

// Debug drawing sink (docs/design/14_physics.md P17, 3.10, 4.4). The engine implements it over the
// renderer's debug draw, tests with counters; physics_world::debug_draw forwards Jolt's lines, triangles
// and labels into it. Colours are 0xAABBGGRR.

namespace conjure::physics {

    /// <summary>
    /// What debug_draw renders.
    /// </summary>
    enum class physics_debug_draw_flags : u16 {
        NONE = 0,
        SHAPES = 1 << 0,
        WIREFRAME = 1 << 1,
        BOUNDING_BOXES = 1 << 2,
        CENTER_OF_MASS = 1 << 3,
        VELOCITIES = 1 << 4,
        SLEEP_STATE = 1 << 5,
        CHARACTERS = 1 << 6,
        CONTACTS = 1 << 7
    };
    CONJURE_ENUM_FLAGS(physics_debug_draw_flags);

    /// <summary>
    /// Receives the primitives of physics_world::debug_draw.
    /// </summary>
    class physics_debug_draw {
        public:
            physics_debug_draw();
            virtual ~physics_debug_draw();

            physics_debug_draw(const physics_debug_draw &) = delete;
            physics_debug_draw &operator=(const physics_debug_draw &) = delete;

        public:
            /// <summary>
            /// A world-space line segment.
            /// </summary>
            virtual void line(const math::vec3 &from, const math::vec3 &to, u32 rgba) = 0;

            /// <summary>
            /// A world-space filled triangle.
            /// </summary>
            virtual void triangle(const math::vec3 &a, const math::vec3 &b, const math::vec3 &c, u32 rgba) = 0;

            /// <summary>
            /// A world-space label of the given height in metres. The default implementation draws nothing.
            /// </summary>
            virtual void text(const math::vec3 &position, std::string_view text, u32 rgba, f32 height);
    };
}
