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
#include "conjure/core/string_utils.h"
#include "conjure/core/uuid.h"
#include "conjure/core/vfs/native_file_system.h"
#include "conjure/modules/module_abi.h"
#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_manifest.h"
#include "conjure/scripting/native_api_hash.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/core/json/value.h"
    #include "conjure/renderer/render_settings.h"
#endif

#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::engine;

namespace {

    constexpr std::string_view COOKED_DIRECTORY = "cooked";
    constexpr std::string_view MANAGED_DIRECTORY = "managed";

    // The Visual C++ runtime libraries an MSVC build of the runtime imports: vcruntime140*.dll and msvcp140*.dll.
    // A Windows Distribution build places the toolchain's redistributable copies next to conjure_runtime
    // (runtime/CMakeLists.txt) and step 5 ships them with the game (06 3.6, code sweep F790).
    constexpr std::array<std::string_view, 2> MSVC_RUNTIME_PREFIXES = {"vcruntime140", "msvcp140"};
    constexpr std::string_view MSVC_RUNTIME_EXTENSION = ".dll";
    // The debug runtime carries the same two prefixes and a 'd' that either follows the "140" (vcruntime140d.dll,
    // msvcp140d_atomic_wait.dll) or ends the name (vcruntime140_1d.dll, msvcp140_2d.dll, vcruntime140_threadsd.dll).
    // Microsoft does not allow it to be redistributed, so step 5 leaves it behind and says so (06 D14).
    constexpr char MSVC_DEBUG_RUNTIME_MARK = 'd';

    // The manifest of a prebuilt SDK (export_pipeline::MANAGED_SDK_MANIFEST_NAME, owner decision OWN-INSTALL) as the
    // build target conjure_managed_sdk_export writes it (managed/CMakeLists.txt): the schema, the kind and the host
    // mode it describes. A self-contained prebuilt SDK would carry the .NET runtime of one platform; none is built.
    constexpr i64 MANAGED_SDK_MANIFEST_SCHEMA = 1;
    constexpr std::string_view MANAGED_SDK_KIND = "conjure_managed_sdk_export";
    constexpr std::string_view MANAGED_SDK_HOST_MODE = "framework_dependent";
    // What the .NET host needs in a managed directory (the anchors of scripting::validate_managed_directory).
    constexpr std::array<std::string_view, 3> MANAGED_SDK_FILES = {
        "Conjure.Bootstrap.dll",
        "Conjure.Bootstrap.runtimeconfig.json",
        "Conjure.Managed.dll",
    };
    constexpr std::string_view DEBUG_SYMBOLS_EXTENSION = ".pdb";
    // How long step 3 waits for `dotnet --version` (find_dotnet_sdk) at most; a smaller process timeout wins.
    constexpr f64 DOTNET_PROBE_TIMEOUT_SECONDS = 120.0;

    [[nodiscard]] core::error export_error(std::string_view step, std::string_view detail) {
        return core::make_error(engine_error::EXPORT_FAILED, std::format("export step '{}' failed: {}", step, detail));
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
    /// Runs a tool, with 'environment' added to the inherited one, and turns a non-zero exit (or a timeout) into
    /// EXPORT_FAILED carrying its output.
    /// </summary>
    [[nodiscard]] core::result<void> run_tool(
      std::string_view step,
      std::string executable,
      std::vector<std::string> arguments,
      const std::filesystem::path &workingDirectory,
      f64 timeoutSeconds,
      std::vector<std::pair<std::string, std::string>> environment
    ) {
        core::process_desc desc = {};
        desc.executable = std::move(executable);
        desc.arguments = std::move(arguments);
        desc.workingDirectory = workingDirectory;
        desc.timeoutSeconds = timeoutSeconds;
        desc.mergeStderr = true;
        desc.environment = std::move(environment);
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
        // copy_file, not copy: a source that is a folder, or a folder where the file belongs, is an error here
        // instead of a copy that does nothing or lands one level too deep.
        std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing, error);
        if (error) {
            return export_error(step, std::format("copying '{}': {}", source.generic_string(), error.message()));
        }
        copied.push_back(destination.lexically_relative(outputDirectory).generic_string());
        return core::result<void>::success();
    }

