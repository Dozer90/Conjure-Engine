#include "conjure/engine/debug_overlay_layer.h"

#if !CONJURE_DISTRIBUTION

#include "conjure/engine/diagnostics/check_registry.h"
#include "conjure/engine/engine_phase.h"
#include "conjure/engine/engine_time.h"
#include "conjure/engine/frame_memory.h"
#include "conjure/engine/frame_stats.h"
#include "conjure/engine/log_category.h"

#include "conjure/core/log.h"
#include "conjure/core/string_utils.h"

#if defined(CONJURE_BUILD_EDITOR) && CONJURE_BUILD_EDITOR
    #include <imgui.h>
#endif

#include <algorithm>
#include <format>

using namespace conjure;
using namespace conjure::engine;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

debug_overlay_layer::debug_overlay_layer() :
    layer("debug_overlay") {}

debug_overlay_layer::~debug_overlay_layer() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void debug_overlay_layer::on_imgui(engine_context &context) {
    CONJURE_UNUSED(context);
#if defined(CONJURE_BUILD_EDITOR) && CONJURE_BUILD_EDITOR
    if (!mVisible || mText.empty() || ImGui::GetCurrentContext() == nullptr || !ImGui::GetIO().Fonts->IsBuilt()) {
        return;
    }
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
      | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav
      | ImGuiWindowFlags_NoInputs;
    ImGui::SetNextWindowPos(ImVec2(8.0f, 8.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.6f);
    if (ImGui::Begin("##conjure_debug_overlay", nullptr, flags)) {
        ImGui::TextUnformatted(mText.c_str(), mText.c_str() + mText.size());
    }
    ImGui::End();
#endif
}

void debug_overlay_layer::on_frame_end(engine_context &context) {
    ++mFramesSeen;
    mText = format_sample(context);
    if (mLogIntervalFrames != 0 && mFramesSeen % mLogIntervalFrames == 0) {
        CONJURE_LOG_INFO(LOG_ENGINE, "{}", mText);
    }
}

std::string debug_overlay_layer::format_sample(const engine_context &context) {
    std::string text = {};
    if (context.time != nullptr) {
        const engine_time &time = *context.time;
        text += std::format(
          "frame {} | t {:.3f} s | dt {:.2f} ms | fixed {} step(s) @ {:.1f} Hz | alpha {:.2f}",
          time.frameIndex,
          time.time,
          static_cast<f64>(time.deltaTime) * 1000.0,
          time.fixedStepsThisFrame,
          time.fixedDeltaTime > 0.0f ? 1.0 / static_cast<f64>(time.fixedDeltaTime) : 0.0,
          time.interpolationAlpha
        );
        if (time.droppedFixedTime) {
            text += " | DROPPED";
        }
    }
    if (context.stats != nullptr && context.stats->sample_count() > 0) {
        const frame_sample &sample = context.stats->latest();
        // frameMilliseconds is measured after the pacer sleep: the CPU's own share is what is left without it.
        const f32 cpuMilliseconds = std::max(sample.frameMilliseconds - sample.pacerSleepMilliseconds, 0.0f);
        text += std::format(
          "\ncpu {:.2f} ms | frame {:.2f} ms (avg {:.2f}) | submit {:.2f} ms | sleep {:.2f} ms | jobs {}",
          cpuMilliseconds,
          sample.frameMilliseconds,
          context.stats->average_frame_milliseconds(60),
          sample.submitMilliseconds,
          sample.pacerSleepMilliseconds,
          sample.jobsExecuted
        );
        text += "\nphases:";
        for (u32 index = 0; index < ecs::SYSTEM_PHASE_COUNT; ++index) {
            const f32 milliseconds = sample.phaseMilliseconds[index];
            if (milliseconds <= 0.0f) {
                continue;
            }
            const std::string_view phaseName = conjure::engine::to_string(static_cast<ecs::system_phase>(index));
            text += std::format(" {} {:.2f}", phaseName, milliseconds);
        }
    }
    if (context.frameMemory != nullptr) {
        const frame_memory_stats memory = context.frameMemory->stats();
        text += std::format(
          "\nframe memory {} / {} (overflows {})",
          core::str::format_bytes(memory.highWaterBytes),
          core::str::format_bytes(memory.capacityBytes),
          memory.overflowCount
        );
    }
    if (context.checks != nullptr && context.checks->record_count() > 0) {
        text += std::format(
          "\nchecks {} recorded, {} failed",
          context.checks->record_count(),
          context.checks->failed_count()
        );
    }
    return text;
}

#endif
