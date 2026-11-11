#include "editor/core/commands/command_support.h"

#include "conjure/editor/editor_module_consumer.h"
#include "conjure/editor/log_category.h"
#include "conjure/editor/play_controller.h"

#include "conjure/core/process.h"
#include "conjure/core/string_utils.h"
#include "conjure/engine/project.h"
#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_manifest.h"
#include "conjure/modules/module_registry.h"
#include "conjure/modules/module_types.h"
#include "conjure/modules/module_version.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <system_error>
#include <utility>

// module.* (docs/design/04_editor.md 4.4, 07_modules.md R14/R15): list and discover read the registry; load,
// unload and reload only enqueue module_manager requests that the engine applies at the next frame start, and a
// frame task reports the outcome through the module.state event. module.build runs the project's CMake preset
// through core::process without blocking the frame, then requests a reload.

namespace conjure::editor::support {

    namespace {
        /// <summary>
        /// module.build always runs the cmake found on the PATH; the program is never a client argument.
        /// </summary>
        constexpr std::string_view CMAKE_EXECUTABLE = "cmake";

        /// <summary>
        /// A default-constructed core::error is "no error".
        /// </summary>
        [[nodiscard]] bool has_error(const core::error &error) noexcept {
            return error.code() != 0 || error.category() != 0 || !error.message().empty();
        }

        core::result<modules::module_manager *> require_modules(editor_context &context) {
            modules::module_manager *manager = context.modules();
            if (manager == nullptr) {
                return not_available("the module manager is not running");
            }
            return manager;
        }

        core::json::document describe_module(
          editor_context &context,
          const modules::module_registry &registry,
          modules::module_id id
        ) {
            const modules::module_manifest &manifest = registry.manifest(id);
            core::json::document dependencies = core::json::document::array();
            for (const modules::module_dependency &dependency : manifest.dependencies) {
                dependencies.push_back(core::json::object({
                  {"name", std::string_view(dependency.name)},
                  {"version", modules::to_string(dependency.version)},
                  {"optional", dependency.optional},
                }));
            }
            usize commands = 0;
            for (const command_descriptor *descriptor : context.registry().descriptors()) {
                if (descriptor->owner == manifest.name) {
                    ++commands;
                }
            }
            const core::error &lastError = registry.last_error(id);
            return core::json::object({
              {"name", std::string_view(manifest.name)},
              {"displayName", std::string_view(manifest.displayName)},
              {"version", modules::to_string(manifest.version)},
              {"kind", modules::to_string(manifest.kind)},
              {"state", modules::to_string(registry.state(id))},
              {"lastError", has_error(lastError) ? lastError.message() : std::string("")},
              {"root", registry.root_directory(id).generic_string()},
              {"binary", registry.binary_path(id).generic_string()},
              {"hasBinary", !manifest.binary.empty()},
              {"editorOnly", manifest.editorOnly},
              {"loadCount", registry.load_count(id)},
              {"dependencies", dependencies},
              {"commands", commands},
            });
        }

        core::result<modules::module_id> resolve_module(
          command_invocation &invocation,
          const modules::module_registry &registry,
          std::string_view name
        ) {
            const modules::module_id id = registry.find(core::string_id(name));
            if (id.is_valid()) {
                return id;
            }
            core::json::document candidates = core::json::document::array();
            for (const modules::module_id other : registry.all()) {
                candidates.push_back(std::string_view(registry.manifest(other).name));
            }
            invocation.errorDetails = core::json::object({{"candidates", candidates}});
            return invalid_args(std::format("no module named '{}'", name));
        }

        /// <summary>
        /// Emits module.state once the manager applied the pending requests.
        /// </summary>
        class module_state_task final : public frame_task {
            private:
                std::string mName = "";
                std::string mOperation = "";
                u32 mClientId = 0;

            public:
                module_state_task(std::string name, std::string operation, u32 clientId) :
                    mName(std::move(name)),
                    mOperation(std::move(operation)),
                    mClientId(clientId) {}

                ~module_state_task() override = default;

