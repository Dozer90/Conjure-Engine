#pragma once

#include "conjure/engine/application_desc.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_context.h"
#include "conjure/engine/engine_subsystem.h"
#include "conjure/engine/engine_time.h"
#include "conjure/engine/exit_code.h"
#include "conjure/engine/fixed_step_clock.h"
#include "conjure/engine/frame_stats.h"
#include "conjure/engine/layer_stack.h"

#include "conjure/core/api.h"
#include "conjure/core/clock.h"
#include "conjure/core/error.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/modules/module_service.h"

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

// The application (docs/design/06_engine_loop.md 1, 3, 4.2): one object that owns every module object
// (through engine_services), starts them in the documented order with reverse unwind on failure, drives
// one frame per step_frame() and exposes the engine_context service table. Headless is configuration, not
// a code path: tests, servers, the editor's step button and the runtime all go through step_frame().
//
//   application_desc desc = {};
//   desc.config = engine_config::headless_defaults();
//   application app(std::move(desc));
//   CONJURE_REQUIRE_OK(app.initialise());
//   CONJURE_REQUIRE_OK(app.step_frame());
//   app.shutdown();

namespace conjure::engine {

    class frame_loop;
    class frame_pacer;
    class main_thread_queue;
    struct engine_services;

    enum class application_state : u8 {
        CREATED,
        STARTING,
        RUNNING,
        STOPPING,
        STOPPED
    };

    /// <summary>
    /// Pause is a simulation mode, not timeScale == 0 (D10): PAUSED skips the fixed loop and every
    /// UPDATE / LATE_UPDATE system without RUNS_WHILE_PAUSED while TRANSFORM, SYNC and EXTRACT keep running.
    /// </summary>
    enum class simulation_mode : u8 {
        RUNNING,
        PAUSED,
        STEP_ONCE
    };

    /// <summary>
    /// The enumerator name of a state ("RUNNING").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(application_state state) noexcept;

    /// <summary>
    /// The enumerator name of a mode ("PAUSED").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(simulation_mode mode) noexcept;

    class CONJURE_API application {
        public:
            CONJURE_MODULE_SERVICE(conjure::engine::application);

        private:
            friend class frame_loop;

            application_desc mDesc;                                     // no default: moved in by the constructor
            engine_context mContext = {};
            engine_time mTime = {};
            fixed_step_clock mFixedClock = {};
            layer_stack mLayers = {};
            frame_stats mStats = {};
            std::unique_ptr<main_thread_queue> mMainThreadQueue = nullptr;
            std::vector<std::unique_ptr<engine_subsystem>> mSubsystems = {};       // startup order
            std::vector<std::unique_ptr<engine_subsystem>> mUserSubsystems = {};   // added before initialise()
            std::unique_ptr<engine_services> mServices = nullptr;                   // owns every module object
            std::unique_ptr<frame_loop> mFrameLoop = nullptr;
            std::unique_ptr<frame_pacer> mPacer = nullptr;
            std::unique_ptr<core::clock> mOwnedClock = nullptr;
            core::clock *mClock = nullptr;
            application_state mState = application_state::CREATED;
            simulation_mode mSimulationMode = simulation_mode::RUNNING;
            exit_code mExitCode = exit_code::OK;
            usize mStartedSubsystemCount = 0;
            usize mUserSubsystemBegin = 0;                              // first user subsystem in mSubsystems
            usize mUserSubsystemCount = 0;                              // user subsystems moved in at initialise()
            u32 mPendingFixedSteps = 0;                                 // step_fixed(): steps the next frame runs
            bool mFixedStepsPending = false;
            bool mQuitRequested = false;
            bool mInFrame = false;
            bool mAcceptingModuleSubsystems = false;                    // true during startup step 15 only

        public:
            explicit application(application_desc desc);

            /// <summary>
            /// Shuts down when the owner forgot (shutdown is idempotent).
            /// </summary>
            ~application();

            application(const application &) = delete;
            application &operator=(const application &) = delete;

        public:
            /// <summary>
            /// Starts every subsystem in the documented order (3.4). On the first failure the already-started
            /// subsystems are shut down in reverse and the error is returned; the object is then reusable.
            /// ALREADY_INITIALISED when the state is not CREATED or STOPPED.
            /// </summary>
            [[nodiscard]] core::result<void> initialise();

            /// <summary>
            /// Runs exactly one frame (3.1). Public so tests, the editor step button and the AI command server
            /// drive the same code as run(). Returns an error only for unrecoverable runtime failures;
            /// NOT_INITIALISED (asserted) when the application is not RUNNING.
            /// </summary>
            [[nodiscard]] core::result<void> step_frame();

            /// <summary>
            /// As step_frame(), but deltaSeconds replaces the clock / forced delta for this frame (tests, R13),
            /// except on the first frame, which always runs exactly fixedDeltaSeconds so a run is deterministic
            /// from frame 0 (06 2.1). The value is clamped like any frame delta: a negative or non-finite delta
            /// is a zero-length frame.
            /// </summary>
            [[nodiscard]] core::result<void> step_frame(f64 deltaSeconds);

