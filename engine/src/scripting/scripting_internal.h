#pragma once

#include <filesystem>
#include <string>
#include <string_view>

// Helpers shared by the scripting sources. Private to engine/src/scripting.
//
// Paths cross the managed ABI, the logs and the dotnet command line as UTF-8. path::string() is the active
// ANSI code page on Windows (and fails for characters outside it), so every conversion goes through these.

namespace conjure::scripting::detail {

    /// <summary>
    /// The UTF-8 text of a path.
    /// </summary>
    [[nodiscard]] inline std::string path_to_utf8(const std::filesystem::path &path) {
        const std::u8string text = path.u8string();
        return std::string(reinterpret_cast<const char *>(text.data()), text.size());
    }

    /// <summary>
    /// The path UTF-8 text names (what the managed_api's load_game_assembly receives).
    /// </summary>
    [[nodiscard]] inline std::filesystem::path utf8_to_path(std::string_view utf8) {
        // Element-wise copy rather than a reinterpret_cast: char8_t may not alias char.
        return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
    }
}