            public:
                bool tick(editor_context &context) override {
                    modules::module_manager *manager = context.modules();
                    if (manager == nullptr) {
                        return true;
                    }
                    if (manager->pending_count() > 0) {
                        return false;
                    }
                    const modules::module_registry &registry = manager->registry();
                    const modules::module_id id = registry.find(core::string_id(mName));
                    core::json::document data = core::json::object({
                      {"name", std::string_view(mName)},
                      {"operation", std::string_view(mOperation)},
                      {"clientId", mClientId},
                    });
                    if (id.is_valid()) {
                        data.set("state", modules::to_string(registry.state(id)));
                        const core::error &lastError = registry.last_error(id);
                        data.set("lastError", has_error(lastError) ? lastError.message() : std::string(""));
                        data.set(
                          "ok", registry.state(
                            id
                          ) != modules::module_state::FAILED && registry.state(id) != modules::module_state::FAULTED
                        );
                    } else {
                        data.set("state", "unknown");
                        data.set("ok", false);
                    }
                    context.events().emit(editor_event::MODULE_STATE, data);
                    return true;
                }
        };

        /// <summary>
        /// Stops play and saves a dirty scene before a module leaves (R14).
        /// </summary>
        core::result<void> prepare_for_detach(editor_context &context) {
            if (!context.play().is_editing()) {
                CONJURE_TRY(context.play().stop());
            }
            if (context.has_scene() && context.scene_dirty() && !context.scene_path().empty()) {
                command_request save = {};
                save.name = "scene.save";
                const core::result<core::json::document> saved = context.dispatcher().execute(save);
                if (!saved.has_value()) {
                    CONJURE_LOG_WARN(
                      LOG_EDITOR,
                      "module operation: the scene could not be saved first: {}",
                      saved.get_error().message()
                    );
                }
            }
            return core::result<void>::success();
        }

        core::result<command_outcome> module_list(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(modules::module_manager *manager, require_modules(context));
            const modules::module_registry &registry = manager->registry();
            core::json::document list = core::json::document::array();
            for (const modules::module_id id : registry.all()) {
                list.push_back(describe_module(context, registry, id));
            }
            core::json::document order = core::json::document::array();
            for (const modules::module_id id : registry.load_order()) {
                order.push_back(std::string_view(registry.manifest(id).name));
            }
            command_outcome outcome = {};
            outcome.result = core::json::object({
              {"modules", list},
              {"loadOrder", order},
              {"pending", manager->pending_count()},
              {"count", list.root().size()},
            });
            return outcome;
        }

        core::result<command_outcome> module_discover(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(modules::module_manager *manager, require_modules(context));
            CONJURE_TRY_ASSIGN(const std::string_view directoryText, require_string(invocation.args, "directory"));
            std::filesystem::path directory = std::filesystem::path(std::string(directoryText));
            if (!directory.is_absolute() && context.project() != nullptr) {
                directory = context.project()->root_directory() / directory;
            }
            std::error_code ignored = {};
            if (!std::filesystem::is_directory(directory, ignored)) {
                return make_editor_error(
                  protocol_error_code::PATH_NOT_FOUND,
                  std::format("'{}' is not a directory", directory.generic_string())
                );
            }
            const core::result<u32> added = manager->discover(directory);
            core::json::document result = core::json::object({{"directory", directory.generic_string()}});
            if (added.has_value()) {
                result.set("added", added.value());
            } else {
                // Valid manifests were still added; the error lists the bad ones.
                result.set("added", 0);
                result.set("warning", added.get_error().message());
            }
            command_outcome outcome = {};
            outcome.result = std::move(result);
            return outcome;
        }

