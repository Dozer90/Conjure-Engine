#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "engine_test_support.h"

#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/core/dynamic_library.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/process.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/types.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/export.h"
#include "conjure/engine/project.h"
#include "conjure/modules/module_abi.h"
#include "conjure/scripting/native_api_hash.h"
#include "engine/export_commands.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

// export.h (docs/design/06_engine_loop.md 3.6, R21): the six steps over a temp project with null backends,
// then the exported folder booted headless as a cooked project. Step 5 runs over a stand-in distribution folder
// of fake files, the Visual C++ runtime DLLs of a Windows Distribution build (code sweep F790), the debug runtime
// that must stay behind, and the licence notices a build writes next to the runtime (06 D15, code sweep F200)
// among them.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;

namespace {

    [[nodiscard]] application_desc project_desc(project loaded, u64 maxFrames) {
        engine_config config = engine_test::inline_config();
        config.projectPath = loaded.root_directory();
        config.maxFrames = maxFrames;
        loaded.apply_to(config);
        application_desc desc = engine_test::make_desc(config);
        desc.project = std::move(loaded);
        return desc;
    }

    // What export_report::warnings says when the distribution directory of an MSVC runtime holds no Visual C++
    // runtime DLL.
    constexpr std::string_view REDISTRIBUTABLE_WARNING = "Microsoft Visual C++ Redistributable";
    // What it says when the distribution directory holds libraries of the debug runtime.
    constexpr std::string_view DEBUG_RUNTIME_WARNING = "debug Visual C++ runtime";
    // What it says when the distribution directory holds no licence notices.
    constexpr std::string_view NOTICES_WARNING = "without their licence notices";

    [[nodiscard]] std::string runtime_name() {
        return CONJURE_PLATFORM_WINDOWS ? "conjure_runtime.exe" : "conjure_runtime";
    }

    /// <summary>
    /// The stand-in distribution folder "dist" of a temp directory, as a Distribution build lays it out: the
    /// runtime executable and the shared library, both empty.
    /// </summary>
    void write_distribution(const temp_directory &directory) {
        CONJURE_REQUIRE_OK(directory.write_text("dist/" + runtime_name(), ""));
        CONJURE_REQUIRE_OK(directory.write_text("dist/" + core::dynamic_library::decorate_name("conjure_shared"), ""));
    }

    /// <summary>
    /// Exports the project into 'outputDir' with step 5 copying from the "dist" folder of 'directory' (no native
    /// module, no managed code) and returns what the pipeline answered.
    /// </summary>
    [[nodiscard]] core::result<export_report> export_with_runtime(
      const project &loaded,
      const temp_directory &directory,
      const std::filesystem::path &outputDir,
      bool copyShaders
    ) {
        application app(project_desc(loaded, 0));
        CONJURE_REQUIRE_OK(app.initialise());
        export_pipeline pipeline(app.context());
        export_settings settings = {};
        settings.outputDirectory = outputDir;
        settings.distributionDirectory = directory.path() / "dist";
        settings.buildNativeModule = false;
        settings.publishManaged = false;
        settings.copyRuntime = true;
        settings.copyShaders = copyShaders;
        core::result<export_report> ran = pipeline.run(settings);
        app.shutdown();
        return ran;
    }

    /// <summary>
    /// How often an export that copies the runtime from a directory without Visual C++ runtime DLLs names the
    /// redistributable: once where the build imports those DLLs, which is a Windows build against Microsoft's
    /// standard library and its DLL runtime (/MD). On Linux and macOS, in a Windows build against another
    /// standard library and in one against the static runtime, such a directory is complete.
    /// </summary>
    [[nodiscard]] usize expected_redistributable_warnings() {
#if CONJURE_PLATFORM_WINDOWS && CONJURE_STDLIB_MSVC
        return (modules::ABI_SIGNATURE_RUNTIME_FLAGS & modules::ABI_RUNTIME_DYNAMIC_CRT) != 0 ? usize{1} : usize{0};
#else
        return usize{0};
#endif
    }

