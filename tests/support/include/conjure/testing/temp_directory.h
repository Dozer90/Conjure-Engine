#pragma once

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <filesystem>
#include <string_view>

// Scratch directories for tests (docs/design/08_testing.md 4.3). File I/O in tests happens here and
// nowhere else.
//
//   temp_directory directory = {};
//   CONJURE_REQUIRE_OK(directory.write_text("scene.json", "{}"));
//   auto loaded = document::parse_file(directory.file("scene.json"));

namespace conjure::testing {

    /// <summary>
    /// A unique, empty directory under environment().temporaryRoot (<build>/test_tmp/<executable>;
    /// the system temporary directory when no environment was installed), named &lt;prefix&gt;_&lt;8 hex
    /// digits&gt; (only the digits without a prefix) and deleted recursively on destruction. A name
    /// another instance or test process already holds is never reused. Removal failure is a WARN, not
    /// a test failure.
    /// </summary>
    class temp_directory {
        private:
            std::filesystem::path mPath = {};

        public:
            temp_directory();
            explicit temp_directory(std::string_view prefix);
            ~temp_directory();

            temp_directory(const temp_directory &) = delete;
            temp_directory &operator=(const temp_directory &) = delete;

        public:
            /// <summary>
            /// Absolute path of the directory.
            /// </summary>
            [[nodiscard]] const std::filesystem::path &path() const noexcept {
                return mPath;
            }

            /// <summary>
            /// Path of a file or sub-path inside the directory; nothing is created.
            /// </summary>
            [[nodiscard]] std::filesystem::path file(std::string_view name) const;

            /// <summary>
            /// Writes (or overwrites) a text file inside the directory, creating parent directories. Works past
            /// Windows' MAX_PATH: the calls go through extended_length_path(); messages name the classic path.
            /// </summary>
            [[nodiscard]] core::result<void> write_text(std::string_view name, std::string_view text) const;

            /// <summary>
            /// Writes a file inside the directory and returns its full path. Fails the case when the
            /// file cannot be written. Prefer write_text; this form exists for the older tests.
            /// </summary>
            std::filesystem::path write_file(std::string_view relativePath, std::string_view contents) const;
    };

    /// <summary>
    /// A folder length past Windows' MAX_PATH (260) with room for the files below it, for long_directory().
    /// </summary>
    inline constexpr usize PAST_MAX_PATH = 300;

    /// <summary>
    /// The form of a path that a test's own file calls use to reach a folder past Windows' MAX_PATH (260): on
    /// Windows the absolute, normalised path behind the extended-length prefix \\?\ (\\?\UNC\ for a share,
    /// \\server\share\...), and a path already in a literal form (\\?\, \\.\ or \??\) unchanged;
    /// elsewhere the path unchanged, since Linux and macOS have no such limit. A temp_directory writes its files
    /// and removes itself through this form, so a case may build a folder past the limit inside one.
    /// </summary>
    [[nodiscard]] std::filesystem::path extended_length_path(const std::filesystem::path &path);

    /// <summary>
    /// Creates and returns a folder below 'directory' whose absolute path has at least 'minimumLength'
    /// characters, made of 40-character segments, so a case can run past Windows' MAX_PATH (260). The path
    /// is returned in its classic form. Fails the case when the folder cannot be created.
    /// </summary>
    std::filesystem::path long_directory(const temp_directory &directory, usize minimumLength);
}
