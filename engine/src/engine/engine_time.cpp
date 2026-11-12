#include "conjure/engine/engine_time.h"

#include <algorithm>
#include <cmath>

using namespace conjure;
using namespace conjure::engine;

void conjure::engine::begin_frame(engine_time &time, const time_settings &settings, f64 rawDeltaSeconds) noexcept {
    time.fixedDeltaTime = settings.fixedDeltaSeconds;
    // NaN and infinity would poison every later time value; they stop time like a negative scale does.
    time.timeScale = std::isfinite(settings.timeScale) && settings.timeScale > 0.0f ? settings.timeScale : 0.0f;

    f64 raw = rawDeltaSeconds;
    if (time.frameIndex == 0) {
        // The first frame has no previous frame to measure from: it runs exactly one fixed step so a run
        // is deterministic from frame 0 (06_engine_loop.md 2.1).
        raw = static_cast<f64>(settings.fixedDeltaSeconds);
    }
    if (!std::isfinite(raw) || raw < 0.0) {
        raw = 0.0;
    }
    const f64 clamped = std::min(raw, static_cast<f64>(std::max(settings.maxFrameDeltaSeconds, 0.0f)));

    time.unscaledDeltaTime = static_cast<f32>(clamped);
    time.deltaTime = static_cast<f32>(clamped * static_cast<f64>(time.timeScale));
    time.unscaledTime += clamped;
    time.time += clamped * static_cast<f64>(time.timeScale);
    time.frameIndex += 1;
    time.fixedStepsThisFrame = 0;
    time.inFixedStep = false;
    time.droppedFixedTime = false;
}

void conjure::engine::advance_fixed_step(engine_time &time) noexcept {
    time.fixedTime += static_cast<f64>(time.fixedDeltaTime) * static_cast<f64>(time.timeScale);
    time.fixedStepIndex += 1;
}

ecs::frame_info conjure::engine::make_frame_info(const engine_time &time) noexcept {
    ecs::frame_info info = {};
    info.deltaTime = time.inFixedStep ? time.fixedDeltaTime : time.deltaTime;
    info.fixedDeltaTime = time.fixedDeltaTime;
    info.interpolationAlpha = time.interpolationAlpha;
    info.time = time.time;
    info.frameIndex = time.frameIndex;
    info.inFixedStep = time.inFixedStep;
    return info;
}
