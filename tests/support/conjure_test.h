#pragma once

// Shared helpers for every Conjure test executable. Include this instead of <doctest/doctest.h>.

#include "conjure/core/assert.h"
#include "conjure/core/log.h"
#include "conjure/core/types.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace conjure::test {

    /// <summary>
    /// Captures every log message written while the instance is alive. Console output is
    /// suppressed for the duration so test runs stay quiet.
    /// </summary>
    class log_capture {
        private:
            struct capture_sink;

            std::shared_ptr<capture_sink> mSink = nullptr;
            bool mPreviousConsoleEnabled = true;

        public:
            log_capture();
            ~log_capture();

            log_capture(const log_capture &) = delete;
            log_capture &operator=(const log_capture &) = delete;

        public:
            /// <summary>
            /// Returns a snapshot of the captured messages, oldest first.
            /// </summary>
            [[nodiscard]] std::vector<std::string> messages() const;

            /// <summary>
            /// Returns a snapshot of the captured messages at the given level.
            /// </summary>
            [[nodiscard]] std::vector<std::string> messages(core::log_level level) const;

            /// <summary>
            /// True when any captured message contains the given text.
            /// </summary>
            [[nodiscard]] bool contains(std::string_view text) const;

            /// <summary>
            /// Number of captured messages at or above the given level.
            /// </summary>
            [[nodiscard]] usize count(core::log_level minimumLevel = core::log_level::TRACE) const;

            void clear();
    };

    /// <summary>
    /// Records assertion failures instead of breaking into the debugger or failing the test.
    /// Use it to test code paths that are expected to assert.
    /// </summary>
    class assert_capture {
        private:
            core::assert_handler mPreviousHandler = nullptr;

        public:
            assert_capture();
            ~assert_capture();

            assert_capture(const assert_capture &) = delete;
            assert_capture &operator=(const assert_capture &) = delete;

        public:
            /// <summary>
            /// Number of assertion failures recorded since construction or the last reset.
            /// </summary>
            [[nodiscard]] usize count() const;

            /// <summary>
            /// The most recently recorded assertion message, or an empty string.
            /// </summary>
            [[nodiscard]] std::string last_message() const;

            void reset();
    };

    /// <summary>
    /// A unique, empty directory under the system temporary directory that is deleted on
    /// destruction.
    /// </summary>
    class temp_directory {
        private:
            std::filesystem::path mPath = {};

        public:
            explicit temp_directory(std::string_view prefix = "conjure_test");
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
            /// Writes a text file inside the directory and returns its full path.
            /// </summary>
            std::filesystem::path write_file(std::string_view relativePath, std::string_view contents) const;
    };
}
