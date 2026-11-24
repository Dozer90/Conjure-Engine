#pragma once

#include "conjure/core/types.h"

#include <filesystem>
#include <string>

// Paths past Windows' classic limit (code sweep FU-LONGPATH). A Win32 call refuses the classic form of
// a path of MAX_PATH (260) characters or more, CreateDirectoryW one of 248 or more, and the loader
// fails a library with an embedded manifest (every MSVC build of a module) from about 250 characters
// on, even in a process that opted into long paths. Every form accepts the extended-length form
// (\\?\C:\..., \\?\UNC\server\share\...) up to about 32,767 characters, so the module system hands
// each path it builds for a file, folder or library call through extend_path() or
// always_extended_path(). The paths it stores and reports stay in the form it was given, a failed
// library load included (module_loader.cpp names the classic form in the loader's message); only the
// JSON reader's and writer's messages it embeds name the path in the form it was opened. POSIX has
// no such limit and gets every path back unchanged (long_path_posix.cpp).
//
// Two more copies exist until core exports one helper (code sweep F190). The VFS has
// extend_native_path / always_extended_native_path (engine/src/core/vfs/native_platform_windows.cpp),
// which measures the threshold on the path as given rather than its absolute form and does not take
// \??\ as a literal form. The tests have conjure::testing::extended_length_path
// (tests/support/src/temp_directory.cpp), which applies always_extended_path()'s rule but returns a
// path given in a literal form as it is, without normalising it. A fix to the prefix or share
// handling here belongs in those two as well.

namespace conjure::modules::detail {

    /// <summary>
    /// The length (in UTF-16 code units of the absolute path) from which extend_path() returns the
    /// extended-length form on Windows: the 248 of CreateDirectoryW, the lowest Win32 limit, which
    /// also stays below the loader's limit for a library with a manifest.
    /// </summary>
    inline constexpr usize EXTENDED_PATH_THRESHOLD = 248;

    /// <summary>
    /// The form of 'path' a file, folder or library call accepts whatever its length. Windows: when the
    /// absolute path has EXTENDED_PATH_THRESHOLD characters or more, it is returned absolute, normalised
    /// (no "." or "..", backslashes) and with the extended-length prefix (\\?\, \\?\UNC\ for a share);
    /// a path already in a literal form (\\?\, \\.\ or \??\, or one that the standard library completes
    /// to such a form, as it does "//?/C:/...") is only normalised, since Windows takes those literally;
    /// any other path is returned unchanged. POSIX: the path unchanged.
    /// </summary>
    [[nodiscard]] std::filesystem::path extend_path(const std::filesystem::path &path);

    /// <summary>
    /// Like extend_path() without the length threshold, for calls that descend below the path
    /// (remove_all, a recursive copy, a folder listing whose entries are used as they come): on Windows
    /// the absolute, normalised, extended-length form of any path. POSIX: the path unchanged.
    /// </summary>
    [[nodiscard]] std::filesystem::path always_extended_path(const std::filesystem::path &path);

    /// <summary>
    /// The clause a failed library load appends to its message for a long path: on Windows, when the
    /// absolute path has EXTENDED_PATH_THRESHOLD characters or more, "; the path has N characters ..."
    /// naming the length, that it was opened in the extended-length form, the threshold and MAX_PATH. It
    /// states facts only and names no cause, since the extended-length form opens such a path. Empty for a
    /// shorter path, for a path already in a literal form (\\?\, \\.\ or \??\), and on every other
    /// platform.
    /// </summary>
    [[nodiscard]] std::string path_length_note(const std::filesystem::path &path);
}
