#pragma once

#include "conjure/editor/command_dispatcher.h"
#include "conjure/editor/command_registry.h"
#include "conjure/editor/console_buffer.h"
#include "conjure/editor/editor_camera.h"
#include "conjure/editor/editor_module_consumer.h"
#include "conjure/editor/event_broadcaster.h"
#include "conjure/editor/play_controller.h"
#include "conjure/editor/project_trust.h"
#include "conjure/editor/protocol/protocol_server.h"
#include "conjure/editor/screenshot_service.h"
#include "conjure/editor/selection_set.h"
#include "conjure/editor/undo_stack.h"

#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/vfs/file_system.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/world.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_context.h"
#include "conjure/engine/project.h"
#include "conjure/platform/input.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The editor's service table (docs/design/04_editor.md 5, R22): owns the command registry, dispatcher, undo
// stack, selection, play controller, events, camera, console and screenshot service, and holds non-owning
// references into the running engine::application (taken from engine_context, rebound when the editor opens
// another project). Handlers reach everything through it. Main thread only.

namespace conjure::core::reflect {
    class type_registry;
}

namespace conjure::renderer {
    class renderer;
}

namespace conjure::physics {
    class physics_world;
}

namespace conjure::scripting {
    class script_runtime;
}

namespace conjure::modules {
    class module_manager;
}

namespace conjure::editor {

    /// <summary>
    /// What the owning executable does for the context: operations that outlive one engine::application.
    /// </summary>
    class CONJURE_API editor_host {
        public:
            editor_host();
            virtual ~editor_host();

            editor_host(const editor_host &) = delete;
            editor_host &operator=(const editor_host &) = delete;

        public:
            /// <summary>
            /// Asks the process to stop after the current frame with an exit code.
            /// </summary>
            virtual void request_quit(int exitCode) = 0;

            /// <summary>
            /// Validates the project directory now and schedules the engine restart over it for the next
            /// frame boundary (the current frame finishes on the old project).
            /// </summary>
            [[nodiscard]] virtual core::result<void> request_open_project(const std::filesystem::path &directory) = 0;

            /// <summary>
            /// True after request_open_project until the restart happened.
            /// </summary>
            [[nodiscard]] virtual bool has_pending_project_open() const noexcept = 0;
    };

    /// <summary>
    /// Work the editor advances once per frame (editor.wait_frames, editor.exec_file): tick() returns true
    /// when the task finished and may be destroyed. A task destroyed before it finished (engine restart,
    /// shutdown) releases what it registered on the context and fails its reply with CANCELLED when that is
    /// still pending.
    /// </summary>
    class CONJURE_API frame_task {
        public:
            frame_task();
            virtual ~frame_task();

            frame_task(const frame_task &) = delete;
            frame_task &operator=(const frame_task &) = delete;

        public:
            [[nodiscard]] virtual bool tick(editor_context &context) = 0;
    };

    class CONJURE_API editor_context {
        public:
            /// <summary>
            /// The width / height a view command assumes until a viewport panel reports its own (headless
            /// sessions keep it).
            /// </summary>
            static constexpr f32 DEFAULT_VIEWPORT_ASPECT = 16.0f / 9.0f;

            /// <summary>
            /// Receives the responses of one in-process client (the headless runner, tests).
            /// </summary>
            using response_sink = std::function<void(const protocol_response &response)>;

            /// <summary>
            /// The viewport projection mode view.set_mode switches.
            /// </summary>
            enum class view_mode : u8 {
                THREE_D = 0,
                TWO_D
            };

            struct debug_draw_flags {
                bool physics = false;
                bool bounds = false;
                bool grid = true;
            };

