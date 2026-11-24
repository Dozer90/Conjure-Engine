#include "editor/core/commands/command_support.h"

#include "conjure/editor/log_category.h"
#include "conjure/editor/project_trust.h"
#include "conjure/editor/protocol/protocol_server.h"

#include "conjure/core/clock.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/vfs/file_system.h"
#include "conjure/core/vfs/path_utils.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/export.h"
#include "conjure/engine/project.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

// project.* (docs/design/04_editor.md 4.4): create, open (an engine restart between frames), save, info,
// scene listing, settings, the export pipeline, and the three commands of project trust (decision D27): trust,
// revoke_trust and trust_state. project.open opens a project only after somebody answered for it; the gate is
// editor_application::request_open_project, and what is recorded lives in project_trust.

namespace conjure::editor::support {

    namespace {
        // The game's scripts project below the project root, as script.create writes it (commands_script.cpp).
        constexpr std::string_view SCRIPTS_PROJECT = "scripts/Game.csproj";

        core::json::document describe_project(const engine::project &project) {
            const engine::project_description &description = project.description();
            core::json::document layers = core::json::document::array();
            for (const std::string &layer : description.layers) {
                layers.push_back(std::string_view(layer));
            }
            return core::json::object({
              {"name", std::string_view(description.name)},
              {"path", project.root_directory().generic_string()},
              {"version", std::string_view(description.version)},
              {"engineVersion", std::string_view(description.engineVersion)},
              {"assetsRoot", description.assetsRoot.generic_string()},
              {"startupScene", description.startupScene.generic_string()},
              {"cooked", description.cooked},
              {"layers", layers},
              {"window", core::json::object({
                 {"title", std::string_view(description.window.title)},
                 {"width", description.window.width},
                 {"height", description.window.height},
               })},
            });
        }

        /// <summary>
        /// The project in a folder, with the errors project.open answers. The engine reports PROJECT_NOT_FOUND
        /// for a missing project.json (PATH_NOT_FOUND here), for one that exists but cannot be opened and for a
        /// 'root' that is a file, not a folder: the last two are IO_ERROR like every other path that exists and
        /// cannot be used.
        /// </summary>
        [[nodiscard]] core::result<engine::project> load_project(const std::filesystem::path &root) {
            core::result<engine::project> project = engine::project::load(root);
            if (project.has_value()) {
                return project;
            }
            const bool notFound = project.get_error().is(engine::engine_error::PROJECT_NOT_FOUND);
            const std::filesystem::path file = root / std::filesystem::path(engine::PROJECT_FILE_NAME);
            std::error_code ignored = {};
            if (notFound && std::filesystem::exists(file, ignored)) {
                return unusable_path(file, "not a readable project file");
            }
            const bool isFolder = std::filesystem::is_directory(root, ignored);
            if (notFound && !isFolder && std::filesystem::exists(root, ignored)) {
                return unusable_path(root, "not a directory");
            }
            return wrap(project.get_error());
        }

        /// <summary>
        /// Who answers a trust question through a command: the transport of a protocol client, "editor" for the
        /// editor's own requests (the window, --exec, a file run by editor.exec_file).
        /// </summary>
        [[nodiscard]] std::string_view trust_origin(editor_context &context, u32 clientId) {
            const protocol_server *server = context.server();
            const client_state *client = server == nullptr ? nullptr : server->client(clientId);
            return client == nullptr ? std::string_view("editor") : to_string(client->kind);
        }

        /// <summary>
        /// {path, state, trusted}: the text the project is remembered under and what is known about it.
        /// </summary>
        [[nodiscard]] core::json::document describe_trust(std::string_view key, trust_state state) {
            return core::json::object({
              {"path", key},
              {"state", to_string(state)},
              {"trusted", state != trust_state::UNTRUSTED},
            });
        }

