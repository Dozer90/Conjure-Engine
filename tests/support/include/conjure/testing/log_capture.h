#pragma once

#include "conjure/core/log.h"
#include "conjure/core/types.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

// Log capture for tests (docs/design/08_testing.md 4.2).
//
//   log_capture logs = {};
//   CONJURE_LOG_WARN(LOG_ECS, "entity {} already destroyed", id);
//   CHECK(logs.contains(log_level::WARN, "already destroyed"));
//
// Capture is thread-safe (job threads log too). A capture that saw an ERROR or FATAL message fails
// the case when it is destroyed unless allow_errors() was called, so "no error logged" is a free
// assertion in every fixture.

namespace conjure::testing {

    /// <summary>
    /// One captured message.
    /// </summary>
    struct captured_log_entry {
        core::log_level level = core::log_level::TRACE;
        std::string category = {};
        std::string text = {};
    };

    /// <summary>
    /// Captures every log message written while the instance is alive. Console output is suppressed
    /// for the duration so test runs stay quiet. Nested captures each see every message.
    /// </summary>
    class log_capture {
        private:
            struct capture_sink;

            std::shared_ptr<capture_sink> mSink = nullptr;
            bool mPreviousConsoleEnabled = true;
            bool mErrorsAllowed = false;

        public:
            log_capture();
            ~log_capture();   // CHECKs that no ERROR or FATAL was logged unless allow_errors() was called

            log_capture(const log_capture &) = delete;
            log_capture &operator=(const log_capture &) = delete;

        public:
            /// <summary>
            /// True when a message at exactly this level contains the substring.
            /// </summary>
            [[nodiscard]] bool contains(core::log_level level, std::string_view substring) const;

            /// <summary>
            /// True when any captured message contains the substring.
            /// </summary>
            [[nodiscard]] bool contains(std::string_view substring) const;

            /// <summary>
            /// Number of messages captured at exactly this level.
            /// </summary>
            [[nodiscard]] u32 count(core::log_level level) const noexcept;

            /// <summary>
            /// Number of messages captured at any level.
            /// </summary>
            [[nodiscard]] u32 count() const noexcept;

            /// <summary>
            /// Snapshot of every captured message, oldest first.
            /// </summary>
            [[nodiscard]] std::vector<captured_log_entry> entries() const;

            /// <summary>
            /// Texts of every captured message, oldest first.
            /// </summary>
            [[nodiscard]] std::vector<std::string> messages() const;

            /// <summary>
            /// Texts of the captured messages at exactly this level, oldest first.
            /// </summary>
            [[nodiscard]] std::vector<std::string> messages(core::log_level level) const;

            /// <summary>
            /// Suppresses the destructor's "no error logged" check for a case that provokes errors
            /// on purpose; assert on contains() instead.
            /// </summary>
            void allow_errors() noexcept;

            /// <summary>
            /// True after allow_errors().
            /// </summary>
            [[nodiscard]] bool errors_allowed() const noexcept {
                return mErrorsAllowed;
            }

            /// <summary>
            /// Forgets every captured message.
            /// </summary>
            void clear();
    };

    namespace detail {
        /// <summary>
        /// Registers the process-wide ring-buffer sink test_main installs instead of the console
        /// sink. Entries hold fixed-size text so logging never allocates through the ring. Calling
        /// it again re-registers the same sink (after a test called log::clear_sinks()).
        /// </summary>
        void install_global_log_ring(usize capacity);

        /// <summary>
        /// Drops every entry in the ring.
        /// </summary>
        void clear_global_log_ring();

        /// <summary>
        /// The ring's entries, oldest first, formatted like the console sink does.
        /// </summary>
        [[nodiscard]] std::vector<std::string> global_log_ring_lines();

        /// <summary>
        /// Capacity passed to install_global_log_ring, 0 when not installed.
        /// </summary>
        [[nodiscard]] usize global_log_ring_capacity() noexcept;
    }
}