        private:
            engine::application *mEngine = nullptr;
            editor_host *mHost = nullptr;
            protocol_server *mServer = nullptr;
            project_trust *mTrust = nullptr;
            std::optional<engine::project> mProject = std::nullopt;
            std::optional<std::filesystem::path> mTrustPrompt = std::nullopt;   // the project the window asks about
            command_registry mRegistry = {};
            undo_stack mUndo = {};
            event_broadcaster mEvents = {};
            console_buffer mConsole{};
            selection_set mSelection = {};
            core::event_subscription mSelectionSubscription = {};
            editor_camera mCamera = {};
            std::unique_ptr<command_dispatcher> mDispatcher = nullptr;
            std::unique_ptr<play_controller> mPlay = nullptr;
            std::unique_ptr<screenshot_service> mScreenshots = nullptr;
            std::unique_ptr<editor_module_consumer> mModuleConsumer = nullptr;
            std::vector<protocol_request> mInbound = {};
            std::vector<protocol_response> mResponses = {};
            std::vector<std::pair<u32, response_sink>> mResponseSinks = {};
            std::vector<std::unique_ptr<frame_task>> mFrameTasks = {};
            std::weak_ptr<void> mScriptCompileToken = {};   // held by the running script.compile task
            std::string mScenePath = "";
            std::string mDotnetExecutable = "dotnet";
            std::string mLastScriptConfiguration = "Debug"; // of the last script.compile that succeeded
            debug_draw_flags mDebugDraw = {};
            view_mode mViewMode = view_mode::THREE_D;
            f32 mViewportAspect = DEFAULT_VIEWPORT_ASPECT;
            u64 mRevision = 0;
            u64 mLastLogSeq = 0;
            usize mRequestsPerFrame = protocol_server::DEFAULT_REQUESTS_PER_FRAME;
            bool mSceneOpen = false;
            bool mSceneDirty = false;
            bool mHeadless = true;
            bool mOffscreen = false;
            bool mConsumerInstalled = false;
            bool mTrustPromptEnabled = false;           // a window is there to ask in (set by the application)

        public:
            /// <summary>
            /// Binds to an initialised application. The built-in commands are registered, the console sink
            /// installed and the engine put into the EDITING mode (PAUSED). 'host' may be null in tests
            /// (editor.quit and project.open then answer NOT_AVAILABLE).
            /// </summary>
            editor_context(engine::application &engine, editor_host *host, bool headless, bool offscreen);
            ~editor_context();

            editor_context(const editor_context &) = delete;
            editor_context &operator=(const editor_context &) = delete;

        public:
            /*---------------------------------------------------------------*/
            // Engine access

            [[nodiscard]] engine::application &engine() noexcept {
                return *mEngine;
            }

            [[nodiscard]] const engine::application &engine() const noexcept {
                return *mEngine;
            }

            [[nodiscard]] engine::engine_context &engine_context() noexcept {
                return mEngine->context();
            }

            [[nodiscard]] const engine::engine_context &engine_context() const noexcept {
                return mEngine->context();
            }

            /// <summary>
            /// The one scene over the world (asserts that the scene manager exists).
            /// </summary>
            [[nodiscard]] scene::scene &scene() noexcept;
            [[nodiscard]] const scene::scene &scene() const noexcept;
            [[nodiscard]] scene::scene_manager &scene_manager() noexcept;
            [[nodiscard]] ecs::world &world() noexcept;
            [[nodiscard]] const ecs::component_registry &components() const noexcept;
            [[nodiscard]] core::vfs::file_system &filesystem() noexcept;
            [[nodiscard]] assets::asset_manager *assets() noexcept;
            [[nodiscard]] assets::asset_database *asset_database() noexcept;
            [[nodiscard]] platform::input *input() noexcept;
            [[nodiscard]] renderer::renderer *renderer() noexcept;
            [[nodiscard]] physics::physics_world *physics() noexcept;
            [[nodiscard]] scripting::script_runtime *scripting() noexcept;
            [[nodiscard]] modules::module_manager *modules() noexcept;
            [[nodiscard]] u64 frame_index() const noexcept;

