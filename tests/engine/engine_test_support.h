#pragma once

// Shared helpers of the engine tests (docs/design/06_engine_loop.md 10): the headless configuration every
// application test starts from (null devices, forced delta, the shader index conjure_shaders wrote), probe
// subsystems and layers that record the calls they receive into a shared log, a scratch project written
// into a temp_directory, and the phase names used by the sequence tests.

#include "conjure/testing/doctest.h"

#include "conjure/testing/temp_directory.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/engine/application.h"
#include "conjure/engine/application_desc.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_context.h"
#include "conjure/engine/engine_subsystem.h"
#include "conjure/engine/layer.h"
#include "conjure/engine/project.h"
#include "conjure/platform/event.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace engine_test {

    /// <summary>
    /// Where conjure_shaders wrote index.json (CONJURE_TEST_SHADER_DIR); empty when the build did not set it.
    /// </summary>
    [[nodiscard]] std::filesystem::path shader_directory();

    /// <summary>
    /// engine_config::headless_defaults() with the test shader directory, two inline-less workers, audio on
    /// the null backend and physics enabled; rendering stays off (enable_null_rendering turns it on).
    /// </summary>
    [[nodiscard]] conjure::engine::engine_config headless_config();

    /// <summary>
    /// The same configuration with every job run inline (workerCount 0): the deterministic variant.
    /// </summary>
    [[nodiscard]] conjure::engine::engine_config inline_config();

    /// <summary>
    /// Turns rendering on with the null device so extraction and render_frame run headless, with GPU validation
    /// OFF whatever the build's default level.
    /// </summary>
    void enable_null_rendering(conjure::engine::engine_config &config);

    /// <summary>
    /// True when this build has the scripting module, so that the application owns a scripting subsystem.
    /// </summary>
    [[nodiscard]] bool scripting_module_built() noexcept;

    /// <summary>
    /// An application descriptor over a configuration (no project, no clock override).
    /// </summary>
    [[nodiscard]] conjure::engine::application_desc make_desc(const conjure::engine::engine_config &config);

    /// <summary>
    /// Writes a minimal project.json (name, assets root, optional startup scene) into a directory and loads
    /// it. The assets root ("assets" by default; relative to the directory or absolute) is created.
    /// </summary>
    [[nodiscard]] conjure::engine::project write_project(
      const std::filesystem::path &directory,
      std::string_view name,
      std::string_view startupScene = {},
      std::string_view extraJson = {},
      const std::filesystem::path &assetsRoot = "assets"
    );

    /// <summary>
    /// A subsystem that records every call into a shared log as "<name>.<call>" and fails startup or
    /// post_startup on demand.
    /// </summary>
    class probe_subsystem final : public conjure::engine::engine_subsystem {
        private:
            std::string mName = {};
            std::vector<std::string> *mLog = nullptr;
            bool mFailStartup = false;
            bool mFailPostStartup = false;
            bool mStarted = false;

        public:
            probe_subsystem(
              std::string name,
              std::vector<std::string> &log,
              bool failStartup = false,
              bool failPostStartup = false
            );
            ~probe_subsystem() override;

        public:
            [[nodiscard]] std::string_view name() const noexcept override;
            [[nodiscard]] conjure::core::result<void> startup(conjure::engine::engine_context &context) override;
            [[nodiscard]] conjure::core::result<void> post_startup(conjure::engine::engine_context &context) override;
            void register_systems(
              conjure::ecs::scheduler &scheduler,
              conjure::engine::engine_context &context
            ) override;
            void begin_frame(conjure::engine::engine_context &context) override;
            void end_frame(conjure::engine::engine_context &context) override;
            void shutdown(conjure::engine::engine_context &context) override;

            [[nodiscard]] bool is_started() const noexcept {
                return mStarted;
            }
    };

    /// <summary>
    /// A layer that records its callbacks into a shared log and optionally consumes events or fails to attach.
    /// </summary>
    class probe_layer final : public conjure::engine::layer {
        private:
            std::vector<std::string> *mLog = nullptr;
            bool mConsumeEvents = false;
            bool mFailAttach = false;
            conjure::u32 mUpdates = 0;

        public:
            probe_layer(
              std::string name,
              std::vector<std::string> &log,
              bool consumeEvents = false,
              bool failAttach = false
            );
            ~probe_layer() override;

        public:
            [[nodiscard]] conjure::core::result<void> on_attach(conjure::engine::engine_context &context) override;
            void on_detach(conjure::engine::engine_context &context) override;
            void on_frame_begin(conjure::engine::engine_context &context) override;
            void on_update(conjure::engine::engine_context &context) override;
            void on_imgui(conjure::engine::engine_context &context) override;
            void on_imgui_end(conjure::engine::engine_context &context) override;
            void on_frame_end(conjure::engine::engine_context &context) override;
            [[nodiscard]] bool on_event(
              conjure::engine::engine_context &context,
              const conjure::platform::event &event
            ) override;

            [[nodiscard]] conjure::u32 updates() const noexcept {
                return mUpdates;
            }
    };

    /// <summary>
    /// Adds one EXCLUSIVE probe system per phase that appends "<phase>" (or "<phase>:<label>") to the log
    /// when it runs; the UPDATE and LATE_UPDATE probes carry RUNS_WHILE_PAUSED only when 'pausedProbes'.
    /// </summary>
    void add_phase_probes(
      conjure::ecs::scheduler &scheduler,
      std::vector<std::string> &log,
      std::string_view label = {},
      bool pausedProbes = false
    );

    /// <summary>
    /// The entries of 'log' that start with 'prefix'.
    /// </summary>
    [[nodiscard]] std::vector<std::string> filter_log(const std::vector<std::string> &log, std::string_view prefix);
}
