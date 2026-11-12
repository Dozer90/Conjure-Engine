#pragma once

#include <string>
#include <vector>

// The platform-specific piece of conjure_editor (docs/CONVENTIONS.md 3: platform code lives in backend files,
// chosen by editor/CMakeLists.txt the way runtime/CMakeLists.txt picks runtime_platform_*.cpp): the process
// arguments as UTF-8, so a project directory or command file outside the legacy code page reaches the editor intact.

namespace conjure::editor_app {

    /// <summary>
    /// The arguments after the program name, as UTF-8. Windows re-reads the wide command line
    /// (GetCommandLineW + CommandLineToArgvW) and converts it with core::str::from_utf16, so a path outside the
    /// active code page survives; POSIX passes the bytes of argv through unchanged.
    /// </summary>
    [[nodiscard]] std::vector<std::string> utf8_arguments(int argc, char **argv);
}