    [[nodiscard]] std::string file_text(const std::filesystem::path &path) {
        std::ifstream stream(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    [[nodiscard]] usize count_warnings(const export_report &report, std::string_view needle) {
        const auto names = [needle](const std::string &warning) {
            return warning.find(needle) != std::string::npos;
        };
        return static_cast<usize>(std::count_if(report.warnings.begin(), report.warnings.end(), names));
    }

    /// <summary>
    /// How often 'argument' appears in an argument list.
    /// </summary>
    [[nodiscard]] usize count_argument(const std::vector<std::string> &arguments, std::string_view argument) {
        return static_cast<usize>(std::count(arguments.begin(), arguments.end(), argument));
    }

    /// <summary>
    /// The arguments that start with 'prefix', in their order.
    /// </summary>
    [[nodiscard]] std::vector<std::string> arguments_with_prefix(
      const std::vector<std::string> &arguments,
      std::string_view prefix
    ) {
        std::vector<std::string> found = {};
        for (const std::string &argument : arguments) {
            if (argument.starts_with(prefix)) {
                found.push_back(argument);
            }
        }
        return found;
    }

    /// <summary>
    /// The argument that follows the first 'flag'; empty when the flag is missing or comes last.
    /// </summary>
    [[nodiscard]] std::string argument_after(const std::vector<std::string> &arguments, std::string_view flag) {
        const auto found = std::find(arguments.begin(), arguments.end(), flag);
        if (found == arguments.end() || found + 1 == arguments.end()) {
            return {};
        }
        return *(found + 1);
    }

    /// <summary>
    /// The path as UTF-8, the encoding core::process takes; path::string() would give the ANSI code page on
    /// Windows.
    /// </summary>
    [[nodiscard]] std::string utf8(const std::filesystem::path &path) {
        const std::u8string text = path.u8string();
        return std::string(text.begin(), text.end());
    }

    /// <summary>
    /// 'relative' below the root of the working directory's volume (C:\ on Windows, / elsewhere): the absolute path
    /// managed_export_commands requires, for the cases that only build command lines and touch no file.
    /// </summary>
    [[nodiscard]] std::filesystem::path absolute_path(const std::filesystem::path &relative) {
        std::error_code error = {};
        const std::filesystem::path current = std::filesystem::current_path(error);
        REQUIRE_MESSAGE(!error, "reading the working directory: " << error.message());
        return current.root_path() / relative;
    }

    /// <summary>
    /// The application of an export with scripting on the null host, which needs no .NET runtime: step 3 runs
    /// whenever the configuration enables scripting.
    /// </summary>
    [[nodiscard]] application_desc scripted_desc(project loaded, bool exportDebugSymbols) {
        application_desc desc = project_desc(std::move(loaded), 0);
        desc.config.scripting.enabled = true;
        desc.config.scripting.required = false;
        desc.config.scripting.useNullHost = true;
        desc.config.scripting.hotReload = false;
        desc.config.scripting.exportDebugSymbols = exportDebugSymbols;
        return desc;
    }

    /// <summary>
    /// The manifest conjure_managed_sdk_export writes (export_pipeline::MANAGED_SDK_MANIFEST_NAME) with this engine's
    /// native API hash.
    /// </summary>
    [[nodiscard]] std::string prebuilt_manifest() {
        return std::format(
          "{{\n  \"schema_version\": 1,\n  \"kind\": \"conjure_managed_sdk_export\",\n  \"host_mode\": "
          "\"framework_dependent\",\n  \"native_api_hash\": \"0x{:016X}\"\n}}\n",
          CONJURE_NATIVE_API_HASH
        );
    }

    // The stand-in files of a prebuilt SDK, as conjure_managed_sdk_export publishes them; their content is their name.
    constexpr std::array<std::string_view, 8> PREBUILT_FILES = {
        "Conjure.Bootstrap.dll",
        "Conjure.Bootstrap.deps.json",
        "Conjure.Bootstrap.pdb",
        "Conjure.Bootstrap.runtimeconfig.json",
        "Conjure.Managed.dll",
        "Conjure.Managed.deps.json",
        "Conjure.Managed.PDB",
        "Conjure.Managed.xml",
    };

    /// <summary>
    /// Lays out a stand-in prebuilt SDK in the folder 'relative' of 'directory' (PREBUILT_FILES and 'manifest' under
    /// export_pipeline::MANAGED_SDK_MANIFEST_NAME) and returns the folder.
    /// </summary>
    [[nodiscard]] std::filesystem::path write_prebuilt_sdk(
      const temp_directory &directory,
      std::string_view relative,
      std::string_view manifest
    ) {
        for (const std::string_view name : PREBUILT_FILES) {
            CONJURE_REQUIRE_OK(directory.write_text(std::format("{}/{}", relative, name), name));
        }
        CONJURE_REQUIRE_OK(directory.write_text(
          std::format("{}/{}", relative, export_pipeline::MANAGED_SDK_MANIFEST_NAME),
          manifest
        ));
        return directory.file(relative);
    }

    /// <summary>
    /// Exports 'loaded' with scripting on the null host (export mode and debug symbols from 'scripting') and nothing
    /// but the managed step: step 3 copies the prebuilt SDK 'prebuilt' into 'outputDir' and builds 'scriptsProject'
    /// (empty for no game scripts, so no dotnet runs) with 'dotnet'. The log level is INFO, so the staging line of
    /// step 3 (STAGING_LINE) shows whether the step went as far as building.
    /// </summary>
    [[nodiscard]] core::result<export_report> export_prebuilt(
      const project &loaded,
      const scripting_settings &scripting,
      const std::filesystem::path &prebuilt,
      const std::filesystem::path &outputDir,
      const std::filesystem::path &dotnet,
      const std::filesystem::path &scriptsProject
    ) {
        application_desc desc = scripted_desc(loaded, scripting.exportDebugSymbols);
        desc.config.scripting.exportMode = scripting.exportMode;
        desc.config.logLevel = core::log_level::INFO;
        application app(std::move(desc));
        CONJURE_REQUIRE_OK(app.initialise());
        export_pipeline pipeline(app.context());
        export_settings settings = {};
        settings.outputDirectory = outputDir;
        settings.buildNativeModule = false;
        settings.copyRuntime = false;
        settings.prebuiltManagedDirectory = prebuilt;
        settings.scriptsProject = scriptsProject;
        const std::u8string dotnetText = dotnet.u8string();
        settings.dotnetExecutable = std::string(dotnetText.begin(), dotnetText.end());
        core::result<export_report> ran = pipeline.run(settings);
        app.shutdown();
        return ran;
    }

    // The INFO lines in which publish_managed names its MSBuild staging directory and the artifacts directory both
    // dotnet commands compile in; each is followed by the path (generic, UTF-8) and a closing quote.
    constexpr std::string_view STAGING_LINE = "export: MSBuild staging directory '";
    constexpr std::string_view ARTIFACTS_LINE = "export: managed artifacts directory '";

    /// <summary>
    /// The paths the captured INFO lines name after 'prefix', in their order.
    /// </summary>
    [[nodiscard]] std::vector<std::filesystem::path> logged_paths(const log_capture &logs, std::string_view prefix) {
        std::vector<std::filesystem::path> paths = {};
        for (const std::string &line : logs.messages(core::log_level::INFO)) {
            const usize start = line.find(prefix);
            if (start == std::string::npos) {
                continue;
            }
            const usize first = start + prefix.size();
            const usize last = line.find('\'', first);
            REQUIRE_MESSAGE(last != std::string::npos, line);
            const std::string text = line.substr(first, last - first);
            paths.emplace_back(std::u8string(text.begin(), text.end()));
        }
        return paths;
    }

#if CONJURE_ENGINE_TEST_HAS_DOTNET
    // The limits of the [dotnet][slow] cases. The export gives up on a dotnet command after
    // DOTNET_COMMAND_SECONDS and the case on a git command or the path check after TOOL_SECONDS, so one SUBCASE
    // (two dotnet commands, at most four git commands and the check) ends within DOTNET_SUBCASE_SECONDS. A case
    // carries that times its number of SUBCASEs as its doctest timeout, and tests/engine/CMakeLists.txt gives the
    // CTest entries of the largest case a minute more, so a slow export fails naming the command that hung
    // instead of being killed by CTest without a word.
    constexpr f64 DOTNET_COMMAND_SECONDS = 360.0;
    constexpr f64 TOOL_SECONDS = 60.0;
    constexpr f64 DOTNET_SUBCASE_SECONDS = 2.0 * DOTNET_COMMAND_SECONDS + 5.0 * TOOL_SECONDS;

    /// <summary>
    /// Writes 'text' to 'path', creating its folders; temp_directory::write_text takes a narrow name, which cannot
    /// spell a non-ASCII folder on Windows.
    /// </summary>
    void write_file(const std::filesystem::path &path, std::string_view text) {
        std::error_code error = {};
        std::filesystem::create_directories(path.parent_path(), error);
        REQUIRE_MESSAGE(!error, "creating the folder of " << utf8(path) << ": " << error.message());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        REQUIRE_MESSAGE(stream.is_open(), "opening " << utf8(path));
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        stream.close();
        REQUIRE_MESSAGE(static_cast<bool>(stream), "writing " << utf8(path));
    }

    /// <summary>
    /// Copies a file or a folder (with everything in it) of the source tree to 'destination'.
    /// </summary>
    void copy_from_source(std::string_view relative, const std::filesystem::path &destination) {
        const std::filesystem::path source = std::filesystem::path(CONJURE_ENGINE_TEST_SOURCE_DIR) / relative;
        std::error_code error = {};
        std::filesystem::create_directories(destination.parent_path(), error);
        REQUIRE_MESSAGE(!error, "creating " << destination.parent_path().string() << ": " << error.message());
        std::filesystem::copy(source, destination, std::filesystem::copy_options::recursive, error);
        REQUIRE_MESSAGE(!error, "copying " << source.string() << ": " << error.message());
    }

    /// <summary>
    /// True when 'data' holds 'root' as UTF-8 or as UTF-16LE text, with '/' or a backslash between its components and
    /// ASCII letters compared without case: the spellings a compiler records a folder in. A narrow check of the
    /// assembly bytes; tools/check_distribution_paths.py also reads the documents of a portable .pdb.
    /// </summary>
    [[nodiscard]] bool holds_root(std::string_view data, const std::filesystem::path &root) {
        const std::u8string generic = root.generic_u8string();
        const std::string forward(generic.begin(), generic.end());
        std::string backward = forward;
        std::replace(backward.begin(), backward.end(), '/', '\\');
        const std::string lowered = core::str::to_lower(data);
        for (const std::string &spelling : {forward, backward}) {
            const std::string needle = core::str::to_lower(spelling);
            std::string wide = {};
            for (const char character : needle) {
                wide += character;
                wide += '\0';
            }
            if (lowered.find(needle) != std::string::npos || lowered.find(wide) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    /// <summary>
    /// The NUL-terminated string of 'data' that ends with 'suffix', such as the .pdb path of an assembly's CodeView
    /// entry; empty when no string ends so.
    /// </summary>
    [[nodiscard]] std::string string_ending_with(std::string_view data, std::string_view suffix) {
        usize position = data.find(suffix);
        while (position != std::string_view::npos) {
            const usize end = position + suffix.size();
            if (end == data.size() || data[end] == '\0') {
                const usize terminator = data.rfind('\0', position);
                const usize start = terminator == std::string_view::npos ? 0 : terminator + 1;
                return std::string(data.substr(start, end - start));
            }
            position = data.find(suffix, position + 1);
        }
        return {};
    }

    /// <summary>
    /// Lays out in 'directory' what the engine side of a [dotnet] export builds and returns the bootstrap project:
    /// global.json, so that the SDK the repository pins builds it; an empty git repository, which gives no source
    /// root as a folder outside git gives none, so the case behaves the same whether the build tree lies in a
    /// checkout or not; and copies of managed/Directory.Build.props, Conjure.Bootstrap and Conjure.Managed below
    /// engine/managed.
    /// </summary>
    [[nodiscard]] std::filesystem::path stage_engine_managed(const temp_directory &directory) {
        CONJURE_REQUIRE_OK(directory.write_text(".git/HEAD", "ref: refs/heads/main\n"));
        for (const std::string_view folder : {"objects", "refs/heads"}) {
            CAPTURE(folder);
            std::error_code error = {};
            std::filesystem::create_directories(directory.path() / ".git" / std::filesystem::path(folder), error);
            REQUIRE_FALSE(error);
        }
        copy_from_source("global.json", directory.path() / "global.json");
        const std::filesystem::path engineManaged = directory.path() / "engine" / "managed";
        copy_from_source("managed/Directory.Build.props", engineManaged / "Directory.Build.props");
        copy_from_source("managed/Conjure.Bootstrap", engineManaged / "Conjure.Bootstrap");
        copy_from_source("managed/Conjure.Managed", engineManaged / "Conjure.Managed");
        return engineManaged / "Conjure.Bootstrap" / "Conjure.Bootstrap.csproj";
    }

    /// <summary>
    /// Exports 'loaded' into 'outputDir' with the real dotnet and nothing but the managed step: the publish of
    /// 'bootstrapProject', then the build of 'scriptsProject', both compiling in 'artifactsDirectory' (empty: the
    /// pipeline's default). The log level is INFO, so a log_capture sees the lines that name the directories.
    /// </summary>
    [[nodiscard]] core::result<export_report> export_managed(
      const project &loaded,
      bool exportDebugSymbols,
      const std::filesystem::path &bootstrapProject,
      const std::filesystem::path &scriptsProject,
      const std::filesystem::path &outputDir,
      const std::filesystem::path &artifactsDirectory
    ) {
        application_desc desc = scripted_desc(loaded, exportDebugSymbols);
        desc.config.logLevel = core::log_level::INFO;
        application app(std::move(desc));
        CONJURE_REQUIRE_OK(app.initialise());
        export_pipeline pipeline(app.context());
        export_settings settings = {};
        settings.outputDirectory = outputDir;
        settings.buildNativeModule = false;
        settings.copyRuntime = false;
        settings.dotnetExecutable = CONJURE_ENGINE_TEST_DOTNET;
        settings.bootstrapProject = bootstrapProject;
        settings.scriptsProject = scriptsProject;
        settings.artifactsDirectory = artifactsDirectory;
        settings.processTimeoutSeconds = DOTNET_COMMAND_SECONDS;
        core::result<export_report> ran = pipeline.run(settings);
        app.shutdown();
        return ran;
    }

    /// <summary>
    /// Runs the git found at configure time in 'repository' and requires it to succeed. Neither the system's nor the
    /// user's git configuration is read (no hook, signing or line-ending rule of the machine applies) and the
    /// identity of the commit is given on the command line.
    /// </summary>
    void run_git(const std::filesystem::path &repository, const std::vector<std::string> &arguments) {
        core::process_desc desc = {};
        desc.executable = CONJURE_ENGINE_TEST_GIT;
        desc.arguments = {
            "-c",
            "user.name=Conjure Test",
            "-c",
            "user.email=test@example.invalid",
            "-c",
            "commit.gpgsign=false",
            "-c",
            "core.hooksPath=.git/no_hooks",
        };
        desc.arguments.insert(desc.arguments.end(), arguments.begin(), arguments.end());
        desc.workingDirectory = repository;
        // A global configuration file that does not exist is read as empty.
        desc.environment = {
            {"GIT_CONFIG_NOSYSTEM", "1"},
            {"GIT_CONFIG_GLOBAL", utf8(repository / ".git" / "no_global_config")},
        };
        desc.mergeStderr = true;
        desc.timeoutSeconds = TOOL_SECONDS;
        CAPTURE(arguments.front());
        const core::result<core::process_result> ran = core::process::run(desc);
        CONJURE_REQUIRE_OK(ran);
        REQUIRE_FALSE(ran.value().timedOut);
        REQUIRE_MESSAGE(ran.value().exitCode == 0, ran.value().standardOutput);
    }

    /// <summary>
    /// Turns the stand-in .git of stage_engine_managed into a real repository holding one commit of everything in
    /// 'directory', with a GitHub remote (never contacted) from which Source Link derives its URLs: the layout in
    /// which source control gives the SDK a source root of its own.
    /// </summary>
    void commit_everything(const temp_directory &directory) {
        std::error_code error = {};
        std::filesystem::remove_all(directory.path() / ".git", error);
        REQUIRE_MESSAGE(!error, "removing the stand-in .git: " << error.message());
        run_git(directory.path(), {"init", "-q"});
        run_git(directory.path(), {"add", "-A"});
        run_git(directory.path(), {"commit", "-q", "-m", "export fixture"});
        run_git(directory.path(), {"remote", "add", "origin", "https://github.com/conjure-engine/export-fixture.git"});
    }

    /// <summary>
    /// The folders no managed binary of a [dotnet] export may name: the case's temp directory, the checkout, the build
    /// tree and the system's temporary directory, below which the export's staging directory and its default
    /// artifacts directory lie (the staging directory itself is gone once the export returns, and
    /// tools/check_distribution_paths.py takes existing folders only).
    /// </summary>
    [[nodiscard]] std::vector<std::filesystem::path> machine_roots(const temp_directory &directory) {
        std::error_code error = {};
        const std::filesystem::path temporary = std::filesystem::temp_directory_path(error);
        REQUIRE_MESSAGE(!error, "finding the temporary directory: " << error.message());
        return {
            directory.path(),
            std::filesystem::path(CONJURE_ENGINE_TEST_SOURCE_DIR),
            std::filesystem::path(CONJURE_ENGINE_TEST_BINARY_DIR),
            std::filesystem::absolute(temporary, error),
        };
    }

    /// <summary>
    /// Checks that no binary holds one of 'roots': in the bytes of each file (holds_root), then, when Python was found
    /// at configure time, in the documents of the .pdb files too, with tools/check_distribution_paths.py.
    /// </summary>
    void check_no_machine_paths(
      const std::vector<std::filesystem::path> &binaries,
      const std::vector<std::filesystem::path> &roots
    ) {
        for (const std::filesystem::path &binary : binaries) {
            CAPTURE(binary.string());
            const std::string data = file_text(binary);
            for (const std::filesystem::path &root : roots) {
                CAPTURE(root.string());
                CHECK_FALSE(holds_root(data, root));
            }
        }

        const std::string_view python = CONJURE_ENGINE_TEST_PYTHON;
        if (python.empty()) {
            MESSAGE("no Python interpreter was found at configure time: the documents of the .pdb files are not "
                    "checked by tools/check_distribution_paths.py");
            return;
        }
        core::process_desc check = {};
        check.executable = std::string(python);
        check.arguments = {
            "-I",
            utf8(std::filesystem::path(CONJURE_ENGINE_TEST_SOURCE_DIR) / "tools" / "check_distribution_paths.py"),
        };
        for (const std::filesystem::path &root : roots) {
            check.arguments.emplace_back("--root");
            check.arguments.push_back(utf8(root));
        }
        for (const std::filesystem::path &binary : binaries) {
            check.arguments.emplace_back("--binary");
            check.arguments.push_back(utf8(binary));
        }
        check.mergeStderr = true;
        check.timeoutSeconds = TOOL_SECONDS;
        const core::result<core::process_result> checked = core::process::run(check);
        CONJURE_REQUIRE_OK(checked);
        CHECK_MESSAGE(checked.value().exitCode == 0, checked.value().standardOutput);
    }

    /// <summary>
    /// The value of the 'constexpr std::string_view <name> =' constant of a C++ source: its adjacent string literals
    /// joined, with the escapes \n, \t, \", \' and \\ decoded. Empty when the constant is missing or is written in
    /// any other way (another escape, a raw string, a macro), so that a change in how the editor spells a template
    /// fails the case reading it instead of letting it build something else.
    /// </summary>
    [[nodiscard]] std::string string_constant(std::string_view source, std::string_view name) {
        const std::string declaration = "constexpr std::string_view " + std::string(name) + " =";
        const usize start = source.find(declaration);
        if (start == std::string_view::npos) {
            return {};
        }
        std::string text = {};
        bool inLiteral = false;
        usize position = start + declaration.size();
        while (position < source.size()) {
            const char character = source[position];
            ++position;
            if (!inLiteral) {
                if (character == ';') {
                    return text;
                }
                if (character == '"') {
                    inLiteral = true;
                    continue;
                }
                if (character != ' ' && character != '\n' && character != '\r' && character != '\t') {
                    return {};
                }
                continue;
            }
            if (character == '"') {
                inLiteral = false;
                continue;
            }
            if (character != '\\') {
                text += character;
                continue;
            }
            if (position == source.size()) {
                return {};
            }
            const char escaped = source[position];
            ++position;
            switch (escaped) {
                case 'n': {
                    text += '\n';
                    break;
                }
                case 't': {
                    text += '\t';
                    break;
                }
                case '"':
                case '\'':
                case '\\': {
                    text += escaped;
                    break;
                }
                default: {
                    return {};
                }
            }
        }
        return {};
    }
#endif
}

TEST_SUITE("engine/export") {

    TEST_CASE("export_pipeline - refuses without a project and without an output directory") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        export_pipeline pipeline(app.context());
        export_settings settings = {};
        settings.outputDirectory = "somewhere";
        CONJURE_CHECK_ERROR(pipeline.run(settings), core::error_code::INVALID_STATE);
        app.shutdown();
        CHECK(export_pipeline::STEP_NAMES.size() == 6);
        CHECK(export_pipeline::STEP_NAMES[1] == "cook_assets");
        CHECK(export_pipeline::STEP_NAMES[5] == "write_project");
    }

    TEST_CASE("export_pipeline - exports a development project and the result boots as a cooked project") {
        log_capture logs = {};
        temp_directory directory("engine_export");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        project loaded = engine_test::write_project(projectDir, "exported");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "gold:10;potion:2"));
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/more.bytes", "a second blob"));

        export_report report = {};
        {
            application app(project_desc(loaded, 0));
            CONJURE_REQUIRE_OK(app.initialise());
            REQUIRE(app.context().assetDatabase != nullptr);
            CHECK(app.context().assetDatabase->record_count() >= 2);

            export_pipeline pipeline(app.context());
            export_settings settings = {};
            settings.outputDirectory = outputDir;
            settings.buildNativeModule = false;
            settings.publishManaged = false;
            settings.copyRuntime = false;
            settings.alwaysInclude = {"assets://data/more.bytes", "assets://data/absent.bytes"};
            std::vector<std::string> announced = {};
            const progress_fn progress = [&announced](std::string_view step, f32 fraction) {
                announced.emplace_back(step);
                CHECK(fraction >= 0.0f);
                CHECK(fraction <= 1.0f);
            };
            const core::result<export_report> ran = pipeline.run(settings, progress);
            CONJURE_REQUIRE_OK(ran);
            report = ran.value();
            CHECK(announced.size() == 7);
            CHECK(announced.front() == "build_game_module");
            CHECK(announced.back() == "done");
            app.shutdown();
        }

        CHECK(report.steps.size() == 6);
        CHECK(report.assetCount >= 2);
        CHECK(report.packBytes > 0);
        CHECK_FALSE(report.nativeModuleBuilt);
        CHECK_FALSE(report.managedPublished);
        CHECK(report.modulesExported == 0);
        const std::vector<std::string> &copied = report.copiedFiles;
        CHECK(std::find(copied.begin(), copied.end(), "cooked/content.cpak") != copied.end());
        CHECK(std::find(copied.begin(), copied.end(), "project.json") != copied.end());
        CHECK(std::any_of(report.warnings.begin(), report.warnings.end(), [](const std::string &warning) {
            return warning.find("absent.bytes") != std::string::npos;
        }));
        CHECK(std::filesystem::is_regular_file(outputDir / "cooked" / "content.cpak"));
        CHECK(std::filesystem::is_regular_file(outputDir / "cooked" / "manifest.json"));
        CHECK(std::filesystem::is_regular_file(outputDir / "project.json"));

        // The exported folder is a cooked project: it mounts the pack and finds the assets by path.
        core::result<project> exported = project::load(outputDir);
        CONJURE_REQUIRE_OK(exported);
        CHECK(exported.value().description().cooked);
        CHECK(exported.value().description().cookedRoot.generic_string() == "cooked");
        CHECK(exported.value().description().name == "exported");

        application cooked(project_desc(std::move(exported).value(), 60));
        CONJURE_REQUIRE_OK(cooked.initialise());
        CHECK(cooked.context().assetDatabase == nullptr);
        REQUIRE(cooked.context().assets != nullptr);
        CHECK(cooked.context().assets->find("assets://data/notes.bytes").has_value());
        CHECK(cooked.context().assets->find("assets://data/more.bytes").has_value());
        CHECK(cooked.context().filesystem->exists("cooked://content.cpak"));
        CHECK(cooked.run() == 0);
        CHECK(cooked.time().frameIndex == 60);
    }

    TEST_CASE("regression - code sweep F458 an exported project.json turns GPU validation off") {
        // The exported project.json used to carry the project's level, so project::apply_to raised the OFF default of
        // the shipped Distribution runtime and players ran with the debug layer or GPU-based validation.
        log_capture logs = {};
        temp_directory directory("engine_export_gpu_validation");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        std::string extraJson = {};
        gpu_validation_level named = gpu_validation_level::OFF;
        SUBCASE("gpu_based") {
            extraJson = R"("rendering": {"backend": "null", "gpu_validation": "gpu_based"})";
            named = gpu_validation_level::GPU_BASED;
        }
        SUBCASE("debug_layer") {
            extraJson = R"("rendering": {"backend": "null", "gpu_validation": "debug_layer"})";
            named = gpu_validation_level::DEBUG_LAYER;
        }
        SUBCASE("the boolean of older projects") {
            extraJson = R"("rendering": {"backend": "null", "gpu_validation": true})";
            named = gpu_validation_level::GPU_BASED;
        }
        project loaded = engine_test::write_project(projectDir, "validated", {}, extraJson);
        REQUIRE(loaded.description().rendering.gpuValidation == named);
        {
            application app(project_desc(loaded, 0));
            CONJURE_REQUIRE_OK(app.initialise());
            export_pipeline pipeline(app.context());
            export_settings settings = {};
            settings.outputDirectory = outputDir;
            settings.buildNativeModule = false;
            settings.publishManaged = false;
            settings.copyRuntime = false;
            CONJURE_REQUIRE_OK(pipeline.run(settings));
            app.shutdown();
        }

        core::result<project> exported = project::load(outputDir);
        CONJURE_REQUIRE_OK(exported);
        CHECK(exported.value().description().rendering.gpuValidation == gpu_validation_level::OFF);
        // The rest of the rendering section ships as the project has it, and the source project keeps its level.
        CHECK(exported.value().description().rendering.backend == rhi_backend_choice::NULL_DEVICE);
        core::result<project> source = project::load(projectDir);
        CONJURE_REQUIRE_OK(source);
        CHECK(source.value().description().rendering.gpuValidation == named);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("regression - code sweep W3-RS-L2 the exported project.json keeps the rendering defaults") {
        // Step 6 writes project::to_json, which used to drop every rendering key but the engine's three, so a game
        // shipped without the look its project defines. Unknown keys, inside the defaults and beside them, are no
        // reason to refuse the export and ship as they are (a newer engine may read them).
        log_capture logs = {};
        temp_directory directory("engine_export_rendering_defaults");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        const project loaded = engine_test::write_project(
          projectDir,
          "looks",
          {},
          R"("rendering": {"studio_note": "kept", "defaults": {"post_process": {"tonemap": "agx",
             "agx_look": "punchy", "future_key": 1}, "shadows": {"cascade_count": 2}}})"
        );
        const core::json::document defaults =
          core::json::document::from_value(loaded.description().renderingSection.root()["defaults"]);
        REQUIRE(defaults.root().is_object());
        {
            application app(project_desc(loaded, 0));
            CONJURE_REQUIRE_OK(app.initialise());
            export_pipeline pipeline(app.context());
            export_settings settings = {};
            settings.outputDirectory = outputDir;
            settings.buildNativeModule = false;
            settings.publishManaged = false;
            settings.copyRuntime = false;
            CONJURE_REQUIRE_OK(pipeline.run(settings));
            app.shutdown();
        }