            /// <summary>
            /// Points the context at another initialised application (after project.open restarted the
            /// engine): scene state, undo stack and selection are cleared, every pending asynchronous request
            /// answers CANCELLED, the frame tasks are dropped and the module consumer re-installed.
            /// </summary>
            void rebind(engine::application &engine);

            /// <summary>
            /// Installs the module consumer into the engine's module manager (unload_all, add_consumer,
            /// load_all, since the engine loads its static modules before the editor exists). Called by the
            /// constructor and rebind(); public for tests that construct the context over a bare application.
            /// </summary>
            void install_module_consumer();

            /*---------------------------------------------------------------*/
            // Services

            [[nodiscard]] command_registry &registry() noexcept {
                return mRegistry;
            }

            [[nodiscard]] command_dispatcher &dispatcher() noexcept {
                return *mDispatcher;
            }

            [[nodiscard]] undo_stack &undo() noexcept {
                return mUndo;
            }

            [[nodiscard]] event_broadcaster &events() noexcept {
                return mEvents;
            }

            [[nodiscard]] console_buffer &console() noexcept {
                return mConsole;
            }

            [[nodiscard]] selection_set &selection() noexcept {
                return mSelection;
            }

            /// <summary>
            /// The current selection as {guids, primary}: the sorted guids and the primary guid, null when nothing
            /// is selected. The one builder of that shape: selection.changed carries exactly this, and the
            /// selection.* answers add "entities" and "count" to it.
            /// </summary>
            [[nodiscard]] core::json::document selection_payload() const;

            /// <summary>
            /// Emits selection.changed with selection_payload(). The selection's on_changed and play.stop both emit
            /// through it, so every selection.changed has the same shape.
            /// </summary>
            void emit_selection_changed();

            [[nodiscard]] editor_camera &camera() noexcept {
                return mCamera;
            }

            [[nodiscard]] play_controller &play() noexcept {
                return *mPlay;
            }

            [[nodiscard]] const play_controller &play() const noexcept {
                return *mPlay;
            }

            [[nodiscard]] screenshot_service &screenshots() noexcept {
                return *mScreenshots;
            }

            [[nodiscard]] editor_module_consumer &module_consumer() noexcept {
                return *mModuleConsumer;
            }

            [[nodiscard]] editor_host *host() noexcept {
                return mHost;
            }

            /// <summary>
            /// The protocol server the application runs, or nullptr (tests without one).
            /// </summary>
            [[nodiscard]] protocol_server *server() noexcept {
                return mServer;
            }

            void set_server(protocol_server *server) noexcept {
                mServer = server;
            }

            /*---------------------------------------------------------------*/
            // Project trust (decision D27 of 04_editor.md)

            /// <summary>
            /// The answers about which projects may be opened, or nullptr (a context built over a bare
            /// application in tests: project.trust, project.revoke_trust and project.trust_state then answer
            /// NOT_AVAILABLE). The editor_application owns it and applies it where a project is opened.
            /// </summary>
            [[nodiscard]] project_trust *trust() noexcept {
                return mTrust;
            }

            void set_trust(project_trust *answers) noexcept {
                mTrust = answers;
            }

            /// <summary>
            /// Whether a refused project is asked about in the editor window. The application turns it on for
            /// a windowed session that runs neither --exec nor --export; everywhere else nobody could answer.
            /// </summary>
            void set_trust_prompt_enabled(bool enabled) noexcept {
                mTrustPromptEnabled = enabled;
            }

            [[nodiscard]] bool trust_prompt_enabled() const noexcept {
                return mTrustPromptEnabled;
            }

            /// <summary>
            /// Makes 'projectRoot' the project the window asks about ("Do you trust the authors of this
            /// project?"), replacing an earlier one, and returns true. The question keeps the folder as
            /// project_trust::project_key resolves it (absolute, links followed), which is what an answer
            /// records and what PROJECT_NOT_TRUSTED reports, so the person is shown the folder that would be
            /// trusted whatever spelling asked for it; the path as given only when it has no key. Does
            /// nothing and returns false while prompts are not enabled or for an empty path. The prompt is
            /// editor state, not a command: it survives an engine restart and is reported by editor.state as
            /// trustPrompt.
            /// </summary>
            bool raise_trust_prompt(const std::filesystem::path &projectRoot);

