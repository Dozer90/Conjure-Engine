#include "engine/frame_loop.h"

#include "engine/frame_pacer.h"
#include "engine/main_thread_queue.h"

#include "conjure/engine/engine_error.h"
#include "conjure/engine/engine_phase.h"
#include "conjure/engine/log_category.h"

#include "conjure/core/clock.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/log.h"
#include "conjure/platform/event.h"
#include "conjure/platform/input.h"
#include "conjure/platform/window.h"
#include "conjure/rhi/rhi_error.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/renderer/renderer.h"
#endif

#include <algorithm>
#include <utility>

using namespace conjure;
using namespace conjure::engine;

namespace {

    [[nodiscard]] f32 to_milliseconds(f64 seconds) noexcept {
        return static_cast<f32>(seconds * 1000.0);
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

frame_loop::frame_loop(application &app, engine_services &services) :
    mApp(&app),
    mServices(&services) {}

frame_loop::~frame_loop() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<void> frame_loop::run_frame(std::optional<f64> deltaOverride) {
    application &app = *mApp;
    engine_context &context = app.mContext;
    const engine_config &config = app.mDesc.config;
    const core::stopwatch frameWatch = {};
    mSample = {};
    mJobsExecutedAtFrameStart = mServices->jobs.stats().executed;
    if (app.mPacer != nullptr) {
        app.mPacer->begin_frame();
    }

    // 1. Time.
    const bool fixedStepsForced = app.mFixedStepsPending;
    const u32 forcedSteps = app.mPendingFixedSteps;
    app.mFixedStepsPending = false;
    app.mPendingFixedSteps = 0;
    f64 rawDelta = measure_raw_delta(deltaOverride);
    time_settings timeSettings = config.time;
    timeSettings.timeScale = app.mTime.timeScale;
    if (fixedStepsForced) {
        // The frame delta is exactly fixedSteps * fixedDeltaTime (R13): the clamp must not shorten it, or
        // engine_time::time drifts away from fixedTime on every editor step.
        const f64 forcedDelta = static_cast<f64>(forcedSteps) * static_cast<f64>(config.time.fixedDeltaSeconds);
        rawDelta = forcedDelta;
        timeSettings.maxFrameDeltaSeconds = std::max(timeSettings.maxFrameDeltaSeconds, static_cast<f32>(forcedDelta));
    }
    if (fixedStepsForced && app.mTime.frameIndex == 0) {
        // Frame 0 would otherwise force one fixed step whatever the request says.
        app.mTime.frameIndex = 1;
        begin_frame(app.mTime, timeSettings, rawDelta);
        app.mTime.frameIndex = 1;
    } else {
        begin_frame(app.mTime, timeSettings, rawDelta);
    }

    // 2. Frame memory.
    if (mServices->frameMemory != nullptr) {
        mServices->frameMemory->reset();
    }

    // 3. Events and input.
    pump_events();
    apply_pending_resize();

    // 4. Main-thread tasks.
    if (app.mMainThreadQueue != nullptr) {
        app.mMainThreadQueue->drain();
    }
    core::jobs::drain_main_thread_queue();

    // 5. Subsystem and layer frame begin.
    for (const std::unique_ptr<engine_subsystem> &subsystem : app.mSubsystems) {
        subsystem->begin_frame(context);
    }
    app.mLayers.frame_begin(context);

    // 6. FRAME_BEGIN.
    run_phase(ecs::system_phase::FRAME_BEGIN);

    // 7. Simulation.
    if (app.mSimulationMode != simulation_mode::PAUSED || fixedStepsForced) {
        u32 steps = 0;
        if (fixedStepsForced) {
            steps = forcedSteps;
            app.mFixedClock.reset();
            app.mTime.interpolationAlpha = 0.0f;
            app.mTime.droppedFixedTime = false;
        } else {
            const fixed_step_result advanced = app.mFixedClock.advance(app.mTime.deltaTime);
            steps = advanced.stepCount;
            app.mTime.interpolationAlpha = advanced.interpolationAlpha;
            app.mTime.droppedFixedTime = advanced.droppedTime;
        }
        app.mTime.fixedStepsThisFrame = steps;
        run_fixed_steps(steps);
        if (app.mTime.droppedFixedTime) {
            const f64 now = app.mClock->now_seconds();
            if (now - mLastDroppedWarningSeconds >= DROPPED_TIME_WARNING_INTERVAL_SECONDS) {
                mLastDroppedWarningSeconds = now;
                CONJURE_LOG_WARN(
                  LOG_ENGINE,
                  "frame {}: fixed-step accumulator exceeded {} steps; leftover time dropped",
                  app.mTime.frameIndex,
                  app.mFixedClock.max_steps_per_frame()
                );
            }
        }
        run_phase(ecs::system_phase::UPDATE);
        app.mLayers.update(context);
        run_phase(ecs::system_phase::LATE_UPDATE);
    } else {
        app.mTime.fixedStepsThisFrame = 0;
        run_phase_filtered(ecs::system_phase::UPDATE, ecs::system_flags::RUNS_WHILE_PAUSED);
        app.mLayers.update(context);
        run_phase_filtered(ecs::system_phase::LATE_UPDATE, ecs::system_flags::RUNS_WHILE_PAUSED);
    }
    if (app.mSimulationMode == simulation_mode::STEP_ONCE || fixedStepsForced) {
        app.mSimulationMode = simulation_mode::PAUSED;
    }

    // 8. TRANSFORM, 9. SYNC: always.
    run_phase(ecs::system_phase::TRANSFORM);
    run_phase(ecs::system_phase::SYNC);

    // 10. Extraction, 11. Render.
    bool presentHappened = false;
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    if (mServices->renderer != nullptr) {
        mServices->extractTarget = &mServices->renderer->begin_extract();
        app.mLayers.imgui(context);
        app.mLayers.imgui_end(context);
        run_phase(ecs::system_phase::EXTRACT);
        mServices->renderer->end_extract();
        mServices->extractTarget = nullptr;

        const bool minimised = mServices->window != nullptr && mServices->window->is_minimised();
        if (!minimised) {
            presentHappened = render();
        }
    } else {
        app.mLayers.imgui(context);
        app.mLayers.imgui_end(context);
    }
#else
    app.mLayers.imgui(context);
    app.mLayers.imgui_end(context);
#endif

    // 12. FRAME_END.
    run_phase(ecs::system_phase::FRAME_END);

    // 13. Subsystem and layer frame end.
    for (usize index = app.mSubsystems.size(); index > 0; --index) {
        app.mSubsystems[index - 1]->end_frame(context);
    }
    app.mLayers.frame_end(context);
    app.mLayers.apply_pending(context);

    // 14. Pacing.
    if (app.mPacer != nullptr) {
        mSample.pacerSleepMilliseconds = to_milliseconds(app.mPacer->end_frame(presentHappened));
    }

    // 15. Statistics.
    mSample.frameMilliseconds = to_milliseconds(frameWatch.elapsed_seconds());
    mSample.fixedSteps = app.mTime.fixedStepsThisFrame;
    mSample.jobsExecuted = static_cast<u32>(std::min<u64>(
      mServices->jobs.stats().executed - mJobsExecutedAtFrameStart,
      0xFFFFFFFFu
    ));
    if (mServices->frameMemory != nullptr) {
        mSample.frameMemoryHighWater = mServices->frameMemory->stats().highWaterBytes;
    }
    mSample.droppedFixedTime = app.mTime.droppedFixedTime;
    app.mStats.commit(mSample);

    // 16. Frame budget.
    if (config.maxFrames != 0 && app.mTime.frameIndex >= config.maxFrames) {
        app.request_quit(exit_code::OK);
    }
    return core::result<void>::success();
}

void frame_loop::reset_clock() noexcept {
    mHasPreviousFrame = false;
}

void frame_loop::inject_render_failure(core::error error, u32 frameCount) {
    if (frameCount == 0) {
        mInjectedRenderFailure = std::nullopt;
        mInjectedRenderFailureFrames = 0;
        return;
    }
    mInjectedRenderFailure = std::move(error);
    mInjectedRenderFailureFrames = frameCount;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

f64 frame_loop::measure_raw_delta(std::optional<f64> deltaOverride) {
    const f64 now = mApp->mClock->now_seconds();
    f64 raw = 0.0;
    if (deltaOverride.has_value()) {
        raw = *deltaOverride;
    } else if (mApp->mDesc.config.time.forcedFrameDeltaSeconds > 0.0f) {
        raw = static_cast<f64>(mApp->mDesc.config.time.forcedFrameDeltaSeconds);
    } else if (mHasPreviousFrame) {
        raw = now - mLastFrameClockSeconds;
    } else {
        raw = static_cast<f64>(mApp->mDesc.config.time.fixedDeltaSeconds);
    }
    mLastFrameClockSeconds = now;
    mHasPreviousFrame = true;
    return raw;
}

void frame_loop::pump_events() {
    application &app = *mApp;
    if (mServices->window == nullptr) {
        return;
    }
    const std::span<const platform::event> events = mServices->window->pump_events();
    for (const platform::event &event : events) {
        handle_window_event(event);
        const bool consumed = app.mLayers.dispatch_event(app.mContext, event);
        if (!consumed && mServices->input != nullptr) {
            mServices->input->apply(event);
        }
    }
    if (mServices->input != nullptr) {
        mServices->input->end_frame();
    }
    if (mServices->window->should_close()) {
        app.request_quit(exit_code::OK);
    }
}

void frame_loop::handle_window_event(const platform::event &event) {
    application &app = *mApp;
    if (platform::event_as<platform::close_event>(event) != nullptr) {
        app.request_quit(exit_code::OK);
        return;
    }
    if (const platform::focus_event *focus = platform::event_as<platform::focus_event>(event); focus != nullptr) {
        if (app.mPacer != nullptr) {
            app.mPacer->set_unfocused(!focus->focused);
        }
        return;
    }
    const platform::framebuffer_resized_event *resized = platform::event_as<platform::framebuffer_resized_event>(event);
    if (resized != nullptr && resized->width > 0 && resized->height > 0) {
        apply_resize(math::uvec2(resized->width, resized->height));
    }
}

void frame_loop::run_phase(ecs::system_phase phase) {
    if (mServices->scheduler == nullptr) {
        return;
    }
    const core::stopwatch watch = {};
    set_phase_in_flight(true);
    mServices->scheduler->run_phase(phase, make_frame_info(mApp->mTime));
    set_phase_in_flight(false);
    mSample.phaseMilliseconds[phase_index(phase)] += to_milliseconds(watch.elapsed_seconds());
}

void frame_loop::run_phase_filtered(ecs::system_phase phase, ecs::system_flags required) {
    if (mServices->scheduler == nullptr) {
        return;
    }
    const core::stopwatch watch = {};
    set_phase_in_flight(true);
    mServices->scheduler->run_phase_filtered(phase, make_frame_info(mApp->mTime), required);
    set_phase_in_flight(false);
    mSample.phaseMilliseconds[phase_index(phase)] += to_milliseconds(watch.elapsed_seconds());
}

void frame_loop::set_phase_in_flight(bool inFlight) noexcept {
    if (mServices->frameMemory != nullptr) {
        mServices->frameMemory->set_phase_in_flight(inFlight);
    }
}

void frame_loop::run_fixed_steps(u32 stepCount) {
    engine_time &time = mApp->mTime;
    time.inFixedStep = true;
    for (u32 step = 0; step < stepCount; ++step) {
        for (const ecs::system_phase phase : FIXED_PHASES) {
            run_phase(phase);
        }
        advance_fixed_step(time);
    }
    time.inFixedStep = false;
}

bool frame_loop::render() {
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    const core::stopwatch watch = {};
    core::result<void> rendered = mServices->renderer->render_frame();
    mSample.submitMilliseconds = to_milliseconds(watch.elapsed_seconds());
    if (mInjectedRenderFailure.has_value()) {
        // Test hook (application::inject_render_failure): the renderer ran, its verdict is overruled.
        rendered = core::result<void>(*mInjectedRenderFailure);
        --mInjectedRenderFailureFrames;
        if (mInjectedRenderFailureFrames == 0) {
            mInjectedRenderFailure = std::nullopt;
        }
    }
    if (mServices->device != nullptr) {
        mServices->device->run_garbage_collection();
    }
    if (!rendered.has_value()) {
        handle_render_failure(rendered.get_error());
        return false;
    }
    mConsecutiveRenderFailures = 0;
    mLastRenderError.clear();
    return mServices->swapchain != nullptr && mApp->mDesc.config.window.vsync;
#else
    return false;
#endif
}

void frame_loop::handle_render_failure(const core::error &error) {
    application &app = *mApp;
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    const bool rendererLost = mServices->renderer != nullptr && mServices->renderer->is_lost();
    const bool lost = error.is(rhi::rhi_error::DEVICE_LOST) || rendererLost;
    if (lost) {
        CONJURE_LOG_ERROR(LOG_ENGINE, "graphics device lost: {}", error.to_string());
        if (mServices->renderer != nullptr) {
            mServices->renderer->on_device_lost();
        }
        app.request_quit(exit_code::DEVICE_LOST);
        return;
    }
#endif
    const std::string message = error.to_string();
    if (message != mLastRenderError) {
        mLastRenderError = message;
        CONJURE_LOG_ERROR(LOG_ENGINE, "render_frame failed: {}", message);
    }
    ++mConsecutiveRenderFailures;
    if (mConsecutiveRenderFailures >= MAX_CONSECUTIVE_RENDER_FAILURES) {
        CONJURE_LOG_ERROR(LOG_ENGINE, "{} consecutive render failures; stopping", mConsecutiveRenderFailures);
        app.request_quit(exit_code::RUNTIME_FAILURE);
    }
}

void frame_loop::apply_pending_resize() {
    if (!mPendingResize.has_value()) {
        return;
    }
    const math::uvec2 size = *mPendingResize;
    mPendingResize = std::nullopt;
    apply_resize(size);
}

void frame_loop::apply_resize(math::uvec2 size) {
    if (mServices->swapchain == nullptr) {
        return;
    }
    const core::result<void> resized = mServices->swapchain->resize(size.x, size.y);
    if (resized.has_value()) {
        mResizeFailures = 0;
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
        if (mServices->renderer != nullptr) {
            mServices->renderer->on_swapchain_resized();
        }
#endif
        return;
    }
    ++mResizeFailures;
    if (mResizeFailures >= 2) {
        CONJURE_LOG_ERROR(LOG_ENGINE, "swapchain resize failed twice: {}", resized.get_error().to_string());
        mApp->request_quit(exit_code::RUNTIME_FAILURE);
        mResizeFailures = 0;
        return;
    }
    CONJURE_LOG_WARN(LOG_ENGINE, "swapchain resize failed, retrying next frame: {}", resized.get_error().to_string());
    mPendingResize = size;
}
