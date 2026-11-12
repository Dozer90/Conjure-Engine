#include "conjure/testing/headless_engine.h"

#include "conjure/testing/doctest.h"
#include "conjure/testing/log_category.h"
#include "conjure/testing/temp_directory.h"
#include "conjure/testing/test_environment.h"

#include "conjure/core/assert.h"
#include "conjure/core/error.h"
#include "conjure/core/hash.h"
#include "conjure/core/log.h"
#include "conjure/core/memory/allocation_counters.h"
#include "conjure/core/string_id.h"
#include "conjure/core/uuid.h"
#include "conjure/core/vfs/file_system.h"
#include "conjure/core/vfs/native_file_system.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/world_hash.h"
#include "conjure/engine/application_desc.h"
#include "conjure/engine/engine_context.h"
#include "conjure/engine/engine_subsystem.h"
#include "conjure/engine/engine_time.h"
#include "conjure/engine/project.h"
#include "conjure/platform/window.h"
#include "conjure/rhi/device.h"

#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
    #include "conjure/audio/audio_engine.h"
#endif
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    #include "conjure/scripting/script_host.h"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <format>
#include <limits>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifndef CONJURE_TEST_SHADER_DIR
    #define CONJURE_TEST_SHADER_DIR ""
#endif
#ifndef CONJURE_TEST_MANAGED_DIR
    #define CONJURE_TEST_MANAGED_DIR ""
#endif

using namespace conjure;
using namespace conjure::testing;

/*===========================================================================*/
/*===========================================================================*/
// Helpers

namespace {
    constexpr std::string_view SCHEME_SEPARATOR = "://";
    constexpr usize MAX_ERROR_TEXT_LENGTH = 511;

    // Names inside every standard library's small-string buffer (15 characters) and names well beyond it:
    // interning a long one costs the table node plus the string's heap block (plus a container proxy in
    // MSVC Debug), interning a short one only the node. The larger of the two steady-state costs is the
    // per-name budget. Several of each because the first interning of a process can carry one-time work
    // (a table rehash, lazily created machinery) that is not what a name costs: the minimum over the
    // samples is the steady state.
    constexpr std::string_view WARM_UP_SENTINEL_NAME = "conjure.testing.headless_engine.sentinel.warm_up";
    constexpr std::string_view SHORT_SENTINEL_NAMES[] = {"cj.tst.sentin.a", "cj.tst.sentin.b", "cj.tst.sentin.c"};
    constexpr std::string_view LONG_SENTINEL_NAMES[] = {
        "conjure.testing.headless_engine.sentinel.long_name.a",
        "conjure.testing.headless_engine.sentinel.long_name.b",
        "conjure.testing.headless_engine.sentinel.long_name.c",
    };

    usize sAllocationsPerInternedName = 0;

    /// <summary>
    /// The user subsystem that carries headless_engine_options::registerSystems into startup step 18: it
    /// owns nothing and only forwards register_systems to the test's function.
    /// </summary>
    class test_systems_subsystem final : public engine::engine_subsystem {
        private:
            register_systems_fn mRegister = nullptr;

        public:
            explicit test_systems_subsystem(register_systems_fn registerSystems) noexcept :
                mRegister(registerSystems) {}

            ~test_systems_subsystem() override = default;

        public:
            [[nodiscard]] std::string_view name() const noexcept override {
                return "test_systems";
            }

            [[nodiscard]] core::result<void> startup(engine::engine_context &) override {
                return core::result<void>::success();
            }

            void register_systems(ecs::scheduler &scheduler, engine::engine_context &context) override {
                if (mRegister != nullptr) {
                    mRegister(scheduler, context);
                }
            }

            void shutdown(engine::engine_context &) override {}
    };

    /// <summary>
    /// Live allocations one interning of 'name' adds to the process (0 when the name is already known).
    /// </summary>
    [[nodiscard]] usize interning_cost(std::string_view name) {
        const core::memory::memory_snapshot before = core::memory::snapshot();
        CONJURE_UNUSED(core::string_id::intern(name));
        const core::memory::memory_snapshot after = core::memory::snapshot();
        if (after.live_count() <= before.live_count()) {
            return 0;
        }
        return static_cast<usize>(after.live_count() - before.live_count());
    }