        core::result<command_outcome> project_create(command_invocation &invocation) {
            CONJURE_TRY_ASSIGN(const std::string_view pathText, require_string(invocation.args, "path"));
            CONJURE_TRY_ASSIGN(const std::string_view name, require_string(invocation.args, "name"));
            if (name.empty()) {
                return invalid_args("args.name: a project needs a name");
            }
            const std::filesystem::path root = std::filesystem::path(std::string(pathText));
            std::error_code ignored = {};
            if (std::filesystem::exists(root / "project.json", ignored)) {
                return invalid_args(std::format("'{}' already holds a project.json", root.generic_string()));
            }
            // Fresh only when this call made the folder, under the account that runs the editor. An empty folder
            // that was already there is not: its owner, or anyone it lets write, can add a module.json later.
            // Asking create_directories, not looking first, leaves no moment for someone to make it in between.
            const std::filesystem::path folder = root.has_filename() ? root : root.parent_path();
            std::error_code createFailure = {};
            const bool freshFolder = std::filesystem::create_directories(folder, createFailure) && !createFailure;
            std::filesystem::create_directories(root / "assets" / "scenes", ignored);
            std::filesystem::create_directories(root / "assets" / "prefabs", ignored);
            std::filesystem::create_directories(root / "scripts", ignored);
            if (!std::filesystem::is_directory(root, ignored)) {
                return io_error(std::format("cannot create '{}'", root.generic_string()));
            }
            // Built as a document so any name (quotes, backslashes, control characters) serialises correctly.
            const core::json::document description = core::json::object({
              {"schema_version", 1},
              {"name", name},
              {"assets_root", "assets"},
            });
            core::result<engine::project> project = engine::project::parse(description.to_string(), root);
            if (!project.has_value()) {
                return wrap(project.get_error());
            }
            const core::result<void> saved = project.value().save();
            if (!saved.has_value()) {
                return io_error(std::format("cannot write project.json: {}", saved.get_error().message()));
            }
            // A project the editor wrote into a folder it made holds nothing but what the editor put there, so it
            // is trusted without a question (decision D27). One created in a folder that was already there, empty
            // or not, is not: what is or will be in it is somebody else's.
            trust_state trusted = trust_state::UNTRUSTED;
            project_trust *trust = invocation.context.trust();
            if (freshFolder && trust != nullptr) {
                const core::result<trust_state> recorded = trust->trust(root, true, "created");
                if (recorded.has_value()) {
                    trusted = recorded.value();
                } else {
                    CONJURE_LOG_WARN(
                      LOG_EDITOR,
                      "project.create: the new project '{}' could not be recorded as trusted: {}",
                      root.generic_string(),
                      recorded.get_error().message()
                    );
                }
            }
            command_outcome outcome = {};
            outcome.label = std::format("Create project '{}'", name);
            outcome.result = describe_project(project.value());
            outcome.result.set("trust", to_string(trusted));
            return outcome;
        }

        core::result<command_outcome> project_open(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(const std::string_view pathText, require_string(invocation.args, "path"));
            const std::filesystem::path root = std::filesystem::path(std::string(pathText));
            CONJURE_TRY_ASSIGN(const engine::project project, load_project(root));
            if (context.host() == nullptr) {
                return not_available("project.open: this session has no process host to restart the engine");
            }
            if (!context.play().is_editing()) {
                return make_editor_error(
                  protocol_error_code::WRONG_PLAY_STATE,
                  "stop playing before opening a project"
                );
            }
            const core::result<void> requested = context.host()->request_open_project(root);
            if (!requested.has_value()) {
                if (requested.get_error().is(protocol_error_code::PROJECT_NOT_TRUSTED)) {
                    // Nobody answered for the project (decision D27). The client is told what to send, and where
                    // a window is open the person at it is asked as well; a refusal is never an answer.
                    const core::result<std::string> key = project_trust::project_key(root);
                    const bool asked = context.raise_trust_prompt(root);
                    invocation.errorDetails = core::json::object({
                      {"path", key.has_value() ? std::string_view(key.value()) : std::string_view(pathText)},
                      {"state", to_string(trust_state::UNTRUSTED)},
                      {"prompt", asked},
                    });
                    CONJURE_LOG_INFO(
                      LOG_EDITOR,
                      "project.open: '{}' is not trusted; client {} was refused{}",
                      root.generic_string(),
                      invocation.clientId,
                      asked ? " and the editor window asks" : ""
                    );
                }
                return wrap(requested.get_error());
            }
            command_outcome outcome = {};
            outcome.result = describe_project(project);
            outcome.result.set("pending", true);
            return outcome;
        }

