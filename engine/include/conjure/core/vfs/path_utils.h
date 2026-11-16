#pragma once

#include "conjure/core/api.h"

#include <string>
#include <string_view>

// Lexical helpers for the '/'-separated UTF-8 paths used by the virtual file system. None of
// these touch the disk. Backslashes are accepted as separators on input and always written as
// forward slashes on output.

namespace conjure::core::vfs::path_utils {

    /// <summary>
    /// Produces the canonical spelling of a path: forward slashes only, no empty or "." segments,
    /// ".." resolved against the preceding segment (a leading ".." of a relative path is kept, a
    /// ".." at the root of an absolute path is dropped) and no trailing slash. Drive letters
    /// ("C:/x") and UNC prefixes ("//server/share") are preserved. "." and "" both become "".
    /// </summary>
    [[nodiscard]] CONJURE_API std::string normalize(std::string_view path);

    /// <summary>
    /// Joins two paths with exactly one separator. An empty side yields the other side unchanged;
    /// an absolute right side replaces the left side. The result is not normalised.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string join(std::string_view left, std::string_view right);

    /// <summary>
    /// Everything before the last separator ("a/b/c.txt" -> "a/b"). A single segment has the
    /// empty parent; the parent of "/a" is "/" and of "C:/a" is "C:/": a root keeps its separator
    /// (in the spelling the input used), so a root is its own parent. A trailing separator is ignored.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view parent(std::string_view path) noexcept;

    /// <summary>
    /// The last segment ("a/b/c.txt" -> "c.txt"). Empty when the path ends with a separator.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view filename(std::string_view path) noexcept;

    /// <summary>
    /// The file name without its extension ("c.tar.gz" -> "c.tar"). Names that start with a dot
    /// and have no other dot (".gitignore") are returned whole.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view stem(std::string_view path) noexcept;

    /// <summary>
    /// The lower-cased extension including the dot (".PNG" -> ".png"), or "" when there is none.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string extension(std::string_view path);

    /// <summary>
    /// True when the path's extension equals 'extension' ignoring case; the dot is optional
    /// ("png" and ".png" both match "a.PNG").
    /// </summary>
    [[nodiscard]] CONJURE_API bool has_extension(std::string_view path, std::string_view extension) noexcept;

    /// <summary>
    /// True for paths starting with a separator, a drive letter ("C:") or a UNC prefix.
    /// </summary>
    [[nodiscard]] CONJURE_API bool is_absolute(std::string_view path) noexcept;

    /// <summary>
    /// Expresses 'path' relative to 'base' lexically ("a/b/c" relative to "a/x" -> "../b/c").
    /// Both inputs are normalised first. Equal paths yield "". When one side is absolute and the
    /// other is not, or the roots differ, the normalised 'path' is returned unchanged. Only the
    /// root prefix (a drive letter) is compared ignoring case; segments are compared exactly, so
    /// "C:/Users/x" relative to "c:/users" is "../Users/x". A lexical helper cannot know the case
    /// rules of the volume a path will be used on.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string relative_to(std::string_view path, std::string_view base);

    /// <summary>
    /// True when 'name' (one path segment, not a path) may be given to a new file or directory: the
    /// rule every file_system applies wherever it creates an entry, on every platform, so that content
    /// authored on one opens on all of them. A name is not portable when it is empty, holds a path
    /// separator, a control character (U+0000 to U+001F, U+007F) or one of : * ? " < > |, ends with a
    /// dot or a space, or is a Windows device name in any case and with any extension (CON, PRN, AUX,
    /// NUL, COM1 to COM9, LPT1 to LPT9: "nul", "Aux.txt"). Check a name the user typed with this before
    /// creating the entry; file_system reports the same verdict as INVALID_ARGUMENT.
    /// </summary>
    [[nodiscard]] CONJURE_API bool is_portable_name(std::string_view name) noexcept;
}
