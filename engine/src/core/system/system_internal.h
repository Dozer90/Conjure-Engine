#pragma once

// Helpers shared by the platform backends of the system area. Private to engine/src/core/system.

#include "conjure/core/file_lock.h"
#include "conjure/core/jobs/dedicated_thread.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/process.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

// 1 when the POSIX backend can close every descriptor above the standard three in the child
// (posix_spawn_file_actions_addclosefrom_np, glibc 2.34 or later on Linux). The standard headers above
// define __GLIBC__ through <features.h> on a glibc system. The process tests read it too, so the guard
// and the test expectation cannot drift apart.
#if CONJURE_PLATFORM_LINUX && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 34))
    #define CONJURE_SPAWN_CLOSEFROM_AVAILABLE 1
#else
    #define CONJURE_SPAWN_CLOSEFROM_AVAILABLE 0
#endif

namespace conjure::core::detail {

    /// <summary>
    /// UTF-8 text of a path without the conversion failures std::filesystem::path::string() can
    /// raise on Windows for characters outside the active code page.
    /// </summary>
    [[nodiscard]] inline std::string path_to_utf8(const std::filesystem::path &path) {
        const std::u8string text = path.u8string();
        return std::string(reinterpret_cast<const char *>(text.data()), text.size());
    }

    /// <summary>
    /// Checks the platform-independent parts of a process description: executable set, no NUL in
    /// the executable, an argument or an environment entry, timeout finite and not negative,
    /// environment names non-empty and free of '=', mergeStderr only together with captureStdout,
    /// working directory existing.
    /// </summary>
    [[nodiscard]] status validate_process_desc(const process_desc &desc);

    /// <summary>
    /// The error code for a failed dlopen of a bare name (no directory part), where only the loader's
    /// text tells what went wrong. NOT_FOUND only when the "no such file" / "image not found" text is
    /// about 'requested' itself; a symbol error, dyld's "Library not loaded", a dyld search location
    /// holding a file of the wrong architecture or with an invalid code signature, and a glibc "cannot
    /// open shared object file" naming another object (a missing dependency) are BACKEND_FAILURE, as is
    /// anything else. Pure, so every platform can test it.
    /// </summary>
    [[nodiscard]] error_code classify_bare_name_load_failure(
      std::string_view requested,
      std::string_view loaderMessage
    );

    /// <summary>
    /// The timeout a backend works with: process_desc::timeoutSeconds limited to
    /// process::MAX_TIMEOUT_SECONDS so converting it to a clock duration cannot overflow.
    /// </summary>
    [[nodiscard]] f64 clamp_timeout(f64 timeoutSeconds) noexcept;

    /// <summary>
    /// The moment a run started at 'start' with the given (clamped, positive) timeout must be
    /// over.
    /// </summary>
    [[nodiscard]] std::chrono::steady_clock::time_point timeout_deadline(
      std::chrono::steady_clock::time_point start,
      f64 timeoutSeconds
    ) noexcept;

    /// <summary>
    /// The captured output of a child, filled by the pipe reader and drained by the owner.
    /// Thread-safe.
    /// </summary>
    class process_output {
        private:
            mutable std::mutex mMutex = {};
            std::string mStandardOutput = {};
            std::string mStandardError = {};

        public:
            process_output() = default;
            ~process_output() = default;

            process_output(const process_output &) = delete;
            process_output &operator=(const process_output &) = delete;

        public:
            void append_output(const char *data, usize size);
            void append_error(const char *data, usize size);

            /// <summary>
            /// Moves everything captured so far to the end of 'out'; returns the byte count.
            /// </summary>
            usize take_output(std::string &out);
            usize take_error(std::string &out);
    };

    /// <summary>
    /// Counts the pipe readers of a child that are still running, so the owner can wait for
    /// them with a deadline: a grandchild holding the pipe must not stall wait() past the
    /// timeout. Thread-safe.
    /// </summary>
    class completion_latch {
        private:
            mutable std::mutex mMutex = {};
            std::condition_variable mCondition = {};
            u32 mPending = 0;

        public:
            completion_latch() = default;
            ~completion_latch() = default;

            completion_latch(const completion_latch &) = delete;
            completion_latch &operator=(const completion_latch &) = delete;

        public:
            /// <summary>
            /// Registers one more party that has to arrive. Call before that party starts.
            /// </summary>
            void expect_one();

            /// <summary>
            /// One party finished.
            /// </summary>
            void arrive();