        core::result<command_outcome> module_load(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(modules::module_manager *manager, require_modules(context));
            CONJURE_TRY_ASSIGN(const std::string_view name, require_string(invocation.args, "name"));
            CONJURE_TRY_ASSIGN(const modules::module_id id, resolve_module(invocation, manager->registry(), name));
            const modules::module_state state = manager->registry().state(id);
            bool requested = false;
            if (state == modules::module_state::LOADED) {
                requested = false;
            } else if (state == modules::module_state::SKIPPED) {
                return not_available(std::format("module '{}' excludes this platform", name));
            } else if (state == modules::module_state::FAULTED) {
                return make_editor_error(
                  protocol_error_code::INTERNAL,
                  std::format("module '{}' is faulted; restart the editor", name)
                );
            } else {
                manager->request_load(id);
                requested = true;
                context.add_frame_task(std::make_unique<module_state_task>(
                  std::string(name),
                  "load",
                  invocation.clientId
                ));
            }
            command_outcome outcome = {};
            outcome.label = std::format("Load module '{}'", name);
            outcome.result = describe_module(context, manager->registry(), id);
            outcome.result.set("requested", requested);
            return outcome;
        }

        core::result<command_outcome> module_unload(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(modules::module_manager *manager, require_modules(context));
            CONJURE_TRY_ASSIGN(const std::string_view name, require_string(invocation.args, "name"));
            CONJURE_TRY_ASSIGN(const modules::module_id id, resolve_module(invocation, manager->registry(), name));
            const std::string_view policyText = string_or(invocation.args, "policy", "refuse_if_in_use");
            modules::unload_policy policy = modules::unload_policy::REFUSE_IF_IN_USE;
            if (policyText == "remove_instances") {
                policy = modules::unload_policy::REMOVE_INSTANCES;
            } else if (policyText != "refuse_if_in_use") {
                return invalid_args("args.policy: expected \"refuse_if_in_use\" or \"remove_instances\"");
            }
            if (manager->registry().manifest(id).kind == modules::module_kind::ENGINE) {
                return invalid_args(std::format("'{}' is an engine module and cannot be unloaded", name));
            }
            if (manager->registry().state(id) != modules::module_state::LOADED) {
                return invalid_args(std::format(
                  "module '{}' is not loaded ({})",
                  name,
                  modules::to_string(manager->registry().state(id))
                ));
            }
            CONJURE_TRY(prepare_for_detach(context));
            manager->request_unload(id, policy);
            context.add_frame_task(std::make_unique<module_state_task>(
              std::string(name),
              "unload",
              invocation.clientId
            ));
            command_outcome outcome = {};
            outcome.label = std::format("Unload module '{}'", name);
            outcome.result = describe_module(context, manager->registry(), id);
            outcome.result.set("requested", true);
            outcome.result.set("policy", policyText);
            return outcome;
        }

        core::result<command_outcome> module_reload(command_invocation &invocation) {
            editor_context &context = invocation.context;
            CONJURE_TRY_ASSIGN(modules::module_manager *manager, require_modules(context));
            CONJURE_TRY_ASSIGN(const std::string_view name, require_string(invocation.args, "name"));
            CONJURE_TRY_ASSIGN(const modules::module_id id, resolve_module(invocation, manager->registry(), name));
            const std::string_view scopeText = string_or(invocation.args, "scope", "full");
            modules::reload_scope scope = modules::reload_scope::FULL;
            if (scopeText == "managed_only") {
                scope = modules::reload_scope::MANAGED_ONLY;
            } else if (scopeText != "full") {
                return invalid_args("args.scope: expected \"full\" or \"managed_only\"");
            }
            if (manager->registry().manifest(id).kind == modules::module_kind::ENGINE) {
                return invalid_args(std::format("'{}' is an engine module and cannot be reloaded", name));
            }
            if (manager->registry().state(id) != modules::module_state::LOADED) {
                return invalid_args(std::format(
                  "module '{}' is not loaded ({})",
                  name,
                  modules::to_string(manager->registry().state(id))
                ));
            }
            CONJURE_TRY(prepare_for_detach(context));
            manager->request_reload(id, scope);
            context.add_frame_task(std::make_unique<module_state_task>(
              std::string(name),
              "reload",
              invocation.clientId
            ));
            command_outcome outcome = {};
            outcome.label = std::format("Reload module '{}'", name);
            outcome.result = describe_module(context, manager->registry(), id);
            outcome.result.set("requested", true);
            outcome.result.set("scope", scopeText);
            return outcome;
        }

