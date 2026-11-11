#pragma once

#include "conjure/testing/input_script.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/memory_guard.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/vfs/memory_file_system.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/application.h"
#include "conjure/engine/application_desc.h"
#include "conjure/engine/diagnostics/check_registry.h"
#include "conjure/engine/engine_config.h"
#include "conjure/platform/null_backend.h"
#include "conjure/rhi/null_device_stats.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"

#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
    #include "conjure/audio/null_backend.h"
#endif

#include <filesystem>
#include <limits>
#include <memory>
#include <string_view>
#include <type_traits>

namespace conjure::ecs {
    class scheduler;
}

// The headless engine fixture (docs/design/08_testing.md 2.3, 3.3, 4.4, D5, D6): an engine::application
// on the null window, the null rhi device, the null audio backend and (opt-in) the script runtime,
// stepped frame by frame with an explicit delta so one frame at 1/60 s is exactly one fixed update.
//
//   headless_engine engine = {};
//   CONJURE_REQUIRE_OK(engine.load_scene(repo_asset("scenes/roundtrip.scene.json").string()));
//   engine.step_frames(60);
//   CHECK(engine.fixed_update_count() == 60);
//
// The fixture owns a log_capture and a memory_guard (D6). Construction fails the case with the engine's
// error message (REQUIRE), never a crash, and reports that one failure only (the memory check is disarmed
// first). Destruction destroys the application (which joins the job workers), then checks that the live
// allocation count is back at its baseline and that no ERROR was logged unless allowErrors was set; a
// run that logged an unallowed ERROR fails once, with the first offending message, never a second time
// through the memory check. Names interned during the run (a loaded scene's entity names, a system
// registered by the test) live in the process-wide string_id table for good and are not leaks: the check
// tolerates, per newly interned name, the number of live allocations one interning costs in this process,
// which the first fixture measures by interning one short and one long sentinel name (two or three
// allocations depending on the standard library and configuration, never guessed). Accessors return
// non-owning references valid until the fixture dies (8.2). Every call is main-thread only, and only one
// fixture may be alive at a time: the platform allows one live window per process (11_platform.md), so a
// second construction asserts.

namespace conjure::testing {

    /// <summary>
    /// A test's systems, registered at startup step 18 next to the engine's own (before the scheduler is
    /// finalised): the determinism and phase scenarios add spawners and probes this way.
    /// </summary>
    using register_systems_fn = void (*)(ecs::scheduler &scheduler, engine::engine_context &context);

    struct headless_engine_options {
        u32 jobThreadCount = std::numeric_limits<u32>::max();   // max = environment().jobThreadCount; 0 = inline
        f64 fixedDeltaSeconds = 1.0 / 60.0;
        bool enableRenderer = true;                             // renderer on the null rhi device
        bool enablePhysics = true;
        bool enableAudio = true;                                // audio::null_backend
        bool enableScripting = false;                           // starts the .NET host; opt in per case
        bool useNullScriptHost = false;                         // with enableScripting: the C++ fake host instead
        bool allowErrors = false;                               // forwarded to log_capture
        bool checkMemory = true;                                // the memory_guard check at destruction (D6)
        const std::filesystem::path *projectDirectory = nullptr;   // non-owning, must outlive the fixture;
                                                                   // null = in-memory project, no asset scan
        const std::filesystem::path *gameAssembly = nullptr;       // non-owning; with enableScripting: the game
                                                                   // assembly the runtime loads (absolute path)
        register_systems_fn registerSystems = nullptr;             // the test's systems (step 18), or null
    };

    // The fixture is usually built from a temporary options object (headless_engine engine(make_options())),
    // which the caller destroys after the constructor has taken the memory baseline. Heap-owning members
    // (a std::string or std::filesystem::path allocates its iterator proxy in Debug builds even when empty)
    // would then read as a negative allocation delta, so the options stay trivially copyable.
    static_assert(std::is_trivially_copyable_v<headless_engine_options>);

    class headless_engine {
        private:
            /// <summary>
            /// Everything the run allocates and the fixture owns, the log capture included. Created after
            /// the memory baseline and destroyed before the check, so its storage never shows as a leak or
            /// as a negative delta.
            /// </summary>
            struct run_state {
                log_capture logs = {};                          // everything the run logs (D6)
                core::vfs::memory_file_system scratch = {};     // load_scene_json writes here
                input_script input = {};
                rhi::null_device_stats rhiStats = {};           // refreshed after every step
#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
                audio::null_backend_stats noAudioStats = {};    // returned while audio is off
#endif
            };

            headless_engine_options mOptions = {};
            memory_guard mMemoryGuard = {};
            std::unique_ptr<run_state> mRun = nullptr;
            std::unique_ptr<engine::application> mApplication = nullptr;
            platform::null_backend *mWindow = nullptr;          // owned by the application
            rhi::device *mDevice = nullptr;                     // nullptr when the renderer is off
#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
            audio::null_backend *mAudioBackend = nullptr;       // nullptr when audio is off
#endif
            u64 mFrameIndex = 0;
            usize mInternedAtBaseline = 0;                      // string_id::interned_count() at the baseline
            u32 mScratchSceneCount = 0;
            bool mCheckMemory = true;

        public:
            headless_engine();
            explicit headless_engine(const headless_engine_options &options);

            /// <summary>
            /// Shuts the application down, then runs the memory and log checks.
            /// </summary>
            ~headless_engine();

            headless_engine(const headless_engine &) = delete;
            headless_engine &operator=(const headless_engine &) = delete;

        public:
            /*---------------------------------------------------------------*/
            // Scenes (both loaders run zero frames)

