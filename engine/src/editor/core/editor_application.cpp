#include "conjure/editor/editor_application.h"

#include "conjure/editor/editor_error.h"
#include "conjure/editor/headless_runner.h"
#include "conjure/editor/log_category.h"

#include "conjure/core/platform_detect.h"
#include "conjure/engine/command_line.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/exit_code.h"
#include "conjure/engine/project.h"

#if defined(CONJURE_BUILD_EDITOR) && CONJURE_BUILD_EDITOR
    #include "conjure/engine/imgui_layer.h"
#endif
#if defined(CONJURE_EDITOR_WITH_BINDINGS) && CONJURE_EDITOR_WITH_BINDINGS
    #include "conjure/bindings/bindings_subsystem.h"
#endif

#include <format>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace conjure::editor {

    namespace {
        /// <summary>
        /// The core layer that drives the frame integration (docs 3.2) in every mode; the UI layer only draws.
        /// </summary>
        class editor_tick_layer final : public engine::layer {
            private:
                editor_application *mApplication = nullptr;

            public:
                explicit editor_tick_layer(editor_application &application) :
                    layer("editor-core"),
                    mApplication(&application) {}

                ~editor_tick_layer() override = default;

            public:
                void on_update(engine::engine_context &context) override {
                    (void) context;
                    mApplication->headless_tick();
                }
        };
    }

    editor_application::editor_application() = default;

    editor_application::~editor_application() {
        shutdown();
    }

    core::result<engine::application_desc> editor_application::make_engine_desc(
      const editor_options &options,
      const std::filesystem::path &projectPath
    ) {
        engine::engine_config base = engine::engine_config::defaults();
        base.editor = true;

        std::vector<std::string> storage = options.engineArguments;
        if (options.headless) {
            storage.emplace_back("--headless");
        }
        const std::string projectText = projectPath.generic_string();
        if (!projectPath.empty()) {
            storage.emplace_back("--project");
            storage.push_back(projectText);
        }
        std::vector<std::string_view> arguments = {};
        arguments.reserve(storage.size());
        for (const std::string &argument : storage) {
            arguments.emplace_back(argument);
        }
        CONJURE_TRY_ASSIGN(const engine::command_line_options parsed, engine::parse_command_line(arguments));
        if (!parsed.unrecognisedArguments.empty()) {
            // The editor took its own flags first, so what the engine does not know either is a mistake. It is
            // refused before the merge loads the project, so a missing project does not hide the bad option.
            std::string unknown = {};
            for (const std::string &argument : parsed.unrecognisedArguments) {
                if (!unknown.empty()) {
                    unknown += ' ';
                }
                unknown += argument;
            }
            return engine::make_error(
              engine::engine_error::BAD_ARGUMENTS,
              std::format("unknown option(s): {}", unknown)
            );
        }
        CONJURE_TRY_ASSIGN(engine::application_desc desc, engine::application_desc::from_options(parsed, base));
        desc.config.editor = true;

        if (options.headless) {
            // Scripting on the null host so script.* commands work without .NET (docs 7); --dotnet keeps the
            // real host for play tests that run C# headless.
            desc.config.scripting.enabled = !options.noScripting;
            desc.config.scripting.useNullHost = !options.dotnetHost;
            desc.config.scripting.required = false;
            if (options.offscreen) {
                engine::engine_config attempt = desc.config;
                attempt.rendering.enabled = true;
                attempt.rendering.backend = engine::rhi_backend_choice::AUTO;
                if (attempt.validate().has_value()) {
                    desc.config = attempt;
                } else {
                    CONJURE_LOG_WARN(
                      LOG_EDITOR,
                      "--offscreen: this engine build refuses a real rhi device without a window; "
                      "rendering runs on the null device and view.screenshot answers NOT_AVAILABLE"
                    );
                    desc.config.rendering.enabled = true;
                    desc.config.rendering.backend = engine::rhi_backend_choice::NULL_DEVICE;
                }
            }
        } else {
            desc.config.scripting.required = false;
            if (options.noScripting) {
                desc.config.scripting.enabled = false;
            }
        }
        CONJURE_TRY(desc.config.validate());
        return desc;
    }

    core::result<void> editor_application::initialise(const editor_options &options, layer_factory uiFactory) {
        if (mInitialised) {
            return make_editor_error(protocol_error_code::INVALID_REQUEST, "editor_application is already initialised");
        }
        mOptions = options;
        mUiFactory = std::move(uiFactory);
        mExecRequests.clear();
        // Fail fast (decision D22): everything that can be wrong without an engine is checked before one starts.
        // The arguments and the project come first, so a bad flag or a missing project is still what is reported
        // when the command file is unusable as well.
        CONJURE_TRY_ASSIGN(engine::application_desc desc, make_engine_desc(options, options.projectPath));
        if (!options.execFile.empty() && options.exportDirectory.empty()) {
            core::result<std::vector<protocol_request>> requests = headless_runner::load_command_file(options.execFile);
            if (!requests.has_value()) {
                // The file is an argument: BAD_ARGUMENTS is what conjure_editor's main turns into exit code 3.
                return engine::make_error(engine::engine_error::BAD_ARGUMENTS, requests.get_error().message());
            }
            mExecRequests = std::move(requests).value();
        }
        CONJURE_TRY(boot_engine(std::move(desc)));
        mServer = std::make_unique<protocol_server>(mContext->events());
        mContext->set_server(mServer.get());
        mContext->set_token(options.aiToken);
        mContext->dispatcher().set_async_timeout_seconds(options.timeoutSeconds);
        const std::filesystem::path projectRoot =
          mContext->project() != nullptr ? mContext->project()->root_directory() : std::filesystem::path();
        const core::result<void> started = mServer->start(options, projectRoot);
        if (!started.has_value()) {
            if (options.headless) {
                return started;
            }
            CONJURE_LOG_ERROR(
              LOG_EDITOR,
              "{}; the editor runs without the AI protocol server",
              started.get_error().message()
            );
        }
        mInitialised = true;
        return core::result<void>::success();
    }

    int editor_application::run() {
        if (!mInitialised) {
            return static_cast<int>(engine::exit_code::INIT_FAILURE);
        }
        if (!mOptions.exportDirectory.empty()) {
            return run_export();
        }
        if (!mOptions.execFile.empty()) {
            return run_exec();
        }
        while (!mQuitRequested && !mEngine->is_quit_requested()) {
            const core::result<void> stepped = pump_frame();
            if (!stepped.has_value()) {
                CONJURE_LOG_ERROR(LOG_EDITOR, "frame failed: {}", stepped.get_error().to_string());
                mExitCode = static_cast<int>(engine::exit_code::RUNTIME_FAILURE);
                break;
            }
            // A headless session whose only transport is stdio has nobody left to serve once standard input
            // ended; a windowed editor, or one with a TCP server, keeps running for its other clients. A quit
            // asked for in the same frame keeps its own exit code.
            const bool stdioOnly = mOptions.headless && mOptions.aiStdio && !mOptions.wants_tcp_server();
            const bool quitPending = mQuitRequested || mEngine->is_quit_requested();
            if (stdioOnly && !quitPending && mServer != nullptr && mServer->stdio_closed()) {
                CONJURE_LOG_INFO(LOG_EDITOR, "ai protocol: standard input closed; quitting");
                request_quit(static_cast<int>(engine::exit_code::OK));
            }
        }
        if (!mQuitRequested && mEngine->is_quit_requested()) {
            mExitCode = static_cast<int>(mEngine->current_exit_code());
        }
        return mExitCode;
    }

    core::result<void> editor_application::pump_frame() {
        if (mEngine == nullptr) {
            return make_editor_error(protocol_error_code::INTERNAL, "no engine to pump");
        }
        CONJURE_TRY(mEngine->step_frame());
        ++mTicks;
        if (mPendingProject.has_value()) {
            CONJURE_TRY(apply_pending_project_open());
        }
        return core::result<void>::success();
    }

    void editor_application::headless_tick() {
        if (mContext != nullptr) {
            mContext->tick();
        }
    }

    void editor_application::shutdown() {
        if (mServer != nullptr) {
            mServer->stop();
        }
        if (mContext != nullptr) {
            mContext->set_server(nullptr);
            if (!mContext->play().is_editing()) {
                // The play snapshot and the frozen undo stack refer to the live scene: leave play before the
                // engine tears it down.
                (void) mContext->play().stop();
            }
        }
        if (mEngine != nullptr) {
            // The context's module consumer must outlive the engine's module manager shutdown.
            mEngine->shutdown();
        }
        mContext.reset();
        mEngine.reset();
        mServer.reset();
        mInitialised = false;
    }

    void editor_application::request_quit(int exitCode) {
        mQuitRequested = true;
        mExitCode = exitCode;
        if (mEngine != nullptr) {
            mEngine->request_quit(engine::exit_code::OK);
        }
    }

    core::result<void> editor_application::request_open_project(const std::filesystem::path &directory) {
        CONJURE_TRY(engine::project::load(directory));
        mPendingProject = directory;
        return core::result<void>::success();
    }

    bool editor_application::has_pending_project_open() const noexcept {
        return mPendingProject.has_value();
    }

    std::optional<core::json::document> editor_application::startup_scene_warning() const {
        if (mEngine == nullptr) {
            return std::nullopt;
        }
        const std::optional<core::error> &failure = mEngine->startup_scene_failure();
        if (!failure.has_value()) {
            return std::nullopt;
        }
        return core::json::object({
          {"code", "STARTUP_SCENE_FAILED"},
          {"message", std::string_view(failure->message())},
        });
    }

    core::result<void> editor_application::start_engine(const std::filesystem::path &projectPath) {
        CONJURE_TRY_ASSIGN(engine::application_desc desc, make_engine_desc(mOptions, projectPath));
        return boot_engine(std::move(desc));
    }

    core::result<void> editor_application::boot_engine(engine::application_desc desc) {
        std::unique_ptr<engine::application> engine = std::make_unique<engine::application>(std::move(desc));
#if defined(CONJURE_BUILD_EDITOR) && CONJURE_BUILD_EDITOR
        if (!mOptions.headless) {
            engine->layers().push_layer(std::make_unique<engine::imgui_layer>());
        }
#endif
#if defined(CONJURE_EDITOR_WITH_BINDINGS) && CONJURE_EDITOR_WITH_BINDINGS
        // Startup step 16 (docs 3.1, 06_engine_loop.md 3.4): the bindings fill the non-core native table before
        // scripting's post_startup loads the game assembly. A no-op when scripting is off, so --no-scripting
        // keeps working; on the null host it fills the same table conjure_runtime gets.
        engine->add_subsystem(std::make_unique<bindings::scripting_bindings_subsystem>());
#endif
        engine->layers().push_layer(std::make_unique<editor_tick_layer>(*this));
        CONJURE_TRY(engine->initialise());
        if (mContext == nullptr) {
            mContext = std::make_unique<editor_context>(*engine, this, mOptions.headless, mOptions.offscreen);
        } else {
            mContext->rebind(*engine);
        }
        if (!mOptions.headless && mUiFactory) {
            std::unique_ptr<engine::layer> ui = mUiFactory(*mContext);
            if (ui != nullptr) {
                engine->layers().push_layer(std::move(ui));
            }
        }
        mEngine = std::move(engine);
        return core::result<void>::success();
    }

    core::result<void> editor_application::apply_pending_project_open() {
        const std::filesystem::path next = *mPendingProject;
        mPendingProject.reset();
        const std::filesystem::path previous = mOptions.projectPath;
        CONJURE_LOG_INFO(LOG_EDITOR, "opening project '{}'", next.generic_string());
        if (mContext != nullptr && !mContext->play().is_editing()) {
            // As in shutdown(): the play snapshot and the frozen undo stack refer to the old engine's scene, and
            // rebind keeps the play controller, so no play session may cross the restart.
            (void) mContext->play().stop();
        }
        mEngine->shutdown();
        std::unique_ptr<engine::application> old = std::move(mEngine);
        core::result<void> started = start_engine(next);
        if (!started.has_value()) {
            CONJURE_LOG_ERROR(
              LOG_EDITOR,
              "project.open '{}' failed: {}",
              next.generic_string(),
              started.get_error().to_string()
            );
            started = start_engine(previous);
            if (!started.has_value()) {
                request_quit(static_cast<int>(engine::exit_code::INIT_FAILURE));
                return started;
            }
        } else {
            mOptions.projectPath = next;
        }
        old.reset();
        if (mServer != nullptr && mContext != nullptr) {
            // <project>/.conjure/ai_port follows the open project: the previous project's file is removed and the
            // new one names the port.
            mServer->set_project_root(
              mContext->project() != nullptr ? mContext->project()->root_directory() : std::filesystem::path()
            );
        }
        if (mContext != nullptr) {
            mContext->events().emit(
              editor_event::SCENE_LOADED,
              core::json::object({
                {"path", std::string_view(mContext->scene_path())},
                {"project", mOptions.projectPath.generic_string()},
              })
            );
            // The clients of the session outlive the restart: they hear at once that the project they now work
            // in started without its startup scene (the runner's first line covers the first start only).
            if (std::optional<core::json::document> warning = startup_scene_warning(); warning.has_value()) {
                (void) mContext->events().emit(editor_event::WARNING, std::move(*warning));
            }
        }
        return core::result<void>::success();
    }

    int editor_application::run_export() {
        protocol_request request = {};
        request.id = make_id(0);
        request.command = "project.export";
        request.args = core::json::object({{"directory", mOptions.exportDirectory.generic_string()}});
        if (!mOptions.exportDistribution.empty()) {
            request.args.set("distributionDirectory", mOptions.exportDistribution.generic_string());
        }
        std::optional<protocol_response> captured = std::nullopt;
        const u32 clientId = mContext->events().register_client({});
        mContext->register_response_sink(clientId, [&captured](const protocol_response &response) {
            captured = response;
        });
        request.clientId = clientId;
        std::optional<protocol_response> response = mContext->dispatcher().dispatch(request);
        while (!response.has_value() && !captured.has_value()) {
            const core::result<void> stepped = pump_frame();
            if (!stepped.has_value()) {
                break;
            }
        }
        if (!response.has_value()) {
            response = captured;
        }
        mContext->unregister_response_sink(clientId);
        mContext->events().unregister_client(clientId);
        if (!response.has_value() || !response->ok) {
            std::cerr << "export failed: " << (
              response.has_value() ? response->errorMessage : std::string("no response")
            ) << '\n';
            return 1;
        }
        std::cout << serialise(*response) << '\n';
        return 0;
    }

    int editor_application::run_exec() {
        // initialise() read and parsed the file before the engine started; all that can still fail is the output.
        headless_runner runner = {};
        const core::result<exec_summary> summary = runner.run(*this, mOptions, mExecRequests);
        if (!summary.has_value()) {
            std::cerr << summary.get_error().message() << '\n';
            return static_cast<int>(engine::exit_code::RUNTIME_FAILURE);
        }
        return summary.value().exitCode;
    }
}
