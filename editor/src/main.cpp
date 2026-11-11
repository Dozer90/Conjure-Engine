#include "conjure/editor/editor_application.h"
#include "conjure/editor/editor_options.h"
#include "conjure/editor/log_category.h"
#include "conjure/editor/ui/editor_layer.h"

#include "conjure/core/log.h"
#include "conjure/engine/exit_code.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// conjure_editor (docs/design/04_editor.md 3.1, 8): parses the editor command line, builds an editor_application
// and runs it. Exit codes: 0 ok, 1 a command of --exec failed, 2 engine or project startup failure, 3 bad
// arguments. With --ai-stdio the reader thread may still block in getline on the terminal when the session
// ends, so the process leaves through std::quick_exit after an orderly shutdown instead of waiting for it.

namespace {
    using namespace conjure;

    int run_editor(std::span<const std::string_view> arguments) {
        const core::result<editor::editor_options> parsed = editor::parse_editor_options(arguments);
        if (!parsed.has_value()) {
            std::cerr << "conjure_editor: " << parsed.get_error().message() << "\n\n" << editor::editor_options_help();
            return static_cast<int>(engine::exit_code::BAD_ARGUMENTS);
        }
        const editor::editor_options &options = parsed.value();
        if (options.showHelp) {
            std::cout << editor::editor_options_help();
            return static_cast<int>(engine::exit_code::OK);
        }
        editor::editor_application application = {};
        const core::result<void> initialised = application.initialise(options, [](editor::editor_context &context) {
            return editor::ui::create_editor_layer(context);
        });
        if (!initialised.has_value()) {
            std::cerr << "conjure_editor: " << initialised.get_error().to_string() << '\n';
            application.shutdown();
            return static_cast<int>(engine::exit_code::INIT_FAILURE);
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
    std::vector<std::string> storage = {};
    storage.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int index = 1; index < argc; ++index) {
        storage.emplace_back(argv[index]);
    }
    std::vector<std::string_view> arguments = {};
    arguments.reserve(storage.size());
    for (const std::string &argument : storage) {
        arguments.emplace_back(argument);
    }
    return run_editor(arguments);
}