    /// <summary>
    /// The smallest cost over a set of names: what one interning costs once the table is in steady state.
    /// </summary>
    [[nodiscard]] usize steady_interning_cost(std::span<const std::string_view> names) {
        usize smallest = std::numeric_limits<usize>::max();
        for (const std::string_view name : names) {
            smallest = std::min(smallest, interning_cost(name));
        }
        return smallest;
    }

    /// <summary>
    /// Measures, once per process, what interning a name costs here instead of guessing a number that
    /// differs between standard libraries and configurations (08_testing.md D6). Every sentinel is
    /// interned before the first fixture takes its baseline, so none of them counts against a run.
    /// </summary>
    void measure_interned_name_cost() {
        static std::once_flag sOnce = {};
        std::call_once(sOnce, []() {
            CONJURE_UNUSED(interning_cost(WARM_UP_SENTINEL_NAME));
            const usize shortCost = steady_interning_cost(SHORT_SENTINEL_NAMES);
            const usize longCost = steady_interning_cost(LONG_SENTINEL_NAMES);
            sAllocationsPerInternedName = std::max(shortCost, longCost);
            CONJURE_LOG_DEBUG(
              LOG_TESTING,
              "headless_engine: interning costs {} allocation(s) per short name and {} per long name here",
              shortCost,
              longCost
            );
        });
    }

    /// <summary>
    /// The first engine start in a process fills caches that live until the process ends: interned
    /// component and system names, the reflection registry, the component type caches, the main
    /// thread's random engine. They would show up as leaks of the first fixture, so one throwaway
    /// application per distinct configuration is started and destroyed before a fixture takes its
    /// memory baseline. Under CTest (one case per process) that is one warm-up per case.
    /// </summary>
    void warm_up_process(const engine::application_desc &warmDesc, register_systems_fn registerSystems) {
        struct warmed_start {
            engine::engine_config config = {};
            register_systems_fn registerSystems = nullptr;
        };
        static std::mutex sMutex = {};
        static std::vector<warmed_start> sWarmed = {};

        const std::scoped_lock lock(sMutex);
        const auto same = [&](const warmed_start &entry) {
            return entry.registerSystems == registerSystems && entry.config == warmDesc.config;
        };
        if (std::find_if(sWarmed.begin(), sWarmed.end(), same) != sWarmed.end()) {
            return;
        }
        sWarmed.push_back(warmed_start{warmDesc.config, registerSystems});

        // The whole descriptor, project included: a project's modules load only with the project present
        // (modules_subsystem allows shared libraries for a headless host with a project), so the warm-up
        // must start the same thing the fixture starts; the test's systems are registered too, so the
        // per-type caches their component types fill on first use are paid here.
        engine::application_desc desc = warmDesc;
        engine::application throwaway(std::move(desc));
        if (registerSystems != nullptr) {
            throwaway.add_subsystem(std::make_unique<test_systems_subsystem>(registerSystems));
        }
        const core::result<void> started = throwaway.initialise();
        if (!started.has_value()) {
            // The real start below reports the same failure with the case's REQUIRE; nothing to warm.
            return;
        }
        throwaway.shutdown();
    }

    /// <summary>
    /// The first successful native file read of a process pays a one-time cost of about a thousand
    /// allocations that live until exit (the C++ stream and locale machinery). A scene loaded from a
    /// native path inside a fixture would be charged for it, so the first fixture of a process reads one
    /// scratch file before taking its baseline.
    /// </summary>
    void warm_up_native_file_io() {
        static std::once_flag sOnce = {};
        std::call_once(sOnce, []() {
            const temp_directory scratch("headless_engine_warm_up");
            const core::result<void> written = scratch.write_text("warm_up.txt", "warm");
            if (!written.has_value()) {
                CONJURE_LOG_WARN(
                  LOG_TESTING,
                  "headless_engine: warm-up file not written: {}",
                  written.get_error().to_string()
                );
                return;
            }
            core::vfs::native_file_system files(scratch.path(), true);
            CONJURE_UNUSED(files.read_text_file("warm_up.txt"));
        });
    }