            /// <summary>
            /// Editor play.step: the next frame runs exactly fixedSteps fixed steps regardless of the clock
            /// (its frame delta is fixedSteps * fixedDeltaTime) and leaves the mode PAUSED afterwards (R13).
            /// </summary>
            void step_fixed(u32 fixedSteps) noexcept;

            /// <summary>
            /// Drops a step_fixed request the next frame has not consumed yet, so that frame runs as the
            /// simulation mode says. Editor play.stop calls it: a forced step overrides PAUSED, and a step
            /// left behind would run the simulation over the restored editing scene. A no-op without a request.
            /// </summary>
            void cancel_fixed_steps() noexcept;

            /// <summary>
            /// Frames until quit or maxFrames, then shutdown(). Returns the process exit code; a step_frame
            /// failure logs, sets RUNTIME_FAILURE and stops. INIT_FAILURE when the application was never
            /// initialised (the caller forgot initialise()).
            /// </summary>
            [[nodiscard]] int run();

            /// <summary>
            /// Shutdown in the documented order (3.5). Idempotent; a CREATED application has nothing to do.
            /// </summary>
            void shutdown();

            /// <summary>
            /// Asks run() to stop after the current frame; the first code wins unless a later one is worse
            /// (OK &lt; INIT_FAILURE &lt; RUNTIME_FAILURE &lt; ... in enumerator order).
            /// </summary>
            void request_quit(exit_code code = exit_code::OK) noexcept;

            [[nodiscard]] bool is_quit_requested() const noexcept {
                return mQuitRequested;
            }

            /// <summary>
            /// Thread-safe. The task runs on the main thread before the next FRAME_BEGIN (frame step 4); a
            /// task posted during the drain runs in the next frame. Pending tasks are dropped at shutdown.
            /// </summary>
            void post_to_main_thread(std::function<void()> task);

            /// <summary>
            /// Executables and plugins add subsystems before initialise() only (a module entry may add one
            /// during startup step 15, through the application service); afterwards the call asserts and is
            /// ignored. A null pointer asserts and is ignored. Startup order (06 3.4): the built-in adapters,
            /// then the subsystems module entries added (step 15), then the ones added here in push order
            /// (step 16); shutdown reverses it.
            /// </summary>
            void add_subsystem(std::unique_ptr<engine_subsystem> subsystem);

            /// <summary>
            /// Makes the next frameCount render_frame results fail with 'error' (the renderer still runs; its
            /// result is replaced). Drives the failure rules of 06 section 6 in headless tests the way
            /// rhi::device::simulate_device_loss() drives device loss. Ignored (asserted) when not RUNNING;
            /// no effect without a renderer.
            /// </summary>
            void inject_render_failure(core::error error, u32 frameCount);

            /// <summary>
            /// Sets engine_time::timeScale from the next frame on; negative and non-finite values (NaN, infinity)
            /// become zero.
            /// </summary>
            void set_time_scale(f32 timeScale) noexcept;

            void set_simulation_mode(simulation_mode mode) noexcept;

            [[nodiscard]] simulation_mode current_simulation_mode() const noexcept {
                return mSimulationMode;
            }

            [[nodiscard]] engine_context &context() noexcept {
                return mContext;
            }

            [[nodiscard]] const engine_context &context() const noexcept {
                return mContext;
            }

            [[nodiscard]] const engine_time &time() const noexcept {
                return mTime;
            }

            [[nodiscard]] const frame_stats &stats() const noexcept {
                return mStats;
            }

            [[nodiscard]] layer_stack &layers() noexcept {
                return mLayers;
            }

            [[nodiscard]] application_state state() const noexcept {
                return mState;
            }

            /// <summary>
            /// The exit code run() will return (OK until a quit request or a failure).
            /// </summary>
            [[nodiscard]] exit_code current_exit_code() const noexcept {
                return mExitCode;
            }

            /// <summary>
            /// The descriptor the application was built from (the project lives in it).
            /// </summary>
            [[nodiscard]] const application_desc &desc() const noexcept {
                return mDesc;
            }

            /// <summary>
            /// Every subsystem in startup order: the built-in adapters, then the user-added ones. Empty before
            /// initialise() and after shutdown().
            /// </summary>
            [[nodiscard]] std::span<const std::unique_ptr<engine_subsystem>> subsystems() const noexcept {
                return mSubsystems;
            }

            /// <summary>
            /// True while a step_frame() is executing (module loading is refused inside a frame).
            /// </summary>
            [[nodiscard]] bool is_inside_frame() const noexcept {
                return mInFrame;
            }

            /// <summary>
            /// The clock frames are timed with: the injected override or the owned steady clock.
            /// </summary>
            [[nodiscard]] core::clock &clock() noexcept {
                return *mClock;
            }

        private:
            [[nodiscard]] core::result<void> start_core();
            void stop_core();
            [[nodiscard]] core::result<void> start_subsystems();
            void stop_subsystems();
            [[nodiscard]] core::result<void> load_startup_scene();
            [[nodiscard]] core::result<void> run_frame(std::optional<f64> deltaOverride);
            void raise_exit_code(exit_code code) noexcept;
    };
}
