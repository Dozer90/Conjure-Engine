#pragma once

#include "conjure/engine/engine_config.h"

#include "conjure/core/api.h"
#include "conjure/core/types.h"
#include "conjure/ecs/system_phase.h"

// Frame time (docs/design/06_engine_loop.md 2.1). Written only by the main thread between phases and
// read from any thread during a phase. Frame 0 uses unscaledDeltaTime = fixedDeltaTime so the first frame
// is deterministic and runs exactly one fixed step; a time scale of zero yields no fixed steps and a
// deltaTime of zero while UPDATE still runs.

namespace conjure::engine {

    struct engine_time {
        f64 time = 0.0;                     // scaled seconds since the first frame
        f64 unscaledTime = 0.0;             // real seconds since the first frame (after the frame clamp)
        f64 fixedTime = 0.0;                // scaled seconds advanced by fixed steps so far
        f32 deltaTime = 0.0f;               // scaled, clamped frame delta
        f32 unscaledDeltaTime = 0.0f;       // clamped frame delta before the time scale
        f32 fixedDeltaTime = 1.0f / 60.0f;
        f32 timeScale = 1.0f;
        f32 interpolationAlpha = 0.0f;      // accumulator / fixedDeltaTime in [0, 1); valid from UPDATE onwards
        u64 frameIndex = 0;
        u64 fixedStepIndex = 0;             // total fixed steps since startup
        u32 fixedStepsThisFrame = 0;
        bool inFixedStep = false;           // true while FIXED_UPDATE, PHYSICS_STEP or FIXED_END runs
        bool droppedFixedTime = false;      // the spiral-of-death guard fired this frame

        [[nodiscard]] bool operator==(const engine_time &) const = default;
    };

    /// <summary>
    /// Step 1 of the frame: clamps the raw delta to maxFrameDeltaSeconds, applies the time scale,
    /// advances time and unscaledTime, increments frameIndex and resets the per-frame fields
    /// (fixedStepsThisFrame, inFixedStep, droppedFixedTime). The first frame (frameIndex 0 on entry)
    /// ignores rawDeltaSeconds and uses fixedDeltaTime; negative or non-finite deltas count as zero.
    /// </summary>
    CONJURE_API void begin_frame(engine_time &time, const time_settings &settings, f64 rawDeltaSeconds) noexcept;

    /// <summary>
    /// Advances fixedTime and fixedStepIndex by one fixed step.
    /// </summary>
    CONJURE_API void advance_fixed_step(engine_time &time) noexcept;

    /// <summary>
    /// The ecs::frame_info handed to scheduler::run_phase: deltaTime is the fixed delta inside a fixed
    /// phase and the frame delta elsewhere (R2).
    /// </summary>
    [[nodiscard]] CONJURE_API ecs::frame_info make_frame_info(const engine_time &time) noexcept;
}
