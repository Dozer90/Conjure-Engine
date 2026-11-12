#pragma once

#include <filesystem>

// Where the running executable lives: the engine's built-in content (engine://), the shader index, the
// managed SDK and the engine plugins are found next to it (docs/design/06_engine_loop.md 3.4 steps 5, 7
// and 15). The implementation is the one platform-specific file of the module.

namespace conjure::engine {

    /// <summary>
    /// The directory of the running executable, however long its path; the current directory, with a warning,
    /// when the platform cannot say.
    /// </summary>
    [[nodiscard]] std::filesystem::path executable_directory();
}