    /// <summary>
    /// True when 'name' reads 'prefix', anything, 'suffix' (both lower-case ASCII), with ASCII letters compared
    /// without case as a Windows volume does. It reads the native characters of a file name, so a name the
    /// active code page cannot express is compared like any other and never converted.
    /// </summary>
    [[nodiscard]] bool has_affixes_ignoring_case(
      std::basic_string_view<std::filesystem::path::value_type> name,
      std::string_view prefix,
      std::string_view suffix
    ) {
        using native_char = std::filesystem::path::value_type;
        if (name.size() < prefix.size() + suffix.size()) {
            return false;
        }
        const auto same = [](native_char found, char expected) {
            const bool upper = found >= static_cast<native_char>('A') && found <= static_cast<native_char>('Z');
            const native_char lowered = upper ? static_cast<native_char>(found + ('a' - 'A')) : found;
            return lowered == static_cast<native_char>(expected);
        };
        for (usize index = 0; index < prefix.size(); ++index) {
            if (!same(name[index], prefix[index])) {
                return false;
            }
        }
        const usize suffixStart = name.size() - suffix.size();
        for (usize index = 0; index < suffix.size(); ++index) {
            if (!same(name[suffixStart + index], suffix[index])) {
                return false;
            }
        }
        return true;
    }

    /// <summary>
    /// What a file name is to step 5: no Visual C++ runtime library, one that ships with the game, or one of
    /// the debug runtime, which does not.
    /// </summary>
    enum class msvc_library_kind : u8 {
        NONE,
        REDISTRIBUTABLE,
        DEBUG_RUNTIME
    };

    [[nodiscard]] msvc_library_kind msvc_library_kind_of(const std::filesystem::path &fileName) {
        using native_char = std::filesystem::path::value_type;
        const std::basic_string_view<native_char> name = fileName.native();
        const auto marksDebug = [](native_char found) {
            const char upperMark = static_cast<char>(MSVC_DEBUG_RUNTIME_MARK - ('a' - 'A'));
            return found == static_cast<native_char>(MSVC_DEBUG_RUNTIME_MARK)
              || found == static_cast<native_char>(upperMark);
        };
        for (const std::string_view prefix : MSVC_RUNTIME_PREFIXES) {
            if (!has_affixes_ignoring_case(name, prefix, MSVC_RUNTIME_EXTENSION)) {
                continue;
            }
            // What stands between the prefix and the extension: "" and "_1" ship, "d" and "_1d" do not.
            const std::basic_string_view<native_char> variant =
              name.substr(prefix.size(), name.size() - prefix.size() - MSVC_RUNTIME_EXTENSION.size());
            const bool debug = !variant.empty() && (marksDebug(variant.front()) || marksDebug(variant.back()));
            return debug ? msvc_library_kind::DEBUG_RUNTIME : msvc_library_kind::REDISTRIBUTABLE;
        }
        return msvc_library_kind::NONE;
    }

    /// <summary>
    /// True for a runtime that imports the Visual C++ runtime DLLs: one built against Microsoft's C++ standard
    /// library and its DLL runtime (/MD, the default). For every other build, one against the static runtime
    /// among them, a distribution directory without the DLLs is complete.
    /// </summary>
    [[nodiscard]] bool runtime_imports_msvc_libraries() {
#if CONJURE_PLATFORM_WINDOWS && CONJURE_STDLIB_MSVC
        return (modules::ABI_SIGNATURE_RUNTIME_FLAGS & modules::ABI_RUNTIME_DYNAMIC_CRT) != 0;
#else
        return false;
#endif
    }

    /// <summary>
    /// What copy_msvc_runtime_libraries did: how many libraries it copied, and the debug runtime libraries it
    /// left behind, in name order.
    /// </summary>
    struct msvc_runtime_copy {
        usize copiedCount = 0;
        std::vector<std::string> debugLibraries = {};
    };