        core::result<command_outcome> project_trust_grant(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(const std::string_view pathText, require_string(invocation.args, "path"));
            project_trust *trust = context.trust();
            if (trust == nullptr) {
                return not_available("project.trust: this session keeps no project trust");
            }
            const std::filesystem::path root = std::filesystem::path(std::string(pathText));
            // A project is trusted, not a folder that may become one: the same checks as project.open.
            CONJURE_TRY(load_project(root));
            const bool remember = bool_or(invocation.args, "remember", true);
            const std::string_view origin = trust_origin(context, invocation.clientId);
            const core::result<trust_state> state = trust->trust(root, remember, origin);
            if (!state.has_value()) {
                return wrap(state.get_error());
            }
            const core::result<std::string> key = project_trust::project_key(root);
            if (!key.has_value()) {
                return wrap(key.get_error());
            }
            context.close_trust_prompt_for(root);
            CONJURE_LOG_INFO(
              LOG_EDITOR,
              "project trust: '{}' is trusted ({}), answered by client {} ({})",
              key.value(),
              to_string(state.value()),
              invocation.clientId,
              origin
            );
            command_outcome outcome = {};
            outcome.result = describe_trust(key.value(), state.value());
            outcome.result.set("remembered", state.value() == trust_state::REMEMBERED);
            return outcome;
        }

        core::result<command_outcome> project_revoke_trust(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(const std::string_view pathText, require_string(invocation.args, "path"));
            project_trust *trust = context.trust();
            if (trust == nullptr) {
                return not_available("project.revoke_trust: this session keeps no project trust");
            }
            // The folder may be gone by now, so nothing is loaded: an answer can be taken back for any path.
            const std::filesystem::path root = std::filesystem::path(std::string(pathText));
            const core::result<std::string> key = project_trust::project_key(root);
            if (!key.has_value()) {
                return wrap(key.get_error());
            }
            const core::result<bool> revoked = trust->revoke(root);
            if (!revoked.has_value()) {
                return wrap(revoked.get_error());
            }
            if (revoked.value()) {
                CONJURE_LOG_INFO(
                  LOG_EDITOR,
                  "project trust: '{}' is no longer trusted, revoked by client {} ({})",
                  key.value(),
                  invocation.clientId,
                  trust_origin(context, invocation.clientId)
                );
            }
            command_outcome outcome = {};
            outcome.result = describe_trust(key.value(), trust->state_of(root));
            outcome.result.set("revoked", revoked.value());
            return outcome;
        }

        core::result<command_outcome> project_trust_state(command_invocation &invocation) {
            editor_context &context = invocation.context;
            project_trust *trust = context.trust();
            if (trust == nullptr) {
                return not_available("project.trust_state: this session keeps no project trust");
            }
            const std::string_view pathText = string_or(invocation.args, "path", "");
            if (pathText.empty() && !context.has_project()) {
                return invalid_args("args.path: no project is open, so the request must name one");
            }
            const std::filesystem::path root =
              pathText.empty() ? context.project()->root_directory() : std::filesystem::path(std::string(pathText));
            const core::result<std::string> key = project_trust::project_key(root);
            if (!key.has_value()) {
                return wrap(key.get_error());
            }
            command_outcome outcome = {};
            outcome.result = describe_trust(key.value(), trust->state_of(root));
            return outcome;
        }

        core::result<command_outcome> project_save(command_invocation &invocation) {
            engine::project *project = invocation.context.project();
            const core::result<void> saved = project->save();
            if (!saved.has_value()) {
                return io_error(std::format("cannot write project.json: {}", saved.get_error().message()));
            }
            command_outcome outcome = {};
            outcome.label = "Save project";
            outcome.result = core::json::object({{"path", project->file_path().generic_string()}});
            return outcome;
        }