            /// <summary>
            /// Loads a scene by asset uuid (through the asset manager), by scheme-qualified vfs path
            /// ("assets://scenes/a.scene.json"), by absolute native path (tests/assets fixtures) or by
            /// project-relative path ("scenes/a.scene.json" under project://). Fails with the scene manager's
            /// errors (LOAD_FAILED, BAD_FORMAT, ...) or NOT_FOUND / IO_FAILURE for a missing file; a uuid
            /// without a mounted project fails with LOAD_FAILED since no asset source can resolve it.
            /// </summary>
            [[nodiscard]] core::result<scene::scene_id> load_scene(
              std::string_view pathOrUuid,
              scene::load_mode mode = scene::load_mode::REPLACE
            );

            /// <summary>
            /// Loads scene JSON text as if it were a file (the text is written into the fixture's in-memory
            /// scratch file system first, so the scene has a record and can be unloaded). Fails with
            /// PARSE_FAILURE for malformed JSON and the scene format errors for a bad document.
            /// </summary>
            [[nodiscard]] core::result<scene::scene_id> load_scene_json(
              std::string_view sceneJson,
              scene::load_mode mode = scene::load_mode::REPLACE
            );

            /*---------------------------------------------------------------*/
            // Stepping

            /// <summary>
            /// Runs frameCount frames of fixedDeltaSeconds each (one fixed update per frame), delivering the
            /// input script's events for each frame first. A frame failure fails the case (REQUIRE). Zero
            /// frames is a no-op.
            /// </summary>
            void step_frames(u32 frameCount);

            /// <summary>
            /// Runs frameCount frames with an explicit delta (the engine clamps it like any frame delta).
            /// </summary>
            void step_frames(u32 frameCount, f64 deltaSeconds);

            /// <summary>
            /// step_frames(round(seconds / fixedDeltaSeconds)). Zero or negative seconds step nothing; a
            /// non-finite value or more frames than a u32 can count is a programming error that asserts and
            /// steps nothing.
            /// </summary>
            void step_seconds(f64 seconds);

            /*---------------------------------------------------------------*/
            // Access

            [[nodiscard]] engine::application &application() noexcept {
                return *mApplication;
            }

            [[nodiscard]] const engine::application &application() const noexcept {
                return *mApplication;
            }

            [[nodiscard]] engine::engine_context &context() noexcept {
                return mApplication->context();
            }

            [[nodiscard]] const engine::engine_context &context() const noexcept {
                return mApplication->context();
            }

            [[nodiscard]] ecs::world &world() noexcept {
                return *mApplication->context().world;
            }

            [[nodiscard]] const ecs::world &world() const noexcept {
                return *mApplication->context().world;
            }

            [[nodiscard]] scene::scene_manager &scenes() noexcept {
                return *mApplication->context().scenes;
            }

            /// <summary>
            /// The one scene over the world (scenes().active()).
            /// </summary>
            [[nodiscard]] scene::scene &active_scene() noexcept {
                return mApplication->context().scenes->active();
            }

            [[nodiscard]] const scene::scene &active_scene() const noexcept {
                return mApplication->context().scenes->active();
            }

            /// <summary>
            /// The virtual window the frames pump; simulate_* controls are delivered by the next frame.
            /// </summary>
            [[nodiscard]] platform::null_backend &window() noexcept {
                return *mWindow;
            }

            [[nodiscard]] input_script &input() noexcept {
                return mRun->input;
            }

            /// <summary>
            /// What the null device recorded over the whole run, refreshed after every step; all zero while
            /// the renderer is off.
            /// </summary>
            [[nodiscard]] const rhi::null_device_stats &rhi_stats() const noexcept {
                return mRun->rhiStats;
            }

            /// <summary>
            /// True when the renderer is running on the null device. False when enableRenderer was off, and
            /// also when it was on but the renderer could not start (no shader index in this build, or the
            /// renderer compiled out); that case logs a WARN on LOG_TESTING at construction.
            /// </summary>
            [[nodiscard]] bool renderer_enabled() const noexcept;

#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
            /// <summary>
            /// The null audio backend's snapshot of the last update; a default snapshot while audio is off.
            /// </summary>
            [[nodiscard]] const audio::null_backend_stats &audio_stats() const noexcept;
#endif

            [[nodiscard]] const engine::diagnostics::check_registry &checks() const noexcept {
                return *mApplication->context().checks;
            }

            [[nodiscard]] log_capture &logs() noexcept {
                return mRun->logs;
            }

            [[nodiscard]] const headless_engine_options &options() const noexcept {
                return mOptions;
            }

            /// <summary>
            /// Frames stepped through this fixture.
            /// </summary>
            [[nodiscard]] u64 frame_index() const noexcept {
                return mFrameIndex;
            }

            /// <summary>
            /// Fixed steps the engine ran since startup (engine_time::fixedStepIndex).
            /// </summary>
            [[nodiscard]] u64 fixed_update_count() const noexcept;

            /// <summary>
            /// ecs::world_hash of the world (08_testing.md 3.4).
            /// </summary>
            [[nodiscard]] u64 world_hash() const;

            /// <summary>
            /// The first entity named 'name' in world order, or the entity at a "Root/Child" hierarchy path
            /// when the name holds a '/'. NULL_ENTITY when nothing matches.
            /// </summary>
            [[nodiscard]] ecs::entity find_entity(std::string_view name) const;

            /// <summary>
            /// Live allocations the leak check tolerates per name interned during a run: what one interning
            /// costs in this process (measured once, before the first fixture takes its baseline). 0 before
            /// any fixture was constructed.
            /// </summary>
            [[nodiscard]] static usize allocations_per_interned_name() noexcept;

        private:
            void start();
            void disarm_memory_guard();
            void check_memory();
            [[nodiscard]] core::result<engine::application_desc> make_desc() const;
            [[nodiscard]] engine::engine_config build_config() const;
            void refresh_stats();
    };
}