            /// <summary>
            /// The project the window asks about, as raise_trust_prompt kept it; nullopt when it asks about
            /// none.
            /// </summary>
            [[nodiscard]] const std::optional<std::filesystem::path> &trust_prompt() const noexcept {
                return mTrustPrompt;
            }

            /// <summary>
            /// Closes the question unanswered ("Don't open"): nothing is recorded, the project stays as it was.
            /// </summary>
            void dismiss_trust_prompt() noexcept {
                mTrustPrompt.reset();
            }

            /// <summary>
            /// Closes the question when it is about 'projectRoot' (the same project_trust::project_key): the
            /// project was trusted by somebody, or it is the project that is now open. Another project's
            /// question stays.
            /// </summary>
            void close_trust_prompt_for(const std::filesystem::path &projectRoot);

            /*---------------------------------------------------------------*/
            // Project and scene state

            [[nodiscard]] bool has_project() const noexcept {
                return mProject.has_value();
            }

            /// <summary>
            /// The editor's copy of the project (mutable: project.set_settings edits and saves it).
            /// </summary>
            [[nodiscard]] engine::project *project() noexcept;
            [[nodiscard]] const engine::project *project() const noexcept;

            [[nodiscard]] bool has_scene() const noexcept {
                return mSceneOpen;
            }

            /// <summary>
            /// The asset path of the open scene ("assets://scenes/main.scene.json"), "" for an unsaved one.
            /// </summary>
            [[nodiscard]] const std::string &scene_path() const noexcept {
                return mScenePath;
            }

            void set_scene_state(bool open, std::string path);

            /// <summary>
            /// True when 'assetPath' names the source file of the registered asset 'id'. Decided by file, not by
            /// spelling (code sweep F656): the database looks a path up exactly, while a case-insensitive volume
            /// opens "Scenes/Main" as the file registered as scenes/main, so a path the lookup does not resolve to
            /// the asset is compared with the asset's file on disk (assets:// only, the mount whose directory the
            /// project names). False for "", for an unknown asset and without an asset database.
            /// </summary>
            [[nodiscard]] bool names_asset(std::string_view assetPath, const assets::asset_id &id);

            /// <summary>
            /// True when the asset is the file behind the open scene (names_asset() of scene_path()): the one test
            /// asset.delete applies before it asks for closeOpenScene and the asset browser before it asks the user
            /// (04_editor.md D20). False without an open scene and for a scene that has no path yet.
            /// </summary>
            [[nodiscard]] bool is_open_scene(const assets::asset_id &id);

            [[nodiscard]] bool scene_dirty() const noexcept {
                return mSceneDirty;
            }

            void set_scene_dirty(bool dirty) noexcept {
                mSceneDirty = dirty;
            }

            /// <summary>
            /// Resolves a scene argument to the canonical asset path the database registers: a "scheme://" path
            /// keeps its scheme, anything else is relative to assets://; the relative part gets forward slashes
            /// and loses empty, "." and resolvable ".." segments and any leading separator; ".scene.json" is
            /// appended when missing (compared without case; a bare ".json" is replaced). "" for a blank path
            /// and for one that leaves its mount ("..", "../x", a drive-letter path).
            /// </summary>
            [[nodiscard]] static std::string normalise_scene_path(std::string_view path);

            /*---------------------------------------------------------------*/
            // Revision, frame, options

            [[nodiscard]] u64 revision() const noexcept {
                return mRevision;
            }

            /// <summary>
            /// Increments the revision (every mutating command). Undoable mutations and history commands also
            /// mark the scene dirty; NO_UNDO commands (scene.save, scene.new, ...) manage the flag themselves.
            /// </summary>
            u64 bump_revision();

