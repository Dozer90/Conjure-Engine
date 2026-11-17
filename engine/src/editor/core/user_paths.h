#pragma once

#include "conjure/core/result.h"

#include <filesystem>
#include <string>
#include <string_view>

// Where the editor keeps what belongs to one user on one machine and to no project (docs/design/04_editor.md D27):
// today the project trust store. user_paths_windows.cpp and user_paths_posix.cpp implement
// user_state_directory(), selected by CMake like the socket layer; the rule the POSIX file applies is the plain
// function below it, in user_paths.cpp, so that it is compiled and tested on every host.

namespace conjure::editor {

    /// <summary>
    /// The folder for the editor's per-user state on this machine. Windows: the known folder LocalAppData (the
    /// one that does not roam to other machines) with "Conjure" appended. Linux and macOS:
    /// posix_user_state_directory() over the environment variables XDG_STATE_HOME and HOME. NOT_FOUND when the
    /// operating system names no such folder. Nothing is created.
    /// </summary>
    [[nodiscard]] core::result<std::filesystem::path> user_state_directory();

    /// <summary>
    /// The POSIX rule as text, "" when no folder can be named. Linux layout: 'xdgStateHome' + "/conjure" when
    /// that value is an absolute path (the XDG base directory specification tells a program to ignore a relative
    /// one), otherwise 'home' + "/.local/state/conjure" when 'home' is an absolute path. macOS layout
    /// ('macosLayout'): 'home' + "/Library/Application Support/Conjure", and 'xdgStateHome' is not looked at.
    /// Trailing separators of the value used are dropped first. Absolute means "starts with '/'": the function
    /// reads its arguments as POSIX paths on every host.
    /// </summary>
    [[nodiscard]] std::string posix_user_state_directory(
      std::string_view xdgStateHome,
      std::string_view home,
      bool macosLayout
    );
}