        core::result<command_outcome> project_info(command_invocation &invocation) {
            command_outcome outcome = {};
            outcome.result = describe_project(*invocation.context.project());
            return outcome;
        }

        core::result<command_outcome> project_list_scenes(command_invocation &invocation) {
            editor_context &context = invocation.context;
            std::vector<std::string> scenes = {};
            const core::result<void> enumerated = context.filesystem().enumerate(
              "assets://",
              core::vfs::enumerate_flags::RECURSIVE | core::vfs::enumerate_flags::FILES,
              [&scenes](std::string_view path, const core::vfs::file_stat &stat) {
                  (void) stat;
                  if (core::str::ends_with(path, ".scene.json")) {
                      scenes.emplace_back(path);
                  }
              }
            );
            if (!enumerated.has_value() && enumerated.get_error().generic_code() != core::error_code::NOT_FOUND) {
                return wrap(enumerated.get_error());
            }
            command_outcome outcome = {};
            outcome.result = core::json::object({{"scenes", string_array(scenes)}, {"count", scenes.size()}});
            return outcome;
        }

        core::result<command_outcome> project_set_settings(command_invocation &invocation) {
            editor_context &context = invocation.context;
            engine::project *project = context.project();
            const core::json::value settings = invocation.args["settings"];
            if (!settings.is_object()) {
                return invalid_args("args.settings: expected an object of project.json members");
            }
            const bool replace = bool_or(invocation.args, "replace", false);
            core::json::document previous = project->to_json();
            core::json::document next = replace ? core::json::document::from_value(settings) : project->to_json();
            if (!replace && !next.merge_patch(settings)) {
                return internal("merging the settings failed");
            }
            core::result<engine::project> parsed = engine::project::parse(next.to_string(), project->root_directory());
            if (!parsed.has_value()) {
                return make_editor_error(protocol_error_code::INVALID_ARGS, parsed.get_error().message());
            }
            *project = std::move(parsed).value();
            const core::result<void> saved = project->save();
            if (!saved.has_value()) {
                return io_error(std::format("cannot write project.json: {}", saved.get_error().message()));
            }
            command_outcome outcome = {};
            outcome.label = "Project settings";
            outcome.result = describe_project(*project);
            outcome.inverse = command_inverse{
              "project.set_settings",
              core::json::object({{"settings", previous}, {"replace", true}}),
            };
            return outcome;
        }

        /// <summary>
        /// The game's scripts/Game.csproj (the file script.create writes and script.compile builds), or an empty path
        /// when the project has none.
        /// </summary>
        [[nodiscard]] std::filesystem::path game_scripts_project(editor_context &context) {
            const std::filesystem::path scripts =
              context.project()->root_directory() / std::filesystem::path(SCRIPTS_PROJECT);
            std::error_code probe = {};
            if (!std::filesystem::is_regular_file(scripts, probe)) {
                return {};
            }
            return scripts;
        }