        std::string_view cmake_config_name(modules::build_config config) {
            switch (config) {
                case modules::build_config::DEBUG:
                    return "Debug";
                case modules::build_config::DISTRIBUTION:
                    return "Distribution";
                case modules::build_config::DEVELOPMENT:
                    break;
            }
            return "Development";
        }

        std::string build_preset_of(const engine::project &project) {
            const core::json::value build = project.description().extensions.root()["build"];
            if (build.is_string()) {
                return std::string(build.as_string());
            }
            if (build.is_object() && build["preset"].is_string()) {
                return std::string(build["preset"].as_string());
            }
            return "";
        }

        /// <summary>
        /// Polls the running CMake build; on exit parses the diagnostics, answers and requests the reload.
        /// </summary>
        class module_build_task final : public frame_task {
            private:
                core::process mProcess = {};
                async_reply mReply = {};
                std::string mModuleName = "";
                std::string mOutput = "";
                u32 mClientId = 0;
                bool mReloadAfter = true;

            public:
                module_build_task(
                  core::process process,
                  async_reply reply,
                  std::string moduleName,
                  u32 clientId,
                  bool reloadAfter
                ) :
                    mProcess(std::move(process)),
                    mReply(reply),
                    mModuleName(std::move(moduleName)),
                    mClientId(clientId),
                    mReloadAfter(reloadAfter) {}

                ~module_build_task() override {
                    if (mReply.valid()) {
                        mReply.fail(protocol_error_code::CANCELLED, "module.build was dropped before it finished");
                    }
                }

            public:
                bool tick(editor_context &context) override {
                    mProcess.read_output(mOutput);
                    mProcess.read_error(mOutput);
                    if (mProcess.is_running()) {
                        return false;
                    }
                    core::result<core::process_result> finished = mProcess.wait();
                    if (!finished.has_value()) {
                        mReply.fail(protocol_error_code::INTERNAL, finished.get_error().message());
                        return true;
                    }
                    mOutput += finished.value().standardOutput;
                    mOutput += finished.value().standardError;
                    core::json::document diagnostics = core::json::document::array();
                    u32 errors = 0;
                    u32 warnings = 0;
                    for (const std::string_view line : core::str::split_lines(mOutput)) {
                        // Only lines in the MSVC, GCC/Clang or CMake diagnostic formats count; a path or a
                        // message that merely contains the word "error" does not.
                        const std::optional<tool_diagnostic> diagnostic = parse_tool_diagnostic(line);
                        if (!diagnostic.has_value()) {
                            continue;
                        }
                        errors += diagnostic->isError ? 1u : 0u;
                        warnings += diagnostic->isError ? 0u : 1u;
                        if (diagnostics.root().size() < 500) {
                            diagnostics.push_back(core::json::object({
                              {"severity", diagnostic->isError ? "error" : "warning"},
                              {"file", std::string_view(diagnostic->file)},
                              {"line", diagnostic->line},
                              {"column", diagnostic->column},
                              {"code", std::string_view(diagnostic->code)},
                              {"message", std::string_view(diagnostic->message)},
                            }));
                        }
                    }
                    const bool succeeded = finished.value().exitCode == 0 && !finished.value().timedOut;
                    core::json::document result = core::json::object({
                      {"succeeded", succeeded},
                      {"exitCode", finished.value().exitCode},
                      {"timedOut", finished.value().timedOut},
                      {"durationMs", finished.value().elapsedSeconds * 1000.0},
                      {"errors", errors},
                      {"warnings", warnings},
                      {"diagnostics", diagnostics},
                      {"module", std::string_view(mModuleName)},
                      {"reloadRequested", false},
                    });
                    if (succeeded && mReloadAfter && context.modules() != nullptr) {
                        const modules::module_id id = context.modules()->registry().find(core::string_id(mModuleName));
                        if (id.is_valid() && context.modules()->registry().state(id) == modules::module_state::LOADED) {
                            const core::result<void> prepared = prepare_for_detach(context);
                            if (prepared.has_value()) {
                                context.modules()->request_reload(id, modules::reload_scope::FULL);
                                context.add_frame_task(std::make_unique<module_state_task>(
                                  mModuleName,
                                  "reload",
                                  mClientId
                                ));
                                result.set("reloadRequested", true);
                            }
                        } else if (id.is_valid()) {
                            context.modules()->request_load(id);
                            context.add_frame_task(std::make_unique<module_state_task>(mModuleName, "load", mClientId));
                            result.set("reloadRequested", true);
                        }
                    }
                    context.events().emit(editor_event::MODULE_STATE, core::json::object({
                      {"name", std::string_view(mModuleName)},
                      {"operation", "build"},
                      {"ok", succeeded},
                      {"clientId", mClientId},
                    }));
                    if (succeeded) {
                        mReply.succeed(std::move(result));
                    } else {
                        mReply.fail(
                          protocol_error_code::COMPILE_FAILED,
                          std::format("the build exited with code {}", finished.value().exitCode),
                          result
                        );
                    }
                    return true;
                }
        };

