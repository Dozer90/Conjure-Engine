#include "conjure/editor/editor_context.h"

#include "conjure/editor/builtin_commands.h"
#include "conjure/editor/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/vfs/path_utils.h"
#include "conjure/modules/module_manager.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/renderer/renderer.h"
#endif
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    #include "conjure/physics/physics_world.h"
#endif
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    #include "conjure/scripting/script_runtime.h"
#endif

#include <cmath>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace conjure::editor {

    namespace {
        constexpr usize LOG_EVENTS_PER_FRAME = 256;

        // The mount the project's assets directory backs (engine::application mounts it under this name).
        constexpr std::string_view ASSETS_MOUNT = "assets";

        /// <summary>
        /// The file an assets:// path names on disk, nullopt for a path of another mount or one that is no asset
        /// path. The file need not exist.
        /// </summary>
        [[nodiscard]] std::optional<std::filesystem::path> assets_file_of(
          const engine::project &project,
          std::string_view assetPath
        ) {
            const std::optional<assets::asset_path_parts> parts = assets::split_asset_path(assetPath);
            if (!parts.has_value() || parts->mount != ASSETS_MOUNT) {
                return std::nullopt;
            }
            // Through char8_t: a narrow string would be read in the ANSI code page on Windows.
            const std::u8string relative(parts->relativePath.begin(), parts->relativePath.end());
            return project.assets_directory() / std::filesystem::path(relative);
        }
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // editor_host

    editor_host::editor_host() = default;
    editor_host::~editor_host() = default;

    frame_task::frame_task() = default;
    frame_task::~frame_task() = default;

    /*===========================================================================*/
    /*===========================================================================*/
    // display_mode_for

    editor_display_mode display_mode_for(const editor_options &options) noexcept {
        if (!options.headless) {
            return editor_display_mode::WINDOWED;
        }
        if (options.offscreen) {
            return editor_display_mode::HEADLESS_OFFSCREEN;
        }
        return editor_display_mode::HEADLESS;
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // editor_context

    editor_context::editor_context(engine::application &engine, editor_host *host, editor_display_mode displayMode) :
        mEngine(&engine),
        mHost(host),
        mDisplayMode(displayMode) {
        mConsole.install_sink();
        register_builtin_commands(mRegistry);
        mDispatcher = std::make_unique<command_dispatcher>(*this, mRegistry, mUndo);
        mPlay = std::make_unique<play_controller>(*this);
        mScreenshots = std::make_unique<screenshot_service>(*this);
        mModuleConsumer = std::make_unique<editor_module_consumer>(mRegistry, mPlay.get());
        mEvents.register_client(0, {});
        mSelectionSubscription = mSelection.on_changed().subscribe([this]() {
            emit_selection_changed();
        });
        bind_engine(engine);
    }

    editor_context::~editor_context() {
        // Frame tasks unregister their sinks and fail their replies while the dispatcher and the broadcaster
        // are still alive.
        mDispatcher->cancel_all(protocol_error_code::CANCELLED, "the editor is shutting down");
        mFrameTasks.clear();
        mScreenshots->cancel_pending();
        mConsole.remove_sink();
    }

    /*---------------------------------------------------------------*/
    // Engine access

    scene::scene &editor_context::scene() noexcept {
        CONJURE_ASSERT(mEngine->context().scenes != nullptr, "editor_context::scene without a scene manager");
        return mEngine->context().scenes->active();
    }

    const scene::scene &editor_context::scene() const noexcept {
        CONJURE_ASSERT(mEngine->context().scenes != nullptr, "editor_context::scene without a scene manager");
        return mEngine->context().scenes->active();
    }

    scene::scene_manager &editor_context::scene_manager() noexcept {
        CONJURE_ASSERT(mEngine->context().scenes != nullptr, "editor_context::scene_manager without a scene manager");
        return *mEngine->context().scenes;
    }

    ecs::world &editor_context::world() noexcept {
        CONJURE_ASSERT(mEngine->context().world != nullptr, "editor_context::world without a world");
        return *mEngine->context().world;
    }

    const ecs::component_registry &editor_context::components() const noexcept {
        CONJURE_ASSERT(mEngine->context().components != nullptr, "editor_context::components without a registry");
        return *mEngine->context().components;
    }

    core::vfs::file_system &editor_context::filesystem() noexcept {
        CONJURE_ASSERT(mEngine->context().filesystem != nullptr, "editor_context::filesystem without a mount table");
        return *mEngine->context().filesystem;
    }

    assets::asset_manager *editor_context::assets() noexcept {
        return mEngine->context().assets;
    }

    assets::asset_database *editor_context::asset_database() noexcept {
        return mEngine->context().assetDatabase;
    }

    platform::input *editor_context::input() noexcept {
        return mEngine->context().input;
    }

    renderer::renderer *editor_context::renderer() noexcept {
        return mEngine->context().renderer;
    }

    physics::physics_world *editor_context::physics() noexcept {
        return mEngine->context().physics;
    }

    scripting::script_runtime *editor_context::scripting() noexcept {
        return mEngine->context().scripting;
    }

    modules::module_manager *editor_context::modules() noexcept {
        return mEngine->context().modules;
    }

    u64 editor_context::frame_index() const noexcept {
        return mEngine->time().frameIndex;
    }

    void editor_context::rebind(engine::application &engine) {
        mEngine = &engine;
        mUndo.clear();
        mSelection.clear();
        mSceneOpen = false;
        mScenePath.clear();
        mSceneDirty = false;
        mInbound.clear();
        mResponses.clear();
        // Everything parked on the old engine answers CANCELLED now instead of TIMEOUT later; the frame tasks
        // then release their sinks and event clients in their destructors.
        mDispatcher->cancel_all(protocol_error_code::CANCELLED, "the engine restarted (project.open)");
        mFrameTasks.clear();
        mScreenshots->cancel_pending();
        mLastScriptConfiguration = "Debug";
        mConsumerInstalled = false;
        bind_engine(engine);
    }

    core::json::document editor_context::selection_payload() const {
        core::json::document guids = core::json::document::array();
        for (const core::uuid &guid : mSelection.guids()) {
            guids.push_back(guid.to_string());
        }
        core::json::document payload = core::json::object({{"guids", std::move(guids)}});
        if (mSelection.empty()) {
            payload.set_null("primary");
        } else {
            payload.set("primary", mSelection.primary().to_string());
        }
        return payload;
    }

    void editor_context::emit_selection_changed() {
        mEvents.emit(editor_event::SELECTION_CHANGED, selection_payload());
    }

    void editor_context::install_module_consumer() {
        modules::module_manager *manager = modules();
        if (manager == nullptr || mConsumerInstalled) {
            return;
        }
        // The engine loads its static modules before the editor exists and add_consumer refuses a manager
        // with loaded modules: unload, add the consumer, load again (the built-ins are manifest-only).
        if (manager->any_loaded()) {
            manager->unload_all();
        }
        manager->add_consumer(*mModuleConsumer);
        mConsumerInstalled = true;
        const core::result<void> loaded = manager->load_all();
        if (!loaded.has_value()) {
            CONJURE_LOG_WARN(LOG_EDITOR, "module consumer: load_all reported: {}", loaded.get_error().to_string());
        }
    }

    /*---------------------------------------------------------------*/
    // Project trust

    bool editor_context::raise_trust_prompt(const std::filesystem::path &projectRoot) {
        if (!mTrustPromptEnabled || projectRoot.empty()) {
            return false;
        }
        // The question names what an answer records: the resolved folder, not the spelling somebody typed (a
        // path relative to the working directory, the name of a link). Through char8_t, as the key is UTF-8.
        const core::result<std::string> key = project_trust::project_key(projectRoot);
        if (key.has_value()) {
            mTrustPrompt = std::filesystem::path(std::u8string(key.value().begin(), key.value().end()));
        } else {
            mTrustPrompt = projectRoot;
        }
        return true;
    }

    void editor_context::close_trust_prompt_for(const std::filesystem::path &projectRoot) {
        if (!mTrustPrompt.has_value()) {
            return;
        }
        const core::result<std::string> asked = project_trust::project_key(*mTrustPrompt);
        const core::result<std::string> answered = project_trust::project_key(projectRoot);
        if (asked.has_value() && answered.has_value() && asked.value() == answered.value()) {
            mTrustPrompt.reset();
        }
    }

    /*---------------------------------------------------------------*/
    // Project and scene state

    engine::project *editor_context::project() noexcept {
        return mProject.has_value() ? &*mProject : nullptr;
    }

    const engine::project *editor_context::project() const noexcept {
        return mProject.has_value() ? &*mProject : nullptr;
    }

    void editor_context::set_scene_state(bool open, std::string path) {
        mSceneOpen = open;
        mScenePath = std::move(path);
        mSceneDirty = false;
    }

    bool editor_context::names_asset(std::string_view assetPath, const assets::asset_id &id) {
        const assets::asset_database *database = asset_database();
        if (database == nullptr || assetPath.empty()) {
            return false;
        }
        const std::optional<assets::asset_id> exact = database->find_by_path(assetPath);
        if (exact.has_value() && *exact == id) {
            return true;
        }
        // Another spelling of the same file: the lookup above is exact, the volume may not be. A path that
        // resolves to another record is compared too, because a file reached under two spellings can end up
        // registered under both.
        const std::optional<std::string> registered = database->asset_path_of(id);
        if (!registered.has_value() || !mProject.has_value()) {
            return false;
        }
        const std::optional<std::filesystem::path> named = assets_file_of(*mProject, assetPath);
        const std::optional<std::filesystem::path> owned = assets_file_of(*mProject, *registered);
        if (!named.has_value() || !owned.has_value()) {
            return false;
        }
        // equivalent() fails when either file is missing: a path that names nothing names no asset.
        std::error_code error = {};
        return std::filesystem::equivalent(*named, *owned, error) && !error;
    }

    bool editor_context::is_open_scene(const assets::asset_id &id) {
        return mSceneOpen && names_asset(mScenePath, id);
    }

    std::string editor_context::normalise_scene_path(std::string_view path) {
        const std::string_view trimmed = core::str::trim(path);
        if (trimmed.empty()) {
            return "";
        }
        std::string_view scheme = "assets";
        std::string_view rest = trimmed;
        const usize separator = trimmed.find("://");
        if (separator != std::string_view::npos) {
            scheme = trimmed.substr(0, separator);
            rest = trimmed.substr(separator + 3);
        }
        // The canonical spelling the asset database registers: forward slashes, no empty, "." or resolvable ".."
        // segments. A leading separator still means the mount root ("/scenes/x" is assets://scenes/x), so it is
        // stripped.
        std::string relative = core::vfs::path_utils::normalize(rest);
        while (!relative.empty() && relative.front() == '/') {
            relative.erase(relative.begin());
        }
        const bool leavesMount = relative == ".." || core::str::starts_with(relative, "../") ||
          core::vfs::path_utils::is_absolute(relative);
        if (relative.empty() || leavesMount) {
            return "";
        }
        std::string result = std::format("{}://{}", scheme, relative);
        if (!core::str::iends_with(result, ".scene.json")) {
            if (core::str::iends_with(result, ".json")) {
                result.erase(result.size() - 5);
            }
            result += ".scene.json";
        }
        return result;
    }

    u64 editor_context::bump_revision() {
        ++mRevision;
        return mRevision;
    }

    void editor_context::set_dotnet_executable(std::string executable) {
        mDotnetExecutable = std::move(executable);
    }

    void editor_context::set_last_script_configuration(std::string configuration) {
        mLastScriptConfiguration = std::move(configuration);
    }

    void editor_context::set_viewport_aspect(f32 aspect) noexcept {
        if (!std::isfinite(aspect) || aspect <= 0.0f) {
            return;
        }
        mViewportAspect = aspect;
    }

    /*---------------------------------------------------------------*/
    // Frame

    void editor_context::tick() {
        const u64 frame = frame_index();
        mConsole.set_frame(frame);
        mEvents.set_frame(frame);

        // 1. Transports.
        mInbound.clear();
        if (mServer != nullptr) {
            mServer->poll(mInbound, mRequestsPerFrame);
            std::vector<u32> closed = {};
            mServer->take_closed(closed);
            for (const u32 clientId : closed) {
                mDispatcher->cancel_client(clientId);
                unregister_response_sink(clientId);
            }
        }

        // 2. Completed asynchronous replies.
        mResponses.clear();
        mDispatcher->resolve_completed(mResponses);
        for (const protocol_response &response : mResponses) {
            deliver_response(response);
        }

        // 3. Inbound requests (the server capped them).
        for (const protocol_request &request : mInbound) {
            std::optional<protocol_response> response = mDispatcher->dispatch(request);
            if (response.has_value()) {
                deliver_response(*response);
            }
        }
        mInbound.clear();

        // 4, 5. Play, screenshots and the multi-frame tasks.
        mPlay->tick();
        mScreenshots->tick();
        for (usize index = 0; index < mFrameTasks.size();) {
            if (mFrameTasks[index]->tick(*this)) {
                mFrameTasks.erase(mFrameTasks.begin() + static_cast<isize>(index));
                continue;
            }
            ++index;
        }

        // 7. Events and outbound bytes.
        forward_log_events();
        if (mServer != nullptr) {
            mServer->flush();
        }
    }

    void editor_context::register_response_sink(u32 clientId, response_sink sink) {
        unregister_response_sink(clientId);
        mResponseSinks.emplace_back(clientId, std::move(sink));
    }

    void editor_context::unregister_response_sink(u32 clientId) {
        std::erase_if(mResponseSinks, [clientId](const auto &entry) {
            return entry.first == clientId;
        });
    }

    void editor_context::add_frame_task(std::unique_ptr<frame_task> task) {
        if (task != nullptr) {
            mFrameTasks.push_back(std::move(task));
        }
    }

    void editor_context::deliver_response(const protocol_response &response) {
        for (const auto &entry : mResponseSinks) {
            if (entry.first == response.clientId) {
                entry.second(response);
                return;
            }
        }
        if (mServer != nullptr) {
            mServer->send(response);
        }
    }

    core::json::document editor_context::describe_state() const {
        core::json::document state = core::json::document::object();
        if (mProject.has_value()) {
            state.set(
              "project",
              core::json::object({
                {"name", std::string_view(mProject->description().name)},
                {"path", mProject->root_directory().generic_string()},
              })
            );
        } else {
            state.set_null("project");
        }
        state.set(
          "scene",
          core::json::object({{"open", mSceneOpen}, {"path", std::string_view(mScenePath)}, {"dirty", mSceneDirty}})
        );
        state.set("playState", to_string(mPlay->state()));
        state.set("revision", mRevision);
        core::json::document guids = core::json::document::array();
        for (const core::uuid &guid : mSelection.guids()) {
            guids.push_back(guid.to_string());
        }
        state.set("selection", guids);
        state.set("headless", headless());
        state.set("offscreen", offscreen());
        state.set("port", mServer == nullptr ? u32{0} : u32{mServer->bound_port()});
        state.set("clients", mServer == nullptr ? usize{0} : mServer->client_count());
        state.set("frame", frame_index());
        if (mTrustPrompt.has_value()) {
            state.set("trustPrompt", mTrustPrompt->generic_string());
        } else {
            state.set_null("trustPrompt");
        }
        return state;
    }

    /*---------------------------------------------------------------*/
    // Internals

    void editor_context::bind_engine(engine::application &engine) {
        const engine::engine_context &context = engine.context();
        if (context.project != nullptr) {
            mProject = *context.project;
            // The project the window asked about is open now: the question is settled.
            close_trust_prompt_for(mProject->root_directory());
        } else {
            mProject.reset();
        }
        // A startup scene the engine loaded counts as the open scene. It is the active scene, the first loaded one
        // (F438, as Scene.ActivePath): a scene loaded additively during startup must not become the save target.
        if (context.scenes != nullptr) {
            const std::vector<scene::loaded_scene_info> loaded = context.scenes->loaded_scenes();
            if (!loaded.empty()) {
                const scene::loaded_scene_info &active = loaded.front();
                std::string path = active.path;
                if (path.empty() && context.assetDatabase != nullptr) {
                    const std::optional<std::string> assetPath = context.assetDatabase->asset_path_of(active.asset);
                    path = assetPath.value_or("");
                }
                mSceneOpen = true;
                mScenePath = std::move(path);
                mSceneDirty = false;
            }
        }
        install_module_consumer();
        mPlay->apply_engine_mode();
        mScreenshots->set_backend(screenshot_service::create_renderer_backend(*this));
        mLastLogSeq = mConsole.last_seq();
    }

    void editor_context::forward_log_events() {
        const u64 last = mConsole.last_seq();
        if (last == mLastLogSeq) {
            return;
        }
        console_query query = {};
        query.sinceSeq = mLastLogSeq;
        query.limit = LOG_EVENTS_PER_FRAME;
        const std::vector<console_record> records = mConsole.read(query);
        for (const console_record &record : records) {
            mEvents.emit_log(
              record.level,
              core::json::object({
                {"seq", record.seq},
                {"frame", record.frame},
                {"level", core::to_string(record.level)},
                {"category", std::string_view(record.category)},
                {"message", std::string_view(record.text)},
              })
            );
            mLastLogSeq = record.seq;
        }
        if (records.size() < LOG_EVENTS_PER_FRAME) {
            mLastLogSeq = last;
        }
    }
}