    /// <summary>
    /// Copies the text of the first ERROR or FATAL entry into fixed storage (truncated), so the failure can
    /// be reported after the run's allocations are gone without holding heap memory across the memory check.
    /// </summary>
    void copy_first_error(const log_capture &logs, std::array<char, MAX_ERROR_TEXT_LENGTH + 1> &out) {
        const std::vector<captured_log_entry> entries = logs.entries();
        for (const captured_log_entry &entry : entries) {
            if (entry.level < core::log_level::ERR) {
                continue;
            }
            const usize length = std::min(entry.text.size(), MAX_ERROR_TEXT_LENGTH);
            std::memcpy(out.data(), entry.text.data(), length);
            out[length] = '\0';
            return;
        }
    }

#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    /// <summary>
    /// The DOTNET_RUNTIME precondition (08_testing.md 2.2, R28): hostfxr found and the managed SDK this
    /// tree built next to it (the real host cannot start without Conjure.Managed.dll); nothing is started.
    /// </summary>
    bool dotnet_runtime_available() noexcept {
        static const bool available = []() noexcept {
            const std::filesystem::path managedDirectory(CONJURE_TEST_MANAGED_DIR);
            if (managedDirectory.empty()) {
                return false;
            }
            std::error_code probe = {};
            const bool sdkPresent = std::filesystem::is_regular_file(managedDirectory / "Conjure.Managed.dll", probe);
            return sdkPresent && scripting::is_runtime_available();
        }();
        return available;
    }

    struct probe_installer {
        probe_installer() {
            set_precondition_probe(precondition::DOTNET_RUNTIME, &dotnet_runtime_available);
        }
    };