        core::result<project> exported = project::load(outputDir);
        CONJURE_REQUIRE_OK(exported);
        const core::json::value shippedRendering = exported.value().description().renderingSection.root();
        CHECK(core::json::document::from_value(shippedRendering["defaults"]) == defaults);
        CHECK(shippedRendering["defaults"]["post_process"]["future_key"].as_i64() == 1);
        CHECK(shippedRendering["studio_note"].as_string() == "kept");
        // The renderer subsystem of a build with the renderer reports the unknown leaf of the defaults once; the
        // key beside the defaults is the project's own business.
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK(logs.contains(core::log_level::WARN, "rendering/defaults/post_process/future_key"));
#else
        CHECK(logs.count(core::log_level::WARN) == 0);
#endif
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("export_pipeline - invalid rendering defaults refuse the export before anything is written") {
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
        // The editor opens such a project leniently (an ERR per problem), but the exported runtime reads the defaults
        // strictly and would not start, so the export refuses with the path of the problem.
        log_capture logs = {};
        logs.allow_errors();
        temp_directory directory("engine_export_bad_rendering_defaults");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        std::string defaults = {};
        std::string_view path = {};
        SUBCASE("a value out of range") {
            defaults = R"({"ssao": {"radius": 100.0}})";
            path = "rendering/defaults/ssao/radius";
        }
        SUBCASE("an unknown enumerator") {
            defaults = R"({"post_process": {"tonemap": "filmic"}})";
            path = "rendering/defaults/post_process/tonemap";
        }
        SUBCASE("a defaults member that is not an object") {
            defaults = "true";
            path = "rendering/defaults";
        }
        const std::string rendering = std::format(R"("rendering": {{"defaults": {}}})", defaults);
        const project loaded = engine_test::write_project(projectDir, "broken_look", {}, rendering);
        application_desc desc = project_desc(loaded, 0);
        desc.config.editor = true;
        application app(std::move(desc));
        CONJURE_REQUIRE_OK(app.initialise());
        export_pipeline pipeline(app.context());
        export_settings settings = {};
        settings.outputDirectory = outputDir;
        settings.buildNativeModule = false;
        settings.publishManaged = false;
        settings.copyRuntime = false;
        const core::result<export_report> ran = pipeline.run(settings);
        app.shutdown();

        CONJURE_CHECK_ERROR(ran, engine_error::EXPORT_FAILED);
        REQUIRE_FALSE(ran.has_value());
        CHECK(ran.get_error().message().find("write_project") != std::string::npos);
        CHECK(ran.get_error().message().find(path) != std::string::npos);
        CHECK_FALSE(std::filesystem::exists(outputDir));
        CHECK(logs.contains(core::log_level::ERR, path));
#else
        MESSAGE("this build has no renderer: nothing reads the rendering defaults, so the export checks none");
#endif
    }

    TEST_CASE("export_pipeline - refuses the project directory and a folder inside the assets root as output") {
        log_capture logs = {};
        temp_directory directory("engine_export_into_project");
        const std::filesystem::path projectDir = directory.path() / "project";
        project loaded = engine_test::write_project(projectDir, "inside");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        application app(project_desc(loaded, 0));
        CONJURE_REQUIRE_OK(app.initialise());
        export_pipeline pipeline(app.context());
        export_settings settings = {};
        settings.buildNativeModule = false;
        settings.publishManaged = false;
        settings.copyRuntime = false;

        settings.outputDirectory = projectDir;
        CONJURE_CHECK_ERROR(pipeline.run(settings), engine_error::BAD_ARGUMENTS);
        settings.outputDirectory = projectDir / "";
        CONJURE_CHECK_ERROR(pipeline.run(settings), engine_error::BAD_ARGUMENTS);
        settings.outputDirectory = projectDir / "assets";
        CONJURE_CHECK_ERROR(pipeline.run(settings), engine_error::BAD_ARGUMENTS);
        settings.outputDirectory = projectDir / "assets" / "out";
        CONJURE_CHECK_ERROR(pipeline.run(settings), engine_error::BAD_ARGUMENTS);
        CHECK_FALSE(std::filesystem::exists(projectDir / "assets" / "out"));

        // The source project.json is still the development one.
        const core::result<project> reloaded = project::load(projectDir);
        CONJURE_REQUIRE_OK(reloaded);
        CHECK_FALSE(reloaded.value().description().cooked);

        // A folder of its own inside the project is fine.
        settings.outputDirectory = projectDir / "export";
        CONJURE_CHECK_OK(pipeline.run(settings));
        CHECK(std::filesystem::is_regular_file(projectDir / "export" / "project.json"));
        app.shutdown();
    }

    TEST_CASE("export_pipeline - refuses an artifacts directory inside the output directory or the assets root") {
        // The intermediates of step 3 would ship with the game, or be imported as assets by the next scan. The check
        // runs before step 1, so nothing is cooked; the output directory exists by then, so that the file system
        // decides whether two spellings name the same folder, and a refusal removes the folders the export created
        // for it (here 'exports' and 'exports/out') again.
        log_capture logs = {};
        temp_directory directory("engine_export_artifacts_inside");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "exports" / "out";
        project loaded = engine_test::write_project(projectDir, "artifacts_inside");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        export_settings settings = {};
        settings.outputDirectory = outputDir;
        settings.buildNativeModule = false;
        settings.publishManaged = false;
        settings.copyRuntime = false;
        bool refused = true;
        bool outputExisted = false;

        SUBCASE("the output directory itself") {
            settings.artifactsDirectory = outputDir;
        }
        SUBCASE("a folder inside the output directory") {
            settings.artifactsDirectory = outputDir / "managed" / "artifacts";
        }
        SUBCASE("a folder inside the output directory, spelt through its parent") {
            settings.artifactsDirectory = outputDir / ".." / "out" / "artifacts";
        }
        SUBCASE("a folder inside an output directory that already exists, which the refusal keeps") {
            CONJURE_REQUIRE_OK(directory.write_text("exports/out/keep.txt", "x"));
            settings.artifactsDirectory = outputDir / "artifacts";
            outputExisted = true;
        }
        SUBCASE("the assets root") {
            settings.artifactsDirectory = projectDir / "assets";
        }
        SUBCASE("a folder inside the assets root") {
            settings.artifactsDirectory = projectDir / "assets" / "artifacts";
        }
        SUBCASE("a folder beside the output directory, which is fine") {
            settings.artifactsDirectory = directory.path() / "artifacts";
            refused = false;
        }
        SUBCASE("a folder inside the project but outside the assets root, which is fine") {
            settings.artifactsDirectory = projectDir / "artifacts";
            refused = false;
        }
        CAPTURE(settings.artifactsDirectory.string());
        CAPTURE(outputExisted);

        application app(project_desc(loaded, 0));
        CONJURE_REQUIRE_OK(app.initialise());
        export_pipeline pipeline(app.context());
        const core::result<export_report> ran = pipeline.run(settings);
        app.shutdown();
        if (!refused) {
            // Without step 3 the folder is never used, and nothing creates it.
            CONJURE_CHECK_OK(ran);
            CHECK(std::filesystem::is_regular_file(outputDir / "project.json"));
            CHECK_FALSE(std::filesystem::exists(settings.artifactsDirectory));
            return;
        }
        CONJURE_CHECK_ERROR(ran, engine_error::BAD_ARGUMENTS);
        REQUIRE_FALSE(ran.has_value());
        CHECK(ran.get_error().message().find("artifacts directory") != std::string::npos);
        CHECK_FALSE(std::filesystem::exists(outputDir / "cooked"));
        CHECK_FALSE(std::filesystem::exists(outputDir / "project.json"));
        if (outputExisted) {
            CHECK(std::filesystem::is_regular_file(outputDir / "keep.txt"));
        } else {
            CHECK_FALSE(std::filesystem::exists(directory.path() / "exports"));
        }
    }

    TEST_CASE("regression - code sweep FU-NOSCRIPTING an export that asks for managed code fails while the "
      "configuration disables scripting and the project enables it") {
        // Step 3 skipped the managed build without a word, so conjure_editor --no-scripting --export of a scripted
        // project shipped a game whose runtime starts the .NET host and finds no managed code. The refusal comes
        // before step 1: no step is announced, nothing is cooked and the output directory is not even created.
        log_capture logs = {};
        temp_directory directory("engine_export_no_scripting");
        const std::filesystem::path outputDir = directory.path() / "out";
        project loaded = engine_test::write_project(directory.path() / "project", "no_scripting");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        export_settings settings = {};
        settings.outputDirectory = outputDir;
        settings.buildNativeModule = false;
        settings.copyRuntime = false;
        settings.bootstrapProject = directory.path() / "Boot.csproj";
        settings.dotnetExecutable = directory.file("no_dotnet_here").string();
        std::string scriptsNamed = "names no game scripts";

        SUBCASE("with game scripts, which the message names") {
            settings.scriptsProject = directory.path() / "project" / "scripts" / "Game.csproj";
            const std::u8string text = settings.scriptsProject.generic_u8string();
            scriptsNamed = std::string(text.begin(), text.end());
        }
        SUBCASE("without game scripts") {
        }

        application_desc desc = project_desc(loaded, 0);
        desc.config.scripting.enabled = false;
        application app(std::move(desc));
        CONJURE_REQUIRE_OK(app.initialise());
        export_pipeline pipeline(app.context());
        usize announced = 0;
        const progress_fn progress = [&announced](std::string_view, f32) {
            ++announced;
        };
        const core::result<export_report> ran = pipeline.run(settings, progress);
        app.shutdown();

        CONJURE_CHECK_ERROR(ran, engine_error::EXPORT_FAILED);
        REQUIRE_FALSE(ran.has_value());
        const std::string message = ran.get_error().message();
        CAPTURE(message);
        CHECK(message.find("publish_managed") != std::string::npos);
        CHECK(message.find("--no-scripting") != std::string::npos);
        CHECK(message.find("'no_scripting'") != std::string::npos);
        CHECK(message.find(scriptsNamed) != std::string::npos);
        CHECK(message.find("publishManaged false") != std::string::npos);
        CHECK(announced == 0);
        CHECK_FALSE(std::filesystem::exists(outputDir));
    }

    TEST_CASE("export_pipeline - with scripting disabled in the configuration an export that needs no managed code "
      "succeeds") {
        log_capture logs = {};
        temp_directory directory("engine_export_unscripted");
        const std::filesystem::path outputDir = directory.path() / "out";
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        export_settings settings = {};
        settings.outputDirectory = outputDir;
        settings.buildNativeModule = false;
        settings.copyRuntime = false;
        settings.bootstrapProject = directory.path() / "Boot.csproj";
        settings.dotnetExecutable = directory.file("no_dotnet_here").string();
        std::string extraJson = {};

        SUBCASE("the project disables scripting, and publishManaged keeps its default") {
            extraJson = R"("scripting": {"enabled": false})";
        }
        SUBCASE("the project enables scripting and publishManaged is false") {
            settings.publishManaged = false;
        }

        project loaded = engine_test::write_project(directory.path() / "project", "unscripted", {}, extraJson);
        application_desc desc = project_desc(loaded, 0);
        desc.config.scripting.enabled = false;
        application app(std::move(desc));
        CONJURE_REQUIRE_OK(app.initialise());
        export_pipeline pipeline(app.context());
        const core::result<export_report> ran = pipeline.run(settings);
        app.shutdown();

        CONJURE_REQUIRE_OK(ran);
        CHECK(ran.value().steps.size() == export_pipeline::STEP_NAMES.size());
        CHECK_FALSE(ran.value().managedPublished);
        CHECK(count_warnings(ran.value(), "managed") == 0);
        CHECK(std::filesystem::is_regular_file(outputDir / "project.json"));
        CHECK_FALSE(std::filesystem::exists(outputDir / "managed"));
    }

