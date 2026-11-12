#include "conjure/engine/export.h"

#include "engine/export_commands.h"

#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/log_category.h"
#include "conjure/engine/project.h"

#include "conjure/assets/asset_cooker.h"
#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/core/clock.h"
#include "conjure/core/dynamic_library.h"
#include "conjure/core/json/document.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/log.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/process.h"
#include "conjure/core/vfs/native_file_system.h"
#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_manifest.h"

#include <format>
#include <system_error>
#include <utility>

using namespace conjure;
using namespace conjure::engine;

namespace {

    constexpr std::string_view COOKED_DIRECTORY = "cooked";
    constexpr std::string_view MANAGED_DIRECTORY = "managed";

    [[nodiscard]] core::error export_error(std::string_view step, std::string_view detail) {
        return make_error(engine_error::EXPORT_FAILED, std::format("export step '{}' failed: {}", step, detail));
    }

    [[nodiscard]] core::error export_error(std::string_view step, const core::error &cause) {
        return export_error(step, cause.to_string());
    }

    [[nodiscard]] std::string runtime_executable_name() {
#if CONJURE_PLATFORM_WINDOWS
        return "conjure_runtime.exe";
#else
        return "conjure_runtime";
#endif
    }

    [[nodiscard]] std::string trim_output(std::string text) {
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) {
            text.pop_back();
        }
        if (text.size() > 2000) {
            text = text.substr(text.size() - 2000);
        }
        return text;
    }

    /// <summary>
    /// Runs a tool and turns a non-zero exit (or a timeout) into EXPORT_FAILED carrying its output.
    /// </summary>
    [[nodiscard]] core::result<void> run_tool(
      std::string_view step,
      std::string executable,
      std::vector<std::string> arguments,
      const std::filesystem::path &workingDirectory,
      f64 timeoutSeconds
    ) {
        core::process_desc desc = {};
        desc.executable = std::move(executable);
        desc.arguments = std::move(arguments);
        desc.workingDirectory = workingDirectory;
        desc.timeoutSeconds = timeoutSeconds;
        desc.mergeStderr = true;
        core::result<core::process_result> ran = core::process::run(desc);
        if (!ran.has_value()) {
            return export_error(
              step,
              std::format("'{}' could not be started: {}", desc.executable, ran.get_error().to_string())
            );
        }
        const core::process_result &result = ran.value();
        if (result.timedOut) {
            return export_error(step, std::format("'{}' timed out after {} s", desc.executable, timeoutSeconds));
        }
        if (result.exitCode != 0) {
            return export_error(
              step,
              std::format(
                "'{}' exited with {}: {}",
                desc.executable,
                result.exitCode,
                trim_output(result.standardOutput)
              )
            );
        }
        return core::result<void>::success();
    }

    [[nodiscard]] core::result<void> copy_into(
      std::string_view step,
      const std::filesystem::path &source,
      const std::filesystem::path &destination,
      std::vector<std::string> &copied,
      const std::filesystem::path &outputDirectory
    ) {
        std::error_code error = {};
        std::filesystem::create_directories(destination.parent_path(), error);
        std::filesystem::copy(source, destination, std::filesystem::copy_options::overwrite_existing, error);
        if (error) {
            return export_error(step, std::format("copying '{}': {}", source.generic_string(), error.message()));
        }
        copied.push_back(destination.lexically_relative(outputDirectory).generic_string());
        return core::result<void>::success();
    }

    /// <summary>
    /// Copies the shader indexes (index.json) and the DXIL / SPIR-V blobs under 'source' at any depth (a module's
    /// conjure_add_shaders() may write shaders/&lt;module&gt;/), keeping their relative paths; the build's .d
    /// depfiles and anything else stay behind (06 3.6 step 5).
    /// </summary>
    [[nodiscard]] core::result<void> copy_shader_blobs(
      std::string_view step,
      const std::filesystem::path &source,
      const std::filesystem::path &destination,
      std::vector<std::string> &copied,
      const std::filesystem::path &outputDirectory
    ) {
        std::error_code error = {};
        std::filesystem::create_directories(destination, error);
        if (error) {
            return export_error(step, std::format("creating '{}': {}", destination.generic_string(), error.message()));
        }
        // The explicit iterator: the range-for increments with the throwing operator++ (CONVENTIONS 3).
        std::filesystem::recursive_directory_iterator walker(source, std::filesystem::directory_options::none, error);
        const std::filesystem::recursive_directory_iterator end = {};
        while (!error && walker != end) {
            std::error_code status = {};
            const std::filesystem::path &path = walker->path();
            const std::filesystem::path extension = path.extension();
            const bool shipped = path.filename() == "index.json" || extension == ".dxil" || extension == ".spv";
            if (shipped && walker->is_regular_file(status)) {
                const std::filesystem::path target = destination / path.lexically_relative(source);
                std::filesystem::create_directories(target.parent_path(), error);
                if (error) {
                    return export_error(
                      step,
                      std::format("creating '{}': {}", target.parent_path().generic_string(), error.message())
                    );
                }
                std::filesystem::copy_file(path, target, std::filesystem::copy_options::overwrite_existing, error);
                if (error) {
                    return export_error(step, std::format("copying '{}': {}", path.generic_string(), error.message()));
                }
            }
            walker.increment(error);
        }
        if (error) {
            return export_error(step, std::format("reading '{}': {}", source.generic_string(), error.message()));
        }
        copied.push_back(destination.lexically_relative(outputDirectory).generic_string());
        return core::result<void>::success();
    }

    /// <summary>
    /// True when 'directory' or one of its ancestors is the existing folder 'folder'. The file system decides
    /// (std::filesystem::equivalent), so a different spelling or case of the same folder still matches.
    /// </summary>
    [[nodiscard]] bool lies_inside(const std::filesystem::path &directory, const std::filesystem::path &folder) {
        std::error_code ignored = {};
        std::filesystem::path candidate = directory;
        while (true) {
            const bool same = std::filesystem::exists(candidate, ignored)
              && std::filesystem::equivalent(candidate, folder, ignored);
            if (same) {
                return true;
            }
            if (!candidate.has_relative_path()) {
                return false;
            }
            candidate = candidate.parent_path();
        }
    }

    /// <summary>
    /// BAD_ARGUMENTS for the project directory itself (step 6 would replace the source project.json with a
    /// cooked one) and for a folder inside the assets root (the next scan would import the exported files).
    /// </summary>
    [[nodiscard]] core::result<void> check_output_directory(
      const std::filesystem::path &output,
      const project &loaded
    ) {
        std::error_code ignored = {};
        const bool isRoot = std::filesystem::exists(output, ignored)
          && std::filesystem::equivalent(output, loaded.root_directory(), ignored);
        if (isRoot) {
            return make_error(
              engine_error::BAD_ARGUMENTS,
              std::format(
                "export output '{}' is the project directory; choose a separate folder",
                output.generic_string()
              )
            );
        }
        if (lies_inside(output, loaded.assets_directory())) {
            return make_error(
              engine_error::BAD_ARGUMENTS,
              std::format(
                "export output '{}' lies inside the assets root '{}'; the exported files would be imported as assets",
                output.generic_string(),
                loaded.assets_directory().generic_string()
              )
            );
        }
        return core::result<void>::success();
    }

    [[nodiscard]] std::string build_preset_of(const project &loaded) {
        const core::json::value build = loaded.description().extensions.root()["build"];
        if (build.is_string()) {
            return std::string(build.as_string());
        }
        if (build.is_object()) {
            return std::string(build["preset"].as_string());
        }
        return "";
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

export_pipeline::export_pipeline(engine_context &context) :
    mContext(&context) {}

export_pipeline::~export_pipeline() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<export_report> export_pipeline::run(const export_settings &settings, const progress_fn &progress) {
    const core::stopwatch watch = {};
    export_report report = {};

    const project *loaded = mContext->project;
    const bool exportable = loaded != nullptr && mContext->assetDatabase != nullptr && mContext->assets != nullptr
      && mContext->jobs != nullptr;
    if (!exportable) {
        return core::error(
          core::error_code::INVALID_STATE,
          "export needs a development project with an asset database"
        );
    }
    if (settings.outputDirectory.empty()) {
        return make_error(engine_error::BAD_ARGUMENTS, "export needs an output directory");
    }
    const engine_config &config = *mContext->config;
    std::error_code fsError = {};
    const std::filesystem::path output =
      std::filesystem::absolute(settings.outputDirectory, fsError).lexically_normal();
    if (fsError) {
        return export_error(
          "prepare",
          std::format("resolving '{}': {}", settings.outputDirectory.generic_string(), fsError.message())
        );
    }
    CONJURE_TRY(check_output_directory(output, *loaded));
    std::filesystem::create_directories(output, fsError);
    if (fsError) {
        return export_error("prepare", std::format("creating '{}': {}", output.generic_string(), fsError.message()));
    }

    const auto announce = [&](usize index) {
        if (progress) {
            progress(STEP_NAMES[index], static_cast<f32>(index) / static_cast<f32>(STEP_NAMES.size()));
        }
        CONJURE_LOG_INFO(LOG_ENGINE, "export: {} ({}/{})", STEP_NAMES[index], index + 1, STEP_NAMES.size());
    };

    // 1. Native game module.
    announce(0);
    {
        const std::filesystem::path manifestPath = loaded->root_directory() / modules::MODULE_MANIFEST_FILE_NAME;
        bool hasBinary = false;
        if (std::filesystem::is_regular_file(manifestPath, fsError)) {
            core::result<modules::module_manifest> manifest = modules::load_module_manifest(manifestPath);
            hasBinary = manifest.has_value() && !manifest.value().binary.empty();
        }
        if (hasBinary && settings.buildNativeModule) {
            const std::string preset = build_preset_of(*loaded);
            if (preset.empty()) {
                report.warnings.emplace_back(
                  "the game module declares a binary but project.json has no extensions.build preset; not built"
                );
            } else {
                CONJURE_TRY(run_tool(
                  STEP_NAMES[0],
                  settings.cmakeExecutable,
                  {"--build", "--preset", preset, "--config", "Distribution"},
                  loaded->root_directory(),
                  settings.processTimeoutSeconds
                ));
                report.nativeModuleBuilt = true;
            }
        }
        report.steps.emplace_back(STEP_NAMES[0]);
    }

    // 2. Cook.
    announce(1);
    {
        assets::asset_database &database = *mContext->assetDatabase;
        assets::cook_settings cook = {};
        cook.outputDirectory = std::string(COOKED_DIRECTORY);
        cook.platform = std::string(database.platform());
        const std::filesystem::path &startupScene = loaded->description().startupScene;
        if (!startupScene.empty()) {
            const std::optional<assets::asset_id> scene =
              database.find_by_path("assets://" + startupScene.lexically_normal().generic_string());
            if (scene.has_value()) {
                cook.roots.push_back(*scene);
            } else {
                report.warnings.push_back(
                  std::format("startup scene '{}' is not a known asset", startupScene.generic_string())
                );
            }
        }
        for (const std::string &path : settings.alwaysInclude) {
            const std::optional<assets::asset_id> id = database.find_by_path(path);
            if (id.has_value()) {
                cook.alwaysInclude.push_back(*id);
            } else {
                report.warnings.push_back(std::format("always_include '{}' is not a known asset", path));
            }
        }
        cook.includeAll = cook.roots.empty();

        core::vfs::native_file_system destination(output, false);
        if (database.cache() == nullptr) {
            return export_error(STEP_NAMES[1], "the asset database is not open");
        }
        assets::asset_cooker cooker(database, *database.cache(), destination, *mContext->jobs);
        core::result<assets::cook_report> cooked = cooker.cook(cook);
        if (!cooked.has_value()) {
            return export_error(STEP_NAMES[1], cooked.get_error());
        }
        report.assetCount = cooked.value().assetCount;
        report.packBytes = cooked.value().packBytes;
        report.contentHash = cooked.value().contentHash;
        for (const assets::cook_failure &failure : cooked.value().failures) {
            report.warnings.push_back(std::format("'{}' did not cook: {}", failure.assetPath, failure.message));
        }
        for (const std::string &warning : cooked.value().warnings) {
            report.warnings.push_back(warning);
        }
        report.copiedFiles.push_back(std::string(COOKED_DIRECTORY) + "/content.cpak");
        report.steps.emplace_back(STEP_NAMES[1]);
    }

    // 3. Managed.
    announce(2);
    if (config.scripting.enabled && settings.publishManaged) {
        if (settings.bootstrapProject.empty()) {
            report.warnings.emplace_back(
              "scripting is enabled but no bootstrap project was given; managed code not published"
            );
        } else {
            const std::filesystem::path managed = output / MANAGED_DIRECTORY;
            std::vector<std::vector<std::string>> commands =
              managed_export_commands(settings, config.scripting, managed);
            for (std::vector<std::string> &arguments : commands) {
                CONJURE_TRY(run_tool(
                  STEP_NAMES[2],
                  settings.dotnetExecutable,
                  std::move(arguments),
                  loaded->root_directory(),
                  settings.processTimeoutSeconds
                ));
            }
            report.managedPublished = true;
            report.copiedFiles.emplace_back(MANAGED_DIRECTORY);
        }
    }
    report.steps.emplace_back(STEP_NAMES[2]);

    // 4. Modules.
    announce(3);
    if (mContext->modules != nullptr) {
        const core::result<void> exported =
          mContext->modules->export_layout(output, modules::build_config::DISTRIBUTION);
        if (!exported.has_value()) {
            return export_error(STEP_NAMES[3], exported.get_error());
        }
        const modules::module_registry &registry = mContext->modules->registry();
        for (const modules::module_id id : registry.load_order()) {
            const modules::module_manifest &manifest = registry.manifest(id);
            const bool shipped = manifest.kind != modules::module_kind::ENGINE && !manifest.editorOnly
              && !registry.root_directory(id).empty();
            if (shipped) {
                ++report.modulesExported;
                report.copiedFiles.push_back("modules/" + manifest.name);
            }
        }
    }
    report.steps.emplace_back(STEP_NAMES[3]);

    // 5. Runtime.
    announce(4);
    if (settings.copyRuntime) {
        if (settings.distributionDirectory.empty()) {
            report.warnings.emplace_back(
              "no distribution directory given; the runtime executable, library and shaders were not copied"
            );
        } else {
            const std::filesystem::path source = settings.distributionDirectory;
            const std::string runtimeName = runtime_executable_name();
            const std::string sharedName = core::dynamic_library::decorate_name("conjure_shared");
            const std::string_view step = STEP_NAMES[4];
            std::vector<std::string> &copied = report.copiedFiles;
            CONJURE_TRY(copy_into(step, source / runtimeName, output / runtimeName, copied, output));
            CONJURE_TRY(copy_into(step, source / sharedName, output / sharedName, copied, output));
            if (std::filesystem::is_regular_file(source / "nethost.dll", fsError)) {
                CONJURE_TRY(copy_into(step, source / "nethost.dll", output / "nethost.dll", copied, output));
            }
            if (settings.copyShaders) {
                if (std::filesystem::is_directory(source / "shaders", fsError)) {
                    CONJURE_TRY(copy_shader_blobs(step, source / "shaders", output / "shaders", copied, output));
                } else {
                    report.warnings.emplace_back("the distribution directory has no shaders/ folder");
                }
            }
        }
    }
    report.steps.emplace_back(STEP_NAMES[4]);

    // 6. project.json.
    announce(5);
    {
        core::json::document document = loaded->to_json();
        document.set("cooked", true);
        document.set("cooked_root", std::string_view(COOKED_DIRECTORY));
        const core::result<void> written = document.write_file(output / PROJECT_FILE_NAME, true);
        if (!written.has_value()) {
            return export_error(STEP_NAMES[5], written.get_error());
        }
        report.copiedFiles.emplace_back(PROJECT_FILE_NAME);
        report.steps.emplace_back(STEP_NAMES[5]);
    }

    if (progress) {
        progress("done", 1.0f);
    }
    report.elapsedSeconds = watch.elapsed_seconds();
    CONJURE_LOG_INFO(
      LOG_ENGINE,
      "export: {} asset(s), {} byte(s) cooked, {} module(s) into '{}' in {:.2f} s",
      report.assetCount,
      report.packBytes,
      report.modulesExported,
      output.generic_string(),
      report.elapsedSeconds
    );
    return report;
}