            [[nodiscard]] bool headless() const noexcept {
                return mHeadless;
            }

            [[nodiscard]] bool offscreen() const noexcept {
                return mOffscreen;
            }

            [[nodiscard]] view_mode current_view_mode() const noexcept {
                return mViewMode;
            }

            void set_view_mode(view_mode mode) noexcept {
                mViewMode = mode;
            }

            [[nodiscard]] debug_draw_flags &debug_draw() noexcept {
                return mDebugDraw;
            }

            /// <summary>
            /// The dotnet program script.compile spawns ("dotnet" by default; tests point it elsewhere).
            /// </summary>
            [[nodiscard]] const std::string &dotnet_executable() const noexcept {
                return mDotnetExecutable;
            }

            void set_dotnet_executable(std::string executable);

            /// <summary>
            /// True while a script.compile runs: its task holds the token registered with
            /// set_script_compile_token, and the token expires when the task finishes or is dropped.
            /// </summary>
            [[nodiscard]] bool script_compile_pending() const noexcept {
                return !mScriptCompileToken.expired();
            }

            /// <summary>
            /// Registers the token the running script.compile task owns (see script_compile_pending).
            /// </summary>
            void set_script_compile_token(const std::shared_ptr<void> &token) noexcept {
                mScriptCompileToken = token;
            }

            /// <summary>
            /// The build configuration of the last script.compile that succeeded ("Debug" until one did, and again
            /// after a project.open): script.reload loads scripts/bin/<configuration>/Game.dll by default.
            /// </summary>
            [[nodiscard]] const std::string &last_script_configuration() const noexcept {
                return mLastScriptConfiguration;
            }

            /// <summary>
            /// Records the configuration of a script.compile that succeeded (compile_task calls it); rebind() resets
            /// it to "Debug".
            /// </summary>
            void set_last_script_configuration(std::string configuration);

            /// <summary>
            /// The width / height of the viewport the UI draws (DEFAULT_VIEWPORT_ASPECT until a viewport panel
            /// reports one): view.pick builds its ray with it when the request names no aspect.
            /// </summary>
            [[nodiscard]] f32 viewport_aspect() const noexcept {
                return mViewportAspect;
            }

            /// <summary>
            /// Records the viewport's width / height; a value that is not finite or not positive is ignored.
            /// </summary>
            void set_viewport_aspect(f32 aspect) noexcept;

            void set_requests_per_frame(usize count) noexcept {
                mRequestsPerFrame = count;
            }

            /*---------------------------------------------------------------*/
            // Frame

            /// <summary>
            /// The per-frame body (docs 3.2): server poll, completed async replies, inbound requests (capped),
            /// play tick, screenshot tick, log events, server flush. Called by the editor layer's on_update or
            /// by editor_application::headless_tick.
            /// </summary>
            void tick();

            /// <summary>
            /// Routes the responses of a client to an in-process sink instead of the server (the headless
            /// runner registers its exec client here). A later registration for the same id replaces it.
            /// </summary>
            void register_response_sink(u32 clientId, response_sink sink);
            void unregister_response_sink(u32 clientId);

            /// <summary>
            /// Hands a response to its client's sink or to the server.
            /// </summary>
            void deliver_response(const protocol_response &response);

            /// <summary>
            /// Adds a task tick() advances every frame until it reports completion.
            /// </summary>
            void add_frame_task(std::unique_ptr<frame_task> task);

            [[nodiscard]] usize frame_task_count() const noexcept {
                return mFrameTasks.size();
            }

            /// <summary>
            /// Writes {project, scene, playState, revision, selection, headless, offscreen, port, clients, frame,
            /// trustPrompt}; trustPrompt is the path of the project the window asks about, or null.
            /// </summary>
            [[nodiscard]] core::json::document describe_state() const;

        private:
            void bind_engine(engine::application &engine);
            void forward_log_events();
    };
}
