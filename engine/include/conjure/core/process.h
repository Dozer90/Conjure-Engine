#pragma once

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Child processes.
//
//   process_desc desc = {};
//   desc.executable = "dotnet";
//   desc.arguments = {"build", "Game.csproj"};
//   desc.timeoutSeconds = 120.0;
//   result<process_result> built = process::run(desc);
//
// Arguments are passed as an array, never through a shell, so callers do not quote anything; the
// Windows backend applies the MSVCRT quoting rules so the child's argv matches exactly. Standard
// output and standard error are drained concurrently while the child runs, so a child producing
// large amounts of output on both streams never deadlocks. The child's standard input is the null
// device.

namespace conjure::core {

    /// <summary>
    /// Everything needed to launch a child process.
    /// </summary>
    struct process_desc {
        /// <summary>
        /// Path of the program, or a bare name searched in the inherited PATH (and, on Windows,
        /// the parent's directory). A relative path is resolved against the parent's current
        /// directory, not against workingDirectory.
        /// </summary>
        std::string executable = {};

        /// <summary>
        /// argv[1..] of the child, unquoted.
        /// </summary>
        std::vector<std::string> arguments = {};

        /// <summary>
        /// Current directory of the child. Empty inherits the parent's. Must exist.
        /// </summary>
        std::filesystem::path workingDirectory = {};

        /// <summary>
        /// Variables added to, or overriding, the inherited environment. A pair with an empty
        /// value removes the variable. Names are case-insensitive on Windows; an empty name or
        /// one containing '=' is INVALID_ARGUMENT.
        /// </summary>
        std::vector<std::pair<std::string, std::string>> environment = {};

        /// <summary>
        /// Capture the child's standard output into process_result::standardOutput. When false
        /// the child inherits the parent's standard output.
        /// </summary>
        bool captureStdout = true;

        /// <summary>
        /// Capture the child's standard error into process_result::standardError. When false
        /// the child inherits the parent's standard error.
        /// </summary>
        bool captureStderr = true;

        /// <summary>
        /// Send the child's standard error into the standard output stream so the two are
        /// interleaved in process_result::standardOutput. Requires captureStdout.
        /// </summary>
        bool mergeStderr = false;

        /// <summary>
        /// Wall-clock limit for the whole run, including the time the child's descendants keep
        /// its captured output open. When exceeded the child (and every process it spawned) is
        /// killed and process_result::timedOut is set. 0 means no limit. Must be finite and not
        /// negative; values above process::MAX_TIMEOUT_SECONDS behave like that maximum.
        /// </summary>
        f64 timeoutSeconds = 0;
    };

    /// <summary>
    /// What a finished child produced.
    /// </summary>
    struct process_result {
        /// <summary>
        /// The child's exit code. process::KILLED_EXIT_CODE when it was killed by kill() or by
        /// the timeout; on POSIX a death by another signal reports the negated signal number.
        /// A child that exited on its own keeps its code even when the timeout later kills the
        /// descendants it left behind (timedOut is set in that case too).
        /// </summary>
        i32 exitCode = 0;

        /// <summary>
        /// Captured standard output not yet consumed through read_output(), byte for byte.
        /// </summary>
        std::string standardOutput = {};

        /// <summary>
        /// Captured standard error not yet consumed through read_error(), byte for byte.
        /// </summary>
        std::string standardError = {};

        /// <summary>
        /// True when the child was killed because process_desc::timeoutSeconds elapsed.
        /// </summary>
        bool timedOut = false;

        /// <summary>
        /// Seconds between the launch and the child's exit as observed by wait().
        /// </summary>
        f64 elapsedSeconds = 0;
    };

    /// <summary>
    /// A child process started by start(), or run to completion by run(). Move-only. Destroying a
    /// process that was not waited for kills the child and its process tree first, so an engine
    /// never leaks a tool it launched and never blocks on a grandchild that kept the output open.
    ///
    /// Threading: read_output(), read_error(), is_running() and kill() may be called from any
    /// thread, including while another thread blocks in wait(). start(), wait() and destruction
    /// belong to one thread at a time.
    /// </summary>
    class process {
        private:
            class state;
            std::unique_ptr<state> mState = nullptr;

        public:
            /// <summary>
            /// Exit code reported for a child killed by kill() or by the timeout.
            /// </summary>
            static constexpr i32 KILLED_EXIT_CODE = -1;

            /// <summary>
            /// Largest timeout honoured (about 31 years); larger finite values are clamped to it.
            /// </summary>
            static constexpr f64 MAX_TIMEOUT_SECONDS = 1.0e9;

        public:
            process();
            ~process();

            process(const process &) = delete;
            process &operator=(const process &) = delete;

            process(process &&other) noexcept;
            process &operator=(process &&other) noexcept;

        public:
            /// <summary>
            /// Starts the child and blocks until it exits or the timeout kills it. Fails with
            /// INVALID_ARGUMENT for an inconsistent description, NOT_FOUND when the executable
            /// or working directory does not exist, PERMISSION_DENIED or BACKEND_FAILURE when the
            /// operating system refuses to start it. A non-zero exit code is not an error.
            /// </summary>
            [[nodiscard]] static result<process_result> run(const process_desc &desc);

            /// <summary>
            /// Starts the child and returns immediately. Same failures as run().
            /// </summary>
            [[nodiscard]] static result<process> start(const process_desc &desc);

            /// <summary>
            /// True while the child has not exited and wait() has not been called.
            /// </summary>
            [[nodiscard]] bool is_running() const noexcept;

            /// <summary>
            /// Blocks until the child exits and its captured output is drained (killing the whole
            /// tree when the description's timeout elapses first), then returns its exit code and
            /// the output not yet consumed by read_output() / read_error(). Without a timeout a
            /// descendant that inherited the output pipe delays the return until it closes it.
            /// Fails with INVALID_STATE when the process was never started or wait() was already
            /// called.
            /// </summary>
            [[nodiscard]] result<process_result> wait();

            /// <summary>
            /// Kills the child and every process it spawned, including descendants that outlived
            /// the child itself. No-op once wait() returned. wait() still has to be called to
            /// collect the result.
            /// </summary>
            void kill() noexcept;

            /// <summary>
            /// Appends the standard output captured since the previous call and returns the
            /// number of bytes appended. Never blocks.
            /// </summary>
            usize read_output(std::string &out);

            /// <summary>
            /// Appends the standard error captured since the previous call and returns the
            /// number of bytes appended. Never blocks.
            /// </summary>
            usize read_error(std::string &out);

            /// <summary>
            /// The operating system's identifier of the child, 0 when not started.
            /// </summary>
            [[nodiscard]] u32 process_id() const noexcept;
    };
}