    TEST_CASE("export_pipeline - copy_runtime ships the shader indexes and blobs but not the build depfiles") {
        log_capture logs = {};
        temp_directory directory("engine_export_shaders");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        project loaded = engine_test::write_project(projectDir, "shaded");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));

        // A distribution folder as the Distribution build lays it out, with a module's shader set in a subfolder.
        const std::string runtimeName = runtime_name();
        write_distribution(directory);
        const std::array<std::string_view, 7> shaderFiles = {
            "index.json",
            "a.vs_main.dxil",
            "a.vs_main.dxil.d",
            "a.vs_main.spv.d",
            "particles/index.json",
            "particles/p.ps_main.spv",
            "particles/p.ps_main.spv.d",
        };
        for (const std::string_view file : shaderFiles) {
            CAPTURE(file);
            CONJURE_REQUIRE_OK(directory.write_text("dist/shaders/" + std::string(file), "blob"));
        }

        const core::result<export_report> ran = export_with_runtime(loaded, directory, outputDir, true);
        CONJURE_REQUIRE_OK(ran);
        const export_report &report = ran.value();

        const std::filesystem::path shaders = outputDir / "shaders";
        CHECK(std::filesystem::is_regular_file(outputDir / runtimeName));
        CHECK(std::filesystem::is_regular_file(shaders / "index.json"));
        CHECK(std::filesystem::is_regular_file(shaders / "a.vs_main.dxil"));
        CHECK(std::filesystem::is_regular_file(shaders / "particles" / "index.json"));
        CHECK(std::filesystem::is_regular_file(shaders / "particles" / "p.ps_main.spv"));
        CHECK_FALSE(std::filesystem::exists(shaders / "a.vs_main.dxil.d"));
        CHECK_FALSE(std::filesystem::exists(shaders / "a.vs_main.spv.d"));
        CHECK_FALSE(std::filesystem::exists(shaders / "particles" / "p.ps_main.spv.d"));
        const std::vector<std::string> &copied = report.copiedFiles;
        CHECK(std::find(copied.begin(), copied.end(), "shaders") != copied.end());
        CHECK(std::find(copied.begin(), copied.end(), runtimeName) != copied.end());
    }

    TEST_CASE("regression - code sweep F790 copy_runtime ships every Visual C++ runtime DLL of the distribution "
      "directory") {
        log_capture logs = {};
        temp_directory directory("engine_export_crt");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        project loaded = engine_test::write_project(projectDir, "runtime_libraries");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        write_distribution(directory);

        // Fake libraries, each with its own text, in the byte order of their names, which is the order the
        // report lists them in. The upper-case one is matched without regard to case and keeps its name.
        const std::array<std::string_view, 5> shipped = {
            "VCRUNTIME140_THREADS.DLL",
            "msvcp140.dll",
            "msvcp140_atomic_wait.dll",
            "vcruntime140.dll",
            "vcruntime140_1.dll",
        };
        for (const std::string_view name : shipped) {
            CAPTURE(name);
            CONJURE_REQUIRE_OK(directory.write_text("dist/" + std::string(name), "library " + std::string(name)));
        }
        // Neighbours that are not Visual C++ runtime DLLs: other runtimes, other extensions, a name that only
        // contains the prefix, a library one folder down and a folder that carries a library's name.
        const std::array<std::string_view, 8> left = {
            "concrt140.dll",
            "ucrtbase.dll",
            "vcruntime14.dll",
            "my_msvcp140.dll",
            "msvcp140.lib",
            "vcruntime140.dll.bak",
            "nested/vcruntime140.dll",
            "msvcp140_folder.dll/inner.txt",
        };
        for (const std::string_view name : left) {
            CAPTURE(name);
            CONJURE_REQUIRE_OK(directory.write_text("dist/" + std::string(name), "not shipped"));
        }

        const core::result<export_report> ran = export_with_runtime(loaded, directory, outputDir, false);
        CONJURE_REQUIRE_OK(ran);
        const export_report &report = ran.value();

        for (const std::string_view name : shipped) {
            CAPTURE(name);
            CHECK(std::filesystem::is_regular_file(outputDir / name));
            CHECK(file_text(outputDir / name) == "library " + std::string(name));
        }
        for (const std::string_view name : left) {
            CAPTURE(name);
            CHECK_FALSE(std::filesystem::exists(outputDir / name));
        }
        CHECK_FALSE(std::filesystem::exists(outputDir / "nested"));
        CHECK_FALSE(std::filesystem::exists(outputDir / "msvcp140_folder.dll"));

        // The report names the runtime executable first and the five libraries as one run, in name order.
        const std::vector<std::string> &copied = report.copiedFiles;
        const auto runtime = std::find(copied.begin(), copied.end(), runtime_name());
        REQUIRE(runtime != copied.end());
        const auto firstLibrary = std::find(copied.begin(), copied.end(), shipped.front());
        REQUIRE(firstLibrary != copied.end());
        CHECK(runtime < firstLibrary);
        REQUIRE(static_cast<usize>(copied.end() - firstLibrary) >= shipped.size());
        CHECK(std::equal(shipped.begin(), shipped.end(), firstLibrary));
        CHECK(std::filesystem::is_regular_file(outputDir / runtime_name()));
        CHECK(std::filesystem::is_regular_file(outputDir / core::dynamic_library::decorate_name("conjure_shared")));
        // With the libraries next to the runtime no platform is told to install the redistributable, and none
        // of them is taken for the debug runtime ("vcruntime140_threads.dll" ends in an 's', not in a 'd').
        CHECK(count_warnings(report, REDISTRIBUTABLE_WARNING) == 0);
        CHECK(count_warnings(report, DEBUG_RUNTIME_WARNING) == 0);
    }

    TEST_CASE("regression - code sweep F790 copy_runtime leaves the debug Visual C++ runtime behind and names it") {
        log_capture logs = {};
        temp_directory directory("engine_export_debug_crt");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        project loaded = engine_test::write_project(projectDir, "debug_runtime");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        write_distribution(directory);

        // Every library of the debug runtime a toolset ships under the two prefixes, in the byte order of the
        // names: the 'd' follows the "140" or ends the name, in either case of the letter.
        const std::array<std::string_view, 8> debugLibraries = {
            "VCRUNTIME140D.DLL",
            "msvcp140_1d.dll",
            "msvcp140_2d.dll",
            "msvcp140d.dll",
            "msvcp140d_atomic_wait.dll",
            "msvcp140d_codecvt_ids.dll",
            "vcruntime140_1d.dll",
            "vcruntime140_threadsd.dll",
        };
        for (const std::string_view name : debugLibraries) {
            CAPTURE(name);
            CONJURE_REQUIRE_OK(directory.write_text("dist/" + std::string(name), "debug library"));
        }
        // The release libraries whose names come closest: a 'd' inside the name is no mark.
        std::vector<std::string_view> releaseLibraries = {};
        usize expectedRedistributableWarnings = 0;

        SUBCASE("next to the release runtime, which ships") {
            releaseLibraries = {"msvcp140_codecvt_ids.dll", "vcruntime140.dll", "vcruntime140_threads.dll"};
        }
        SUBCASE("alone, which leaves the game without a runtime of its own") {
            expectedRedistributableWarnings = expected_redistributable_warnings();
        }
        for (const std::string_view name : releaseLibraries) {
            CAPTURE(name);
            CONJURE_REQUIRE_OK(directory.write_text("dist/" + std::string(name), "release library"));
        }

        const core::result<export_report> ran = export_with_runtime(loaded, directory, outputDir, false);
        CONJURE_REQUIRE_OK(ran);
        const export_report &report = ran.value();
        const std::vector<std::string> &copied = report.copiedFiles;

        for (const std::string_view name : debugLibraries) {
            CAPTURE(name);
            CHECK_FALSE(std::filesystem::exists(outputDir / name));
            CHECK(std::find(copied.begin(), copied.end(), name) == copied.end());
            // The distribution directory keeps its file: the export copies, it never moves.
            CHECK(std::filesystem::is_regular_file(directory.path() / "dist" / name));
        }
        for (const std::string_view name : releaseLibraries) {
            CAPTURE(name);
            CHECK(file_text(outputDir / name) == "release library");
            CHECK(std::find(copied.begin(), copied.end(), name) != copied.end());
        }

        // One warning names all eight, in name order, and none of the libraries that shipped.
        REQUIRE(count_warnings(report, DEBUG_RUNTIME_WARNING) == 1);
        const auto namesDebugRuntime = [](const std::string &text) {
            return text.find(DEBUG_RUNTIME_WARNING) != std::string::npos;
        };
        const std::string &warning =
          *std::find_if(report.warnings.begin(), report.warnings.end(), namesDebugRuntime);
        usize position = 0;
        for (const std::string_view name : debugLibraries) {
            CAPTURE(name);
            position = warning.find(name, position);
            REQUIRE(position != std::string::npos);
        }
        for (const std::string_view name : releaseLibraries) {
            CAPTURE(name);
            CHECK(warning.find(name) == std::string::npos);
        }
        CHECK(count_warnings(report, REDISTRIBUTABLE_WARNING) == expectedRedistributableWarnings);
    }

    TEST_CASE("regression - code sweep F200 copy_runtime ships the licence notices of the distribution directory") {
        log_capture logs = {};
        temp_directory directory("engine_export_notices");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        project loaded = engine_test::write_project(projectDir, "notices");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        write_distribution(directory);
        const std::string noticesName(export_pipeline::NOTICES_FILE_NAME);
        CHECK(noticesName == "THIRD_PARTY_NOTICES.txt");
        // The file is copied byte for byte: either kind of line end and a name outside ASCII (UTF-8) survive.
        const std::string notices = "Third-party notices\r\nGLFW: Camilla L\xC3\xB6wy\nzlib licence text\n";
        CONJURE_REQUIRE_OK(directory.write_text("dist/" + noticesName, notices));
        // A file whose name only resembles the notices stays behind.
        CONJURE_REQUIRE_OK(directory.write_text("dist/THIRD_PARTY_NOTICES.md", "not shipped"));

        SUBCASE("into an empty folder") {}
        SUBCASE("over the notices an earlier export left in the folder") {
            // Longer than the new file, so a copy that did not replace the old one would show.
            const std::string older = "the notices of an older build\n" + std::string(4096, '.');
            CONJURE_REQUIRE_OK(directory.write_text("out/" + noticesName, older));
        }

        const core::result<export_report> ran = export_with_runtime(loaded, directory, outputDir, false);
        CONJURE_REQUIRE_OK(ran);
        const export_report &report = ran.value();

        REQUIRE(std::filesystem::is_regular_file(outputDir / noticesName));
        CHECK(file_text(outputDir / noticesName) == notices);
        CHECK_FALSE(std::filesystem::exists(outputDir / "THIRD_PARTY_NOTICES.md"));
        // The report names the notices once, after the runtime executable they belong to.
        const std::vector<std::string> &copied = report.copiedFiles;
        CHECK(std::count(copied.begin(), copied.end(), noticesName) == 1);
        const auto runtime = std::find(copied.begin(), copied.end(), runtime_name());
        const auto shipped = std::find(copied.begin(), copied.end(), noticesName);
        REQUIRE(runtime != copied.end());
        REQUIRE(shipped != copied.end());
        CHECK(runtime < shipped);
        CHECK(count_warnings(report, NOTICES_WARNING) == 0);
    }

    TEST_CASE("export_pipeline - a distribution directory without the licence notices is named in a warning") {
        log_capture logs = {};
        temp_directory directory("engine_export_no_notices");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        project loaded = engine_test::write_project(projectDir, "no_notices");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        write_distribution(directory);
        const std::string noticesName(export_pipeline::NOTICES_FILE_NAME);

        SUBCASE("the directory holds no such file") {}
        SUBCASE("the directory holds a folder of that name") {
            // A folder is not the notices, and nothing of it is copied.
            CONJURE_REQUIRE_OK(directory.write_text("dist/" + noticesName + "/inner.txt", "not shipped"));
        }

        const core::result<export_report> ran = export_with_runtime(loaded, directory, outputDir, false);
        // The export itself succeeds: the runtime is there, and the report says what is missing, once.
        CONJURE_REQUIRE_OK(ran);
        CHECK(count_warnings(ran.value(), NOTICES_WARNING) == 1);
        CHECK(count_warnings(ran.value(), noticesName) == 1);
        CHECK(std::filesystem::is_regular_file(outputDir / runtime_name()));
        CHECK_FALSE(std::filesystem::exists(outputDir / noticesName));
        const std::vector<std::string> &copied = ran.value().copiedFiles;
        CHECK(std::find(copied.begin(), copied.end(), noticesName) == copied.end());
    }

    TEST_CASE("export_pipeline - a distribution directory without Visual C++ runtime DLLs is named in a warning "
      "where the runtime imports them") {
        log_capture logs = {};
        temp_directory directory("engine_export_no_crt");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        project loaded = engine_test::write_project(projectDir, "no_runtime_libraries");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        write_distribution(directory);
        // The Concurrency Runtime is no substitute: it is not one of the libraries step 5 ships.
        CONJURE_REQUIRE_OK(directory.write_text("dist/concrt140.dll", "not shipped"));
        const usize expectedWarnings = expected_redistributable_warnings();

        SUBCASE("the runtime is copied") {
            const core::result<export_report> ran = export_with_runtime(loaded, directory, outputDir, false);
            CONJURE_REQUIRE_OK(ran);
            CHECK(count_warnings(ran.value(), REDISTRIBUTABLE_WARNING) == expectedWarnings);
            CHECK(count_warnings(ran.value(), "vcruntime140*.dll") == expectedWarnings);
            CHECK(std::filesystem::is_regular_file(outputDir / runtime_name()));
            CHECK_FALSE(std::filesystem::exists(outputDir / "concrt140.dll"));
        }

        SUBCASE("an export that does not copy the runtime says nothing about its libraries or its notices") {
            application app(project_desc(loaded, 0));
            CONJURE_REQUIRE_OK(app.initialise());
            export_pipeline pipeline(app.context());
            export_settings settings = {};
            settings.outputDirectory = outputDir;
            settings.buildNativeModule = false;
            settings.publishManaged = false;
            SUBCASE("copyRuntime is false") {
                settings.distributionDirectory = directory.path() / "dist";
                settings.copyRuntime = false;
            }
            SUBCASE("no distribution directory is given") {
                settings.copyRuntime = true;
            }
            const core::result<export_report> ran = pipeline.run(settings);
            CONJURE_REQUIRE_OK(ran);
            CHECK(count_warnings(ran.value(), REDISTRIBUTABLE_WARNING) == 0);
            // No engine binary went into the folder, so there is nothing whose notices would be missing.
            CHECK(count_warnings(ran.value(), NOTICES_WARNING) == 0);
            CHECK_FALSE(std::filesystem::exists(outputDir / runtime_name()));
            CHECK_FALSE(std::filesystem::exists(outputDir / export_pipeline::NOTICES_FILE_NAME));
            app.shutdown();
        }
    }

    TEST_CASE("export_pipeline - copy_runtime fails on a file it cannot copy and keeps what it copied before") {
        log_capture logs = {};
        temp_directory directory("engine_export_copy_failure");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        project loaded = engine_test::write_project(projectDir, "copy_failure");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        write_distribution(directory);
        CONJURE_REQUIRE_OK(directory.write_text("dist/vcruntime140.dll", "library"));
        const std::string noticesName(export_pipeline::NOTICES_FILE_NAME);
        CONJURE_REQUIRE_OK(directory.write_text("dist/" + noticesName, "notices"));
        const std::string sharedName = core::dynamic_library::decorate_name("conjure_shared");
        std::error_code error = {};
        std::string failing = {};

        // A folder where a file belongs is not replaced by the file, and a folder is not a file to copy, on
        // every platform.
        SUBCASE("a folder in the output carries the name of a Visual C++ runtime DLL") {
            failing = "vcruntime140.dll";
            std::filesystem::create_directories(outputDir / failing, error);
            REQUIRE_FALSE(error);
        }
        SUBCASE("a folder in the output carries the name of the licence notices") {
            failing = noticesName;
            std::filesystem::create_directories(outputDir / failing, error);
            REQUIRE_FALSE(error);
        }
        SUBCASE("a folder in the output carries the name of the shared library") {
            failing = sharedName;
            std::filesystem::create_directories(outputDir / failing, error);
            REQUIRE_FALSE(error);
        }
        SUBCASE("the shared library is a folder in the distribution directory") {
            failing = sharedName;
            REQUIRE(std::filesystem::remove(directory.path() / "dist" / failing, error));
            std::filesystem::create_directories(directory.path() / "dist" / failing, error);
            REQUIRE_FALSE(error);
        }
        SUBCASE("the shared library is missing from the distribution directory") {
            failing = sharedName;
            REQUIRE(std::filesystem::remove(directory.path() / "dist" / failing, error));
        }
        CAPTURE(failing);

        const core::result<export_report> ran = export_with_runtime(loaded, directory, outputDir, false);
        CONJURE_CHECK_ERROR(ran, engine_error::EXPORT_FAILED);
        REQUIRE_FALSE(ran.has_value());
        const std::string message = ran.get_error().to_string();
        CHECK(message.find("copy_runtime") != std::string::npos);
        CHECK(message.find(failing) != std::string::npos);
        // Step 5 copies file by file: the runtime executable, copied first, stays. Nothing is put inside the
        // folder that was in the way, and step 6 never ran.
        CHECK(std::filesystem::is_regular_file(outputDir / runtime_name()));
        CHECK_FALSE(std::filesystem::exists(outputDir / failing / failing));
        CHECK_FALSE(std::filesystem::exists(outputDir / "project.json"));
    }

    TEST_CASE("export_pipeline - managed commands strip symbols and the apphost unless asked to keep them") {
        const std::filesystem::path managed = absolute_path(std::filesystem::path("out") / "managed");
        const std::filesystem::path targets = absolute_path(std::filesystem::path("staging") / "roots.targets");
        const std::filesystem::path artifacts = absolute_path(std::filesystem::path("staging") / "artifacts");
        export_settings settings = {};
        settings.bootstrapProject = "Boot.csproj";
        settings.scriptsProject = "Scripts/Game.csproj";

        // The defaults: framework-dependent, no debug symbols.
        const scripting_settings defaults = {};
        const std::vector<std::vector<std::string>> stripped =
          managed_export_commands(settings, defaults, managed, targets, artifacts, {});
        REQUIRE(stripped.size() == 2);
        CHECK(stripped[0].front() == "publish");
        CHECK(count_argument(stripped[0], "Boot.csproj") == 1);
        CHECK(argument_after(stripped[0], "-o") == managed.string());
        CHECK(count_argument(stripped[0], "-p:DebugType=none") == 1);
        CHECK(count_argument(stripped[0], "-p:DebugSymbols=false") == 1);
        CHECK(count_argument(stripped[0], "-p:UseAppHost=false") == 1);
        CHECK(stripped[1].front() == "build");
        CHECK(count_argument(stripped[1], "-p:DebugType=none") == 1);
        CHECK(arguments_with_prefix(stripped[1], "-p:UseAppHost").empty());

        scripting_settings symbols = {};
        symbols.exportDebugSymbols = true;
        const std::vector<std::vector<std::string>> kept =
          managed_export_commands(settings, symbols, managed, targets, artifacts, {});
        REQUIRE(kept.size() == 2);
        CHECK(arguments_with_prefix(kept[0], "-p:DebugType").empty());
        CHECK(arguments_with_prefix(kept[1], "-p:DebugType").empty());

        // A self-contained publish needs its apphost (NETSDK1067).
        scripting_settings selfContained = {};
        selfContained.exportMode = script_export_mode::SELF_CONTAINED;
        const std::vector<std::vector<std::string>> standalone =
          managed_export_commands(settings, selfContained, managed, targets, artifacts, {});
        REQUIRE_FALSE(standalone.empty());
        CHECK(arguments_with_prefix(standalone[0], "-p:UseAppHost").empty());
        CHECK(argument_after(standalone[0], "--self-contained") == "true");

        // Without game scripts only the bootstrap is published.
        settings.scriptsProject.clear();
        CHECK(managed_export_commands(settings, defaults, managed, targets, artifacts, {}).size() == 1);
    }

    TEST_CASE("regression - code sweep F776 every dotnet command of an export is a CI build") {
        // The CMake and editor builds used to pass ContinuousIntegrationBuild and the export did not. Now only the
        // export does, so the folders of the exporting machine stay out of what ships while the engine's and the
        // editor's own builds keep them for the debugger. MSBuild's incremental check does not see the flag, so each
        // command also compiles into intermediates no build without the flag shares: a pivot of the export's own in
        // the artifacts directory (follow-up FU-ARTIFACTS moved the scripts build there too).
        const std::filesystem::path managed = absolute_path(std::filesystem::path("out") / "managed");
        export_settings settings = {};
        settings.bootstrapProject = "Boot.csproj";
        settings.scriptsProject = std::filesystem::path("Scripts") / "Game.csproj";
        scripting_settings scripting = {};
        usize expectedCommands = 2;
        std::string expectedPivot = "-p:ArtifactsPivots=release_export_framework_dependent";

        SUBCASE("framework-dependent without debug symbols") {
            scripting.exportDebugSymbols = false;
        }
        SUBCASE("with debug symbols") {
            scripting.exportDebugSymbols = true;
        }
        SUBCASE("self-contained for a runtime identifier") {
            scripting.exportMode = script_export_mode::SELF_CONTAINED;
            settings.runtimeIdentifier = "linux-x64";
            expectedPivot = "-p:ArtifactsPivots=release_export_self_contained_linux-x64";
        }
        SUBCASE("framework-dependent for a runtime identifier") {
            settings.runtimeIdentifier = "osx-arm64";
            expectedPivot = "-p:ArtifactsPivots=release_export_framework_dependent_osx-arm64";
        }
        SUBCASE("without game scripts") {
            settings.scriptsProject.clear();
            expectedCommands = 1;
        }

        const std::vector<std::vector<std::string>> commands = managed_export_commands(
          settings,
          scripting,
          managed,
          absolute_path("roots.targets"),
          absolute_path("artifacts"),
          {}
        );
        REQUIRE(commands.size() == expectedCommands);
        const std::vector<std::string> expectedPivots = {expectedPivot};
        for (usize index = 0; index < commands.size(); ++index) {
            CAPTURE(index);
            CHECK(count_argument(commands[index], "-p:ContinuousIntegrationBuild=true") == 1);
            CHECK(arguments_with_prefix(commands[index], "-p:ArtifactsPivots=") == expectedPivots);
        }
    }

    TEST_CASE(
      "regression - code sweep FU-ARTIFACTS both dotnet commands of an export compile in its artifacts directory"
    ) {
        // The publish passed no artifacts path, so managed/Directory.Build.props sent the intermediates of the
        // bootstrap and Conjure.Managed to build/managed_artifacts of the engine's source tree, outside the export and
        // outside the build tree of the editor that exported; the scripts build compiled into the scripts project's
        // obj/. Both now compile in the one artifacts directory the pipeline hands over, whatever the projects'
        // folders.
        const std::filesystem::path managed = absolute_path(std::filesystem::path("out") / "managed");
        std::filesystem::path artifacts = absolute_path(std::filesystem::path("staging") / "artifacts");
        export_settings settings = {};
        settings.bootstrapProject = std::filesystem::path("engine") / "Boot.csproj";
        settings.scriptsProject = std::filesystem::path("Scripts") / "Game.csproj";
        scripting_settings scripting = {};
        usize expectedCommands = 2;

        SUBCASE("framework-dependent without debug symbols") {
            scripting.exportDebugSymbols = false;
        }
        SUBCASE("self-contained for a runtime identifier with debug symbols") {
            scripting.exportMode = script_export_mode::SELF_CONTAINED;
            scripting.exportDebugSymbols = true;
            settings.runtimeIdentifier = "win-x64";
        }
        SUBCASE("a scripts project in the working directory") {
            settings.scriptsProject = "Game.csproj";
        }
        SUBCASE("an artifacts directory with spaces, which stays one argument without quotes of its own") {
            artifacts = absolute_path(std::filesystem::path("My Build") / "export artifacts");
        }
        SUBCASE("without game scripts") {
            settings.scriptsProject.clear();
            expectedCommands = 1;
        }

        const std::vector<std::vector<std::string>> commands =
          managed_export_commands(settings, scripting, managed, absolute_path("roots.targets"), artifacts, {});
        REQUIRE(commands.size() == expectedCommands);
        for (usize index = 0; index < commands.size(); ++index) {
            CAPTURE(index);
            CHECK(count_argument(commands[index], "--artifacts-path") == 1);
            CHECK(argument_after(commands[index], "--artifacts-path") == artifacts.string());
        }
    }

    TEST_CASE("regression - code sweep FU-PDB every dotnet command of an export imports the source roots file") {
        // A CI build maps the source paths the compiler records only for the source roots MSBuild knows, and source
        // control supplies one only in a git repository with a commit: the managed assemblies of a project outside
        // one shipped the folders of the exporting machine. Every command now imports the file that adds the
        // project's own roots in that case.
        const std::filesystem::path managed = absolute_path(std::filesystem::path("out") / "managed");
        const std::filesystem::path targets =
          absolute_path(std::filesystem::path("staging") / "conjure_export_source_roots.targets");
        export_settings settings = {};
        settings.bootstrapProject = "Boot.csproj";
        settings.scriptsProject = std::filesystem::path("Scripts") / "Game.csproj";
        scripting_settings scripting = {};
        usize expectedCommands = 2;

        SUBCASE("framework-dependent without debug symbols") {
            scripting.exportDebugSymbols = false;
        }
        SUBCASE("self-contained for a runtime identifier with debug symbols") {
            scripting.exportMode = script_export_mode::SELF_CONTAINED;
            scripting.exportDebugSymbols = true;
            settings.runtimeIdentifier = "linux-x64";
        }
        SUBCASE("without game scripts") {
            settings.scriptsProject.clear();
            expectedCommands = 1;
        }

        const std::vector<std::vector<std::string>> commands =
          managed_export_commands(settings, scripting, managed, targets, absolute_path("artifacts"), {});
        REQUIRE(commands.size() == expectedCommands);
        const std::vector<std::string> expected = {"-p:CustomAfterMicrosoftCommonTargets=" + targets.string()};
        for (usize index = 0; index < commands.size(); ++index) {
            CAPTURE(index);
            CHECK(arguments_with_prefix(commands[index], "-p:CustomAfterMicrosoftCommonTargets=") == expected);
        }
    }

    TEST_CASE(
      "export_pipeline - the source roots file adds the project's directory only when source control gave none"
    ) {
        const std::string_view targets = managed_export_source_roots_targets();
        // Between source control filling SourceRoot and the SDK mapping the roots, and only in a build that maps.
        CHECK(targets.find("BeforeTargets=\"InitializeSourceRootMappedPaths\"") != std::string_view::npos);
        CHECK(targets.find("'$(DeterministicSourcePaths)' == 'true'") != std::string_view::npos);
        CHECK(targets.find("'%(SourceRoot.SourceControl)' != ''") != std::string_view::npos);
        // The project's directory, escaped so that an item Include neither splits it at ';' nor expands a '*', in
        // the group that only a project without a root of source control gets.
        const usize conditional =
          targets.find("<ItemGroup Condition=\"'@(_ConjureExportControlledSourceRoot)' == ''\">");
        REQUIRE(conditional != std::string_view::npos);
        const usize conditionalEnd = targets.find("</ItemGroup>", conditional);
        const usize projectRoot =
          targets.find("$([MSBuild]::Escape($([MSBuild]::NormalizeDirectory('$(MSBuildProjectDirectory)'))))");
        REQUIRE(projectRoot != std::string_view::npos);
        CHECK(projectRoot > conditional);
        CHECK(projectRoot < conditionalEnd);
        CHECK(targets.starts_with("<Project>"));
        CHECK(targets.ends_with("</Project>\n"));
    }

    TEST_CASE(
      "regression - code sweep FU-ARTIFACTS the source roots file adds the intermediate directory for every project"
    ) {
        // The intermediates moved to the export's artifacts directory, by default below the system's temporary
        // directory: in a git repository with a commit, whose root source control gives, the generated sources and
        // the .pdb path would keep that folder unless the intermediate directory is a root of its own there too.
        const std::string_view targets = managed_export_source_roots_targets();
        const usize conditional =
          targets.find("<ItemGroup Condition=\"'@(_ConjureExportControlledSourceRoot)' == ''\">");
        REQUIRE(conditional != std::string_view::npos);
        const usize conditionalEnd = targets.find("</ItemGroup>", conditional);
        REQUIRE(conditionalEnd != std::string_view::npos);
        const usize intermediateRoot = targets.find(
          "<SourceRoot Include=\"$([MSBuild]::Escape($([MSBuild]::NormalizeDirectory("
          "'$(MSBuildProjectDirectory)', '$(BaseIntermediateOutputPath)'))))\" />"
        );
        REQUIRE(intermediateRoot != std::string_view::npos);
        CHECK(intermediateRoot > conditionalEnd);
        // Its group has no condition, and it lies inside the target, after the roots of source control are known.
        const usize group = targets.rfind("<ItemGroup", intermediateRoot);
        REQUIRE(group != std::string_view::npos);
        CHECK(group > conditionalEnd);
        CHECK(targets.substr(group).starts_with("<ItemGroup>"));
        CHECK(intermediateRoot < targets.find("</Target>"));
    }

    TEST_CASE("regression - code sweep FU-MANAGEDDIR the scripts build names the directory the publish fills") {
        // The game template (managed/Conjure.Templates/Game, and the project file the editor's script.create writes)
        // references Conjure.Managed.dll through ConjureManagedDir and stops its build without it, and the export
        // never passed it: every export of a project with game scripts failed at the scripts build. The publish
        // writes the dll into the managed directory first, so the build is pointed at the very directory of -o.
        std::filesystem::path managed = absolute_path(std::filesystem::path("out") / "managed");
        export_settings settings = {};
        settings.bootstrapProject = "Boot.csproj";
        settings.scriptsProject = std::filesystem::path("Scripts") / "Game.csproj";
        scripting_settings scripting = {};

        SUBCASE("framework-dependent without debug symbols") {
            scripting.exportDebugSymbols = false;
        }
        SUBCASE("self-contained for a runtime identifier with debug symbols") {
            scripting.exportMode = script_export_mode::SELF_CONTAINED;
            scripting.exportDebugSymbols = true;
            settings.runtimeIdentifier = "win-x64";
        }
        SUBCASE("a scripts project in the working directory") {
            settings.scriptsProject = "Game.csproj";
        }
        SUBCASE("a managed directory with spaces, which stays one argument without quotes of its own") {
            managed = absolute_path(std::filesystem::path("My Game") / "export folder" / "managed");
        }

        const std::vector<std::vector<std::string>> commands = managed_export_commands(
          settings,
          scripting,
          managed,
          absolute_path("roots.targets"),
          absolute_path("artifacts"),
          {}
        );
        REQUIRE(commands.size() == 2);
        // Only the scripts build reads it: the bootstrap and Conjure.Managed are the engine's own projects.
        CHECK(arguments_with_prefix(commands[0], "-p:ConjureManagedDir=").empty());
        CHECK(argument_after(commands[0], "-o") == managed.string());
        const std::vector<std::string> expected = {"-p:ConjureManagedDir=" + managed.string()};
        CHECK(arguments_with_prefix(commands[1], "-p:ConjureManagedDir=") == expected);
        CHECK(argument_after(commands[1], "-o") == managed.string());
    }

    TEST_CASE(
      "export_pipeline - managed commands assert on a relative managed directory, source roots file or artifacts "
      "directory"
    ) {
        // MSBuild resolves a relative property value against each project's own directory and dotnet resolves -o
        // against its working directory, so a relative path would name a folder other than the one meant.
        export_settings settings = {};
        settings.bootstrapProject = "Boot.csproj";
        settings.scriptsProject = std::filesystem::path("Scripts") / "Game.csproj";
        std::filesystem::path managed = absolute_path(std::filesystem::path("out") / "managed");
        std::filesystem::path targets = absolute_path("roots.targets");
        std::filesystem::path artifacts = absolute_path("artifacts");

        SUBCASE("the managed directory is relative") {
            managed = std::filesystem::path("out") / "managed";
        }
        SUBCASE("the managed directory is empty") {
            managed.clear();
        }
        SUBCASE("the source roots file is relative") {
            targets = "roots.targets";
        }
        SUBCASE("the source roots file is empty") {
            targets.clear();
        }
        SUBCASE("the artifacts directory is relative") {
            artifacts = "artifacts";
        }
        SUBCASE("the artifacts directory is empty") {
            artifacts.clear();
        }

        CONJURE_CHECK_ASSERTS(
          (void) managed_export_commands(settings, scripting_settings{}, managed, targets, artifacts, {})
        );
    }

    TEST_CASE(
      "regression - code sweep FU-PDB managed commands pass UTF-8 paths and escape what MSBuild reads as properties"
    ) {
        // dotnet hands -o and --artifacts-path to MSBuild as properties, where ';' and ',' separate properties, '"'
        // quotes and '%' escapes; the project paths are positional and reach MSBuild as they are.
        const std::string separator = CONJURE_PLATFORM_WINDOWS ? "\\" : "/";
        // The root of the volume (C:\ or /) holds none of the characters MSBuild needs escaped.
        const std::u8string rootText = absolute_path({}).u8string();
        const std::string root(rootText.begin(), rootText.end());
        const std::filesystem::path managed =
          absolute_path(std::filesystem::path(u8"out,1;x%2\"q") / u8"Spiel\u00e4");
        const std::filesystem::path targets = absolute_path(std::filesystem::path(u8"st;age,%") / "roots.targets");
        const std::filesystem::path artifacts = absolute_path(std::filesystem::path(u8"art,i;fa%cts\"") / u8"\u00e4");
        export_settings settings = {};
        settings.bootstrapProject = std::filesystem::path(u8"Boot,1;\u00e4.csproj");
        settings.scriptsProject = std::filesystem::path(u8"Sc,ripts;\u00e4") / "Game.csproj";
        const std::vector<std::vector<std::string>> commands =
          managed_export_commands(settings, scripting_settings{}, managed, targets, artifacts, {});
        REQUIRE(commands.size() == 2);

        const std::string escapedManaged = root + "out%2C1%3Bx%252%22q" + separator + "Spiel\xC3\xA4";
        const std::string escapedTargets = root + "st%3Bage%2C%25" + separator + "roots.targets";
        const std::string escapedArtifacts = root + "art%2Ci%3Bfa%25cts%22" + separator + "\xC3\xA4";
        const std::vector<std::string> expectedTargets = {"-p:CustomAfterMicrosoftCommonTargets=" + escapedTargets};
        for (usize index = 0; index < commands.size(); ++index) {
            CAPTURE(index);
            CHECK(argument_after(commands[index], "-o") == escapedManaged);
            CHECK(argument_after(commands[index], "--artifacts-path") == escapedArtifacts);
            CHECK(arguments_with_prefix(commands[index], "-p:CustomAfterMicrosoftCommonTargets=") == expectedTargets);
        }
        CHECK(commands[0][1] == "Boot,1;\xC3\xA4.csproj");
        CHECK(commands[1][1] == "Sc,ripts;\xC3\xA4" + separator + "Game.csproj");
        const std::vector<std::string> expectedManagedDir = {"-p:ConjureManagedDir=" + escapedManaged};
        CHECK(arguments_with_prefix(commands[1], "-p:ConjureManagedDir=") == expectedManagedDir);
    }

    TEST_CASE("managed_export_staging - writes the source roots file and removes its directory on destruction") {
        temp_directory directory("engine_export_staging");
        const std::filesystem::path staged = directory.path() / "staging";
        {
            core::result<managed_export_staging> created = managed_export_staging::create(staged);
            CONJURE_REQUIRE_OK(created);
            const managed_export_staging staging = std::move(created).value();
            CHECK(staging.directory() == staged);
            CHECK(staging.source_roots_targets().parent_path() == staged);
            REQUIRE(std::filesystem::is_regular_file(staging.source_roots_targets()));
            CHECK(file_text(staging.source_roots_targets()) == managed_export_source_roots_targets());
            // The default artifacts directory of step 3 lies inside it and is left for dotnet to create; what dotnet
            // writes there goes with the staging directory.
            CHECK(staging.artifacts_directory().parent_path() == staged);
            CHECK(staging.artifacts_directory() != staging.source_roots_targets());
            CHECK_FALSE(std::filesystem::exists(staging.artifacts_directory()));
            CONJURE_REQUIRE_OK(directory.write_text("staging/artifacts/obj/Game/release_export_x/Game.dll", "x"));
        }
        CHECK_FALSE(std::filesystem::exists(staged));
        CHECK(std::filesystem::is_directory(directory.path()));
    }

    TEST_CASE("managed_export_staging - refuses a directory that exists and leaves it and its files alone") {
        temp_directory directory("engine_export_staging_taken");
        CONJURE_REQUIRE_OK(directory.write_text("taken/keep.txt", "mine"));
        CONJURE_CHECK_ERROR(managed_export_staging::create(directory.file("taken")), core::error_code::ALREADY_EXISTS);
        CHECK(file_text(directory.file("taken/keep.txt")) == "mine");
    }

    TEST_CASE("managed_export_staging - a directory that cannot be created is IO_FAILURE and nothing appears") {
        temp_directory directory("engine_export_staging_uncreatable");
        std::filesystem::path staged = {};
        SUBCASE("the parent is missing") {
            staged = directory.path() / "missing" / "staging";
        }
        SUBCASE("the parent is a file") {
            CONJURE_REQUIRE_OK(directory.write_text("file", "x"));
            staged = directory.path() / "file" / "staging";
        }
        CONJURE_CHECK_ERROR(managed_export_staging::create(staged), core::error_code::IO_FAILURE);
        std::error_code ignored = {};
        CHECK_FALSE(std::filesystem::exists(staged, ignored));
        CHECK_FALSE(std::filesystem::exists(directory.path() / "missing", ignored));
    }

    TEST_CASE("managed_export_staging - a move hands the directory over and the moved-from object removes nothing") {
        temp_directory directory("engine_export_staging_move");
        const std::filesystem::path first = directory.path() / "first";
        const std::filesystem::path second = directory.path() / "second";
        core::result<managed_export_staging> createdFirst = managed_export_staging::create(first);
        CONJURE_REQUIRE_OK(createdFirst);
        core::result<managed_export_staging> createdSecond = managed_export_staging::create(second);
        CONJURE_REQUIRE_OK(createdSecond);
        {
            managed_export_staging target = std::move(createdSecond).value();
            {
                managed_export_staging moved = std::move(createdFirst).value();
                managed_export_staging constructed(std::move(moved));
                CHECK(moved.directory().empty());
                CHECK(constructed.directory() == first);
                // Assigning over 'target' releases the directory it owned before.
                target = std::move(constructed);
                CHECK(constructed.directory().empty());
                CHECK(target.directory() == first);
                CHECK_FALSE(std::filesystem::exists(second));
            }
            // 'moved' and 'constructed' are gone and removed nothing.
            CHECK(std::filesystem::is_regular_file(target.source_roots_targets()));
        }
        CHECK_FALSE(std::filesystem::exists(first));
    }

    TEST_CASE("export_pipeline - every dotnet command runs without telemetry, banner or lingering MSBuild nodes") {
        // An MSBuild node that outlives the build inherits the command's output pipe and keeps it open, so the
        // export waited for its process timeout.
        const std::vector<std::pair<std::string, std::string>> environment = managed_export_environment();
        const std::array<std::pair<std::string_view, std::string_view>, 3> expected = {{
            {"DOTNET_CLI_TELEMETRY_OPTOUT", "1"},
            {"DOTNET_NOLOGO", "1"},
            {"MSBUILDNODEREUSE", "0"},
        }};
        CHECK(environment.size() == expected.size());
        for (const std::pair<std::string_view, std::string_view> &variable : expected) {
            const std::string_view name = variable.first;
            CAPTURE(name);
            const auto named = [name](const std::pair<std::string, std::string> &candidate) {
                return candidate.first == name;
            };
            const auto found = std::find_if(environment.begin(), environment.end(), named);
            REQUIRE(found != environment.end());
            CHECK(found->second == variable.second);
        }
    }

    TEST_CASE(
      "export_pipeline - publish_managed fails naming a dotnet that cannot be started or has no SDK and stops there"
    ) {
        // The step also removes the MSBuild staging directory it made below the system's temporary directory before
        // the first command, on this way out as on success; the pipeline names it in an INFO line.
        log_capture logs = {};
        temp_directory directory("engine_export_no_dotnet");
        const std::filesystem::path outputDir = directory.path() / "out";
        project loaded = engine_test::write_project(directory.path() / "project", "no_dotnet");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        application_desc desc = scripted_desc(loaded, false);
        desc.config.logLevel = core::log_level::INFO;
        application app(std::move(desc));
        CONJURE_REQUIRE_OK(app.initialise());
        export_pipeline pipeline(app.context());
        export_settings settings = {};
        settings.outputDirectory = outputDir;
        settings.buildNativeModule = false;
        settings.copyRuntime = false;
        settings.bootstrapProject = directory.path() / "Boot.csproj";
        // Owner decision OWN-INSTALL: from the engine source too, the failure names the SDK global.json asks for and
        // Microsoft's download page, for a dotnet that cannot be started and for one without an SDK, which answers
        // --version with an error (conjure_process_child answers every unknown mode with exit code 2).
        std::string found = "no_dotnet_here";
        SUBCASE("a dotnet that cannot be started") {
            settings.dotnetExecutable = directory.file("no_dotnet_here").string();
            found = "no_dotnet_here";
        }
        SUBCASE("a dotnet without an SDK") {
            settings.dotnetExecutable = CONJURE_ENGINE_TEST_PROCESS_CHILD;
            found = "--version' exited with 2";
        }
        const core::result<export_report> ran = pipeline.run(settings);
        app.shutdown();

        REQUIRE_FALSE(ran.has_value());
        const std::string message = ran.get_error().to_string();
        CAPTURE(message);
        CHECK(message.find("publish_managed") != std::string::npos);
        CHECK(message.find(found) != std::string::npos);
        CHECK(message.find(std::string(required_dotnet_sdk_version())) != std::string::npos);
        CHECK(message.find(dotnet_sdk_download_page()) != std::string::npos);
        CHECK_FALSE(std::filesystem::exists(outputDir / "managed"));
        CHECK_FALSE(std::filesystem::exists(outputDir / "project.json"));

        const std::vector<std::filesystem::path> staged = logged_paths(logs, STAGING_LINE);
        REQUIRE(staged.size() == 1);
        CAPTURE(staged.front().string());
        CHECK(staged.front().is_absolute());
        CHECK(staged.front().filename().string().starts_with("conjure_export_"));
        std::error_code error = {};
        CHECK_FALSE(std::filesystem::exists(staged.front(), error));
        CHECK_FALSE(error);
    }

    TEST_CASE(
      "export_pipeline - publish_managed compiles in the artifacts directory named, or in its staging directory"
    ) {
        // Follow-up FU-ARTIFACTS: the pipeline names in an INFO line the folder it hands to both dotnet commands as
        // --artifacts-path. A dotnet that cannot be started ends the step before anything is compiled, so no folder
        // is created anywhere, the working directory included.
        log_capture logs = {};
        temp_directory directory("engine_export_artifacts_named");
        const std::filesystem::path outputDir = directory.path() / "out";
        project loaded = engine_test::write_project(directory.path() / "project", "artifacts_named");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        export_settings settings = {};
        settings.outputDirectory = outputDir;
        settings.buildNativeModule = false;
        settings.copyRuntime = false;
        settings.bootstrapProject = directory.path() / "Boot.csproj";
        settings.dotnetExecutable = directory.file("no_dotnet_here").string();
        std::filesystem::path expected = {};
        bool inStaging = false;

        SUBCASE("none named: a folder of the staging directory, removed with it") {
            inStaging = true;
        }
        SUBCASE("an absolute folder, used as it is") {
            settings.artifactsDirectory = directory.path() / "export artifacts";
            expected = settings.artifactsDirectory;
        }
        SUBCASE("a relative folder, resolved against the working directory") {
            settings.artifactsDirectory = std::filesystem::path("conjure_relative") / "artifacts";
            std::error_code error = {};
            expected = std::filesystem::absolute(settings.artifactsDirectory, error);
            REQUIRE_FALSE(error);
        }

        application_desc desc = scripted_desc(loaded, false);
        desc.config.logLevel = core::log_level::INFO;
        application app(std::move(desc));
        CONJURE_REQUIRE_OK(app.initialise());
        export_pipeline pipeline(app.context());
        const core::result<export_report> ran = pipeline.run(settings);
        app.shutdown();
        CONJURE_CHECK_ERROR(ran, engine_error::EXPORT_FAILED);

        const std::vector<std::filesystem::path> staged = logged_paths(logs, STAGING_LINE);
        const std::vector<std::filesystem::path> artifacts = logged_paths(logs, ARTIFACTS_LINE);
        REQUIRE(staged.size() == 1);
        REQUIRE(artifacts.size() == 1);
        CAPTURE(artifacts.front().string());
        CHECK(artifacts.front().is_absolute());
        if (inStaging) {
            CHECK(artifacts.front().parent_path() == staged.front());
        } else {
            CHECK(artifacts.front().generic_u8string() == expected.lexically_normal().generic_u8string());
        }
        std::error_code error = {};
        CHECK_FALSE(std::filesystem::exists(artifacts.front(), error));
        CHECK_FALSE(error);
    }

    TEST_CASE("export_pipeline - with a prebuilt SDK the managed commands build only the game's scripts against it") {
        // Owner decision OWN-INSTALL: an installed editor has no bootstrap project to publish; the pipeline copies the
        // prebuilt SDK itself, and the scripts build still writes into the export's managed directory.
        export_settings settings = {};
        settings.scriptsProject = absolute_path(std::filesystem::path("game") / "scripts" / "Game.csproj");
        const std::filesystem::path managed = absolute_path(std::filesystem::path("out") / "managed");
        const std::filesystem::path prebuilt = absolute_path(std::filesystem::path("editor") / "managed");
        const std::filesystem::path targets = absolute_path("roots.targets");
        const std::filesystem::path artifacts = absolute_path("artifacts");

        const std::vector<std::vector<std::string>> commands =
          managed_export_commands(settings, scripting_settings{}, managed, targets, artifacts, prebuilt);
        REQUIRE(commands.size() == 1);
        const std::vector<std::string> &build = commands.front();
        CHECK(build.front() == "build");
        CHECK(count_argument(build, "publish") == 0);
        CHECK(argument_after(build, "-o") == utf8(managed));
        const std::vector<std::string> managedDir = arguments_with_prefix(build, "-p:ConjureManagedDir=");
        REQUIRE(managedDir.size() == 1);
        CHECK(managedDir.front() == "-p:ConjureManagedDir=" + utf8(prebuilt));
        CHECK(count_argument(build, "-p:ContinuousIntegrationBuild=true") == 1);

        settings.scriptsProject.clear();
        CHECK(managed_export_commands(settings, scripting_settings{}, managed, targets, artifacts, prebuilt).empty());
    }

    TEST_CASE("export_pipeline - managed commands assert on a relative prebuilt SDK directory") {
        export_settings settings = {};
        settings.scriptsProject = absolute_path("Game.csproj");
        CONJURE_CHECK_ASSERTS(
          (void) managed_export_commands(
            settings,
            scripting_settings{},
            absolute_path("managed"),
            absolute_path("roots.targets"),
            absolute_path("artifacts"),
            std::filesystem::path("editor") / "managed"
          )
        );
    }

    TEST_CASE("export_pipeline - publish_managed copies a prebuilt SDK without its manifest and runs no dotnet") {
        // Owner decision OWN-INSTALL: a project without game scripts needs no .NET SDK at all, so the dotnet of this
        // case does not exist and the export still succeeds. The .pdb files travel only with the debug symbols, named
        // without regard to case. A folder named relative to the current directory is resolved against it before it
        // is checked and copied (and before managed_export_commands, which asserts on a relative one).
        log_capture logs = {};
        temp_directory directory("engine_export_prebuilt");
        const std::filesystem::path prebuilt = write_prebuilt_sdk(directory, "editor/managed", prebuilt_manifest());
        project loaded = engine_test::write_project(directory.path() / "project", "prebuilt");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        scripting_settings scripting = {};
        std::filesystem::path given = prebuilt;
        SUBCASE("without debug symbols") {
            scripting.exportDebugSymbols = false;
        }
        SUBCASE("with debug symbols") {
            scripting.exportDebugSymbols = true;
        }
        SUBCASE("named relative to the current directory") {
            scripting.exportDebugSymbols = false;
            given = std::filesystem::relative(prebuilt);
            REQUIRE_FALSE(given.empty());
            REQUIRE(given.is_relative());
        }
        const std::filesystem::path outputDir = directory.path() / "out";
        const core::result<export_report> ran =
          export_prebuilt(loaded, scripting, given, outputDir, directory.file("no_dotnet_here"), {});

        CONJURE_REQUIRE_OK(ran);
        const std::vector<std::filesystem::path> copied = logged_paths(logs, "export: copying the prebuilt SDK '");
        REQUIRE(copied.size() == 1);
        CAPTURE(copied.front().string());
        CHECK(copied.front().is_absolute());
        std::error_code same = {};
        CHECK(std::filesystem::equivalent(copied.front(), prebuilt, same));
        CHECK_FALSE(same);
        CHECK(ran.value().managedPublished);
        CHECK(ran.value().steps.size() == export_pipeline::STEP_NAMES.size());
        const std::filesystem::path managed = outputDir / "managed";
        for (const std::string_view name : PREBUILT_FILES) {
            CAPTURE(name);
            const bool symbols = core::str::iends_with(name, ".pdb");
            const bool expected = !symbols || scripting.exportDebugSymbols;
            CHECK(std::filesystem::is_regular_file(managed / std::filesystem::path(name)) == expected);
            if (expected) {
                CHECK(file_text(managed / std::filesystem::path(name)) == name);
            }
        }
        const std::filesystem::path manifest = std::filesystem::path(export_pipeline::MANAGED_SDK_MANIFEST_NAME);
        CHECK_FALSE(std::filesystem::exists(managed / manifest));
        CHECK(std::filesystem::is_regular_file(outputDir / "project.json"));
        // No staging directory: nothing was built.
        CHECK(logged_paths(logs, STAGING_LINE).empty());
    }

    TEST_CASE("export_pipeline - a prebuilt SDK the engine cannot ship is refused before anything is written") {
        log_capture logs = {};
        temp_directory directory("engine_export_prebuilt_refused");
        project loaded = engine_test::write_project(directory.path() / "project", "prebuilt_refused");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        const std::string hash = std::format("0x{:016X}", CONJURE_NATIVE_API_HASH);
        std::string manifest = prebuilt_manifest();
        std::string_view missingFile = {};
        std::string expected = {};
        std::string_view alsoExpected = {};
        scripting_settings scripting = {};
        const auto replaced = [&manifest](std::string_view from, std::string_view to) {
            const usize at = manifest.find(from);
            REQUIRE(at != std::string::npos);
            manifest.replace(at, from.size(), to);
        };
        SUBCASE("no manifest") {
            manifest.clear();
            expected = "holds no conjure_managed_sdk.json";
        }
        SUBCASE("a manifest that is no JSON") {
            manifest = "{ not json";
            expected = "reading '";
        }
        SUBCASE("a manifest of another kind") {
            replaced("conjure_managed_sdk_export", "something_else");
            expected = "is no manifest of schema 1";
        }
        SUBCASE("a manifest of another schema") {
            replaced("\"schema_version\": 1", "\"schema_version\": 2");
            expected = "is no manifest of schema 1";
        }
        SUBCASE("a self-contained SDK") {
            replaced("framework_dependent", "self_contained");
            expected = "describes a 'self_contained' SDK";
        }
        SUBCASE("another native API hash") {
            replaced(hash, std::format("0x{:016X}", CONJURE_NATIVE_API_HASH ^ 1ull));
            expected = "was built for the native API hash";
        }
        SUBCASE("no native API hash") {
            replaced(hash, "");
            expected = "was built for the native API hash ''";
        }
        SUBCASE("no Conjure.Managed.dll") {
            missingFile = "Conjure.Managed.dll";
            expected = "has no Conjure.Managed.dll";
        }
        SUBCASE("no runtime configuration of the bootstrap") {
            missingFile = "Conjure.Bootstrap.runtimeconfig.json";
            expected = "has no Conjure.Bootstrap.runtimeconfig.json";
        }
        SUBCASE("a self-contained export") {
            scripting.exportMode = script_export_mode::SELF_CONTAINED;
            expected = "cannot make a self-contained export";
            // The advice names the spelling parse_script_export_mode accepts.
            alsoExpected = "scripting.export_mode to \"framework-dependent\"";
        }
        const std::filesystem::path prebuilt = write_prebuilt_sdk(directory, "editor/managed", manifest);
        std::error_code error = {};
        if (manifest.empty()) {
            const std::filesystem::path manifestName(export_pipeline::MANAGED_SDK_MANIFEST_NAME);
            std::filesystem::remove(prebuilt / manifestName, error);
            REQUIRE_FALSE(error);
        }
        if (!missingFile.empty()) {
            std::filesystem::remove(prebuilt / std::filesystem::path(missingFile), error);
            REQUIRE_FALSE(error);
        }
        const std::filesystem::path outputDir = directory.path() / "out";
        const core::result<export_report> ran =
          export_prebuilt(loaded, scripting, prebuilt, outputDir, directory.file("no_dotnet_here"), {});

        CONJURE_CHECK_ERROR(ran, engine_error::EXPORT_FAILED);
        REQUIRE_FALSE(ran.has_value());
        const std::string message = ran.get_error().message();
        CAPTURE(message);
        CHECK(message.find("publish_managed") != std::string::npos);
        CHECK(message.find(expected) != std::string::npos);
        CHECK(message.find(alsoExpected) != std::string::npos);
        CHECK_FALSE(std::filesystem::exists(outputDir));
    }

    TEST_CASE("export_pipeline - a prebuilt SDK is not checked when no managed code is published") {
        // The prebuilt folder has no manifest; with scripting disabled or publishManaged false step 3 copies nothing,
        // so nothing refuses the export.
        log_capture logs = {};
        temp_directory directory("engine_export_prebuilt_unused");
        CONJURE_REQUIRE_OK(directory.write_text("editor/managed/Conjure.Managed.dll", "x"));
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        export_settings settings = {};
        settings.outputDirectory = directory.path() / "out";
        settings.buildNativeModule = false;
        settings.copyRuntime = false;
        settings.prebuiltManagedDirectory = directory.path() / "editor" / "managed";
        bool scriptingEnabled = true;
        std::string extraJson = {};
        SUBCASE("publishManaged is false") {
            settings.publishManaged = false;
        }
        SUBCASE("scripting is disabled in the configuration and the project") {
            scriptingEnabled = false;
            extraJson = R"("scripting": {"enabled": false})";
        }
        project loaded = engine_test::write_project(directory.path() / "project", "prebuilt_unused", {}, extraJson);
        application_desc desc = scripted_desc(loaded, false);
        desc.config.scripting.enabled = scriptingEnabled;
        application app(std::move(desc));
        CONJURE_REQUIRE_OK(app.initialise());
        export_pipeline pipeline(app.context());
        const core::result<export_report> ran = pipeline.run(settings);
        app.shutdown();

        CONJURE_REQUIRE_OK(ran);
        CHECK_FALSE(ran.value().managedPublished);
        CHECK_FALSE(std::filesystem::exists(directory.path() / "out" / "managed"));
    }

    TEST_CASE("export_pipeline - a bootstrap project and a prebuilt SDK together are BAD_ARGUMENTS") {
        log_capture logs = {};
        temp_directory directory("engine_export_prebuilt_both");
        const std::filesystem::path prebuilt = write_prebuilt_sdk(directory, "editor/managed", prebuilt_manifest());
        project loaded = engine_test::write_project(directory.path() / "project", "prebuilt_both");
        application app(scripted_desc(loaded, false));
        CONJURE_REQUIRE_OK(app.initialise());
        export_pipeline pipeline(app.context());
        export_settings settings = {};
        settings.outputDirectory = directory.path() / "out";
        settings.buildNativeModule = false;
        settings.copyRuntime = false;
        settings.bootstrapProject = directory.path() / "Boot.csproj";
        settings.prebuiltManagedDirectory = prebuilt;
        const core::result<export_report> ran = pipeline.run(settings);
        app.shutdown();

        CONJURE_CHECK_ERROR(ran, engine_error::BAD_ARGUMENTS);
        CHECK_FALSE(std::filesystem::exists(directory.path() / "out"));
    }

    TEST_CASE("export_pipeline - a prebuilt SDK inside the output is BAD_ARGUMENTS and nothing is written") {
        // An export into the editor's own folder would copy the SDK onto itself.
        log_capture logs = {};
        temp_directory directory("engine_export_prebuilt_inside");
        const std::filesystem::path prebuilt = write_prebuilt_sdk(directory, "editor/managed", prebuilt_manifest());
        project loaded = engine_test::write_project(directory.path() / "project", "prebuilt_inside");
        std::filesystem::path outputDir = {};
        SUBCASE("the output is the editor's folder") {
            outputDir = directory.path() / "editor";
        }
        SUBCASE("the output is the prebuilt SDK itself") {
            outputDir = prebuilt;
        }
        const core::result<export_report> ran =
          export_prebuilt(loaded, scripting_settings{}, prebuilt, outputDir, directory.file("no_dotnet_here"), {});

        CONJURE_CHECK_ERROR(ran, engine_error::BAD_ARGUMENTS);
        CHECK(std::filesystem::is_directory(outputDir));
        CHECK_FALSE(std::filesystem::exists(outputDir / "project.json"));
        CHECK_FALSE(std::filesystem::exists(outputDir / "cooked"));
        CHECK(file_text(prebuilt / "Conjure.Managed.dll") == "Conjure.Managed.dll");
    }

    TEST_CASE("export_pipeline - publish_managed names the SDK it needs when no dotnet can build the game's scripts") {
        // Owner decision OWN-INSTALL: the .NET SDK is an optional download. A dotnet that cannot be started, or one
        // that answers --version with an error as a dotnet without an SDK does (conjure_process_child answers every
        // unknown mode with exit code 2), ends the step with the version global.json asks for and Microsoft's
        // download page; the prebuilt SDK was copied before.
        log_capture logs = {};
        temp_directory directory("engine_export_prebuilt_no_sdk");
        const std::filesystem::path prebuilt = write_prebuilt_sdk(directory, "editor/managed", prebuilt_manifest());
        project loaded = engine_test::write_project(directory.path() / "project", "prebuilt_no_sdk");
        CONJURE_REQUIRE_OK(directory.write_text("project/scripts/Game.csproj", "<Project />\n"));
        std::filesystem::path dotnet = directory.file("no_dotnet_here");
        std::string found = "could not be started";
        SUBCASE("a dotnet that cannot be started") {
            found = "could not be started";
        }
        SUBCASE("a dotnet without an SDK") {
            dotnet = std::filesystem::path(CONJURE_ENGINE_TEST_PROCESS_CHILD);
            found = "--version' exited with 2";
        }
        const std::filesystem::path outputDir = directory.path() / "out";
        const core::result<export_report> ran = export_prebuilt(
          loaded,
          scripting_settings{},
          prebuilt,
          outputDir,
          dotnet,
          directory.path() / "project" / "scripts" / "Game.csproj"
        );

        CONJURE_CHECK_ERROR(ran, engine_error::EXPORT_FAILED);
        REQUIRE_FALSE(ran.has_value());
        const std::string message = ran.get_error().message();
        CAPTURE(message);
        CHECK(message.find("publish_managed") != std::string::npos);
        CHECK(message.find(found) != std::string::npos);
        CHECK(message.find(std::string(required_dotnet_sdk_version())) != std::string::npos);
        CHECK(message.find(dotnet_sdk_download_page()) != std::string::npos);
        CHECK(std::filesystem::is_regular_file(outputDir / "managed" / "Conjure.Managed.dll"));
        CHECK_FALSE(std::filesystem::exists(outputDir / "project.json"));
        // The game's scripts took the step as far as building: it made its staging directory before asking dotnet.
        CHECK(logged_paths(logs, STAGING_LINE).size() == 1);
    }

