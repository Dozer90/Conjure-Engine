#include "editor/core/commands/command_support.h"

#include "conjure/core/platform_detect.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_time.h"
#include "conjure/engine/frame_stats.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/renderer/renderer.h"
#endif

#include <algorithm>
#include <array>
#include <utility>

// profiler.* (docs/design/04_editor.md 4.4, 7): the engine's frame_stats ring averaged over the last n frames,
// the per-phase breakdown of the latest sample and the renderer's counters when a renderer exists. gpuMs is
// null without a device that reports GPU timings.

namespace conjure::editor::support {

    namespace {
        constexpr u32 MAX_FRAMES = static_cast<u32>(engine::frame_stats::HISTORY_SIZE);

        core::result<command_outcome> profiler_frame_stats(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(const u32 framesRequested, u32_or(invocation.args, "frames", 60));
            if (framesRequested == 0) {
                return invalid_args("args.frames: expected at least 1");
            }
            const u32 frames = std::min(framesRequested, MAX_FRAMES);
            const engine::frame_stats &stats = context.engine().stats();
            const engine::frame_sample &latest = stats.latest();
            const usize available = std::min<usize>(frames, stats.sample_count());

            core::json::document systems = core::json::document::array();
            std::array<f32, ecs::SYSTEM_PHASE_COUNT> phaseAverages = {};
            f32 fixedStepsTotal = 0.0f;
            u32 droppedFixedTime = 0;
            for (usize age = 0; age < available; ++age) {
                const engine::frame_sample &sample = stats.sample_at(age);
                for (u32 phase = 0; phase < ecs::SYSTEM_PHASE_COUNT; ++phase) {
                    phaseAverages[phase] += sample.phaseMilliseconds[phase];
                }
                fixedStepsTotal += static_cast<f32>(sample.fixedSteps);
                droppedFixedTime += sample.droppedFixedTime ? 1u : 0u;
            }
            const f32 divisor = available == 0 ? 1.0f : static_cast<f32>(available);
            for (u32 phase = 0; phase < ecs::SYSTEM_PHASE_COUNT; ++phase) {
                systems.push_back(core::json::object({
                  {"name", ecs::to_string(static_cast<ecs::system_phase>(phase))},
                  {"ms", latest.phaseMilliseconds[phase]},
                  {"averageMs", phaseAverages[phase] / divisor},
                }));
            }
            core::json::document history = core::json::document::array();
            for (usize age = available; age > 0; --age) {
                history.push_back(stats.sample_at(age - 1).frameMilliseconds);
            }

            core::json::document result = core::json::object({
              {"frame", context.frame_index()},
              {"frames", available},
              {"cpuMs", latest.frameMilliseconds},
              {"averageCpuMs", stats.average_frame_milliseconds(frames)},
              {"submitMs", latest.submitMilliseconds},
              {"pacerSleepMs", latest.pacerSleepMilliseconds},
              {"fixedSteps", latest.fixedSteps},
              {"averageFixedSteps", fixedStepsTotal / divisor},
              {"droppedFixedTimeFrames", droppedFixedTime},
              {"jobsExecuted", latest.jobsExecuted},
              {"frameMemoryHighWater", latest.frameMemoryHighWater},
              {"systems", systems},
              {"history", history},
              {"entityCount", context.world().entity_count()},
              {"simulationTime", context.engine().time().time},
              {"fixedStepIndex", context.engine().time().fixedStepIndex},
            });
            result.set_null("gpuMs");
            result.set("drawCalls", 0);
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
            if (context.renderer() != nullptr) {
                const renderer::render_stats &render = context.renderer()->stats();
                result.set("drawCalls", render.drawCalls);
                result.set(
                  "renderer",
                  core::json::object({
                    {"viewsRendered", render.viewsRendered},
                    {"passesExecuted", render.passesExecuted},
                    {"drawCalls", render.drawCalls},
                    {"dispatches", render.dispatches},
                    {"instancesDrawn", render.instancesDrawn},
                    {"instancesCulled", render.instancesCulled},
                    {"trianglesDrawn", render.trianglesDrawn},
                    {"lightsRendered", render.lightsRendered},
                  })
                );
            }
#endif
            command_outcome outcome = {};
            outcome.result = std::move(result);
            return outcome;
        }
    }

    void register_profiler_commands(command_registry &registry) {
        add_command(
          registry,
          make_command(
            "profiler.frame_stats",
            "Frame timings: the latest CPU frame, the average over the last n frames, per-phase milliseconds, fixed "
            "steps, "
            "renderer counters and the entity count.",
            R"({"type":"object","properties":{"frames":{"type":"integer","minimum":1}}})",
            R"({"type":"object","properties":{"cpuMs":{"type":"number"},"gpuMs":{"type":["number","null"]},)"
            R"("systems":{"type":"array"},)"
            R"("drawCalls":{"type":"integer"},"entityCount":{"type":"integer"}},"additionalProperties":true})",
            command_flags::NONE,
            &profiler_frame_stats
          )
        );
    }
}
