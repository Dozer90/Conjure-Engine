#include "conjure/engine/application.h"

#include "engine/engine_services.h"
#include "engine/executable_path.h"
#include "engine/frame_loop.h"
#include "engine/frame_pacer.h"
#include "engine/main_thread_queue.h"
#include "engine/subsystems/assets_subsystem.h"
#include "engine/subsystems/ecs_subsystem.h"
#include "engine/subsystems/modules_subsystem.h"
#include "engine/subsystems/platform_subsystem.h"
#include "engine/subsystems/rhi_subsystem.h"
#include "engine/subsystems/scene_subsystem.h"

#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
    #include "engine/subsystems/audio_subsystem.h"
#endif
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #include "engine/subsystems/physics_subsystem.h"
#endif
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "engine/subsystems/renderer_subsystem.h"
#endif
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    #include "engine/subsystems/scripting_subsystem.h"
#endif

#include "conjure/engine/engine_error.h"
#include "conjure/engine/engine_version.h"
#include "conjure/engine/log_category.h"
#include "conjure/engine/project.h"

#include "conjure/core/assert.h"
#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/log.h"
#include "conjure/core/vfs/memory_file_system.h"
#include "conjure/core/vfs/native_file_system.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <system_error>
#include <utility>

using namespace conjure;
using namespace conjure::engine;

namespace {