#if CONJURE_ENGINE_TEST_HAS_DOTNET
    TEST_CASE(
      "regression - code sweep FU-PDB an export ships managed assemblies naming no folder of the machine, outside git "
      "and in a repository with a commit [dotnet][slow]" * doctest::timeout(3.0 * DOTNET_SUBCASE_SECONDS)
    ) {
        // The engine's managed projects, the game template and global.json are copied into a temp directory that
        // holds an empty git repository: source control gives a repository without a commit no source root, as it
        // gives none to a project outside git, and the case behaves the same whether the build tree lies in a
        // checkout or not. Step 3 then runs the real dotnet, and nothing it wrote may name the temp directory, the
        // checkout, the build tree or the system's temporary directory, where the intermediates are compiled
        // (follow-up FU-ARTIFACTS). In a repository with a commit source control gives the root, and the export's
        // source roots file must not add the project's directory: the SDK maps the repository to /_/, the path Source
        // Link's URLs start from, and a root of the project's would take the longer match. It adds the intermediate
        // directory, which lies outside the repository and gets a root of its own.
        log_capture logs = {};
        temp_directory directory("engine_export_managed_paths");
        bool exportDebugSymbols = false;
        bool inRepositoryWithCommit = false;
        SUBCASE("outside git, with debug symbols") {
            exportDebugSymbols = true;
        }
        SUBCASE("outside git, without debug symbols") {
            exportDebugSymbols = false;
        }
        SUBCASE("in a git repository with a commit, with debug symbols") {
            // The CodeView PDB path the check below reads exists only with debug symbols.
            exportDebugSymbols = true;
            inRepositoryWithCommit = true;
        }
        const std::filesystem::path bootstrap = stage_engine_managed(directory);
        const std::filesystem::path projectDir = directory.path() / "project";
        copy_from_source("managed/Conjure.Templates/Game", projectDir / "scripts");
        const project loaded = engine_test::write_project(projectDir, "managed_paths");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        if (inRepositoryWithCommit) {
            const std::string_view git = CONJURE_ENGINE_TEST_GIT;
            if (git.empty()) {
                MESSAGE("no git was found at configure time: the mapping inside a repository with a commit, which "
                        "Source Link relies on, is not checked");
                return;
            }
            commit_everything(directory);
        }

        const std::filesystem::path outputDir = directory.path() / "out";
        const core::result<export_report> ran =
          export_managed(loaded, exportDebugSymbols, bootstrap, projectDir / "scripts" / "Game.csproj", outputDir, {});
        REQUIRE_MESSAGE(ran.has_value(), ran.get_error().to_string());
        CHECK(ran.value().managedPublished);

        const std::filesystem::path managed = outputDir / "managed";
        std::vector<std::filesystem::path> binaries = {};
        for (const std::string_view name : {"Conjure.Bootstrap", "Conjure.Managed", "Game"}) {
            CAPTURE(name);
            const std::filesystem::path assembly = managed / (std::string(name) + ".dll");
            const std::filesystem::path symbols = managed / (std::string(name) + ".pdb");
            REQUIRE(std::filesystem::is_regular_file(assembly));
            binaries.push_back(assembly);
            CHECK(std::filesystem::is_regular_file(symbols) == exportDebugSymbols);
            if (exportDebugSymbols) {
                binaries.push_back(symbols);
            }
        }
        if (inRepositoryWithCommit) {
            // The .pdb lies in the scripts build's intermediates below the staging directory, outside the repository,
            // and its CodeView path in Game.dll is mapped against the intermediate directory: /_<n>/, where n counts
            // the roots before it (the repository's, then those the SDK adds for the NuGet package folders of the
            // machine).
            constexpr std::string_view PIVOT_PDB = "/release_export_framework_dependent/Game.pdb";
            const std::string codeView = string_ending_with(file_text(managed / "Game.dll"), PIVOT_PDB);
            CAPTURE(codeView);
            REQUIRE(codeView.size() > PIVOT_PDB.size() + 2);
            CHECK(codeView.starts_with("/_"));
            const std::string_view rootNumber =
              std::string_view(codeView).substr(2, codeView.size() - PIVOT_PDB.size() - 2);
            const auto isDigit = [](char character) {
                return character >= '0' && character <= '9';
            };
            CHECK(std::all_of(rootNumber.begin(), rootNumber.end(), isDigit));
        }
        check_no_machine_paths(binaries, machine_roots(directory));
    }

    TEST_CASE(
      "regression - code sweep FU-MANAGEDDIR an export builds the scripts project the editor's script.create writes "
      "[dotnet][slow]" * doctest::timeout(3.0 * DOTNET_SUBCASE_SECONDS)
    ) {
        // script.create writes scripts/Game.csproj and the script from the templates of the editor's
        // commands_script.cpp; the case reads them from that source, so it builds what the editor writes today. The
        // project file references Conjure.Managed.dll through ConjureManagedDir and stops the build without it,
        // which every export of such a project did.
        log_capture logs = {};
        std::u8string projectFolder = u8"project";
        std::u8string outputFolder = u8"out";
        bool leftovers = false;
        SUBCASE("a project only script.create has touched") {}
        SUBCASE("a project whose bin/ and obj/ hold what a script.compile left") {
            leftovers = true;
        }
        SUBCASE("a project and an output folder whose names hold spaces, separators, a '%' and a non-ASCII letter") {
            // Every path the export hands to MSBuild (-o, --artifacts-path, ConjureManagedDir and the project
            // directory the source roots file reads) reaches the real dotnet: one escaped too little splits a
            // property (MSB1006 on the comma), one escaped twice lands the assemblies in a folder named "out%2C ...".
            projectFolder = u8"projekt ä";
            outputFolder = u8"out, 1;%ä";
        }
        temp_directory directory("engine_export_editor_scripts");
        const std::filesystem::path bootstrap = stage_engine_managed(directory);
        const std::string editorSource = file_text(
          std::filesystem::path(CONJURE_ENGINE_TEST_SOURCE_DIR) / "engine" / "src" / "editor" / "core" / "commands"
          / "commands_script.cpp"
        );
        const std::string csproj = string_constant(editorSource, "CSPROJ_TEMPLATE");
        const std::string behaviour = string_constant(editorSource, "BEHAVIOUR_TEMPLATE");
        REQUIRE_MESSAGE(!csproj.empty(), "CSPROJ_TEMPLATE is no longer a constant of adjacent string literals");
        REQUIRE_MESSAGE(!behaviour.empty(), "BEHAVIOUR_TEMPLATE is no longer a constant of adjacent string literals");
        // What script.create writes for {"name": "Mover"}: its default namespace is Game.
        const std::string mover = core::str::replace_all(
          core::str::replace_all(behaviour, "{NAMESPACE}", "Game"),
          "{NAME}",
          "Mover"
        );
        const std::filesystem::path projectDir = directory.path() / projectFolder;
        write_file(projectDir / "scripts" / "Game.csproj", csproj);
        write_file(projectDir / "scripts" / "Mover.cs", mover);
        if (leftovers) {
            // The project file compiles every .cs below it except bin/ and obj/, where the editor's own builds
            // write: none of this may reach the compiler. The export compiles in its artifacts directory instead.
            write_file(projectDir / "scripts" / "obj" / "Debug" / "Game.AssemblyInfo.cs", "not C#");
            write_file(projectDir / "scripts" / "bin" / "Debug" / "Stale.cs", "not C#");
        }

        const project loaded = engine_test::write_project(projectDir, "editor_scripts");
        write_file(projectDir / "assets" / "data" / "notes.bytes", "x");
        const std::filesystem::path outputDir = directory.path() / outputFolder;
        const core::result<export_report> ran =
          export_managed(loaded, false, bootstrap, projectDir / "scripts" / "Game.csproj", outputDir, {});
        REQUIRE_MESSAGE(ran.has_value(), ran.get_error().to_string());
        CHECK(ran.value().managedPublished);

        // The scripts compiled against the Conjure.Managed the publish wrote beside them.
        const std::filesystem::path managed = outputDir / "managed";
        REQUIRE(std::filesystem::is_regular_file(managed / "Conjure.Managed.dll"));
        REQUIRE(std::filesystem::is_regular_file(managed / "Game.dll"));
        const std::string game = file_text(managed / "Game.dll");
        CHECK(game.find("Mover") != std::string::npos);
        CHECK(game.find("Conjure.Managed") != std::string::npos);
    }

    TEST_CASE(
      "regression - code sweep FU-ARTIFACTS an export leaves nothing in the engine's build/managed_artifacts or the "
      "game's scripts folder [dotnet][slow]" * doctest::timeout(2.0 * DOTNET_SUBCASE_SECONDS)
    ) {
        // The publish passed no artifacts path, so an export from a build tree compiled the bootstrap and
        // Conjure.Managed into build/managed_artifacts beside the engine's managed/ folder (the default of
        // managed/Directory.Build.props), and the scripts build into the game's scripts/obj. The staged copy of the
        // engine below keeps that layout: its default is <temp>/engine/build/managed_artifacts.
        log_capture logs = {};
        temp_directory directory("engine_export_artifacts");
        std::filesystem::path artifactsDirectory = {};
        SUBCASE("none named: a folder of the staging directory, removed when the step ends") {}
        SUBCASE("a folder the caller names, which the export keeps") {
            artifactsDirectory = directory.path() / "export artifacts";
        }
        const std::filesystem::path bootstrap = stage_engine_managed(directory);
        const std::filesystem::path projectDir = directory.path() / "project";
        copy_from_source("managed/Conjure.Templates/Game", projectDir / "scripts");
        const project loaded = engine_test::write_project(projectDir, "artifacts");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));

        const std::filesystem::path outputDir = directory.path() / "out";
        const core::result<export_report> ran = export_managed(
          loaded,
          true,
          bootstrap,
          projectDir / "scripts" / "Game.csproj",
          outputDir,
          artifactsDirectory
        );
        REQUIRE_MESSAGE(ran.has_value(), ran.get_error().to_string());
        CHECK(ran.value().managedPublished);

        const std::filesystem::path managed = outputDir / "managed";
        std::vector<std::filesystem::path> binaries = {};
        for (const std::string_view name : {"Conjure.Bootstrap", "Conjure.Managed", "Game"}) {
            CAPTURE(name);
            for (const std::string_view extension : {".dll", ".pdb"}) {
                CAPTURE(extension);
                const std::filesystem::path binary = managed / (std::string(name) + std::string(extension));
                CHECK(std::filesystem::is_regular_file(binary));
                binaries.push_back(binary);
            }
        }
        std::error_code error = {};
        CHECK_FALSE(std::filesystem::exists(directory.path() / "engine" / "build" / "managed_artifacts", error));
        CHECK_FALSE(std::filesystem::exists(projectDir / "scripts" / "obj", error));
        CHECK_FALSE(std::filesystem::exists(projectDir / "scripts" / "bin", error));
        // -o receives the assemblies only.
        CHECK_FALSE(std::filesystem::exists(managed / "obj", error));
        CHECK_FALSE(std::filesystem::exists(managed / "bin", error));

        const std::vector<std::filesystem::path> staged = logged_paths(logs, STAGING_LINE);
        const std::vector<std::filesystem::path> artifacts = logged_paths(logs, ARTIFACTS_LINE);
        REQUIRE(staged.size() == 1);
        REQUIRE(artifacts.size() == 1);
        CAPTURE(artifacts.front().string());
        CHECK_FALSE(std::filesystem::exists(staged.front(), error));
        if (artifactsDirectory.empty()) {
            CHECK(artifacts.front().parent_path() == staged.front());
        } else {
            // Each project compiled into the export's own pivot of the folder, which stays.
            CHECK(artifacts.front().generic_u8string() == artifactsDirectory.generic_u8string());
            for (const std::string_view name : {"Conjure.Bootstrap", "Conjure.Managed", "Game"}) {
                CAPTURE(name);
                const std::filesystem::path pivot =
                  artifactsDirectory / "obj" / std::filesystem::path(name) / "release_export_framework_dependent";
                CHECK(std::filesystem::is_directory(pivot, error));
            }
        }
        check_no_machine_paths(binaries, machine_roots(directory));
    }
