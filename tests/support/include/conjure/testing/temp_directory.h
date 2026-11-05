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
    /// the system temporary directory when no environment was installed), deleted recursively on
    /// destruction. Removal failure is a WARN, not a test failure.
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
            /// Writes (or overwrites) a text file inside the directory, creating parent directories.
            /// </summary>
            [[nodiscard]] core::result<void> write_text(std::string_view name, std::string_view text) const;

            /// <summary>
            /// Writes a file inside the directory and returns its full path. Fails the case when the
            /// file cannot be written. Prefer write_text; this form exists for the older tests.
            /// </summary>
            std::filesystem::path write_file(std::string_view relativePath, std::string_view contents) const;
    };
}
