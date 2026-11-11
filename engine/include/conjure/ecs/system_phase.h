#pragma once

#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <string_view>

// Frame phases (docs/design/01_ecs.md 4.7, 09_cross_area_resolutions.md R1/R2). The list is owned by
// the engine loop design (06_engine_loop.md 2.4); this header is owned by the ecs module.

namespace conjure::ecs {

    /// <summary>
    /// The phases of one engine frame, in execution order. The fixed-step phases run zero or more
    /// times per frame.
    /// </summary>
    enum class system_phase : u8 {
        FRAME_BEGIN,
        FIXED_UPDATE,
        PHYSICS_STEP,
        FIXED_END,
        UPDATE,
        LATE_UPDATE,
        TRANSFORM,
        SYNC,
        EXTRACT,
        FRAME_END,
        COUNT
    };

    /// <summary>
    /// Number of real phases (COUNT excluded).
    /// </summary>
    inline constexpr u32 SYSTEM_PHASE_COUNT = static_cast<u32>(system_phase::COUNT);

    /// <summary>
    /// The upper-case enumerator name of a phase ("UPDATE"); "COUNT" for COUNT and out-of-range values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(system_phase phase) noexcept;

    /// <summary>
    /// Per-frame timing filled by the engine before every scheduler::run_phase call (R2).
    /// </summary>
    struct frame_info {
        f32 deltaTime = 0.0f;              // seconds since the previous frame (or the fixed step inside one)
        f32 fixedDeltaTime = 0.0f;         // length of one fixed step in seconds
        f32 interpolationAlpha = 0.0f;     // position of the frame between the last two fixed steps
        f64 time = 0.0;                    // seconds since the engine started
        u64 frameIndex = 0;                // frames rendered so far
        bool inFixedStep = false;          // true inside FIXED_UPDATE / PHYSICS_STEP / FIXED_END
    };
}