            /// <summary>
            /// True when every expected party has arrived (trivially true when none was expected).
            /// </summary>
            [[nodiscard]] bool is_complete() const;

            /// <summary>
            /// Blocks until every expected party has arrived.
            /// </summary>
            void wait();

            /// <summary>
            /// Blocks until every expected party has arrived or the deadline passes; returns
            /// whether they all arrived.
            /// </summary>
            [[nodiscard]] bool wait_until(std::chrono::steady_clock::time_point deadline);
    };

    /// <summary>
    /// Name of the thread that drains a child's standard output on Windows, as debuggers and
    /// profilers show it. Like the two names below it fits the 15 bytes Linux keeps for a thread
    /// name, so no platform cuts it short.
    /// </summary>
    inline constexpr std::string_view STANDARD_OUTPUT_READER_NAME = "process-stdout";

    /// <summary>
    /// Name of the thread that drains a child's standard error on Windows.
    /// </summary>
    inline constexpr std::string_view STANDARD_ERROR_READER_NAME = "process-stderr";

    /// <summary>
    /// Name of the one thread that drains both pipes of a child on POSIX.
    /// </summary>
    inline constexpr std::string_view COMBINED_READER_NAME = "process-pipes";

    /// <summary>
    /// The thread that drains the captured pipes of a child: a named jobs::dedicated_thread, which
    /// is the only way core::system creates a thread (docs/design/10_core.md C2 and C4). Both
    /// process backends start their readers through this class. The thread ends when its drain
    /// function returns, which is when every writer of the pipe is gone: a stop request cannot
    /// interrupt a blocking read, so the owner ends a reader by killing the writers. Not
    /// thread-safe: start(), join() and destruction belong to one thread, as process::start(),
    /// wait() and destruction do.
    /// </summary>
    class pipe_reader {
        private:
            std::optional<jobs::dedicated_thread> mThread = {};

        public:
            pipe_reader() = default;

            /// <summary>
            /// Joins a reader that is still running; the owner makes sure its pipe has closed.
            /// </summary>
            ~pipe_reader() = default;

            pipe_reader(const pipe_reader &) = delete;
            pipe_reader &operator=(const pipe_reader &) = delete;

            pipe_reader(pipe_reader &&) = delete;
            pipe_reader &operator=(pipe_reader &&) = delete;

        public:
            /// <summary>
            /// Registers one party with 'done' and starts a thread called 'name' that runs 'drain'
            /// and then arrives at 'done'. 'done' and everything 'drain' touches must outlive the
            /// thread. An empty 'drain' and a second call both assert and start nothing. When the
            /// operating system refuses the thread the process ends, as for every dedicated_thread
            /// (docs/CONVENTIONS.md section 3).
            /// </summary>
            void start(std::string_view name, completion_latch &done, std::function<void()> drain);

            /// <summary>
            /// True from start() until join() has returned.
            /// </summary>
            [[nodiscard]] bool joinable() const noexcept;

            /// <summary>
            /// Waits for the drain function to return. No-op when never started or already joined.
            /// </summary>
            void join();
    };

    /// <summary>
    /// How many pipe_reader threads this process has started. A statistic: the process tests read
    /// it to see that a backend drains each captured pipe through a pipe_reader, the one place
    /// core::system creates a thread, and through nothing else. It only ever grows. Thread-safe.
    /// </summary>
    [[nodiscard]] u64 started_pipe_reader_count() noexcept;

    /// <summary>
    /// The native value of a file_lock that holds nothing: INVALID_HANDLE_VALUE on Windows, no descriptor on POSIX.
    /// </summary>
    inline constexpr std::intptr_t FILE_LOCK_NOT_HELD = -1;

    /// <summary>
    /// The platform half of file_lock::try_acquire (file_lock_windows.cpp, file_lock_posix.cpp), called with
    /// arguments already checked: the open, locked handle or descriptor, or FILE_LOCK_NOT_HELD when another holder
    /// has the file. Never removes a file.
    /// </summary>
    [[nodiscard]] result<std::intptr_t> file_lock_open_native(const std::filesystem::path &path, file_lock_open open);

    /// <summary>
    /// Unlocks and closes a handle or descriptor file_lock_open_native returned.
    /// </summary>
    void file_lock_close_native(std::intptr_t native) noexcept;

    /// <summary>
    /// Deletes the file 'native' holds through 'path', then unlocks and closes 'native' on every outcome.
    /// </summary>
    [[nodiscard]] result<void> file_lock_remove_native(std::intptr_t native, const std::filesystem::path &path);
}