        /// <summary>
        /// Names the two projects export step 3 builds (follow-up FU-PDB): the engine's Conjure.Bootstrap, from the
        /// source tree this editor was built from (CONJURE_EDITOR_BOOTSTRAP_PROJECT, which
        /// engine/src/editor/CMakeLists.txt sets), and the game's scripts/Game.csproj when the project has one (the
        /// file script.create writes and script.compile builds). An editor without that source tree, because it was
        /// built without scripting or no longer lies next to its sources, is NOT_AVAILABLE naming what it misses:
        /// exporting anyway would ship a game whose runtime starts the .NET host and finds no managed code. Where an
        /// installed editor, which has no source tree, finds the bootstrap is not decided yet.
        /// </summary>
        [[nodiscard]] core::result<void> name_managed_projects(
          editor_context &context,
          engine::export_settings &settings
        ) {
#if defined(CONJURE_EDITOR_BOOTSTRAP_PROJECT)
            const std::string_view bootstrapText = CONJURE_EDITOR_BOOTSTRAP_PROJECT;
            const std::filesystem::path bootstrap(std::u8string(bootstrapText.begin(), bootstrapText.end()));
            std::error_code probe = {};
            if (!std::filesystem::is_regular_file(bootstrap, probe)) {
                return not_available(std::format(
                  "managed code cannot be exported: the engine source this editor was built from has no '{}' (an "
                  "editor publishes Conjure.Bootstrap from its source tree); pass publishManaged false to export "
                  "without managed code",
                  bootstrap.generic_string()
                ));
            }
            settings.bootstrapProject = bootstrap;
#else
            (void) settings;
            return not_available(
              "managed code cannot be exported: this editor was built without scripting and knows no "
              "Conjure.Bootstrap project; pass publishManaged false to export without managed code"
            );
#endif
            settings.scriptsProject = game_scripts_project(context);
            return core::result<void>::success();
        }

        core::result<command_outcome> project_export(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(const std::string_view directoryText, require_string(invocation.args, "directory"));
            if (directoryText.empty()) {
                return invalid_args("args.directory: an output directory is required");
            }
            engine::export_settings settings = {};
            // Both folders follow the rule of every path argument: relative means relative to the project root,
            // not to wherever the editor was started (code sweep F187).
            settings.outputDirectory = project_relative_path(context, directoryText);
            const std::string_view distribution = string_or(invocation.args, "distributionDirectory", "");
            settings.distributionDirectory = project_relative_path(context, distribution);
            settings.copyRuntime = bool_or(invocation.args, "copyRuntime", !distribution.empty());
            settings.copyShaders = settings.copyRuntime;
            if (settings.copyRuntime && !distribution.empty()) {
                // The folder the runtime is copied from must be there before anything is cooked: checked here, a
                // missing one answers PATH_NOT_FOUND like every other missing path (code sweep F656) instead of
                // the pipeline's IO_ERROR for the first file it cannot copy.
                std::error_code ignored = {};
                if (!std::filesystem::is_directory(settings.distributionDirectory, ignored)) {
                    return unusable_path(settings.distributionDirectory, "not a directory");
                }
            }
            settings.buildNativeModule = bool_or(invocation.args, "buildNativeModule", false);
            // The default follows what the exported game needs, not how this editor runs its own scripts: a project
            // that enables scripting makes the runtime start the .NET host, which needs the managed directory, also
            // when the editor runs the null host (a headless editor does unless --dotnet) or no script runtime at all
            // (--no-scripting). The pipeline publishes only while scripting is enabled in this editor's
            // configuration; with --no-scripting it refuses the export of such a project, and the reply carries its
            // message, instead of shipping the game without its managed code (follow-up FU-NOSCRIPTING).
            // publishManaged false exports without managed code.
            const engine::engine_config *config = context.engine_context().config;
            const bool scriptingEnabled = config != nullptr && config->scripting.enabled;
            settings.publishManaged =
              bool_or(invocation.args, "publishManaged", context.project()->description().scripting.enabled);
            if (settings.publishManaged) {
                if (scriptingEnabled) {
                    CONJURE_TRY(name_managed_projects(context, settings));
                } else {
                    // Only so that the pipeline's refusal names the scripts that would be missing.
                    settings.scriptsProject = game_scripts_project(context);
                }
            }
            settings.dotnetExecutable = context.dotnet_executable();

            const async_reply reply = invocation.reply;
            const u32 clientId = invocation.clientId;
            event_broadcaster &events = context.events();
            const core::stopwatch watch = {};
            engine::export_pipeline pipeline(context.engine_context());
            core::result<engine::export_report> report = pipeline.run(
              settings,
              [&events, clientId](std::string_view step, f32 fraction) {
                  events.emit(
                    editor_event::PROGRESS,
                    core::json::object({
                      {"operation", "project.export"},
                      {"fraction", fraction},
                      {"message", step},
                      {"clientId", clientId},
                    })
                  );
              }
            );
            if (!report.has_value()) {
                reply.fail(classify_error(report.get_error()), report.get_error().message());
                return command_outcome{};
            }
            core::json::document steps = core::json::document::array();
            for (const std::string &step : report.value().steps) {
                steps.push_back(std::string_view(step));
            }
            core::json::document warnings = core::json::document::array();
            for (const std::string &warning : report.value().warnings) {
                warnings.push_back(std::string_view(warning));
            }
            // A plain export is the pack and the project files only: say so, or the folder looks runnable.
            if (distribution.empty() && !invocation.args.contains("copyRuntime")) {
                warnings.push_back(
                  "no distribution directory given; the runtime executable, library and shaders were not copied "
                  "(pass distributionDirectory, or --distribution with --export)"
                );
            }
            core::json::document result = core::json::object({
              {"directory", settings.outputDirectory.generic_string()},
              {"bytes", report.value().packBytes},
              {"durationMs", watch.elapsed_seconds() * 1000.0},
              {"assetCount", report.value().assetCount},
              {"steps", steps},
              {"warnings", warnings},
            });
            // The runtime executable the pipeline actually copied (its name carries the platform suffix), or
            // null when the runtime was not copied: a path that does not exist would mislead a client.
            result.set_null("runtimePath");
            for (const std::string &copied : report.value().copiedFiles) {
                const bool topLevel = copied.find('/') == std::string::npos;
                if (topLevel && core::vfs::path_utils::stem(copied) == "conjure_runtime") {
                    result.set("runtimePath", (settings.outputDirectory / copied).generic_string());
                    break;
                }
            }
            reply.succeed(std::move(result));
            return command_outcome{};
        }
    }