        core::result<command_outcome> module_build(command_invocation &invocation) {
            editor_context &context = invocation.context;
            const engine::project *project = context.project();
            if (project == nullptr) {
                return make_editor_error(protocol_error_code::PROJECT_NOT_OPEN, "module.build needs a project");
            }
            std::string name = std::string(string_or(invocation.args, "name", ""));
            std::filesystem::path moduleRoot = project->root_directory();
            if (!name.empty()) {
                CONJURE_TRY_ASSIGN(modules::module_manager *manager, require_modules(context));
                CONJURE_TRY_ASSIGN(const modules::module_id id, resolve_module(invocation, manager->registry(), name));
                const std::filesystem::path &root = manager->registry().root_directory(id);
                if (!root.empty()) {
                    moduleRoot = root;
                }
            } else {
                // The game module at the project root.
                const std::filesystem::path manifestPath = project->root_directory() / std::filesystem::path(
                  modules::MODULE_MANIFEST_FILE_NAME
                );
                const core::result<modules::module_manifest> manifest = modules::load_module_manifest(manifestPath);
                if (!manifest.has_value()) {
                    return not_available("the project has no module.json game module to build");
                }
                name = manifest.value().name;
                if (manifest.value().binary.empty()) {
                    return not_available(std::format("module '{}' declares no native binary", name));
                }
            }
            const std::string preset = build_preset_of(*project);
            std::error_code ignored = {};
            std::vector<std::string> arguments = {};
            // Everything handed to the child comes from the project (preset) or passes a conservative character
            // set: a localhost protocol client must not be able to run arbitrary programs or inject options.
            const std::string_view configuration = string_or(
              invocation.args,
              "config",
              cmake_config_name(modules::host_build_config())
            );
            if (!is_safe_tool_argument(configuration)) {
                return invalid_args("args.config: expected a configuration name (letters, digits, '_', '-', '.')");
            }
            const std::string_view target = string_or(invocation.args, "target", "");
            if (!target.empty() && !is_safe_tool_argument(target)) {
                return invalid_args("args.target: expected a target name (letters, digits, '_', '-', '.')");
            }
            if (!preset.empty() && !is_safe_tool_argument(preset)) {
                return not_available("project.json extensions.build names a preset with unsupported characters");
            }
            if (!preset.empty()) {
                arguments = {"--build", "--preset", preset, "--config", std::string(configuration)};
            } else if (std::filesystem::is_directory(moduleRoot / "build", ignored)) {
                arguments = {"--build", (
                  moduleRoot / "build"
                ).generic_string(), "--config", std::string(configuration)};
            } else {
                return not_available(
                  "project.json has no extensions.build preset and the module has no build/ directory; configure the "
                  "module first"
                );
            }
            if (!target.empty()) {
                arguments.emplace_back("--target");
                arguments.emplace_back(target);
            }
            core::process_desc desc = {};
            desc.executable = std::string(CMAKE_EXECUTABLE);
            desc.arguments = std::move(arguments);
            desc.workingDirectory = project->root_directory();
            desc.timeoutSeconds = std::max(1.0, context.dispatcher().async_timeout_seconds() * 0.9);
            core::result<core::process> started = core::process::start(desc);
            if (!started.has_value()) {
                return not_available(std::format("cmake could not be started: {}", started.get_error().message()));
            }
            context.events().emit(editor_event::PROGRESS, core::json::object({
              {"operation", "module.build"},
              {"fraction", 0.0},
              {"message", std::format("building {}", name)},
              {"clientId", invocation.clientId},
            }));
            context.add_frame_task(std::make_unique<module_build_task>(
              std::move(started).value(),
              invocation.reply,
              name,
              invocation.clientId,
              bool_or(invocation.args, "reload", true)
            ));
            return command_outcome{};
        }
    }