    // The engine support library is the one that can ask the scripting host (test_environment.h); the
    // installer lives in this translation unit because every user of the library references it.
    const probe_installer sProbeInstaller = {};
#endif
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

headless_engine::headless_engine() :
    headless_engine(headless_engine_options{}) {}

headless_engine::headless_engine(const headless_engine_options &options) :
    mOptions(options) {
    start();
}

headless_engine::~headless_engine() {
    if (mApplication != nullptr) {
        mApplication->shutdown();
        mApplication.reset();
    }
    mWindow = nullptr;
    mDevice = nullptr;
#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
    mAudioBackend = nullptr;
#endif

    // An unallowed ERROR fails the case exactly once, below, after the memory check. The capture's own
    // destructor check is suppressed because doctest keeps a failed assertion's record alive, which the
    // memory check would then report as a second, bogus leak; the offending text is kept in fixed storage
    // for the same reason.
    std::array<char, MAX_ERROR_TEXT_LENGTH + 1> firstError = {};
    u32 errorCount = 0;
    if (mRun != nullptr) {
        if (!mRun->logs.errors_allowed()) {
            errorCount = mRun->logs.count(core::log_level::ERR) + mRun->logs.count(core::log_level::FATAL);
            if (errorCount > 0) {
                copy_first_error(mRun->logs, firstError);
            }
            mRun->logs.allow_errors();
        }
        // Everything the run allocated and the fixture still holds goes before the check.
        mRun.reset();
    }

    if (mCheckMemory) {
        check_memory();
    }
    disarm_memory_guard();

    if (errorCount > 0) {
        FAIL_CHECK(
          "headless_engine: " << errorCount << " ERROR/FATAL message(s) were logged during the run and "
          "allowErrors was not set; the first: " << std::string_view(firstError.data())
        );
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Scenes

core::result<scene::scene_id> headless_engine::load_scene(std::string_view pathOrUuid, scene::load_mode mode) {
    const std::optional<core::uuid> id = core::uuid::parse(pathOrUuid);
    if (id.has_value() && !id->is_null()) {
        return scenes().load(*id, mode);
    }

    core::vfs::file_system &mounts = *context().filesystem;
    if (pathOrUuid.find(SCHEME_SEPARATOR) != std::string_view::npos) {
        return scenes().load_file(mounts, pathOrUuid, mode);
    }

    const std::filesystem::path nativePath(pathOrUuid);
    if (nativePath.is_absolute()) {
        std::error_code ignored = {};
        if (!std::filesystem::is_regular_file(nativePath, ignored)) {
            return core::make_error(core::error_code::NOT_FOUND, "scene file '{}' does not exist", pathOrUuid);
        }
        core::vfs::native_file_system files(nativePath.parent_path(), true);
        return scenes().load_file(files, nativePath.filename().generic_string(), mode);
    }

    // Project-relative: the project root first, then the assets mount a project provides.
    const std::string projectPath = std::format("project://{}", pathOrUuid);
    if (mounts.exists(projectPath)) {
        return scenes().load_file(mounts, projectPath, mode);
    }
    const std::string assetsPath = std::format("assets://{}", pathOrUuid);
    if (mounts.exists(assetsPath)) {
        return scenes().load_file(mounts, assetsPath, mode);
    }
    return core::make_error(
      core::error_code::NOT_FOUND,
      "scene '{}' was not found under project:// or assets:// (nor is it a uuid or an absolute path)",
      pathOrUuid
    );
}

core::result<scene::scene_id> headless_engine::load_scene_json(std::string_view sceneJson, scene::load_mode mode) {
    const std::string name = std::format("scene_{}.scene.json", mScratchSceneCount);
    ++mScratchSceneCount;
    CONJURE_TRY(mRun->scratch.write_text_file(name, sceneJson));
    CONJURE_LOG_DEBUG(LOG_TESTING, "headless_engine: loading inline scene JSON as {}", name);
    return scenes().load_file(mRun->scratch, name, mode);
}


/*===========================================================================*/
/*===========================================================================*/
// Stepping

void headless_engine::step_frames(u32 frameCount) {
    step_frames(frameCount, mOptions.fixedDeltaSeconds);
}

void headless_engine::step_frames(u32 frameCount, f64 deltaSeconds) {
    // events_for() delivers by exact frame, so an event scheduled for a frame that already ran would be
    // dropped without a word. Each late batch fails the case once; the memory check goes off because
    // doctest keeps the failure's record alive.
    const u64 late = mRun->input.late_event_count();
    if (late > mLateEventsReported) {
        const u64 newlyLate = late - mLateEventsReported;
        mLateEventsReported = late;
        mCheckMemory = false;
        FAIL_CHECK(
          "headless_engine: " << newlyLate << " input event(s) were scheduled for a frame before " << mFrameIndex
          << ", which already ran, and are never delivered; input_script frames are absolute (schedule at "
          "frame_index() + n)"
        );
    }

    for (u32 frame = 0; frame < frameCount; ++frame) {
        for (const platform::input_event &event : mRun->input.events_for(mFrameIndex)) {
            mWindow->push_event(platform::to_event(event));
        }
        const core::result<void> stepped = mApplication->step_frame(deltaSeconds);
        if (!stepped.has_value()) {
            // The REQUIRE below leaves the case through an exception and its record stays allocated; the
            // memory check would report that as a second failure of a case that already failed.
            mCheckMemory = false;
            REQUIRE_MESSAGE(false, "headless_engine: frame " << mFrameIndex << " failed: " << describe_error(stepped));
            return;
        }
        ++mFrameIndex;
        mRun->input.close_frames_before(mFrameIndex);
    }
    refresh_stats();
}

void headless_engine::step_seconds(f64 seconds) {
    const f64 frames = std::round(seconds / mOptions.fixedDeltaSeconds);
    if (frames <= 0.0) {
        return;   // zero, negative and negative infinity: nothing to step
    }
    if (!std::isfinite(frames)) {
        CONJURE_ASSERT(false, "headless_engine::step_seconds: {} seconds is not a finite frame count", seconds);
        return;
    }
    if (frames > static_cast<f64>(std::numeric_limits<u32>::max())) {
        CONJURE_ASSERT(false, "headless_engine::step_seconds: {} seconds is more frames than a u32 holds", seconds);
        return;
    }
    step_frames(static_cast<u32>(frames));
}


/*===========================================================================*/
/*===========================================================================*/
// Access

bool headless_engine::renderer_enabled() const noexcept {
    return mApplication != nullptr && mApplication->context().renderer != nullptr;
}

#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
const audio::null_backend_stats &headless_engine::audio_stats() const noexcept {
    if (mAudioBackend == nullptr) {
        return mRun->noAudioStats;
    }
    return mAudioBackend->stats();
}
#endif

u64 headless_engine::fixed_update_count() const noexcept {
    return mApplication->time().fixedStepIndex;
}

u64 headless_engine::world_hash() const {
    return ecs::world_hash(world());
}

ecs::entity headless_engine::find_entity(std::string_view name) const {
    const scene::scene &active = mApplication->context().scenes->active();
    if (name.find('/') != std::string_view::npos) {
        return active.find_path(name);
    }

    // from_hash spells the lookup without interning the test's string into the process-wide table.
    std::vector<ecs::entity> matches = {};
    active.find_by_name(core::string_id::from_hash(core::hash::fnv1a64(name)), matches);
    if (matches.empty()) {
        return ecs::NULL_ENTITY;
    }
    return matches.front();
}

usize headless_engine::allocations_per_interned_name() noexcept {
    return sAllocationsPerInternedName;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

void headless_engine::start() {
    // Nothing allocated before the baseline may outlive it, or its release during the run would read as
    // a negative delta: the warm-up works on its own copy of the descriptor inside this scope, and the
    // descriptor the application keeps is built again after the baseline. (The caller's options object
    // is trivially copyable for the same reason; see headless_engine.h.)
    {
        const core::result<engine::application_desc> warmDesc = make_desc();
        if (!warmDesc.has_value()) {
            mCheckMemory = false;
            disarm_memory_guard();
            REQUIRE_MESSAGE(false, "headless_engine: project load failed: " << describe_error(warmDesc));
            return;
        }
        // The .NET host keeps its heap for the rest of the process; only the null host is leak-checkable.
        const engine::scripting_settings &scripting = warmDesc->config.scripting;
        mCheckMemory = mOptions.checkMemory && !(scripting.enabled && !scripting.useNullHost);
        measure_interned_name_cost();
        warm_up_native_file_io();
        warm_up_process(*warmDesc, mOptions.registerSystems);
    }
    mMemoryGuard.reset_baseline();
    mInternedAtBaseline = core::string_id::interned_count();

    mRun = std::make_unique<run_state>();
    if (mOptions.allowErrors) {
        mRun->logs.allow_errors();
    }
    core::result<engine::application_desc> desc = make_desc();
    if (!desc.has_value()) {
        // A REQUIRE from the constructor skips the destructor: the guard and the capture are disarmed here
        // so the case shows this one failure and not their follow-ups.
        mCheckMemory = false;
        mRun->logs.allow_errors();
        disarm_memory_guard();
        REQUIRE_MESSAGE(false, "headless_engine: project load failed: " << describe_error(desc));
        return;
    }
    mApplication = std::make_unique<engine::application>(std::move(desc).value());
    if (mOptions.registerSystems != nullptr) {
        mApplication->add_subsystem(std::make_unique<test_systems_subsystem>(mOptions.registerSystems));
    }
    const core::result<void> started = mApplication->initialise();
    if (!started.has_value()) {
        mApplication.reset();
        mCheckMemory = false;
        mRun->logs.allow_errors();
        disarm_memory_guard();
        REQUIRE_MESSAGE(false, "headless_engine: application start failed: " << describe_error(started));
        return;
    }

    engine::engine_context &services = mApplication->context();
    mWindow = services.window != nullptr ? services.window->as_null() : nullptr;
    if (mWindow == nullptr) {
        mCheckMemory = false;
        mRun->logs.allow_errors();
        disarm_memory_guard();
        REQUIRE_MESSAGE(false, "headless_engine: the application did not create the null window");
        return;
    }
    mDevice = services.device;
#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
    if (services.audio != nullptr) {
        mAudioBackend = services.audio->null_backend_instance();
    }
#endif
    if (mOptions.enableRenderer && services.renderer == nullptr) {
        CONJURE_LOG_WARN(
          LOG_TESTING,
          "headless_engine: the renderer was requested but is not running (no shader index in this build, "
          "CONJURE_TEST_SHADER_DIR is empty, or the renderer is compiled out); rhi_stats() stays zero"
        );
    }
    refresh_stats();
}

void headless_engine::disarm_memory_guard() {
    // A fresh baseline and an immediate (passing) check: the guard's destructor then stays silent whatever
    // doctest allocates for the failure reported right after this call.
    mMemoryGuard.reset_baseline();
    mMemoryGuard.check();
}

void headless_engine::check_memory() {
    // Interned names are permanent by design (string_id.h); a delta inside the measured per-name budget is
    // not a leak. Anything else, including a negative delta (something allocated before the baseline was
    // released during the run), fails the case naming every number that went into the verdict.
    const i64 delta = mMemoryGuard.live_delta();
    const usize interned = core::string_id::interned_count();
    const usize newNames = interned > mInternedAtBaseline ? interned - mInternedAtBaseline : 0;
    const i64 budget = static_cast<i64>(newNames * sAllocationsPerInternedName);
    const bool withinBudget = delta >= 0 && delta <= budget;
    CHECK_MESSAGE(
      withinBudget,
      "headless_engine: " << delta << " allocation(s) still live relative to the baseline (positive = leak); "
      << newNames << " name(s) were interned during the run, which tolerates up to " << budget
      << " (" << sAllocationsPerInternedName << " per name)"
    );
    if (withinBudget && delta > 0) {
        CONJURE_LOG_DEBUG(
          LOG_TESTING,
          "headless_engine: {} allocation(s) attributed to {} newly interned name(s)",
          delta,
          newNames
        );
    }
}

core::result<engine::application_desc> headless_engine::make_desc() const {
    engine::application_desc desc = {};
    desc.config = build_config();
    if (mOptions.projectDirectory == nullptr) {
        return desc;
    }
    CONJURE_TRY_ASSIGN(engine::project loaded, engine::project::load(*mOptions.projectDirectory));
    desc.config.projectPath = loaded.root_directory();
    loaded.apply_to(desc.config);
    desc.project = std::move(loaded);
    return desc;
}

engine::engine_config headless_engine::build_config() const {
    engine::engine_config config = engine::engine_config::headless_defaults();
    config.logLevel = core::log_level::WARN;
    config.modules.enginePlugins = false;

    const f32 fixedDelta = static_cast<f32>(mOptions.fixedDeltaSeconds);
    config.time.fixedDeltaSeconds = fixedDelta;
    config.time.forcedFrameDeltaSeconds = fixedDelta;
    config.time.maxFrameDeltaSeconds = std::max(
      config.time.maxFrameDeltaSeconds,
      fixedDelta * static_cast<f32>(config.time.maxFixedStepsPerFrame)
    );

    // 0 is the engine's inline job mode (engine_config.h, 08_testing.md D4): every job runs on the calling
    // thread, the determinism baseline of test_determinism.
    u32 workers = mOptions.jobThreadCount;
    if (workers == std::numeric_limits<u32>::max()) {
        workers = environment().jobThreadCount;
    }
    config.jobs.workerCount = workers;

    const std::filesystem::path shaderDirectory(CONJURE_TEST_SHADER_DIR);
    config.rendering.enabled = false;
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    if (mOptions.enableRenderer && !shaderDirectory.empty()) {
        config.rendering.enabled = true;
        config.rendering.backend = engine::rhi_backend_choice::NULL_DEVICE;
        config.rendering.gpuValidation = false;
        config.rendering.shaderDirectory = shaderDirectory;
    }
#endif

    config.physics.enabled = mOptions.enablePhysics;
    config.audio.enabled = mOptions.enableAudio;
    config.audio.backend = engine::audio_backend_choice::NULL_DEVICE;

    config.scripting.enabled = mOptions.enableScripting;
    config.scripting.required = true;
    config.scripting.useNullHost = mOptions.useNullScriptHost;
    config.scripting.hotReload = false;
    if (mOptions.gameAssembly != nullptr) {
        config.scripting.gameAssembly = *mOptions.gameAssembly;
    }
    // The managed SDK this tree built, wherever the test executable lives (the engine's default is the
    // directory next to the executable, which only holds the SDK when the executable sits in bin/).
    config.scripting.managedDirectory = std::filesystem::path(CONJURE_TEST_MANAGED_DIR);
    return config;
}

void headless_engine::refresh_stats() {
    if (mDevice == nullptr) {
        return;
    }
    const std::optional<rhi::null_device_stats> stats = mDevice->null_stats();
    if (stats.has_value()) {
        mRun->rhiStats = *stats;
    }
}