    void register_project_commands(command_registry &registry) {
        add_command(
          registry,
          make_command(
            "project.create",
            "Creates a project directory with project.json, assets/ and scripts/. A project created in a folder "
            "that did not exist is trusted (result.trust); one created in an existing folder, even an empty one, "
            "is not, see project.trust.",
            R"({"type":"object","properties":{"path":{"type":"string"},"name":{"type":"string"}},)"
            R"("required":["path","name"]})",
            ANY_OBJECT_SCHEMA.data(),
            command_flags::MUTATES | command_flags::NO_UNDO | command_flags::NOT_IN_BATCH,
            &project_create
          )
        );
        add_command(
          registry,
          make_command(
            "project.open",
            "Opens a project: the engine restarts over it at the next frame boundary. Opening runs the project's "
            "scripts and native modules, so a project nobody answered for on this machine is refused with "
            "PROJECT_NOT_TRUSTED (details {path, state, prompt}); send project.trust first.",
            R"({"type":"object","properties":{"path":{"type":"string"}},"required":["path"]})",
            ANY_OBJECT_SCHEMA.data(),
            command_flags::NOT_IN_BATCH,
            &project_open
          )
        );
        add_command(
          registry,
          make_command(
            "project.trust",
            "Answers yes for a project: its scripts and native modules may run with the rights of the user who "
            "runs the editor, and project.open then opens it. project.open never answers by itself. With remember "
            "(the default) the answer is kept in the per-user trust store of this machine, outside the project; "
            "otherwise, or when there is no store or it cannot be written, it holds until the editor exits (state "
            "says which). The path must hold a project.",
            R"({"type":"object","properties":{"path":{"type":"string"},"remember":{"type":"boolean"}},)"
            R"("required":["path"]})",
            R"({"type":"object","properties":{"path":{"type":"string"},)"
            R"("state":{"type":"string","enum":["session","remembered"]},"trusted":{"type":"boolean"},)"
            R"("remembered":{"type":"boolean"}}})",
            command_flags::NOT_IN_BATCH,
            &project_trust_grant
          )
        );
        add_command(
          registry,
          make_command(
            "project.revoke_trust",
            "Takes the answer for a project back, for the session and in the trust store. The folder need not "
            "exist any more. A project that is open stays open; it is asked about the next time it is opened.",
            R"({"type":"object","properties":{"path":{"type":"string"}},"required":["path"]})",
            R"({"type":"object","properties":{"path":{"type":"string"},)"
            R"("state":{"type":"string","enum":["untrusted","session","remembered"]},"trusted":{"type":"boolean"},)"
            R"("revoked":{"type":"boolean"}}})",
            command_flags::NOT_IN_BATCH,
            &project_revoke_trust
          )
        );
        add_command(
          registry,
          make_command(
            "project.trust_state",
            "Whether a project is trusted: untrusted, session (until the editor exits) or remembered (in the "
            "trust store). Without path, or with an empty one, the open project. Changes nothing and asks nobody.",
            R"({"type":"object","properties":{"path":{"type":"string"}}})",
            R"({"type":"object","properties":{"path":{"type":"string"},)"
            R"("state":{"type":"string","enum":["untrusted","session","remembered"]},"trusted":{"type":"boolean"}}})",
            command_flags::READ_ONLY,
            &project_trust_state
          )
        );
        add_command(
          registry,
          make_command(
            "project.save",
            "Writes project.json.",
            R"({"type":"object"})",
            R"({"type":"object","properties":{"path":{"type":"string"}}})",
            command_flags::MUTATES | command_flags::NO_UNDO | command_flags::REQUIRES_PROJECT,
            &project_save
          )
        );
        add_command(
          registry,
          make_command(
            "project.info",
            "The open project's description.",
            R"({"type":"object"})",
            ANY_OBJECT_SCHEMA.data(),
            command_flags::REQUIRES_PROJECT | command_flags::READ_ONLY,
            &project_info
          )
        );
        add_command(
          registry,
          make_command(
            "project.list_scenes",
            "Every .scene.json under assets://.",
            R"({"type":"object"})",
            R"({"type":"object","properties":{"scenes":{"type":"array"},"count":{"type":"integer"}}})",
            command_flags::REQUIRES_PROJECT | command_flags::READ_ONLY,
            &project_list_scenes
          )
        );
        add_command(
          registry,
          make_command(
            "project.set_settings",
            "Merges members into project.json (startup_scene, window, time, ...) and saves it.",
            R"({"type":"object","properties":{"settings":{"type":"object","additionalProperties":true},)"
            R"("replace":{"type":"boolean"}},"required":["settings"]})",
            ANY_OBJECT_SCHEMA.data(),
            command_flags::MUTATES | command_flags::REQUIRES_PROJECT | command_flags::ON_DISK,
            &project_set_settings
          )
        );
        add_command(
          registry,
          make_command(
            "project.export",
            "Runs the export pipeline into a directory (progress events while it runs); runtimePath names the copied "
            "runtime executable, null when the runtime was not copied. An export is always a Distribution export: "
            "distributionDirectory names the Distribution build whose runtime is copied. Relative directories are "
            "relative to the project root. publishManaged (default: the project enables scripting) publishes "
            "Conjure.Bootstrap from the engine source the editor was built from and builds scripts/Game.csproj "
            "when present; an editor without that source answers NOT_AVAILABLE, and an editor run with "
            "--no-scripting refuses a project that enables scripting.",
            R"({"type":"object","properties":{"directory":{"type":"string"},)"
            R"("distributionDirectory":{"type":"string"},"copyRuntime":{"type":"boolean"},)"
            R"("buildNativeModule":{"type":"boolean"},"publishManaged":{"type":"boolean"}},"required":["directory"]})",
            R"({"type":"object","properties":{"directory":{"type":"string"},"runtimePath":{"type":["string","null"]},)"
            R"("bytes":{"type":"integer"},"durationMs":{"type":"number"},"assetCount":{"type":"integer"},)"
            R"("steps":{"type":"array"},"warnings":{"type":"array"}},"additionalProperties":true})",
            command_flags::ASYNC | command_flags::REQUIRES_PROJECT,
            &project_export
          )
        );
    }
}