    void register_module_commands(command_registry &registry) {
        const std::string moduleSchema = ANY_OBJECT_SCHEMA.data();
        add_command(
          registry,
          make_command(
            "module.list",
            "Every registered module with its state, version, last error, dependencies and command count.",
            std::string(EMPTY_SCHEMA),
            R"({"type":"object","properties":{"modules":{"type":"array"},"loadOrder":{"type":"array"},)"
            R"("pending":{"type":"integer"},"count":{"type":"integer"}}})",
            command_flags::NONE,
            &module_list
          )
        );
        add_command(
          registry,
          make_command(
            "module.discover",
            "Registers every <directory>/*/module.json (relative to the project root when not absolute).",
            R"({"type":"object","properties":{"directory":{"type":"string"}},"required":["directory"]})",
            R"({"type":"object","properties":{"directory":{"type":"string"},"added":{"type":"integer"},)"
            R"("warning":{"type":"string"}}})",
            command_flags::NONE,
            &module_discover
          )
        );
        add_command(
          registry,
          make_command(
            "module.load",
            "Enqueues a load; the engine applies it at the next frame start and module.state reports the outcome.",
            R"({"type":"object","properties":{"name":{"type":"string"}},"required":["name"]})",
            moduleSchema,
            command_flags::MUTATES | command_flags::NO_UNDO | command_flags::NOT_IN_BATCH | command_flags::EDITING_ONLY,
            &module_load
          )
        );
        add_command(
          registry,
          make_command(
            "module.unload",
            "Enqueues an unload (play stops and a dirty scene is saved first); refused while the module is in use "
            "unless "
            "policy is remove_instances.",
            R"({"type":"object","properties":{"name":{"type":"string"},"policy":{"type":"string",)"
            R"("enum":["refuse_if_in_use","remove_instances"]}},"required":["name"]})",
            moduleSchema,
            command_flags::MUTATES | command_flags::NO_UNDO | command_flags::NOT_IN_BATCH,
            &module_unload
          )
        );
        add_command(
          registry,
          make_command(
            "module.reload",
            "Enqueues a reload of the module and its dependents (play stops and a dirty scene is saved first).",
            R"({"type":"object","properties":{"name":{"type":"string"},"scope":{"type":"string","enum":["full",)"
            R"("managed_only"]}},"required":["name"]})",
            moduleSchema,
            command_flags::MUTATES | command_flags::NO_UNDO | command_flags::NOT_IN_BATCH,
            &module_reload
          )
        );
        add_command(
          registry,
          make_command(
            "module.build",
            "Runs the project's CMake build (extensions.build preset, or <module>/build) for the editor's "
            "configuration with the cmake on the PATH, then reloads the module.",
            R"({"type":"object","properties":{"name":{"type":"string"},"config":{"type":"string"},)"
            R"("target":{"type":"string"},"reload":{"type":"boolean"}}})",
            R"({"type":"object","properties":{"succeeded":{"type":"boolean"},"diagnostics":{"type":"array"},)"
            R"("exitCode":{"type":"integer"}},"additionalProperties":true})",
            command_flags::ASYNC | command_flags::REQUIRES_PROJECT | command_flags::EDITING_ONLY,
            &module_build
          )
        );
    }
}
