#include "conjure/engine/engine_config.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/string_utils.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <thread>

using namespace conjure;
using namespace conjure::engine;

namespace {

    /// <summary>
    /// The INVALID_CONFIG error naming the offending field.
    /// </summary>
    [[nodiscard]] core::error invalid_field(std::string_view field, std::string_view reason) {
        return core::make_error(engine_error::INVALID_CONFIG, std::format("engine_config.{}: {}", field, reason));
    }
}

/*===========================================================================*/
/*===========================================================================*/
// engine_config

engine_config engine_config::defaults() {
    engine_config config = {};
    // The cheap debug layer in Debug; GPU-based validation slows every frame down, so a run asks for it with
    // --gpu-validation gpu or the project's "gpu_validation": "gpu_based" (code sweep F458).
    config.rendering.gpuValidation =
      CONJURE_DEBUG != 0 ? gpu_validation_level::DEBUG_LAYER : gpu_validation_level::OFF;
    return config;
}

engine_config engine_config::headless_defaults() {
    engine_config config = defaults();
    config.headless = true;
    config.window.width = 1280;
    config.window.height = 720;
    config.rendering.enabled = false;
    config.rendering.backend = rhi_backend_choice::NULL_DEVICE;
    config.audio.backend = audio_backend_choice::NULL_DEVICE;
    config.assets.hotReload = false;
    config.scripting.enabled = false;
    config.time.forcedFrameDeltaSeconds = 1.0f / 60.0f;
    config.jobs.workerCount = 2;
    return config;
}

core::result<void> engine_config::validate() const {
    if (!std::isfinite(time.fixedDeltaSeconds) || time.fixedDeltaSeconds <= 0.0f) {
        return invalid_field("time.fixedDeltaSeconds", "must be a positive finite number of seconds");
    }
    if (!std::isfinite(time.maxFrameDeltaSeconds) || time.maxFrameDeltaSeconds < time.fixedDeltaSeconds) {
        return invalid_field("time.maxFrameDeltaSeconds", "must be at least fixedDeltaSeconds");
    }
    if (time.maxFixedStepsPerFrame == 0) {
        return invalid_field("time.maxFixedStepsPerFrame", "must be at least 1");
    }
    if (!std::isfinite(time.forcedFrameDeltaSeconds) || time.forcedFrameDeltaSeconds < 0.0f) {
        return invalid_field("time.forcedFrameDeltaSeconds", "must not be negative");
    }
    if (time.forcedFrameDeltaSeconds > time.maxFrameDeltaSeconds) {
        return invalid_field(
          "time.forcedFrameDeltaSeconds",
          "must not exceed maxFrameDeltaSeconds (the frame clamp would shorten it)"
        );
    }
    if (!std::isfinite(time.timeScale) || time.timeScale < 0.0f) {
        return invalid_field("time.timeScale", "must not be negative");
    }
    if (!headless && (window.width == 0 || window.height == 0)) {
        return invalid_field("window", "a zero window size is only allowed when headless");
    }
    if (!headless && rendering.backend == rhi_backend_choice::NULL_DEVICE) {
        return invalid_field("rendering.backend", "the null device is only allowed when headless");
    }
    if (headless && rendering.enabled && rendering.backend != rhi_backend_choice::NULL_DEVICE) {
        return invalid_field("rendering.backend", "a headless application renders on the null device only");
    }
    if (memory.frameThreadBlockBytes > memory.frameArenaBytes) {
        return invalid_field("memory.frameThreadBlockBytes", "must not exceed frameArenaBytes");
    }
    if (!std::isfinite(audio.masterGain) || audio.masterGain < 0.0f) {
        return invalid_field("audio.masterGain", "must not be negative");
    }
    if (jobs.workerCount != AUTO_WORKER_COUNT && jobs.workerCount > core::jobs::MAX_WORKER_COUNT) {
        // The same bound job_system::start enforces, so the two cannot drift apart.
        return invalid_field(
          "jobs.workerCount",
          std::format("must be at most {} (0 = inline, AUTO_WORKER_COUNT = automatic)", core::jobs::MAX_WORKER_COUNT)
        );
    }
    return core::result<void>::success();
}


/*===========================================================================*/
/*===========================================================================*/
// Free functions

