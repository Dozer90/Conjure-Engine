#include "editor_arguments.h"

#include "conjure/editor/editor_application.h"
#include "conjure/editor/editor_options.h"
#include "conjure/editor/ui/editor_layer.h"

#include "conjure/core/error.h"
#include "conjure/core/log.h"
#include "conjure/core/result.h"
#include "conjure/engine/application_desc.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/exit_code.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// conjure_editor (docs/design/04_editor.md 3.1, 3.11, 8): parses the editor command line, builds an
// editor_application and runs it. Exit codes:
//
//   0  ok
//   1  a command of --exec failed, or --export failed
//   2  engine or project startup failure: project.json missing or invalid (its own settings included), a device
//      or subsystem that did not start, an explicit --ai-port that is busy in headless mode; also a frame failure
//   3  bad arguments: a bad editor flag, an option neither the editor nor the engine knows, a bad engine flag
//      value or one that makes the merged configuration invalid, an unreadable --exec file
//   4  the graphics device was lost (windowed only)
//
// The arguments are read as UTF-8 (editor_arguments.h; on Windows the manifest makes UTF-8 the process code page),
// so a project path outside the legacy code page opens. Whenever standard output carries protocol lines
// (--ai-stdio, --exec without --out, --export) every log message goes to standard error instead. With --ai-stdio
// the reader thread may still block in getline on the terminal when the session ends, so the process leaves
// through std::quick_exit after an orderly shutdown instead of waiting for it.

namespace {
    using namespace conjure;

    constexpr int EXIT_STARTUP_FAILURE = 2;         // 04_editor.md 3.11: engine or project startup failure
    constexpr int EXIT_BAD_ARGUMENTS = static_cast<int>(engine::exit_code::BAD_ARGUMENTS);
    constexpr std::string_view HELP_HINT = "conjure_editor: run 'conjure_editor --help' for the options\n";

    /// <summary>
    /// Replaces the console sink, which writes every message below WARN to standard output, with one that writes
    /// every message to standard error, uncoloured. For the lifetime of the process: standard output belongs to
    /// the protocol. Standard error is unbuffered and fprintf locks the stream per call, so concurrent writers
    /// are safe.
    /// </summary>
    void route_log_to_standard_error() {
        core::log::set_console_enabled(false);
        core::log::add_sink(std::make_shared<core::callback_log_sink>([](const core::log_message &message) {
            const std::string line = core::format_log_line(message);
            std::fprintf(stderr, "%s\n", line.c_str());
        }));
    }

    /// <summary>
    /// The exit code of a failed initialise(): 3 when the arguments are at fault (an unknown option, a bad flag
    /// value, or a flag that makes the merged configuration invalid), 2 for everything else. An invalid merge is
    /// the project's fault when the merge without any engine flag fails the same way (as conjure_runtime decides).
    /// </summary>
    [[nodiscard]] int startup_exit_code(const editor::editor_options &options, const core::error &error) {
        if (error.is(engine::engine_error::BAD_ARGUMENTS)) {
            return EXIT_BAD_ARGUMENTS;
        }
        if (!error.is(engine::engine_error::INVALID_CONFIG)) {
            return EXIT_STARTUP_FAILURE;
        }
        // --no-scripting stays in effect through options.noScripting, which make_engine_desc applies itself.
        editor::editor_options bare = options;
        bare.engineArguments.clear();
        const core::result<engine::application_desc> merged =
          editor::editor_application::make_engine_desc(bare, options.projectPath);
        if (!merged.has_value() && merged.get_error().is(engine::engine_error::INVALID_CONFIG)) {
            return EXIT_STARTUP_FAILURE;
        }
        return EXIT_BAD_ARGUMENTS;
    }

    int run_editor(std::span<const std::string_view> arguments) {
        const core::result<editor::editor_options> parsed = editor::parse_editor_options(arguments);
        if (!parsed.has_value()) {
            std::cerr << "conjure_editor: " << parsed.get_error().message() << "\n\n" << editor::editor_options_help();
            return EXIT_BAD_ARGUMENTS;
        }
        const editor::editor_options &options = parsed.value();
        if (options.showHelp) {
            std::cout << editor::editor_options_help();
            return static_cast<int>(engine::exit_code::OK);
        }
        if (options.stdout_carries_protocol()) {
            route_log_to_standard_error();
        }
        editor::editor_application application = {};
        const core::result<void> initialised = application.initialise(options, [](editor::editor_context &context) {
            return editor::ui::create_editor_layer(context);
        });
        if (!initialised.has_value()) {
            std::cerr << "conjure_editor: " << initialised.get_error().to_string() << '\n';
            application.shutdown();
            const int exitCode = startup_exit_code(options, initialised.get_error());
            if (exitCode == EXIT_BAD_ARGUMENTS) {
                std::cerr << HELP_HINT;
            }
            return exitCode;
        }
        const int exitCode = application.run();
        application.shutdown();
        if (options.aiStdio) {
            std::cout.flush();
            std::cerr.flush();
            std::quick_exit(exitCode);
        }
        return exitCode;
    }
}

int main(int argc, char **argv) {
    const std::vector<std::string> storage = conjure::editor_app::utf8_arguments(argc, argv);
    const std::vector<std::string_view> arguments(storage.begin(), storage.end());
    return run_editor(arguments);
}
