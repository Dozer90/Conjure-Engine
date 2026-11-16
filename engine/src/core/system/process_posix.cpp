// POSIX backend of core::process: posix_spawnp with pipes, the child in its own process group so
// kill() and the timeout take the whole tree down, and one reader thread (a named
// jobs::dedicated_thread) that poll()s both pipes so a child writing to both streams never blocks
// on a full pipe.

#include "conjure/core/process.h"

#include "core/system/system_internal.h"
#include "conjure/core/log.h"
#include "conjure/core/platform_detect.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#if CONJURE_PLATFORM_MACOS
    #include <crt_externs.h>
#else
extern char **environ;
#endif

using namespace conjure::core;

namespace {
    // The parent's environment. macOS only exposes it to shared libraries through _NSGetEnviron.
    char **current_environment() noexcept {
#if CONJURE_PLATFORM_MACOS
        return *_NSGetEnviron();
#else
        return environ;
#endif
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Descriptors and errors

namespace {
    using steady_clock = std::chrono::steady_clock;

    class unique_fd {
        private:
            int mDescriptor = -1;

        public:
            unique_fd() = default;

            explicit unique_fd(int descriptor) noexcept :
                mDescriptor(descriptor) {}

            ~unique_fd() {
                reset();
            }

            unique_fd(const unique_fd &) = delete;
            unique_fd &operator=(const unique_fd &) = delete;

            unique_fd(unique_fd &&other) noexcept :
                mDescriptor(other.release()) {}

            unique_fd &operator=(unique_fd &&other) noexcept {
                if (this != &other) {
                    reset(other.release());
                }
                return *this;
            }

        public:
            [[nodiscard]] int get() const noexcept {
                return mDescriptor;
            }

            [[nodiscard]] bool is_valid() const noexcept {
                return mDescriptor >= 0;
            }

            [[nodiscard]] int release() noexcept {
                const int descriptor = mDescriptor;
                mDescriptor = -1;
                return descriptor;
            }

            void reset(int descriptor = -1) noexcept {
                if (mDescriptor >= 0) {
                    ::close(mDescriptor);
                }
                mDescriptor = descriptor;
            }
    };

    std::string errno_message(int code) {
        return std::system_category().message(code);
    }

    error_code error_code_from_errno(int code) noexcept {
        switch (code) {
            case ENOENT:
            case ENOTDIR: {
                return error_code::NOT_FOUND;
            }
            case EACCES:
            case EPERM: {
                return error_code::PERMISSION_DENIED;
            }
            default: {
                return error_code::BACKEND_FAILURE;
            }
        }
    }

    error errno_error(int code, std::string_view what) {
        return make_error(error_code_from_errno(code), "{}: {}", what, errno_message(code));
    }

    // A pipe whose write end the child receives through dup2 in the spawn file actions. Both
    // descriptors are close-on-exec from the moment they exist, so a child another thread spawns
    // at the same time cannot inherit them (and keep this child's reader waiting for end-of-file).
    result<void> create_pipe(unique_fd &readEnd, unique_fd &writeEnd) {
        std::array<int, 2> descriptors = {-1, -1};
#if CONJURE_PLATFORM_LINUX
        if (::pipe2(descriptors.data(), O_CLOEXEC) != 0) {
            return errno_error(errno, "cannot create pipe");
        }
        readEnd.reset(descriptors[0]);
        writeEnd.reset(descriptors[1]);
#else
        // No pipe2 on macOS: the flag is set right after, and POSIX_SPAWN_CLOEXEC_DEFAULT in the
        // spawn attributes closes the window in which a concurrent spawn could still inherit.
        if (::pipe(descriptors.data()) != 0) {
            return errno_error(errno, "cannot create pipe");
        }
        readEnd.reset(descriptors[0]);
        writeEnd.reset(descriptors[1]);

        if (::fcntl(readEnd.get(), F_SETFD, FD_CLOEXEC) != 0 || ::fcntl(writeEnd.get(), F_SETFD, FD_CLOEXEC) != 0) {
            return errno_error(errno, "cannot configure pipe");
        }
#endif
        return {};
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Arguments and environment

namespace {
    std::vector<std::string> build_arguments(const process_desc &desc) {
        std::vector<std::string> arguments = {};
        arguments.reserve(desc.arguments.size() + 1);
        arguments.push_back(desc.executable);
        for (const std::string &argument : desc.arguments) {
            arguments.push_back(argument);
        }
        return arguments;
    }

    // The file posix_spawnp receives. A name without a separator is searched in PATH; one with
    // a separator is made absolute here because the chdir file action runs in the child before
    // exec and would otherwise resolve it against workingDirectory, unlike the header's contract
    // (and CreateProcessW).
    std::string spawn_file(const process_desc &desc) {
        if (desc.executable.find('/') == std::string::npos) {
            return desc.executable;
        }
        std::error_code ignored = {};
        const std::filesystem::path absolute = std::filesystem::absolute(desc.executable, ignored);
        if (ignored) {
            return desc.executable;
        }
        return absolute.string();
    }

    // "NAME=value" entries: the inherited environment with the overrides applied.
    std::vector<std::string> build_environment(const std::vector<std::pair<std::string, std::string>> &overrides) {
        std::vector<std::string> entries = {};
        for (char **cursor = current_environment(); cursor != nullptr && *cursor != nullptr; ++cursor) {
            entries.emplace_back(*cursor);
        }

        for (const auto &[name, value] : overrides) {
            const std::string prefix = name + "=";
            const auto existing = std::find_if(entries.begin(), entries.end(), [&prefix](const std::string &entry) {
                return entry.compare(0, prefix.size(), prefix) == 0;
            });

            if (value.empty()) {
                if (existing != entries.end()) {
                    entries.erase(existing);
                }
                continue;
            }
            if (existing != entries.end()) {
                *existing = prefix + value;
            } else {
                entries.push_back(prefix + value);
            }
        }
        return entries;
    }

    // Null-terminated pointer array over owned strings, as execve-style calls expect.
    std::vector<char *> pointer_array(std::vector<std::string> &strings) {
        std::vector<char *> pointers = {};
        pointers.reserve(strings.size() + 1);
        for (std::string &text : strings) {
            pointers.push_back(text.data());
        }
        pointers.push_back(nullptr);
        return pointers;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Pipe reader

namespace {
    // Runs on its own thread until both pipes (or the one that is captured) reached end-of-file.
    void drain_pipes(int standardOutput, int standardError, detail::process_output *output) {
        std::array<char, 16 * 1024> chunk = {};
        std::array<pollfd, 2> watched = {};

        while (standardOutput >= 0 || standardError >= 0) {
            nfds_t count = 0;
            if (standardOutput >= 0) {
                watched[count] = pollfd{standardOutput, static_cast<short>(POLLIN), 0};
                ++count;
            }
            if (standardError >= 0) {
                watched[count] = pollfd{standardError, static_cast<short>(POLLIN), 0};
                ++count;
            }

            const int ready = ::poll(watched.data(), count, -1);
            if (ready < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return;
            }

            for (nfds_t index = 0; index < count; ++index) {
                const pollfd &entry = watched[index];
                if (entry.revents == 0) {
                    continue;
                }

                const ssize_t bytesRead = ::read(entry.fd, chunk.data(), chunk.size());
                if (bytesRead > 0) {
                    if (entry.fd == standardOutput) {
                        output->append_output(chunk.data(), static_cast<conjure::usize>(bytesRead));
                    } else {
                        output->append_error(chunk.data(), static_cast<conjure::usize>(bytesRead));
                    }
                    continue;
                }
                if (bytesRead < 0 && (errno == EINTR || errno == EAGAIN)) {
                    continue;
                }

                // End-of-file (every writer closed) or a failure: stop watching this pipe.
                if (entry.fd == standardOutput) {
                    standardOutput = -1;
                } else {
                    standardError = -1;
                }
            }
        }
    }

    // Everything start() hands to the state once the child exists.
    struct launch {
        pid_t processId = -1;
        unique_fd standardOutputRead = {};
        unique_fd standardErrorRead = {};
        conjure::f64 timeoutSeconds = 0;
    };
}


/*===========================================================================*/
/*===========================================================================*/
// process::state

class process::state {
    private:
        pid_t mProcessId = -1;
        unique_fd mStandardOutputRead = {};
        unique_fd mStandardErrorRead = {};
        detail::process_output mOutput = {};
        detail::completion_latch mReaderDone = {};
        // A named jobs::dedicated_thread (docs/design/10_core.md C4). Declared after the buffers
        // and the latch it writes to, so those outlive a reader that is still being joined.
        detail::pipe_reader mReader = {};
        steady_clock::time_point mStartTime = {};
        f64 mTimeoutSeconds = 0;
        // Read by is_running() and kill() from other threads while wait() runs.
        std::atomic<bool> mHasReader = false;
        std::atomic<bool> mFinished = false;
        std::atomic<bool> mExited = false;   // waitpid reaped the child; mWaitStatus is valid
        std::mutex mExitMutex = {};          // serialises the reapers; held while waitpid runs
        int mWaitStatus = 0;
        bool mStatusLost = false;            // written with mWaitStatus, before mExited is set

    public:
        explicit state(launch &&launched);
        ~state();

        state(const state &) = delete;
        state &operator=(const state &) = delete;

    public:
        [[nodiscard]] bool is_alive() noexcept;
        [[nodiscard]] u32 process_id() const noexcept;
        [[nodiscard]] detail::process_output &output() noexcept;
        [[nodiscard]] result<process_result> wait();
        void terminate() noexcept;

    private:
        void record_exit(int status) noexcept;
        void record_lost_exit() noexcept;
        [[nodiscard]] bool poll_exit() noexcept;
        void wait_exit() noexcept;
        void join_reader();
        [[nodiscard]] i32 exit_code() const noexcept;
};

process::state::state(launch &&launched) :
    mProcessId(launched.processId),
    mStandardOutputRead(std::move(launched.standardOutputRead)),
    mStandardErrorRead(std::move(launched.standardErrorRead)),
    mTimeoutSeconds(detail::clamp_timeout(launched.timeoutSeconds)) {
    if (mStandardOutputRead.is_valid() || mStandardErrorRead.is_valid()) {
        mHasReader.store(true);
        mReader.start(detail::COMBINED_READER_NAME, mReaderDone, [this] {
            drain_pipes(mStandardOutputRead.get(), mStandardErrorRead.get(), &mOutput);
        });
    }
    mStartTime = steady_clock::now();
}

process::state::~state() {
    // Destruction means the owner wants nothing more from the child: take the whole tree down
    // first, because the reader only finishes once every holder of the pipes is gone.
    if (mProcessId >= 0 && !mFinished.load()) {
        terminate();
        if (!mExited.load()) {
            wait_exit();
        }
    }
    join_reader();
}

bool process::state::is_alive() noexcept {
    if (mProcessId < 0 || mFinished.load()) {
        return false;
    }
    return !poll_exit();
}

conjure::u32 process::state::process_id() const noexcept {
    if (mProcessId < 0) {
        return 0;
    }
    return static_cast<u32>(mProcessId);
}

detail::process_output &process::state::output() noexcept {
    return mOutput;
}

result<process_result> process::state::wait() {
    if (mProcessId < 0) {
        return error(error_code::INVALID_STATE, "process::wait: the process was never started");
    }
    if (mFinished.load()) {
        return error(error_code::INVALID_STATE, "process::wait: already waited for this process");
    }

    const bool hasDeadline = mTimeoutSeconds > 0.0;
    steady_clock::time_point deadline = {};
    bool timedOut = false;
    if (hasDeadline) {
        deadline = detail::timeout_deadline(mStartTime, mTimeoutSeconds);
        while (!poll_exit()) {
            if (steady_clock::now() >= deadline) {
                timedOut = true;
                terminate();
                wait_exit();
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    } else {
        wait_exit();
    }

    // The reader ends when the last writer closes, which includes anything the child spawned and
    // handed the pipe to. The timeout bounds that too: past the deadline the tree is killed.
    if (hasDeadline) {
        if (!mReaderDone.wait_until(deadline)) {
            timedOut = true;
            terminate();
        }
    } else {
        mReaderDone.wait();
    }
    join_reader();

    process_result finished = {};
    finished.exitCode = exit_code();
    finished.timedOut = timedOut;
    finished.elapsedSeconds = std::chrono::duration<f64>(steady_clock::now() - mStartTime).count();
    mOutput.take_output(finished.standardOutput);
    mOutput.take_error(finished.standardError);
    mFinished.store(true);
    return finished;
}

void process::state::terminate() noexcept {
    if (mProcessId < 0 || mFinished.load()) {
        return;
    }

    // The child leads its own process group (POSIX_SPAWN_SETPGROUP), so the negative id reaches
    // every process it spawned, also after the child itself was reaped: a group id stays
    // reserved while any member lives. Once the child is gone and nothing holds the pipes any
    // more there is nothing observable left to kill, and signalling the id could hit a process
    // that reused it.
    const bool childAlive = !mExited.load();
    const bool pipesOpen = mHasReader.load() && !mReaderDone.is_complete();
    if (!childAlive && !pipesOpen) {
        return;
    }
    if (::kill(-mProcessId, SIGKILL) != 0 && childAlive) {
        ::kill(mProcessId, SIGKILL);
    }
}

// Only one reaper may run waitpid at a time: is_running() on one thread and wait() on another
// would otherwise race, and the loser (ECHILD) could publish a made-up status before the winner
// records the real one. The mutex is held across the waitpid call itself.
void process::state::record_exit(int status) noexcept {
    mWaitStatus = status;
    mExited.store(true);
}

// ECHILD with the lock held: nobody else in this object reaped the child, so it is gone without a
// status (SIGCHLD ignored by the host, or reaped elsewhere). KILLED_EXIT_CODE is reported rather than a
// made-up success, so a failing tool never reads as one that passed.
void process::state::record_lost_exit() noexcept {
    mStatusLost = true;
    record_exit(0);
    CONJURE_LOG_WARN(LOG_CORE, "process {}: the exit status was lost (SIGCHLD ignored?)", mProcessId);
}

bool process::state::poll_exit() noexcept {
    if (mExited.load()) {
        return true;
    }

    std::unique_lock lock(mExitMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        // A blocking waiter owns the child; it publishes the status when the child goes.
        return mExited.load();
    }
    if (mExited.load()) {
        return true;
    }

    int status = 0;
    const pid_t reaped = ::waitpid(mProcessId, &status, WNOHANG);
    if (reaped == mProcessId) {
        record_exit(status);
        return true;
    }
    if (reaped < 0 && errno != EINTR) {
        record_lost_exit();
        return true;
    }
    return false;
}

void process::state::wait_exit() noexcept {
    const std::scoped_lock lock(mExitMutex);
    while (!mExited.load()) {
        int status = 0;
        const pid_t reaped = ::waitpid(mProcessId, &status, 0);
        if (reaped == mProcessId) {
            record_exit(status);
        } else if (reaped < 0 && errno != EINTR) {
            record_lost_exit();
        }
    }
}

void process::state::join_reader() {
    mReader.join();
}

i32 process::state::exit_code() const noexcept {
    if (mStatusLost) {
        return process::KILLED_EXIT_CODE;
    }
    if (WIFEXITED(mWaitStatus)) {
        return static_cast<i32>(WEXITSTATUS(mWaitStatus));
    }
    if (WIFSIGNALED(mWaitStatus)) {
        const int signalNumber = WTERMSIG(mWaitStatus);
        if (signalNumber == SIGKILL) {
            return process::KILLED_EXIT_CODE;
        }
        return -signalNumber;
    }
    return process::KILLED_EXIT_CODE;
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

// Every special member that destroys the state is defined here, where state is complete.

process::process() {}

process::~process() {}

process::process(process &&other) noexcept :
    mState(std::move(other.mState)) {}

process &process::operator=(process &&other) noexcept {
    if (this != &other) {
        mState = std::move(other.mState);
    }
    return *this;
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

result<process> process::start(const process_desc &desc) {
    CONJURE_TRY(detail::validate_process_desc(desc));

    unique_fd childStandardOutput = {};
    unique_fd childStandardError = {};
    launch launched = {};
    launched.timeoutSeconds = desc.timeoutSeconds;
    if (desc.captureStdout) {
        CONJURE_TRY(create_pipe(launched.standardOutputRead, childStandardOutput));
    }
    if (!desc.mergeStderr && desc.captureStderr) {
        CONJURE_TRY(create_pipe(launched.standardErrorRead, childStandardError));
    }

    std::string workingDirectory = {};
    if (!desc.workingDirectory.empty()) {
        std::error_code ignored = {};
        workingDirectory = std::filesystem::absolute(desc.workingDirectory, ignored).string();
    }

    posix_spawn_file_actions_t fileActions = {};
    posix_spawn_file_actions_init(&fileActions);
    posix_spawn_file_actions_addopen(&fileActions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    if (desc.captureStdout) {
        posix_spawn_file_actions_adddup2(&fileActions, childStandardOutput.get(), STDOUT_FILENO);
    }
    if (desc.mergeStderr) {
        posix_spawn_file_actions_adddup2(&fileActions, childStandardOutput.get(), STDERR_FILENO);
    } else if (desc.captureStderr) {
        posix_spawn_file_actions_adddup2(&fileActions, childStandardError.get(), STDERR_FILENO);
    }
#if CONJURE_PLATFORM_MACOS
    // With POSIX_SPAWN_CLOEXEC_DEFAULT only descriptors named in the file actions survive into
    // the child, so the inherited standard streams have to be listed explicitly.
    if (!desc.captureStdout) {
        posix_spawn_file_actions_addinherit_np(&fileActions, STDOUT_FILENO);
    }
    if (!desc.mergeStderr && !desc.captureStderr) {
        posix_spawn_file_actions_addinherit_np(&fileActions, STDERR_FILENO);
    }
#endif
#if CONJURE_SPAWN_CLOSEFROM_AVAILABLE
    // Linux has no POSIX_SPAWN_CLOEXEC_DEFAULT, so every descriptor above the standard three is closed in
    // the child, as macOS and the Windows handle list do. The action runs after the dup2s, so the pipe
    // ends already sit on 1 and 2. A C library without the call (glibc before 2.34, musl) keeps only
    // close-on-exec descriptors out.
    const int closeError = posix_spawn_file_actions_addclosefrom_np(&fileActions, STDERR_FILENO + 1);
    if (closeError != 0) {
        posix_spawn_file_actions_destroy(&fileActions);
        return errno_error(closeError, "cannot configure the spawn file actions");
    }
#endif
    if (!workingDirectory.empty()) {
        posix_spawn_file_actions_addchdir_np(&fileActions, workingDirectory.c_str());
    }

    // Its own process group so kill() can address the whole tree; an empty signal mask and
    // default dispositions so the child does not inherit what the engine blocks or ignores on
    // the spawning thread (job threads, SIGPIPE for networking).
    posix_spawnattr_t attributes = {};
    posix_spawnattr_init(&attributes);
    short spawnFlags = POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF;
#if CONJURE_PLATFORM_MACOS
    spawnFlags = static_cast<short>(spawnFlags | POSIX_SPAWN_CLOEXEC_DEFAULT);
#endif
    posix_spawnattr_setflags(&attributes, spawnFlags);
    posix_spawnattr_setpgroup(&attributes, 0);

    sigset_t emptySet = {};
    sigemptyset(&emptySet);
    posix_spawnattr_setsigmask(&attributes, &emptySet);

    sigset_t defaultSet = {};
    sigfillset(&defaultSet);
    sigdelset(&defaultSet, SIGKILL);
    sigdelset(&defaultSet, SIGSTOP);
    posix_spawnattr_setsigdefault(&attributes, &defaultSet);

    const std::string file = spawn_file(desc);
    std::vector<std::string> argumentStrings = build_arguments(desc);
    std::vector<char *> argv = pointer_array(argumentStrings);
    std::vector<std::string> environmentStrings = {};
    char **envp = current_environment();
    std::vector<char *> environmentPointers = {};
    if (!desc.environment.empty()) {
        environmentStrings = build_environment(desc.environment);
        environmentPointers = pointer_array(environmentStrings);
        envp = environmentPointers.data();
    }

    pid_t processId = -1;
    const int spawnError = ::posix_spawnp(&processId, file.c_str(), &fileActions, &attributes, argv.data(), envp);
    posix_spawnattr_destroy(&attributes);
    posix_spawn_file_actions_destroy(&fileActions);
    if (spawnError != 0) {
        return make_error(
          error_code_from_errno(spawnError),
          "cannot start '{}': {}",
          desc.executable,
          errno_message(spawnError)
        );
    }
    launched.processId = processId;

    // Close this side's copies of the child's ends so the pipes report end-of-file when the
    // child exits.
    childStandardOutput.reset();
    childStandardError.reset();

    process started = {};
    started.mState = std::make_unique<state>(std::move(launched));
    return started;
}

bool process::is_running() const noexcept {
    return mState != nullptr && mState->is_alive();
}

result<process_result> process::wait() {
    if (mState == nullptr) {
        return error(error_code::INVALID_STATE, "process::wait: the process was never started");
    }
    return mState->wait();
}

void process::kill() noexcept {
    if (mState == nullptr) {
        return;
    }
    mState->terminate();
}

conjure::usize process::read_output(std::string &out) {
    if (mState == nullptr) {
        return 0;
    }
    return mState->output().take_output(out);
}

conjure::usize process::read_error(std::string &out) {
    if (mState == nullptr) {
        return 0;
    }
    return mState->output().take_error(out);
}

conjure::u32 process::process_id() const noexcept {
    if (mState == nullptr) {
        return 0;
    }
    return mState->process_id();
}