u32 conjure::engine::resolve_worker_count(const jobs_settings &settings, u32 hardwareConcurrency) noexcept {
    if (settings.workerCount != AUTO_WORKER_COUNT) {
        return settings.workerCount;
    }
    const u32 available = hardwareConcurrency > 0 ? hardwareConcurrency - 1 : 0;
    return std::clamp(available, MIN_AUTO_WORKER_COUNT, MAX_AUTO_WORKER_COUNT);
}

u32 conjure::engine::resolve_worker_count(const jobs_settings &settings) noexcept {
    return resolve_worker_count(settings, std::thread::hardware_concurrency());
}

std::string_view conjure::engine::to_string(rhi_backend_choice value) noexcept {
    switch (value) {
        case rhi_backend_choice::AUTO:
            return "auto";
        case rhi_backend_choice::D3D12:
            return "d3d12";
        case rhi_backend_choice::VULKAN:
            return "vulkan";
        case rhi_backend_choice::NULL_DEVICE:
            return "null";
    }
    return "auto";
}

std::string_view conjure::engine::to_string(audio_backend_choice value) noexcept {
    switch (value) {
        case audio_backend_choice::AUTO:
            return "auto";
        case audio_backend_choice::MINIAUDIO:
            return "miniaudio";
        case audio_backend_choice::NULL_DEVICE:
            return "null";
    }
    return "auto";
}

std::string_view conjure::engine::to_string(gpu_validation_level value) noexcept {
    switch (value) {
        case gpu_validation_level::OFF:
            return "off";
        case gpu_validation_level::DEBUG_LAYER:
            return "debug_layer";
        case gpu_validation_level::GPU_BASED:
            return "gpu_based";
    }
    return "off";
}

std::string_view conjure::engine::to_string(script_export_mode value) noexcept {
    switch (value) {
        case script_export_mode::FRAMEWORK_DEPENDENT:
            return "framework-dependent";
        case script_export_mode::SELF_CONTAINED:
            return "self-contained";
    }
    return "framework-dependent";
}

core::result<rhi_backend_choice> conjure::engine::parse_rhi_backend_choice(std::string_view text) {
    if (core::str::iequals(text, "auto")) {
        return rhi_backend_choice::AUTO;
    }
    if (core::str::iequals(text, "d3d12")) {
        return rhi_backend_choice::D3D12;
    }
    if (core::str::iequals(text, "vulkan")) {
        return rhi_backend_choice::VULKAN;
    }
    if (core::str::iequals(text, "null")) {
        return rhi_backend_choice::NULL_DEVICE;
    }
    return core::make_error(engine_error::BAD_ARGUMENTS, std::format("unknown rhi backend '{}'", text));
}

core::result<audio_backend_choice> conjure::engine::parse_audio_backend_choice(std::string_view text) {
    if (core::str::iequals(text, "auto")) {
        return audio_backend_choice::AUTO;
    }
    if (core::str::iequals(text, "miniaudio")) {
        return audio_backend_choice::MINIAUDIO;
    }
    if (core::str::iequals(text, "null")) {
        return audio_backend_choice::NULL_DEVICE;
    }
    return core::make_error(engine_error::BAD_ARGUMENTS, std::format("unknown audio backend '{}'", text));
}

core::result<script_export_mode> conjure::engine::parse_script_export_mode(std::string_view text) {
    if (core::str::iequals(text, "framework-dependent")) {
        return script_export_mode::FRAMEWORK_DEPENDENT;
    }
    if (core::str::iequals(text, "self-contained")) {
        return script_export_mode::SELF_CONTAINED;
    }
    return core::make_error(engine_error::BAD_ARGUMENTS, std::format("unknown script export mode '{}'", text));
}

core::result<gpu_validation_level> conjure::engine::parse_gpu_validation_level(std::string_view text) {
    if (core::str::iequals(text, "off")) {
        return gpu_validation_level::OFF;
    }
    if (core::str::iequals(text, "debug_layer")) {
        return gpu_validation_level::DEBUG_LAYER;
    }
    if (core::str::iequals(text, "gpu_based")) {
        return gpu_validation_level::GPU_BASED;
    }
    return core::make_error(
      engine_error::BAD_ARGUMENTS,
      std::format("unknown gpu validation level '{}' (expected off, debug_layer or gpu_based)", text)
    );
}