    /// <summary>
    /// Copies every vcruntime140*.dll and msvcp140*.dll that lies directly in 'source' (subfolders are not
    /// searched, and a folder of such a name is not a library) next to the exported runtime, in name order,
    /// except those of the debug runtime, which must not be redistributed and are returned by name instead. A
    /// distribution directory of another platform holds none, which is no error; one that cannot be listed, or
    /// a library that cannot be copied, is EXPORT_FAILED.
    /// </summary>
    [[nodiscard]] core::result<msvc_runtime_copy> copy_msvc_runtime_libraries(
      std::string_view step,
      const std::filesystem::path &source,
      std::vector<std::string> &copied,
      const std::filesystem::path &outputDirectory
    ) {
        std::error_code error = {};
        std::vector<std::filesystem::path> names = {};
        std::vector<std::filesystem::path> debugNames = {};
        // The explicit iterator: the range-for increments with the throwing operator++ (CONVENTIONS 3).
        std::filesystem::directory_iterator walker(source, error);
        const std::filesystem::directory_iterator end = {};
        while (!error && walker != end) {
            std::error_code status = {};
            const std::filesystem::path name = walker->path().filename();
            const msvc_library_kind kind = msvc_library_kind_of(name);
            if (kind != msvc_library_kind::NONE && walker->is_regular_file(status)) {
                if (kind == msvc_library_kind::DEBUG_RUNTIME) {
                    debugNames.push_back(name);
                } else {
                    names.push_back(name);
                }
            }
            walker.increment(error);
        }
        if (error) {
            return export_error(step, std::format("reading '{}': {}", source.generic_string(), error.message()));
        }
        // A directory lists its files in the file system's order; in the byte order of their names the report
        // reads the same everywhere.
        const auto byName = [](const std::filesystem::path &left, const std::filesystem::path &right) {
            return left.native() < right.native();
        };
        std::sort(names.begin(), names.end(), byName);
        std::sort(debugNames.begin(), debugNames.end(), byName);
        for (const std::filesystem::path &name : names) {
            CONJURE_TRY(copy_into(step, source / name, outputDirectory / name, copied, outputDirectory));
        }
        msvc_runtime_copy outcome = {};
        outcome.copiedCount = names.size();
        for (const std::filesystem::path &name : debugNames) {
            outcome.debugLibraries.push_back(name.generic_string());
        }
        return outcome;
    }

