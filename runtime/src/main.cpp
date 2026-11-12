#include "runtime_platform.h"

#include "conjure/core/error.h"
#include "conjure/core/log.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/engine/application.h"
#include "conjure/engine/application_desc.h"
#include "conjure/engine/command_line.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/engine_version.h"
#include "conjure/engine/exit_code.h"
#include "conjure/engine/game_layer.h"

#if defined(CONJURE_RUNTIME_WITH_BINDINGS) && CONJURE_RUNTIME_WITH_BINDINGS
    #include "conjure/bindings/bindings_subsystem.h"
#endif

#include <cstdio>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

// conjure_runtime (docs/design/06_engine_loop.md 4.2, 2.12; docs/ARCHITECTURE.md 8): the standalone game
// runtime. The whole configuration merge (defaults < --headless < project.json < flags, D9) is
// application_desc::from_options; this file decides the process exit code of each outcome and makes sure the
// message behind it reaches someone:
//
//   0  OK               run() finished (--frames reached, window closed, --help printed)
//   1  INIT_FAILURE     application::initialise() failed (startup unwound, the error is logged)
//   2  RUNTIME_FAILURE  a frame failed, or project.json is missing, malformed, invalid, too new, or its own
//                       settings do not validate
//   3  BAD_ARGUMENTS    an unknown flag, a bad value, a merge a flag made invalid, no project anywhere
//   4  DEVICE_LOST      the graphics device was lost during a frame
//
// Diagnostics: every runtime message goes to standard error as "conjure_runtime: <message>". It is also logged
// through core::log into the log file when there is one (--log-file; in Distribution, where WIN32_GUI hides the
// console, the default <project>/conjure_runtime.log or <executable dir>/conjure_runtime.log), and shown in a
// message box when the process has no standard error at all (the console-less Distribution build on Windows,
// double-clicked or started from a terminal without redirection).
// --help writes the usage text to standard output and, when nothing reads standard output (the console-less
// Distribution build, double-clicked or started from a terminal), also shows it in an information box.
// Without --project the runtime runs the project.json next to the executable, which is how an exported folder
// starts from a double-click.
//
// Nothing of the editor is linked (04_editor.md 8): tools/check_distribution_symbols.py proves the
// Distribution binaries free of conjure::editor and ImGui symbols.

using namespace conjure;
using namespace conjure::engine;

namespace {

    constexpr std::string_view RUNTIME_NAME = "conjure_runtime";
    constexpr std::string_view PROJECT_FILE_NAME = "project.json";
    constexpr std::string_view DEFAULT_LOG_FILE_NAME = "conjure_runtime.log";
    constexpr std::string_view HELP_HINT = "Run 'conjure_runtime --help' for the option list.\n";

    CONJURE_DEFINE_LOG_CATEGORY(LOG_RUNTIME, "runtime");

    /// <summary>
    /// The usage text: the runtime's own line plus the engine's option list.
    /// </summary>
    [[nodiscard]] std::string usage_text() {
        return std::format(
          "{} {} - the Conjure standalone runtime\n"
          "Usage: conjure_runtime [--project <dir>] [options]\n"
          "Without --project the runtime runs the project.json next to the executable.\n\n{}",
          RUNTIME_NAME,
          engine_version_string(),
          command_line_help()
        );
    }

    /// <summary>
    /// Writes the usage text to the stream.
    /// </summary>
    void print_usage(std::FILE *stream) {
        const std::string text = usage_text();
        std::fputs(text.c_str(), stream);
    }

    /// <summary>
    /// Where the runtime's own messages go besides standard error: the log file, so a player without a console
    /// finds the error next to the game, and a message box when nothing reads standard error.
    /// </summary>
    class runtime_diagnostics {
        private:
            std::filesystem::path mLogFile = {};

        public:
            runtime_diagnostics() = default;

        public:
            void set_log_file(std::filesystem::path path) {
                mLogFile = std::move(path);
            }

