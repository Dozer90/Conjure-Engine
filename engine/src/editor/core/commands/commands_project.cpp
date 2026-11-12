#include "editor/core/commands/command_support.h"

#include "conjure/editor/log_category.h"

#include "conjure/core/clock.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/vfs/file_system.h"
#include "conjure/core/vfs/path_utils.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/export.h"
#include "conjure/engine/project.h"

#include <format>
#include <fstream>
#include <system_error>
#include <utility>

// project.* (docs/design/04_editor.md 4.4): create, open (an engine restart between frames), save, info,
// scene listing, settings and the export pipeline.

namespace conjure::editor::support {

    namespace {
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
            std::filesystem::create_directories(root, ignored);
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
            command_outcome outcome = {};
            outcome.label = std::format("Create project '{}'", name);
            outcome.result = describe_project(project.value());
            return outcome;
        }

        core::result<command_outcome> project_open(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(const std::string_view pathText, require_string(invocation.args, "path"));
            const std::filesystem::path root = std::filesystem::path(std::string(pathText));
            core::result<engine::project> project = engine::project::load(root);
            if (!project.has_value()) {
                return wrap(project.get_error());
            }
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
                return wrap(requested.get_error());
            }
            command_outcome outcome = {};
            outcome.result = describe_project(project.value());
            outcome.result.set("pending", true);
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

        core::result<command_outcome> project_export(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(const std::string_view directoryText, require_string(invocation.args, "directory"));
            if (directoryText.empty()) {
                return invalid_args("args.directory: an output directory is required");
            }
            engine::export_settings settings = {};
            settings.outputDirectory = std::filesystem::path(std::string(directoryText));
            const std::string_view distribution = string_or(invocation.args, "distributionDirectory", "");
            settings.distributionDirectory = std::filesystem::path(std::string(distribution));
            settings.copyRuntime = bool_or(invocation.args, "copyRuntime", !distribution.empty());
            settings.copyShaders = settings.copyRuntime;
            settings.buildNativeModule = bool_or(invocation.args, "buildNativeModule", false);
            const engine::engine_config *config = context.engine_context().config;
            settings.publishManaged = bool_or(
              invocation.args,
              "publishManaged",
              config != nullptr && config->scripting.enabled && !config->scripting.useNullHost
            );
            settings.dotnetExecutable = context.dotnet_executable();
            const std::string_view configuration = string_or(invocation.args, "config", "distribution");
            (void) configuration;

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
            "Creates a project directory with project.json, assets/ and scripts/.",
            R"({"type":"object","properties":{"path":{"type":"string"},"name":{"type":"string"},)"
            R"("template":{"type":"string"}},)"
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
            "Opens a project: the engine restarts over it at the next frame boundary.",
            R"({"type":"object","properties":{"path":{"type":"string"}},"required":["path"]})",
            ANY_OBJECT_SCHEMA.data(),
            command_flags::NOT_IN_BATCH,
            &project_open
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
            command_flags::REQUIRES_PROJECT,
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
            command_flags::REQUIRES_PROJECT,
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
            command_flags::MUTATES | command_flags::REQUIRES_PROJECT,
            &project_set_settings
          )
        );
        add_command(
          registry,
          make_command(
            "project.export",
            "Runs the export pipeline into a directory (progress events while it runs); runtimePath names the copied "
            "runtime executable, null when the runtime was not copied.",
            R"({"type":"object","properties":{"directory":{"type":"string"},"config":{"type":"string"},)"
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