    /// <summary>
    /// The names in one line, separated by ", ".
    /// </summary>
    [[nodiscard]] std::string joined_names(const std::vector<std::string> &names) {
        std::string text = {};
        for (const std::string &name : names) {
            if (!text.empty()) {
                text += ", ";
            }
            text += name;
        }
        return text;
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
    /// The folders of 'directory' that do not exist yet, 'directory' first and its outermost missing ancestor last:
    /// what create_directories is about to create.
    /// </summary>
    [[nodiscard]] std::vector<std::filesystem::path> missing_folders(const std::filesystem::path &directory) {
        std::vector<std::filesystem::path> missing = {};
        std::error_code ignored = {};
        std::filesystem::path candidate = directory;
        while (!candidate.empty() && !std::filesystem::exists(candidate, ignored)) {
            missing.push_back(candidate);
            if (!candidate.has_relative_path()) {
                break;
            }
            candidate = candidate.parent_path();
        }
        return missing;
    }

    /// <summary>
    /// Removes again the folders missing_folders() listed, innermost first, for an export refused right after it
    /// created them. std::filesystem::remove deletes only an empty folder, so one another program wrote into in the
    /// meantime stays.
    /// </summary>
    void remove_created_folders(std::span<const std::filesystem::path> created) {
        for (const std::filesystem::path &folder : created) {
            std::error_code ignored = {};
            std::filesystem::remove(folder, ignored);
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
            return core::make_error(
              engine_error::BAD_ARGUMENTS,
              std::format(
                "export output '{}' is the project directory; choose a separate folder",
                output.generic_string()
              )
            );
        }
        if (lies_inside(output, loaded.assets_directory())) {
            return core::make_error(
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

    /// <summary>
    /// The path with '/' separators as UTF-8, for the log and error messages: generic_string() would give the ANSI
    /// code page on Windows.
    /// </summary>
    [[nodiscard]] std::string display_utf8(const std::filesystem::path &path) {
        const std::u8string text = path.generic_u8string();
        return std::string(text.begin(), text.end());
    }

    /// <summary>
    /// BAD_ARGUMENTS for an artifacts directory inside the output directory (the intermediates would ship with the
    /// game) or inside the assets root (the next scan would import them). The output directory must exist: the file
    /// system decides whether two spellings name the same folder.
    /// </summary>
    [[nodiscard]] core::result<void> check_artifacts_directory(
      const std::filesystem::path &artifacts,
      const std::filesystem::path &output,
      const project &loaded
    ) {
        if (lies_inside(artifacts, output)) {
            return core::make_error(
              engine_error::BAD_ARGUMENTS,
              std::format(
                "export artifacts directory '{}' lies inside the output '{}'; the intermediates would ship with the "
                "game",
                display_utf8(artifacts),
                display_utf8(output)
              )
            );
        }
        if (lies_inside(artifacts, loaded.assets_directory())) {
            return core::make_error(
              engine_error::BAD_ARGUMENTS,
              std::format(
                "export artifacts directory '{}' lies inside the assets root '{}'; the intermediates would be imported "
                "as assets",
                display_utf8(artifacts),
                display_utf8(loaded.assets_directory())
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

    /// <summary>
    /// EXPORT_FAILED naming step 3 when the settings ask for managed code and the project enables scripting while
    /// this engine's configuration disables it (--no-scripting). Step 3 publishes only while scripting is enabled,
    /// and the exported runtime of such a project starts the .NET host, so the export would ship a game without the
    /// managed code it needs (follow-up FU-NOSCRIPTING). A project that disables scripting needs none, and settings
    /// that ask for none (publishManaged false) want none.
    /// </summary>
    [[nodiscard]] core::result<void> check_managed_publishable(
      const export_settings &settings,
      const scripting_settings &engineScripting,
      const project &loaded
    ) {
        const bool unpublishable =
          settings.publishManaged && !engineScripting.enabled && loaded.description().scripting.enabled;
        if (!unpublishable) {
            return core::result<void>::success();
        }
        const std::string scripts = settings.scriptsProject.empty()
          ? std::string("it names no game scripts, and its runtime still starts the .NET host")
          : std::format("its scripts '{}' would not be built", display_utf8(settings.scriptsProject));
        return export_error(
          export_pipeline::STEP_NAMES[2],
          std::format(
            "project '{}' enables scripting ({}), but this engine runs with scripting disabled (--no-scripting), so "
            "the game's managed code cannot be published; export without --no-scripting, turn scripting off in the "
            "project (\"scripting\": {{\"enabled\": false}}), or set publishManaged false to export without managed "
            "code",
            loaded.description().name,
            scripts
          )
        );
    }

    /// <summary>
    /// EXPORT_FAILED naming write_project, with the path and reason of the first problem, when the project's
    /// "rendering.defaults" is not valid (render-settings design 2.4): the exported runtime reads it strictly and
    /// would refuse to start, while the editor that exports only reported it. Unknown keys are not problems. A
    /// build without the renderer has no reader and no renderer to refuse to start, so it checks nothing.
    /// </summary>
    [[nodiscard]] core::result<void> check_rendering_defaults(const project &loaded) {
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
        const core::json::value defaults = loaded.description().renderingSection.root()["defaults"];
        const core::result<renderer::render_settings> parsed = renderer::render_settings_from_json(defaults);
        if (!parsed.has_value()) {
            return export_error(
              export_pipeline::STEP_NAMES[5],
              std::format(
                "project '{}' has invalid rendering defaults, which the exported game would refuse to start with: {}",
                loaded.description().name,
                parsed.get_error().message()
              )
            );
        }
#else
        CONJURE_UNUSED(loaded);
#endif
        return core::result<void>::success();
    }

    /// <summary>
    /// EXPORT_FAILED naming step 3 unless 'directory' holds a prebuilt SDK this engine can ship (owner decision
    /// OWN-INSTALL): the manifest export_pipeline::MANAGED_SDK_MANIFEST_NAME of the kind, schema and host mode the
    /// build target conjure_managed_sdk_export writes, naming this engine's native API hash (Conjure.Managed.dll
    /// carries it, and the exported runtime's handshake refuses any other), and the files the .NET host needs. A
    /// self-contained export is refused as well: the prebuilt SDK is framework-dependent.
    /// </summary>
    [[nodiscard]] core::result<void> check_prebuilt_managed_sdk(
      const std::filesystem::path &directory,
      const scripting_settings &scripting
    ) {
        const std::string_view step = export_pipeline::STEP_NAMES[2];
        if (scripting.exportMode == script_export_mode::SELF_CONTAINED) {
            return export_error(
              step,
              std::format(
                "the prebuilt SDK in '{}' is framework-dependent and cannot make a self-contained export; set the "
                "project's scripting.export_mode to \"{}\", or export with an editor built from the engine's source",
                display_utf8(directory),
                to_string(script_export_mode::FRAMEWORK_DEPENDENT)
              )
            );
        }
        const std::filesystem::path manifestPath = directory / export_pipeline::MANAGED_SDK_MANIFEST_NAME;
        std::error_code probe = {};
        if (!std::filesystem::is_regular_file(manifestPath, probe)) {
            return export_error(
              step,
              std::format(
                "'{}' holds no {}, so it is no prebuilt SDK of an export (the build target conjure_managed_sdk_export "
                "publishes one into <build>/bin/managed_export)",
                display_utf8(directory),
                export_pipeline::MANAGED_SDK_MANIFEST_NAME
              )
            );
        }
        const core::result<core::json::document> parsed = core::json::document::parse_file(manifestPath);
        if (!parsed.has_value()) {
            return export_error(
              step,
              std::format("reading '{}': {}", display_utf8(manifestPath), parsed.get_error().message())
            );
        }
        const core::json::value root = parsed.value().root();
        const core::json::value schema = root["schema_version"];
        const bool known = root["kind"].as_string() == MANAGED_SDK_KIND && schema.is_integer()
          && schema.as_i64() == MANAGED_SDK_MANIFEST_SCHEMA;
        if (!known) {
            return export_error(
              step,
              std::format(
                "'{}' is no manifest of schema {} written by conjure_managed_sdk_export",
                display_utf8(manifestPath),
                MANAGED_SDK_MANIFEST_SCHEMA
              )
            );
        }
        const std::string_view hostMode = root["host_mode"].as_string();
        if (hostMode != MANAGED_SDK_HOST_MODE) {
            return export_error(
              step,
              std::format(
                "'{}' describes a '{}' SDK; an export copies only a {} one",
                display_utf8(manifestPath),
                hostMode,
                MANAGED_SDK_HOST_MODE
              )
            );
        }
        const std::string_view hashText = root["native_api_hash"].as_string();
        const std::optional<u64> hash = core::str::parse_uint(hashText, 16);
        if (!hash.has_value() || *hash != CONJURE_NATIVE_API_HASH) {
            return export_error(
              step,
              std::format(
                "the prebuilt SDK in '{}' was built for the native API hash '{}' and this engine's is 0x{:016X}; the "
                "exported runtime would refuse it, so ship the managed_export folder of the build that produced this "
                "engine",
                display_utf8(directory),
                hashText,
                CONJURE_NATIVE_API_HASH
              )
            );
        }
        for (const std::string_view name : MANAGED_SDK_FILES) {
            if (!std::filesystem::is_regular_file(directory / name, probe)) {
                return export_error(
                  step,
                  std::format("the prebuilt SDK in '{}' has no {}", display_utf8(directory), name)
                );
            }
        }
        return core::result<void>::success();
    }

    /// <summary>
    /// True when the file name 'name' is 'expected' (lower-case ASCII) with ASCII letters compared without case.
    /// </summary>
    [[nodiscard]] bool is_name_ignoring_case(
      std::basic_string_view<std::filesystem::path::value_type> name,
      std::string_view expected
    ) {
        return name.size() == expected.size() && has_affixes_ignoring_case(name, expected, "");
    }

    /// <summary>
    /// Copies every regular file directly in the prebuilt SDK 'source' into 'destination' (created when missing),
    /// but its manifest and, unless 'withDebugSymbols', its .pdb files, both named without regard to ASCII case.
    /// Subfolders are not copied: the SDK conjure_managed_sdk_export publishes has none. A folder that cannot be
    /// created or listed, or a file that cannot be copied, is EXPORT_FAILED naming step 3; the files copied before it
    /// stay.
    /// </summary>
    [[nodiscard]] core::result<void> copy_prebuilt_managed_sdk(
      const std::filesystem::path &source,
      const std::filesystem::path &destination,
      bool withDebugSymbols
    ) {
        const std::string_view step = export_pipeline::STEP_NAMES[2];
        std::error_code error = {};
        std::filesystem::create_directories(destination, error);
        if (error) {
            return export_error(step, std::format("creating '{}': {}", display_utf8(destination), error.message()));
        }
        // The explicit iterator: the range-for increments with the throwing operator++ (CONVENTIONS 3).
        std::filesystem::directory_iterator walker(source, error);
        const std::filesystem::directory_iterator end = {};
        while (!error && walker != end) {
            std::error_code status = {};
            const std::filesystem::path name = walker->path().filename();
            const bool manifest = is_name_ignoring_case(name.native(), export_pipeline::MANAGED_SDK_MANIFEST_NAME);
            const bool symbols = has_affixes_ignoring_case(name.native(), "", DEBUG_SYMBOLS_EXTENSION);
            const bool shipped = !manifest && (withDebugSymbols || !symbols);
            if (shipped && walker->is_regular_file(status)) {
                std::filesystem::copy_file(
                  walker->path(),
                  destination / name,
                  std::filesystem::copy_options::overwrite_existing,
                  error
                );
                if (error) {
                    return export_error(
                      step,
                      std::format("copying '{}': {}", display_utf8(walker->path()), error.message())
                    );
                }
            }
            walker.increment(error);
        }
        if (error) {
            return export_error(step, std::format("reading '{}': {}", display_utf8(source), error.message()));
        }
        return core::result<void>::success();
    }

    /// <summary>
    /// The dotnet commands of step 3 (managed_export_commands): the publish of the bootstrap unless 'prebuilt' names
    /// the SDK the step copied, then the build of the game's scripts, all into 'managed'. 'artifacts' is the
    /// caller's artifacts directory, resolved, or empty for one in the staging directory. EXPORT_FAILED naming the
    /// step when no usable .NET SDK answers (find_dotnet_sdk) or a command fails.
    /// </summary>
    [[nodiscard]] core::result<void> build_managed(
      const export_settings &settings,
      const scripting_settings &scripting,
      const project &loaded,
      const std::filesystem::path &managed,
      const std::filesystem::path &artifacts,
      const std::filesystem::path &prebuilt
    ) {
        const std::string_view step = export_pipeline::STEP_NAMES[2];
        // The source roots file both commands import (follow-up FU-PDB) lives in a directory of its own below the
        // system's temporary directory, outside the output that ships and outside every project, as do the
        // intermediates when the settings name no artifacts directory. It is written anew by every export, which
        // also makes MSBuild compile again instead of reusing assemblies an export without the mapping left in the
        // same intermediates. 'staging' removes it when the step ends, whether it succeeded or failed; a removal
        // that fails leaves it there with a warning.
        std::error_code temporaryError = {};
        std::filesystem::path temporary = std::filesystem::temp_directory_path(temporaryError);
        // TMPDIR may be relative on POSIX, and managed_export_commands needs an absolute import: MSBuild would
        // resolve a relative one against each project's directory and silently import nothing.
        if (!temporaryError) {
            temporary = std::filesystem::absolute(temporary, temporaryError);
        }
        if (temporaryError) {
            return export_error(step, std::format("finding the temporary directory: {}", temporaryError.message()));
        }
        core::result<managed_export_staging> staged =
          managed_export_staging::create(temporary / std::format("conjure_export_{}", core::uuid::generate()));
        if (!staged.has_value()) {
            return export_error(step, staged.get_error());
        }
        const managed_export_staging staging = std::move(staged).value();
        CONJURE_LOG_INFO(
          LOG_ENGINE,
          "export: MSBuild staging directory '{}' (removed when the step ends)",
          display_utf8(staging.directory())
        );
        // Without a folder of the caller's, the intermediates go with the staging directory: an export from a
        // build tree used to compile the bootstrap into build/managed_artifacts of the engine's source tree, the
        // default of managed/Directory.Build.props (follow-up FU-ARTIFACTS).
        const std::filesystem::path artifactsDirectory = artifacts.empty() ? staging.artifacts_directory() : artifacts;
        CONJURE_LOG_INFO(LOG_ENGINE, "export: managed artifacts directory '{}'", display_utf8(artifactsDirectory));
        std::vector<std::vector<std::string>> commands = managed_export_commands(
          settings,
          scripting,
          managed,
          staging.source_roots_targets(),
          artifactsDirectory,
          prebuilt
        );
        // Owner decision OWN-INSTALL: the .NET SDK is an optional download, so a machine without one is told which
        // version global.json asks for and where Microsoft offers it, instead of the exit code of a dotnet that has
        // no SDK to run the command with.
        const f64 probeTimeout = settings.processTimeoutSeconds > 0.0
          ? std::min(settings.processTimeoutSeconds, DOTNET_PROBE_TIMEOUT_SECONDS)
          : DOTNET_PROBE_TIMEOUT_SECONDS;
        const core::result<std::string> sdk =
          find_dotnet_sdk(settings.dotnetExecutable, loaded.root_directory(), probeTimeout);
        if (!sdk.has_value()) {
            return export_error(step, sdk.get_error().message());
        }
        CONJURE_LOG_INFO(LOG_ENGINE, "export: building managed code with the .NET SDK {}", sdk.value());
        for (std::vector<std::string> &arguments : commands) {
            CONJURE_TRY(run_tool(
              step,
              settings.dotnetExecutable,
              std::move(arguments),
              loaded.root_directory(),
              settings.processTimeoutSeconds,
              managed_export_environment()
            ));
        }
        return core::result<void>::success();
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
        return core::make_error(engine_error::BAD_ARGUMENTS, "export needs an output directory");
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
    // Before the output directory is created: an export that cannot publish the managed code it was asked for
    // writes nothing (follow-up FU-NOSCRIPTING).
    CONJURE_TRY(check_managed_publishable(settings, config.scripting, *loaded));
    // Before the output directory is created too: a game the export would ship could not start (step 6 writes
    // rendering.defaults as the project has it).
    CONJURE_TRY(check_rendering_defaults(*loaded));
    // Empty: step 3 compiles in a folder of its staging directory (follow-up FU-ARTIFACTS).
    std::filesystem::path artifacts = {};
    if (!settings.artifactsDirectory.empty()) {
        artifacts = std::filesystem::absolute(settings.artifactsDirectory, fsError).lexically_normal();
        if (fsError) {
            return export_error(
              "prepare",
              std::format("resolving '{}': {}", display_utf8(settings.artifactsDirectory), fsError.message())
            );
        }
    }
    // Owner decision OWN-INSTALL: an installed editor names the prebuilt SDK of its install layout instead of a
    // bootstrap project. Its folder is checked before anything is written, when step 3 is going to copy it.
    if (!settings.bootstrapProject.empty() && !settings.prebuiltManagedDirectory.empty()) {
        return core::make_error(
          engine_error::BAD_ARGUMENTS,
          std::format(
            "export settings name both the bootstrap project '{}' and the prebuilt SDK '{}'; step 3 publishes one or "
            "copies the other",
            display_utf8(settings.bootstrapProject),
            display_utf8(settings.prebuiltManagedDirectory)
          )
        );
    }
    std::filesystem::path prebuilt = {};
    if (!settings.prebuiltManagedDirectory.empty()) {
        prebuilt = std::filesystem::absolute(settings.prebuiltManagedDirectory, fsError).lexically_normal();
        if (fsError) {
            return export_error(
              "prepare",
              std::format("resolving '{}': {}", display_utf8(settings.prebuiltManagedDirectory), fsError.message())
            );
        }
        if (config.scripting.enabled && settings.publishManaged) {
            CONJURE_TRY(check_prebuilt_managed_sdk(prebuilt, config.scripting));
        }
    }
    const std::vector<std::filesystem::path> createdFolders = missing_folders(output);
    std::filesystem::create_directories(output, fsError);
    if (fsError) {
        return export_error("prepare", std::format("creating '{}': {}", output.generic_string(), fsError.message()));
    }
    if (!artifacts.empty()) {
        // Once the output directory exists, so that the file system decides whether two spellings name the same
        // folder; a refused export takes the folders it just created away again and so writes nothing.
        core::result<void> checked = check_artifacts_directory(artifacts, output, *loaded);
        if (!checked.has_value()) {
            remove_created_folders(createdFolders);
            return core::error(std::move(checked).get_error());
        }
    }
    // A prebuilt SDK inside the output (an export into the editor's own folder) would be copied onto itself.
    const bool copiesPrebuilt = !prebuilt.empty() && config.scripting.enabled && settings.publishManaged;
    if (copiesPrebuilt && lies_inside(prebuilt, output)) {
        remove_created_folders(createdFolders);
        return core::make_error(
          engine_error::BAD_ARGUMENTS,
          std::format(
            "the prebuilt SDK '{}' lies inside the export output '{}'; export into a folder outside the editor's",
            display_utf8(prebuilt),
            display_utf8(output)
          )
        );
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
                  settings.processTimeoutSeconds,
                  {}
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

    // 3. Managed. With scripting disabled in the configuration the step publishes nothing: the check before step 1
    // refused such an export unless the project disables scripting as well or the settings ask for no managed code.
    announce(2);
    if (config.scripting.enabled && settings.publishManaged) {
        if (settings.bootstrapProject.empty() && prebuilt.empty()) {
            report.warnings.emplace_back(
              "scripting is enabled but neither a bootstrap project nor a prebuilt SDK was given; managed code not "
              "published"
            );
        } else {
            const std::filesystem::path managed = output / MANAGED_DIRECTORY;
            // Owner decision OWN-INSTALL: the prebuilt SDK replaces the publish of the bootstrap. Only the game's
            // scripts still need dotnet, so an export of a project without them runs no dotnet command at all.
            if (!prebuilt.empty()) {
                CONJURE_LOG_INFO(LOG_ENGINE, "export: copying the prebuilt SDK '{}'", display_utf8(prebuilt));
                CONJURE_TRY(copy_prebuilt_managed_sdk(prebuilt, managed, config.scripting.exportDebugSymbols));
            }
            const bool runsDotnet = prebuilt.empty() || !settings.scriptsProject.empty();
            if (runsDotnet) {
                CONJURE_TRY(build_managed(settings, config.scripting, *loaded, managed, artifacts, prebuilt));
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
            CONJURE_TRY_ASSIGN(
              const msvc_runtime_copy runtimeLibraries,
              copy_msvc_runtime_libraries(step, source, copied, output)
            );
            if (!runtimeLibraries.debugLibraries.empty()) {
                report.warnings.push_back(
                  std::format(
                    "the distribution directory holds the debug Visual C++ runtime ({}); Microsoft does not allow "
                    "it to be redistributed, so it was not copied",
                    joined_names(runtimeLibraries.debugLibraries)
                  )
                );
            }
            if (runtimeLibraries.copiedCount == 0 && runtime_imports_msvc_libraries()) {
                report.warnings.emplace_back(
                  "the distribution directory holds no Visual C++ runtime DLL (vcruntime140*.dll, msvcp140*.dll); "
                  "the exported game starts only where the Microsoft Visual C++ Redistributable is installed"
                );
            }
            // The licence notices of the engine and of what is linked into it travel with the binaries (06 D15).
            const std::string noticesName(NOTICES_FILE_NAME);
            if (std::filesystem::is_regular_file(source / noticesName, fsError)) {
                CONJURE_TRY(copy_into(step, source / noticesName, output / noticesName, copied, output));
            } else {
                report.warnings.push_back(
                  std::format(
                    "the distribution directory holds no {}; the exported game carries the engine and the "
                    "third-party software linked into it without their licence notices (building conjure_runtime "
                    "writes the file next to it)",
                    NOTICES_FILE_NAME
                  )
                );
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

    // 6. project.json. An export is a Distribution export (04_editor D19), which never enables GPU validation
    // (code sweep F458): the shipped file says "off" whatever the project names, so project::apply_to cannot raise
    // the runtime's OFF default for players. --gpu-validation on the shipped runtime still sets a level for one run.
    announce(5);
    {
        project shipped = *loaded;
        shipped.description().rendering.gpuValidation = gpu_validation_level::OFF;
        core::json::document document = shipped.to_json();
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