            /// <summary>
            /// Prints "conjure_runtime: message" on standard error, appends it to the log file through core::log
            /// (the engine's own lines of the failed start are already there), shows it in a message box when
            /// the process has no standard error, and returns the code as main() must.
            /// </summary>
            [[nodiscard]] int fail(exit_code code, std::string_view message) const {
                std::fprintf(
                  stderr,
                  "%.*s: %.*s\n",
                  static_cast<int>(RUNTIME_NAME.size()),
                  RUNTIME_NAME.data(),
                  static_cast<int>(message.size()),
                  message.data()
                );
                std::fflush(stderr);
                append_to_log_file(message);
                if (!runtime::has_standard_error()) {
                    std::string text(message);
                    if (!mLogFile.empty()) {
                        text += std::format("\n\nThe log is {}", mLogFile.generic_string());
                    }
                    runtime::show_error_dialog(RUNTIME_NAME, text);
                }
                return static_cast<int>(code);
            }

        private:
            /// <summary>
            /// The engine's file sink is installed only while the application runs, so the runtime opens its
            /// own in append mode for the one line. The console sink is paused for it: standard error already
            /// carries the plain line.
            /// </summary>
            void append_to_log_file(std::string_view message) const {
                if (mLogFile.empty()) {
                    return;
                }
                const std::shared_ptr<core::file_log_sink> sink =
                  std::make_shared<core::file_log_sink>(mLogFile, false);
                if (!sink->is_open()) {
                    return;
                }
                const bool consoleWasEnabled = core::log::console_enabled();
                core::log::set_console_enabled(false);
                core::log::add_sink(sink);
                CONJURE_LOG_ERROR(LOG_RUNTIME, "{}: {}", RUNTIME_NAME, message);
                core::log::remove_sink(sink);
                core::log::set_console_enabled(consoleWasEnabled);
                sink->flush();
            }
    };

    /// <summary>
    /// True for the project.json errors of 06 2.10 (missing, unparsable, invalid, too new).
    /// </summary>
    [[nodiscard]] bool is_project_error(const core::error &error) noexcept {
        return error.is(engine_error::PROJECT_NOT_FOUND) || error.is(engine_error::PROJECT_PARSE_FAILED)
          || error.is(engine_error::PROJECT_INVALID) || error.is(engine_error::PROJECT_SCHEMA_TOO_NEW);
    }

    /// <summary>
    /// True when the project's own settings fail validation: the merge of the project alone (no flag but the
    /// --headless mode, which is applied before the project) fails with INVALID_CONFIG too. Decides whether an
    /// invalid merge is the project's fault (exit 2) or a flag's (exit 3).
    /// </summary>
    [[nodiscard]] bool project_settings_invalid(const command_line_options &options) {
        if (!options.projectPath.has_value()) {
            return false;
        }
        command_line_options bare = {};
        bare.projectPath = options.projectPath;
        bare.headless = options.headless;
        const core::result<application_desc> merged = application_desc::from_options(bare, engine_config::defaults());
        return !merged.has_value() && merged.get_error().is(engine_error::INVALID_CONFIG);
    }

    /// <summary>
    /// The directory of the executable when a project.json sits in it (an exported folder started by a
    /// double-click), otherwise nothing.
    /// </summary>
    [[nodiscard]] std::optional<std::filesystem::path> project_next_to_executable() {
        const std::filesystem::path directory = runtime::executable_directory();
        std::error_code ignored = {};
        if (!std::filesystem::is_regular_file(directory / PROJECT_FILE_NAME, ignored)) {
            return std::nullopt;
        }
        return directory;
    }

    /// <summary>
    /// The Distribution log file when --log-file is absent: conjure_runtime.log in the project directory when
    /// one is known and exists (the exported folder), else next to the executable.
    /// </summary>
    [[nodiscard]] std::filesystem::path default_log_file(const std::optional<std::filesystem::path> &projectPath) {
        std::error_code ignored = {};
        if (projectPath.has_value() && std::filesystem::is_directory(*projectPath, ignored)) {
            return *projectPath / DEFAULT_LOG_FILE_NAME;
        }
        return runtime::executable_directory() / DEFAULT_LOG_FILE_NAME;
    }

    [[nodiscard]] std::string join_arguments(const std::vector<std::string> &arguments) {
        std::string joined = {};
        for (const std::string &argument : arguments) {
            if (!joined.empty()) {
                joined += ' ';
            }
            joined += argument;
        }
        return joined;
    }
}

