#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// The platform-specific pieces of conjure_runtime (docs/CONVENTIONS.md 3: platform code lives in backend files,
// chosen by runtime/CMakeLists.txt the way core/system picks its backends): the process arguments as UTF-8, the
// directory of the executable, and the two facts the diagnostics of main.cpp need about a process that was
// started without a console (a double-clicked Distribution build).

namespace conjure::runtime {

    /// <summary>
    /// The arguments after the program name, as UTF-8. Windows re-reads the wide command line
    /// (GetCommandLineW + CommandLineToArgvW) and converts it with core::str::from_utf16, so a path outside the
    /// active code page survives; POSIX passes the bytes of argv through unchanged.
    /// </summary>
    [[nodiscard]] std::vector<std::string> utf8_arguments(int argc, char **argv);

    /// <summary>
    /// The directory of the running executable; the current directory when the platform cannot say.
    /// </summary>
    [[nodiscard]] std::filesystem::path executable_directory();

    /// <summary>
    /// True when something reads standard error: a console, a pipe or a redirected file. A Distribution build
    /// started by a double-click on Windows has none, so its messages need another way out.
    /// </summary>
    [[nodiscard]] bool has_standard_error();

    /// <summary>
    /// Shows a modal error box where the platform has one (Windows: MessageBoxW); a no-op elsewhere.
    /// </summary>
    void show_error_dialog(std::string_view title, std::string_view text);
}