#endif

    TEST_CASE("export_pipeline - a cooked project cannot be exported again") {
        log_capture logs = {};
        temp_directory directory("engine_export_twice");
        const std::filesystem::path projectDir = directory.path() / "project";
        const std::filesystem::path outputDir = directory.path() / "out";
        project loaded = engine_test::write_project(projectDir, "once");
        CONJURE_REQUIRE_OK(directory.write_text("project/assets/data/notes.bytes", "x"));
        {
            application app(project_desc(loaded, 0));
            CONJURE_REQUIRE_OK(app.initialise());
            export_pipeline pipeline(app.context());
            export_settings settings = {};
            settings.outputDirectory = outputDir;
            settings.buildNativeModule = false;
            settings.publishManaged = false;
            settings.copyRuntime = false;
            CONJURE_REQUIRE_OK(pipeline.run(settings));
            app.shutdown();
        }
        core::result<project> exported = project::load(outputDir);
        CONJURE_REQUIRE_OK(exported);
        application cooked(project_desc(std::move(exported).value(), 0));
        CONJURE_REQUIRE_OK(cooked.initialise());
        export_pipeline again(cooked.context());
        export_settings settings = {};
        settings.outputDirectory = directory.path() / "out2";
        CONJURE_CHECK_ERROR(again.run(settings), core::error_code::INVALID_STATE);
        cooked.shutdown();
    }
}