int main(int argc, char **argv) {
    const std::vector<std::string> utf8Arguments = runtime::utf8_arguments(argc, argv);
    const std::vector<std::string_view> arguments(utf8Arguments.begin(), utf8Arguments.end());

    runtime_diagnostics report = {};
#if CONJURE_DISTRIBUTION
    // The console is hidden (WIN32_GUI), so even a failure to parse the arguments must reach a file.
    report.set_log_file(default_log_file(std::nullopt));
#endif

    core::result<command_line_options> parsed = parse_command_line(arguments);
    if (!parsed.has_value()) {
        const int code = report.fail(exit_code::BAD_ARGUMENTS, parsed.get_error().to_string());
        std::fprintf(stderr, "%.*s", static_cast<int>(HELP_HINT.size()), HELP_HINT.data());
        return code;
    }
    command_line_options options = std::move(parsed).value();
    if (options.help) {
        const std::string usage = usage_text();
        std::fputs(usage.c_str(), stdout);
        std::fflush(stdout);
        if (!runtime::has_standard_output()) {
            // A console-less Distribution build (double-clicked, or started from a terminal a GUI process cannot
            // write to) would otherwise show nothing at all.
            runtime::show_info_dialog(RUNTIME_NAME, usage);
        }
        return static_cast<int>(exit_code::OK);
    }
    if (!options.unrecognisedArguments.empty()) {
        const std::string unknown = join_arguments(options.unrecognisedArguments);
        const int code = report.fail(exit_code::BAD_ARGUMENTS, "unknown option(s): " + unknown);
        print_usage(stderr);
        return code;
    }
    if (!options.projectPath.has_value()) {
        options.projectPath = project_next_to_executable();
        if (!options.projectPath.has_value()) {
            const int code = report.fail(
              exit_code::BAD_ARGUMENTS,
              "--project <dir> is required (no project.json next to the executable)"
            );
            print_usage(stderr);
            return code;
        }
    }
#if CONJURE_DISTRIBUTION
    if (!options.logFile.has_value()) {
        options.logFile = default_log_file(options.projectPath);
    }
#endif
    if (options.logFile.has_value()) {
        report.set_log_file(*options.logFile);
    }

    core::result<application_desc> merged = application_desc::from_options(options, engine_config::defaults());
    if (!merged.has_value()) {
        const core::error &error = merged.get_error();
        if (is_project_error(error)) {
            return report.fail(exit_code::RUNTIME_FAILURE, error.to_string());
        }
        if (error.is(engine_error::INVALID_CONFIG) && project_settings_invalid(options)) {
            return report.fail(
              exit_code::RUNTIME_FAILURE,
              std::format("project.json settings do not validate: {}", error.to_string())
            );
        }
        const int code = report.fail(exit_code::BAD_ARGUMENTS, error.to_string());
        std::fprintf(stderr, "%.*s", static_cast<int>(HELP_HINT.size()), HELP_HINT.data());
        return code;
    }

    application app(std::move(merged).value());
#if defined(CONJURE_RUNTIME_WITH_BINDINGS) && CONJURE_RUNTIME_WITH_BINDINGS
    // Startup step 16 (06 3.4): the bindings fill the non-core native table before the game assembly loads.
    app.add_subsystem(std::make_unique<bindings::scripting_bindings_subsystem>());
#endif
    app.layers().push_layer(std::make_unique<game_layer>());

    const core::result<void> started = app.initialise();
    if (!started.has_value()) {
        // The engine already logged the step that failed; the summary goes to stderr for scripts and CI and
        // to the log file for a player.
        return report.fail(
          exit_code::INIT_FAILURE,
          "initialisation failed: " + started.get_error().to_string()
        );
    }
    const int code = app.run();
    if (code != static_cast<int>(exit_code::OK)) {
        // A frame failed or the device was lost: the engine logged why; name the outcome where a player looks.
        const exit_code outcome = static_cast<exit_code>(code);
        return report.fail(outcome, std::format("stopped with {} ({}); see the log", to_string(outcome), code));
    }
    return code;
}