    /// <summary>
    /// The asset path of a startup scene: an already scheme-qualified path ("assets://scenes/x.scene.json")
    /// is used as given, anything else is relative to the assets root.
    /// </summary>
    [[nodiscard]] std::string asset_path_of(const std::filesystem::path &scene) {
        const std::string given = scene.generic_string();
        if (given.find("://") != std::string::npos) {
            return given;
        }
        std::string text = scene.lexically_normal().generic_string();
        while (!text.empty() && text.front() == '/') {
            text.erase(text.begin());
        }
        return "assets://" + text;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Free functions

std::string_view conjure::engine::to_string(application_state state) noexcept {
    switch (state) {
        case application_state::CREATED:
            return "CREATED";
        case application_state::STARTING:
            return "STARTING";
        case application_state::RUNNING:
            return "RUNNING";
        case application_state::STOPPING:
            return "STOPPING";
        case application_state::STOPPED:
            return "STOPPED";
    }
    return "";
}

std::string_view conjure::engine::to_string(simulation_mode mode) noexcept {
    switch (mode) {
        case simulation_mode::RUNNING:
            return "RUNNING";
        case simulation_mode::PAUSED:
            return "PAUSED";
        case simulation_mode::STEP_ONCE:
            return "STEP_ONCE";
    }
    return "";
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

application::application(application_desc desc) :
    mDesc(std::move(desc)),
    mMainThreadQueue(std::make_unique<main_thread_queue>()) {
    if (mDesc.clockOverride != nullptr) {
        mClock = mDesc.clockOverride;
    } else {
        mOwnedClock = std::make_unique<core::steady_clock>();
        mClock = mOwnedClock.get();
    }
    mTime.fixedDeltaTime = mDesc.config.time.fixedDeltaSeconds;
    mTime.timeScale = std::max(mDesc.config.time.timeScale, 0.0f);
    mContext.app = this;
    mContext.config = &mDesc.config;
    mContext.project = mDesc.project.has_value() ? &*mDesc.project : nullptr;
    mContext.time = &mTime;
    mContext.stats = &mStats;
}

application::~application() {
    shutdown();
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<void> application::initialise() {
    if (mState != application_state::CREATED && mState != application_state::STOPPED) {
        return make_error(
          engine_error::ALREADY_INITIALISED,
          std::format("initialise() while {}", to_string(mState))
        );
    }
    core::jobs::set_main_thread();

    // Steps 1-5.
    const core::result<void> validated = mDesc.config.validate();
    if (!validated.has_value()) {
        return validated;
    }
    mState = application_state::STARTING;
    mExitCode = exit_code::OK;
    mQuitRequested = false;
    mTime = {};
    mTime.fixedDeltaTime = mDesc.config.time.fixedDeltaSeconds;
    mTime.timeScale = std::max(mDesc.config.time.timeScale, 0.0f);
    mFixedClock = fixed_step_clock(mDesc.config.time.fixedDeltaSeconds, mDesc.config.time.maxFixedStepsPerFrame);
    mStats.clear();
    mSimulationMode = simulation_mode::RUNNING;
    mPendingFixedSteps = 0;
    mFixedStepsPending = false;

    const core::result<void> core = start_core();
    if (!core.has_value()) {
        stop_core();
        mState = application_state::CREATED;
        return core;
    }

    // Steps 6-20.
    const core::result<void> started = start_subsystems();
    if (!started.has_value()) {
        CONJURE_LOG_ERROR(LOG_ENGINE, "startup failed: {}", started.get_error().to_string());
        // A failure after step 19 (the startup scene) leaves the layers attached: detach them while the
        // window, device and module objects they point at are still alive.
        mLayers.detach_all(mContext);
        stop_subsystems();
        stop_core();
        mState = application_state::CREATED;
        return started;
    }

    mFrameLoop = std::make_unique<frame_loop>(*this, *mServices);
    mPacer = std::make_unique<frame_pacer>(*mClock, mDesc.config);
    mState = application_state::RUNNING;
    CONJURE_LOG_INFO(
      LOG_ENGINE,
      "{} {} running ({} subsystems)",
      engine_name(),
      engine_version_string(),
      mSubsystems.size()
    );
    return core::result<void>::success();
}

core::result<void> application::step_frame() {
    return run_frame(std::nullopt);
}

core::result<void> application::step_frame(f64 deltaSeconds) {
    // The override is passed through unconditionally: begin_frame() clamps a negative or non-finite delta to a
    // zero-length frame, which is what the caller asked for (the clock is never consulted).
    return run_frame(std::optional<f64>(deltaSeconds));
}

void application::step_fixed(u32 fixedSteps) noexcept {
    mPendingFixedSteps = fixedSteps;
    mFixedStepsPending = true;
}

void application::cancel_fixed_steps() noexcept {
    mPendingFixedSteps = 0;
    mFixedStepsPending = false;
}

int application::run() {
    if (mState != application_state::RUNNING) {
        CONJURE_LOG_ERROR(LOG_ENGINE, "run() on an application that was not initialised ({})", to_string(mState));
        return static_cast<int>(exit_code::INIT_FAILURE);
    }
    while (!mQuitRequested) {
        const core::result<void> stepped = step_frame();
        if (!stepped.has_value()) {
            CONJURE_LOG_ERROR(LOG_ENGINE, "frame {} failed: {}", mTime.frameIndex, stepped.get_error().to_string());
            raise_exit_code(exit_code::RUNTIME_FAILURE);
            break;
        }
    }
    shutdown();
    return static_cast<int>(mExitCode);
}

void application::shutdown() {
    if (mState != application_state::RUNNING && mState != application_state::STARTING) {
        return;
    }
    mState = application_state::STOPPING;
    CONJURE_LOG_INFO(LOG_ENGINE, "shutting down after {} frame(s)", mTime.frameIndex);

    // 1. Quiesce.
    if (mServices != nullptr) {
        mServices->jobs.wait_idle();
        if (mServices->device != nullptr) {
            mServices->device->wait_idle();
        }
        // 2. Scenes first: component destructors run while every owner is alive (R29).
        if (mServices->scenes != nullptr) {
            mServices->scenes->unload_all();
        }
        if (mServices->assetManager != nullptr) {
            CONJURE_UNUSED(mServices->assetManager->collect());
        }
    }

    // 3. Layers.
    mLayers.detach_all(mContext);

    // 4, 5. Subsystems then core.
    mPacer.reset();
    mFrameLoop.reset();
    stop_subsystems();
    stop_core();
    mState = application_state::STOPPED;
}

void application::request_quit(exit_code code) noexcept {
    mQuitRequested = true;
    raise_exit_code(code);
}

void application::post_to_main_thread(std::function<void()> task) {
    mMainThreadQueue->post(std::move(task));
}

void application::add_subsystem(std::unique_ptr<engine_subsystem> subsystem) {
    CONJURE_ASSERT(subsystem != nullptr, "add_subsystem(nullptr)");
    if (subsystem == nullptr) {
        return;
    }
    if (mState == application_state::CREATED || mState == application_state::STOPPED) {
        mUserSubsystems.push_back(std::move(subsystem));
        return;
    }
    if (mState == application_state::STARTING && mAcceptingModuleSubsystems) {
        mSubsystems.push_back(std::move(subsystem));
        return;
    }
    CONJURE_ASSERT(false, "add_subsystem() after initialise(): subsystems are added before startup only");
}

void application::inject_render_failure(core::error error, u32 frameCount) {
    CONJURE_ASSERT(
      mState == application_state::RUNNING,
      "inject_render_failure() on an application that is not RUNNING"
    );
    if (mFrameLoop == nullptr) {
        return;
    }
    mFrameLoop->inject_render_failure(std::move(error), frameCount);
}

void application::set_time_scale(f32 timeScale) noexcept {
    const f32 sanitised = std::isfinite(timeScale) && timeScale > 0.0f ? timeScale : 0.0f;
    mTime.timeScale = sanitised;
    mDesc.config.time.timeScale = sanitised;
}

void application::set_simulation_mode(simulation_mode mode) noexcept {
    mSimulationMode = mode;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

core::result<void> application::start_core() {
    const engine_config &config = mDesc.config;
    mServices = std::make_unique<engine_services>();
    engine_services &services = *mServices;

    // 1. Log.
    services.previousGlobalLevel = core::log::global_level();
    core::log::set_global_level(config.logLevel);
    if (!config.logFile.empty()) {
        services.logFileSink = std::make_shared<core::file_log_sink>(config.logFile);
        core::log::add_sink(services.logFileSink);
    }

    // 3. Jobs.
    core::jobs::job_system_desc jobsDesc = {};
    jobsDesc.workerCount = resolve_worker_count(config.jobs);
    const core::result<void> jobsStarted = services.jobs.start(jobsDesc);
    if (!jobsStarted.has_value()) {
        return make_error(
          engine_error::JOBS_START_FAILED,
          std::format("job system failed to start: {}", jobsStarted.get_error().to_string())
        );
    }

    // 4. Frame memory.
    services.frameMemory = std::make_unique<frame_memory>(config.memory);

    // 5. File systems and mounts.
    services.engineDirectory = executable_directory();
    std::error_code ignored = {};
    const std::filesystem::path engineContent = services.engineDirectory / "engine";
    if (std::filesystem::is_directory(engineContent, ignored)) {
        services.engineFileSystem = std::make_shared<core::vfs::native_file_system>(engineContent, true);
    } else {
        services.engineFileSystem = std::make_shared<core::vfs::memory_file_system>();
    }

    const project *loaded = mContext.project;
    services.cooked = loaded != nullptr && loaded->description().cooked;
    if (loaded != nullptr) {
        const std::filesystem::path &root = loaded->root_directory();
        if (!std::filesystem::is_directory(root, ignored)) {
            return make_error(
              engine_error::PROJECT_NOT_FOUND,
              std::format("project directory '{}' does not exist", root.generic_string())
            );
        }
        services.projectFileSystem = std::make_shared<core::vfs::native_file_system>(root, false);
        CONJURE_TRY(services.mounts.mount("project", services.projectFileSystem));
        if (services.cooked) {
            const std::filesystem::path cookedRoot = loaded->cooked_directory();
            if (!std::filesystem::is_directory(cookedRoot, ignored)) {
                return make_error(
                  engine_error::PROJECT_NOT_FOUND,
                  std::format("cooked root '{}' does not exist", cookedRoot.generic_string())
                );
            }
            services.cookedFileSystem = std::make_shared<core::vfs::native_file_system>(cookedRoot, true);
            CONJURE_TRY(services.mounts.mount("cooked", services.cookedFileSystem));
        } else {
            // The same file system backs the assets:// mount and, when the assets root lies outside the
            // project root, the asset database (assets_subsystem).
            const std::filesystem::path assetsRoot = loaded->assets_directory();
            std::filesystem::create_directories(assetsRoot, ignored);
            services.assetsFileSystem = std::make_shared<core::vfs::native_file_system>(assetsRoot, false);
            CONJURE_TRY(services.mounts.mount("assets", services.assetsFileSystem));
        }
    } else {
        std::shared_ptr<core::vfs::memory_file_system> scratch = std::make_shared<core::vfs::memory_file_system>();
        CONJURE_TRY(scratch->create_directories("assets"));
        services.projectFileSystem = scratch;
        CONJURE_TRY(services.mounts.mount("project", services.projectFileSystem));
    }
    CONJURE_TRY(services.mounts.mount("engine", services.engineFileSystem));

    mContext.frameMemory = services.frameMemory.get();
    mContext.jobs = &services.jobs;
    mContext.filesystem = &services.mounts;
    mContext.checks = &services.checks;
    return core::result<void>::success();
}

void application::stop_core() {
    mContext.checks = nullptr;
    mContext.filesystem = nullptr;
    mContext.jobs = nullptr;
    mContext.frameMemory = nullptr;
    if (mServices == nullptr) {
        return;
    }
    engine_services &services = *mServices;
    for (const std::string &scheme : services.mounts.schemes()) {
        CONJURE_UNUSED(services.mounts.unmount(scheme));
    }
    services.cookedFileSystem.reset();
    services.assetsFileSystem.reset();
    services.projectFileSystem.reset();
    services.engineFileSystem.reset();
    services.frameMemory.reset();
    services.jobs.stop();
    mMainThreadQueue->clear();
    if (services.logFileSink != nullptr) {
        core::log::remove_sink(services.logFileSink);
        services.logFileSink.reset();
    }
    core::log::flush();
    core::log::set_global_level(services.previousGlobalLevel);
    mServices.reset();
}

core::result<void> application::start_subsystems() {
    engine_services &services = *mServices;
    mSubsystems.clear();
    mStartedSubsystemCount = 0;

    // Built-in adapters in the documented order (steps 6-15).
    mSubsystems.push_back(std::make_unique<platform_subsystem>(services));
    mSubsystems.push_back(std::make_unique<rhi_subsystem>(services));
    mSubsystems.push_back(std::make_unique<assets_subsystem>(services));
    mSubsystems.push_back(std::make_unique<ecs_subsystem>(services));
    mSubsystems.push_back(std::make_unique<scene_subsystem>(services));
#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
    mSubsystems.push_back(std::make_unique<audio_subsystem>(services));
#endif
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    mSubsystems.push_back(std::make_unique<physics_subsystem>(services));
#endif
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    mSubsystems.push_back(std::make_unique<renderer_subsystem>(services));
#endif
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    mSubsystems.push_back(std::make_unique<scripting_subsystem>(services));
#endif
    mSubsystems.push_back(std::make_unique<modules_subsystem>(services));
    const usize modulesIndex = mSubsystems.size() - 1;
    mUserSubsystemBegin = mSubsystems.size();
    mUserSubsystemCount = 0;

    // Steps 6-16: startup in order. Module entries append subsystems during step 15 (they belong to that
    // step); the user-added ones join after them, so step 16 follows every module subsystem (06 3.4).
    for (usize index = 0; index < mSubsystems.size(); ++index) {
        mAcceptingModuleSubsystems = index == modulesIndex;
        engine_subsystem &subsystem = *mSubsystems[index];
        const core::result<void> started = subsystem.startup(mContext);
        mAcceptingModuleSubsystems = false;
        if (!started.has_value()) {
            // The failed step cleans up what it half-built; the started ones unwind in reverse.
            subsystem.shutdown(mContext);
            return started;
        }
        ++mStartedSubsystemCount;
        CONJURE_LOG_DEBUG(LOG_ENGINE, "subsystem '{}' started (step {})", subsystem.name(), index + 6);
        if (index == modulesIndex) {
            mUserSubsystemBegin = mSubsystems.size();
            mUserSubsystemCount = mUserSubsystems.size();
            for (std::unique_ptr<engine_subsystem> &user : mUserSubsystems) {
                mSubsystems.push_back(std::move(user));
            }
            mUserSubsystems.clear();
        }
    }

    // Step 17.
    for (const std::unique_ptr<engine_subsystem> &subsystem : mSubsystems) {
        CONJURE_TRY(subsystem->post_startup(mContext));
    }

    // Step 18.
    if (services.scheduler != nullptr) {
        for (const std::unique_ptr<engine_subsystem> &subsystem : mSubsystems) {
            subsystem->register_systems(*services.scheduler, mContext);
        }
        const core::result<void> finalised = services.scheduler->finalise();
        if (!finalised.has_value()) {
            return make_error(
              engine_error::SCHEDULER_CONFLICT,
              std::format("scheduler rejected the registered systems: {}", finalised.get_error().to_string())
            );
        }
    }

    // Step 19.
    const core::result<void> attached = mLayers.attach_all(mContext);
    if (!attached.has_value()) {
        return attached;
    }

    // Step 20.
    return load_startup_scene();
}

void application::stop_subsystems() {
    const usize userBegin = std::min(mUserSubsystemBegin, mSubsystems.size());

    // Reverse startup order, with one exception: the asset manager owns payloads (scene and prefab archives,
    // the fallbacks of the scene types) that reference the component registry, so the assets adapter stops
    // before the ecs adapter instead of after it.
    std::vector<usize> order = {};
    for (usize index = mSubsystems.size(); index > 0; --index) {
        order.push_back(index - 1);
    }
    const auto position_of = [this, &order](std::string_view name) {
        for (usize slot = 0; slot < order.size(); ++slot) {
            if (mSubsystems[order[slot]]->name() == name) {
                return slot;
            }
        }
        return order.size();
    };
    const usize assetsSlot = position_of("assets");
    const usize ecsSlot = position_of("ecs");
    if (assetsSlot < order.size() && ecsSlot < order.size() && assetsSlot > ecsSlot) {
        const usize assetsIndex = order[assetsSlot];
        order.erase(order.begin() + static_cast<isize>(assetsSlot));
        order.insert(order.begin() + static_cast<isize>(ecsSlot), assetsIndex);
    }
    for (const usize index : order) {
        if (index < mStartedSubsystemCount) {
            mSubsystems[index]->shutdown(mContext);
        }
    }
    // Hand the user subsystems back so a reused application starts them again (initialise after a failure).
    std::vector<std::unique_ptr<engine_subsystem>> survivors = {};
    for (usize index = userBegin; index < mSubsystems.size() && index < userBegin + mUserSubsystemCount; ++index) {
        survivors.push_back(std::move(mSubsystems[index]));
    }
    mSubsystems.clear();
    for (std::unique_ptr<engine_subsystem> &survivor : survivors) {
        mUserSubsystems.push_back(std::move(survivor));
    }
    mStartedSubsystemCount = 0;
    mUserSubsystemBegin = 0;
    mUserSubsystemCount = 0;
}

core::result<void> application::load_startup_scene() {
    const engine_config &config = mDesc.config;
    if (config.startupSceneOverride.empty()) {
        return core::result<void>::success();
    }
    if (mServices->scenes == nullptr || mServices->assetManager == nullptr) {
        return core::result<void>::success();
    }

    const std::string assetPath = asset_path_of(config.startupSceneOverride);
    std::optional<core::error> failure = std::nullopt;
    const std::optional<assets::asset_id> id = mServices->assetManager->find(assetPath);
    // A source that no longer imports keeps its previous blob in the cache; a runtime must not run that stale
    // copy and exit OK. The editor keeps loading the cached copy so the scene can still be opened and fixed.
    const bool importFailed = id.has_value() && mServices->assetDatabase != nullptr
      && mServices->assetDatabase->status_of(*id) == assets::asset_record_status::IMPORT_FAILED;
    if (!id.has_value()) {
        failure = make_error(
          engine_error::STARTUP_SCENE_FAILED,
          std::format("startup scene '{}' is not a known asset", assetPath)
        );
    } else if (importFailed && !config.editor) {
        failure = make_error(
          engine_error::STARTUP_SCENE_FAILED,
          std::format(
            "startup scene '{}' failed to import; refusing the stale cached copy (the import error is logged above)",
            assetPath
          )
        );
    } else {
        const core::result<scene::scene_id> loaded = mServices->scenes->load(*id, scene::load_mode::REPLACE);
        if (!loaded.has_value()) {
            failure = make_error(
              engine_error::STARTUP_SCENE_FAILED,
              std::format("startup scene '{}' failed to load: {}", assetPath, loaded.get_error().to_string())
            );
        }
    }
    if (!failure.has_value()) {
        CONJURE_LOG_INFO(LOG_ENGINE, "startup scene '{}' loaded", assetPath);
        return core::result<void>::success();
    }
    if (config.editor) {
        CONJURE_LOG_WARN(LOG_ENGINE, "{} (editor continues with an empty world)", failure->message());
        return core::result<void>::success();
    }
    return *failure;
}

core::result<void> application::run_frame(std::optional<f64> deltaOverride) {
    CONJURE_ASSERT(mState == application_state::RUNNING, "step_frame() on an application that is not RUNNING");
    if (mState != application_state::RUNNING || mFrameLoop == nullptr) {
        return make_error(engine_error::NOT_INITIALISED, std::format("step_frame() while {}", to_string(mState)));
    }
    CONJURE_ASSERT(!mInFrame, "step_frame() re-entered");
    if (mInFrame) {
        return make_error(engine_error::NOT_INITIALISED, "step_frame() re-entered from inside a frame");
    }
    mInFrame = true;
    const core::result<void> result = mFrameLoop->run_frame(deltaOverride);
    mInFrame = false;
    return result;
}

void application::raise_exit_code(exit_code code) noexcept {
    if (static_cast<int>(code) > static_cast<int>(mExitCode)) {
        mExitCode = code;
    }
}
