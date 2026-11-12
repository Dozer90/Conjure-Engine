// POSIX backend of editor_arguments.h (Linux and macOS): argv is already bytes the file system understands.

#include "editor_arguments.h"

std::vector<std::string> conjure::editor_app::utf8_arguments(int argc, char **argv) {
    std::vector<std::string> arguments = {};
    arguments.reserve(argc > 1 ? static_cast<size_t>(argc) - 1 : 0);
    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }
    return arguments;
}
