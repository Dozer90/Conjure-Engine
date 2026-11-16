#pragma once

#include "conjure/core/vfs/file_system.h"

#include "conjure/core/result.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

// Helpers shared by the backends. Not part of the public API.

namespace conjure::core::vfs::internal {

    /// <summary>
    /// Normalises a backend-relative path: forward slashes, no "." or ".." segments, no leading
    /// slash ("" is the root). Paths that would leave the root (a leading "..", a drive letter or
    /// a UNC prefix) are PERMISSION_DENIED.
    /// </summary>
    [[nodiscard]] result<std::string> resolve_backend_path(std::string_view path);

    /// <summary>
    /// Why 'name' (one path segment) may not be given to a new entry, or an empty view when it may. The
    /// rule is the same on every platform and backend, so content authored anywhere opens everywhere
    /// (code sweep F753). A name is refused when it
    ///   - is empty or holds a path separator ('/' or '\'),
    ///   - holds a control character (U+0000 to U+001F, U+007F) or one of : * ? " < > |,
    ///   - ends with a dot or a space (Windows drops both, so the entry would land under another name),
    ///   - is a Windows device name, in any case and with any extension: CON, PRN, AUX, NUL, COM1 to
    ///     COM9, LPT1 to LPT9 ("nul", "Aux.txt", "com1.tar.gz").
    /// The returned text completes the sentence "the name '...' <text>".
    /// </summary>
    [[nodiscard]] std::string_view portable_name_problem(std::string_view name) noexcept;

    /// <summary>
    /// Checks the path of an entry about to be created (write_file, write_file_atomic,
    /// create_directories, the destination of rename): every segment of the already normalised
    /// 'normalizedPath' must pass portable_name_problem, whether or not it exists already.
    /// INVALID_ARGUMENT names the first segment that does not, quoting 'displayPath', the path as the
    /// caller wrote it. The root ("") passes. Reading, describing, listing and removing never call this,
    /// so entries that already carry such a name stay reachable.
    /// </summary>
    [[nodiscard]] result<void> validate_new_path(std::string_view normalizedPath, std::string_view displayPath);

    /// <summary>
    /// Splits an already normalised relative path into its segments ("" yields none).
    /// </summary>
    [[nodiscard]] std::vector<std::string_view> split_segments(std::string_view normalizedPath);

    /// <summary>
    /// True when 'path' is 'ancestor' itself or lies below it ("a/b" is inside "a" and "").
    /// </summary>
    [[nodiscard]] bool is_same_or_below(std::string_view path, std::string_view ancestor) noexcept;

    /// <summary>
    /// Whether an entry of the given kind passes the FILES / DIRECTORIES filter of 'flags'.
    /// </summary>
    [[nodiscard]] bool passes_kind_filter(enumerate_flags flags, bool isDirectory) noexcept;

    /// <summary>
    /// Builds the NOT_FOUND error every backend uses for a missing path.
    /// </summary>
    [[nodiscard]] error not_found(std::string_view path);

    /// <summary>
    /// Builds an IO_FAILURE error carrying the operating system's description of 'code'.
    /// </summary>
    [[nodiscard]] error io_error(std::string_view operation, std::string_view path, const std::error_code &code);

    /// <summary>
    /// Converts a UTF-8 path to the native std::filesystem::path.
    /// </summary>
    [[nodiscard]] std::filesystem::path path_from_utf8(std::string_view utf8);

    /// <summary>
    /// Converts a native std::filesystem::path to a UTF-8, '/'-separated string.
    /// </summary>
    [[nodiscard]] std::string path_to_utf8(const std::filesystem::path &path);

    /// <summary>
    /// Converts a file time to nanoseconds since the Unix epoch (0 for times before it).
    /// </summary>
    [[nodiscard]] u64 file_time_to_nanoseconds(std::filesystem::file_time_type time) noexcept;
}
