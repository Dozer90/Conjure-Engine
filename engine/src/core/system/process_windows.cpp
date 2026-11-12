// Windows backend of core::process: CreateProcessW with anonymous pipes, a job object so kill()
// and the timeout take the whole process tree down, and one reader thread per captured pipe so
// a child writing to both streams never blocks on a full pipe.

#include "conjure/core/process.h"

#include "core/system/system_internal.h"
#include "core/system/win32_utils.h"
#include "conjure/core/log.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// Handles

namespace {
    using steady_clock = std::chrono::steady_clock;

    class unique_handle {
        private:
            HANDLE mHandle = nullptr;

        public:
            unique_handle() = default;

            explicit unique_handle(HANDLE handle) noexcept :
                mHandle(handle) {}

            ~unique_handle() {
                reset();
            }

            unique_handle(const unique_handle &) = delete;
            unique_handle &operator=(const unique_handle &) = delete;

            unique_handle(unique_handle &&other) noexcept :
                mHandle(other.release()) {}

            unique_handle &operator=(unique_handle &&other) noexcept {
                if (this != &other) {
                    reset(other.release());
                }
                return *this;
            }

        public:
            [[nodiscard]] HANDLE get() const noexcept {
                return mHandle;
            }

            [[nodiscard]] bool is_valid() const noexcept {
                return mHandle != nullptr && mHandle != INVALID_HANDLE_VALUE;
            }

            [[nodiscard]] HANDLE release() noexcept {
                HANDLE handle = mHandle;
                mHandle = nullptr;
                return handle;
            }

            void reset(HANDLE handle = nullptr) noexcept {
                if (is_valid()) {
                    CloseHandle(mHandle);
                }
                mHandle = handle;
            }
    };

    error last_error(std::string_view what) {
        const DWORD code = GetLastError();
        return make_error(detail::error_code_from_win32(code), "{}: {}", what, detail::win32_error_message(code));
    }

    SECURITY_ATTRIBUTES inheritable_attributes() noexcept {
        SECURITY_ATTRIBUTES attributes = {};
        attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
        attributes.lpSecurityDescriptor = nullptr;
        attributes.bInheritHandle = TRUE;
        return attributes;
    }

    // An anonymous pipe whose write end the child inherits. The read end stays private.
    result<void> create_pipe(unique_handle &readEnd, unique_handle &writeEnd) {
        SECURITY_ATTRIBUTES attributes = inheritable_attributes();
        HANDLE rawRead = nullptr;
        HANDLE rawWrite = nullptr;
        if (!CreatePipe(&rawRead, &rawWrite, &attributes, 0)) {
            return last_error("cannot create pipe");
        }
        readEnd.reset(rawRead);
        writeEnd.reset(rawWrite);

        if (!SetHandleInformation(readEnd.get(), HANDLE_FLAG_INHERIT, 0)) {
            return last_error("cannot configure pipe");
        }
        return {};
    }

    result<unique_handle> open_null_device(DWORD access) {
        SECURITY_ATTRIBUTES attributes = inheritable_attributes();
        unique_handle device(CreateFileW(
          L"NUL",
          access,
          FILE_SHARE_READ | FILE_SHARE_WRITE,
          &attributes,
          OPEN_EXISTING,
          0,
          nullptr
        ));
        if (!device.is_valid()) {
            return last_error("cannot open the null device");
        }
        return device;
    }

    // An inheritable copy of one of the parent's standard handles, or the null device when the
    // parent has none (GUI processes).
    result<unique_handle> inheritable_std_handle(DWORD standardHandleId, DWORD nullAccess) {
        HANDLE original = GetStdHandle(standardHandleId);
        if (original == nullptr || original == INVALID_HANDLE_VALUE) {
            return open_null_device(nullAccess);
        }

        HANDLE duplicate = nullptr;
        const BOOL duplicated = DuplicateHandle(
          GetCurrentProcess(),
          original,
          GetCurrentProcess(),
          &duplicate,
          0,
          TRUE,
          DUPLICATE_SAME_ACCESS
        );
        if (!duplicated) {
            return last_error("cannot duplicate standard handle");
        }
        return unique_handle(duplicate);
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Command line and environment

namespace {
    // Quotes one argument so that the MSVCRT command-line parser in the child reproduces it
    // exactly: backslashes are literal except when they precede a double quote.
    void append_quoted_argument(std::wstring &commandLine, std::wstring_view argument) {
        const bool needsQuotes = argument.empty() || argument.find_first_of(L" \t\n\v\"") != std::wstring_view::npos;
        if (!needsQuotes) {
            commandLine.append(argument);
            return;
        }

        commandLine.push_back(L'"');
        for (std::wstring_view::const_iterator cursor = argument.begin();; ++cursor) {
            std::size_t backslashes = 0;
            while (cursor != argument.end() && *cursor == L'\\') {
                ++cursor;
                ++backslashes;
            }

            if (cursor == argument.end()) {
                // Backslashes before the closing quote must be doubled so the quote stays a
                // delimiter.
                commandLine.append(backslashes * 2, L'\\');
                break;
            }

            if (*cursor == L'"') {
                commandLine.append(backslashes * 2 + 1, L'\\');
            } else {
                commandLine.append(backslashes, L'\\');
            }
            commandLine.push_back(*cursor);
        }
        commandLine.push_back(L'"');
    }

    bool equals_ascii_ignore_case(std::string_view left, std::string_view right) noexcept {
        const auto lower = [](char character) {
            return character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a') : character;
        };
        return std::ranges::equal(left, right, [&lower](char leftCharacter, char rightCharacter) {
            return lower(leftCharacter) == lower(rightCharacter);
        });
    }

    // CreateProcessW runs a .bat or .cmd target through cmd.exe, which parses the command line again with
    // its own rules ('&', '|', '%', '^' ...): the MSVCRT quoting below cannot keep an argument intact
    // there, and process_desc promises that nothing goes through a shell. Win32 drops trailing dots and
    // spaces from a file name, so "tool.cmd." still names a batch file.
    bool is_batch_file(std::string_view executable) noexcept {
        const conjure::usize separator = executable.find_last_of("/\\");
        std::string_view name = separator == std::string_view::npos ? executable : executable.substr(separator + 1);
        while (!name.empty() && (name.back() == '.' || name.back() == ' ')) {
            name.remove_suffix(1);
        }

        const conjure::usize dot = name.rfind('.');
        if (dot == std::string_view::npos) {
            return false;
        }
        const std::string_view extension = name.substr(dot + 1);
        return equals_ascii_ignore_case(extension, "bat") || equals_ascii_ignore_case(extension, "cmd");
    }

    std::wstring build_command_line(const process_desc &desc) {
        // argv[0] follows simpler rules than the other arguments (no backslash escaping), so it is
        // just wrapped in quotes, which also lets CreateProcessW find a path containing spaces.
        std::wstring commandLine = L"\"";
        commandLine.append(detail::to_wide(desc.executable));
        commandLine.push_back(L'"');

        for (const std::string &argument : desc.arguments) {
            commandLine.push_back(L' ');
            append_quoted_argument(commandLine, detail::to_wide(argument));
        }
        return commandLine;
    }

    // Environment variable names are case-insensitive on Windows, for every character the
    // system knows how to fold, not only ASCII.
    int compare_names(std::wstring_view left, std::wstring_view right) noexcept {
        const int order = CompareStringOrdinal(
          left.data(),
          static_cast<int>(left.size()),
          right.data(),
          static_cast<int>(right.size()),
          TRUE
        );
        if (order == CSTR_LESS_THAN) {
            return -1;
        }
        if (order == CSTR_GREATER_THAN) {
            return 1;
        }
        return 0;
    }

    struct environment_variable {
        std::wstring name = {};
        std::wstring value = {};
    };

    std::vector<environment_variable> inherited_environment() {
        std::vector<environment_variable> variables = {};
        LPWCH block = GetEnvironmentStringsW();
        if (block == nullptr) {
            return variables;
        }

        const wchar_t *cursor = block;
        while (*cursor != L'\0') {
            const std::wstring_view entry(cursor);
            cursor += entry.size() + 1;

            // Hidden drive-letter entries look like "=C:=C:\dir"; their name starts with '='.
            const std::size_t searchStart = entry.starts_with(L'=') ? 1 : 0;
            const std::size_t separator = entry.find(L'=', searchStart);
            if (separator == std::wstring_view::npos) {
                continue;
            }
            variables.push_back({std::wstring(entry.substr(0, separator)), std::wstring(entry.substr(separator + 1))});
        }
        FreeEnvironmentStringsW(block);
        return variables;
    }

    // Empty when there is nothing to override, meaning "inherit" to CreateProcessW.
    std::vector<wchar_t> build_environment_block(const std::vector<std::pair<std::string, std::string>> &overrides) {
        std::vector<wchar_t> block = {};
        if (overrides.empty()) {
            return block;
        }

        std::vector<environment_variable> variables = inherited_environment();
        for (const auto &[name, value] : overrides) {
            const std::wstring wideName = detail::to_wide(name);
            const auto existing = std::find_if(
              variables.begin(),
              variables.end(),
              [&wideName](const environment_variable &variable) {
                  return compare_names(variable.name, wideName) == 0;
              }
            );

            if (value.empty()) {
                if (existing != variables.end()) {
                    variables.erase(existing);
                }
                continue;
            }
            if (existing != variables.end()) {
                existing->value = detail::to_wide(value);
            } else {
                variables.push_back({wideName, detail::to_wide(value)});
            }
        }

        // The documented block format is sorted case-insensitively by name.
        std::sort(
          variables.begin(),
          variables.end(),
          [](const environment_variable &left, const environment_variable &right) {
              return compare_names(left.name, right.name) < 0;
          }
        );

        for (const environment_variable &variable : variables) {
            block.insert(block.end(), variable.name.begin(), variable.name.end());
            block.push_back(L'=');
            block.insert(block.end(), variable.value.begin(), variable.value.end());
            block.push_back(L'\0');
        }

        // The block ends with an empty entry; when there is no entry at all that still means
        // two terminators, or CreateProcessW rejects the block.
        if (variables.empty()) {
            block.push_back(L'\0');
        }
        block.push_back(L'\0');
        return block;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Pipe reader

namespace {
    // Runs on its own thread until every writer of the pipe has gone away.
    void drain_pipe(HANDLE pipe, detail::process_output *output, bool isStandardError) {
        std::array<char, 16 * 1024> chunk = {};
        for (;;) {
            DWORD bytesRead = 0;
            const BOOL ok = ReadFile(pipe, chunk.data(), static_cast<DWORD>(chunk.size()), &bytesRead, nullptr);
            if (!ok) {
                // ERROR_BROKEN_PIPE: the child and everything that inherited the handle exited.
                return;
            }
            if (bytesRead == 0) {
                // A zero-length write by the child completes a read with nothing; the pipe is
                // still open.
                continue;
            }
            if (isStandardError) {
                output->append_error(chunk.data(), static_cast<conjure::usize>(bytesRead));
            } else {
                output->append_output(chunk.data(), static_cast<conjure::usize>(bytesRead));
            }
        }
    }

    // Everything start() hands to the state once the child exists.
    struct launch {
        unique_handle processHandle = {};
        unique_handle jobHandle = {};
        unique_handle standardOutputRead = {};
        unique_handle standardErrorRead = {};
        conjure::u32 processId = 0;
        conjure::f64 timeoutSeconds = 0;
    };
}


/*===========================================================================*/
/*===========================================================================*/
// process::state

class process::state {
    private:
        unique_handle mProcessHandle = {};
        unique_handle mJobHandle = {};
        unique_handle mStandardOutputRead = {};
        unique_handle mStandardErrorRead = {};
        // The pipe readers are plain std::threads, the documented exception to "no std::thread
        // outside core::jobs" (docs/design/10_core.md C4): C2 limits system's dependencies to
        // foundation and stb, so jobs::dedicated_thread is out of reach here.
        std::thread mStandardOutputReader = {};
        std::thread mStandardErrorReader = {};
        detail::process_output mOutput = {};
        detail::completion_latch mReadersDone = {};
        steady_clock::time_point mStartTime = {};
        f64 mTimeoutSeconds = 0;
        u32 mProcessId = 0;
        // Read by is_running() and kill() from other threads while wait() runs.
        std::atomic<bool> mFinished = false;

    public:
        explicit state(launch &&launched);
        ~state();

        state(const state &) = delete;
        state &operator=(const state &) = delete;

    public:
        [[nodiscard]] bool is_alive() const noexcept;
        [[nodiscard]] u32 process_id() const noexcept;
        [[nodiscard]] detail::process_output &output() noexcept;
        [[nodiscard]] result<process_result> wait();
        void terminate() noexcept;

    private:
        void start_reader(std::thread &reader, HANDLE pipe, bool isStandardError);
        void join_readers();
};

process::state::state(launch &&launched) :
    mProcessHandle(std::move(launched.processHandle)),
    mJobHandle(std::move(launched.jobHandle)),
    mStandardOutputRead(std::move(launched.standardOutputRead)),
    mStandardErrorRead(std::move(launched.standardErrorRead)),
    mTimeoutSeconds(detail::clamp_timeout(launched.timeoutSeconds)),
    mProcessId(launched.processId) {
    if (mStandardOutputRead.is_valid()) {
        start_reader(mStandardOutputReader, mStandardOutputRead.get(), false);
    }
    if (mStandardErrorRead.is_valid()) {
        start_reader(mStandardErrorReader, mStandardErrorRead.get(), true);
    }
    mStartTime = steady_clock::now();
}

process::state::~state() {
    // Destruction means the owner wants nothing more from the child: take the whole tree down
    // first, because the readers only finish once every holder of the pipes is gone.
    if (mProcessHandle.is_valid() && !mFinished.load()) {
        terminate();
        WaitForSingleObject(mProcessHandle.get(), INFINITE);
    }
    join_readers();
}

bool process::state::is_alive() const noexcept {
    if (!mProcessHandle.is_valid() || mFinished.load()) {
        return false;
    }
    return WaitForSingleObject(mProcessHandle.get(), 0) == WAIT_TIMEOUT;
}

conjure::u32 process::state::process_id() const noexcept {
    return mProcessId;
}

detail::process_output &process::state::output() noexcept {
    return mOutput;
}

result<process_result> process::state::wait() {
    if (!mProcessHandle.is_valid()) {
        return error(error_code::INVALID_STATE, "process::wait: the process was never started");
    }
    if (mFinished.load()) {
        return error(error_code::INVALID_STATE, "process::wait: already waited for this process");
    }

    const bool hasDeadline = mTimeoutSeconds > 0.0;
    steady_clock::time_point deadline = {};
    DWORD waitMilliseconds = INFINITE;
    if (hasDeadline) {
        deadline = detail::timeout_deadline(mStartTime, mTimeoutSeconds);
        const steady_clock::time_point now = steady_clock::now();
        if (deadline <= now) {
            waitMilliseconds = 0;
        } else {
            const i64 remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - now).count();
            waitMilliseconds = static_cast<DWORD>(std::min<i64>(remaining, static_cast<i64>(INFINITE) - 1));
        }
    }

    bool timedOut = false;
    const DWORD waitResult = WaitForSingleObject(mProcessHandle.get(), waitMilliseconds);
    if (waitResult == WAIT_TIMEOUT) {
        timedOut = true;
        terminate();
        WaitForSingleObject(mProcessHandle.get(), INFINITE);
    } else if (waitResult != WAIT_OBJECT_0) {
        return last_error("process::wait: waiting for the process failed");
    }

    // The readers end when the last writer closes, which includes anything the child spawned
    // and handed the pipe to. The timeout bounds that too: past the deadline the tree is killed.
    if (hasDeadline) {
        if (!mReadersDone.wait_until(deadline)) {
            timedOut = true;
            terminate();
        }
    } else {
        mReadersDone.wait();
    }
    join_readers();

    const f64 elapsed = std::chrono::duration<f64>(steady_clock::now() - mStartTime).count();
    DWORD exitCode = 0;
    if (!GetExitCodeProcess(mProcessHandle.get(), &exitCode)) {
        return last_error("process::wait: cannot read the exit code");
    }

    process_result finished = {};
    finished.exitCode = static_cast<i32>(exitCode);
    finished.timedOut = timedOut;
    finished.elapsedSeconds = elapsed;
    mOutput.take_output(finished.standardOutput);
    mOutput.take_error(finished.standardError);
    mFinished.store(true);
    return finished;
}

void process::state::terminate() noexcept {
    if (!mProcessHandle.is_valid() || mFinished.load()) {
        return;
    }
    const UINT exitCode = static_cast<UINT>(process::KILLED_EXIT_CODE);
    if (mJobHandle.is_valid()) {
        // Reaches every descendant, also after the child itself exited (the job keeps them).
        TerminateJobObject(mJobHandle.get(), exitCode);
    }
    // Also terminate directly: the job may not exist, and a process that broke away from it
    // would otherwise survive. On an exited process this fails and leaves its exit code alone.
    TerminateProcess(mProcessHandle.get(), exitCode);
}

void process::state::start_reader(std::thread &reader, HANDLE pipe, bool isStandardError) {
    mReadersDone.expect_one();
    reader = std::thread([this, pipe, isStandardError] {
        drain_pipe(pipe, &mOutput, isStandardError);
        mReadersDone.arrive();
    });
}

void process::state::join_readers() {
    if (mStandardOutputReader.joinable()) {
        mStandardOutputReader.join();
    }
    if (mStandardErrorReader.joinable()) {
        mStandardErrorReader.join();
    }
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
    if (is_batch_file(desc.executable)) {
        return make_error(
          error_code::INVALID_ARGUMENT,
          "cannot start '{}': batch files run through cmd.exe, which re-parses the arguments; start cmd.exe "
          "explicitly instead",
          desc.executable
        );
    }

    // Handles the child inherits. Standard error may alias standard output when merged, so it is
    // tracked as a raw handle and owned only when it is a distinct object.
    CONJURE_TRY_ASSIGN(unique_handle childStandardInput, open_null_device(GENERIC_READ));
    unique_handle childStandardOutput = {};
    unique_handle childStandardErrorOwned = {};
    launch launched = {};
    launched.timeoutSeconds = desc.timeoutSeconds;

    if (desc.captureStdout) {
        CONJURE_TRY(create_pipe(launched.standardOutputRead, childStandardOutput));
    } else {
        CONJURE_TRY_ASSIGN(childStandardOutput, inheritable_std_handle(STD_OUTPUT_HANDLE, GENERIC_WRITE));
    }

    HANDLE childStandardError = nullptr;
    if (desc.mergeStderr) {
        childStandardError = childStandardOutput.get();
    } else {
        if (desc.captureStderr) {
            CONJURE_TRY(create_pipe(launched.standardErrorRead, childStandardErrorOwned));
        } else {
            CONJURE_TRY_ASSIGN(childStandardErrorOwned, inheritable_std_handle(STD_ERROR_HANDLE, GENERIC_WRITE));
        }
        childStandardError = childStandardErrorOwned.get();
    }

    // Restrict inheritance to exactly these handles. Without the list every inheritable handle
    // of this process leaks into the child, including the pipes of other children started
    // concurrently, whose readers would then never see end-of-file.
    std::array<HANDLE, 3> inheritedHandles = {childStandardInput.get(), childStandardOutput.get(), childStandardError};
    std::size_t inheritedCount = inheritedHandles.size();
    if (childStandardError == childStandardOutput.get()) {
        inheritedCount = 2;
    }

    SIZE_T attributeListSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeListSize);
    std::vector<std::byte> attributeStorage(attributeListSize);
    const LPPROC_THREAD_ATTRIBUTE_LIST attributes =
      reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeStorage.data());
    if (!InitializeProcThreadAttributeList(attributes, 1, 0, &attributeListSize)) {
        return last_error("cannot initialise the process attribute list");
    }
    const BOOL attributeSet = UpdateProcThreadAttribute(
      attributes,
      0,
      PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
      inheritedHandles.data(),
      inheritedCount * sizeof(HANDLE),
      nullptr,
      nullptr
    );
    if (!attributeSet) {
        const error failure = last_error("cannot restrict inherited handles");
        DeleteProcThreadAttributeList(attributes);
        return failure;
    }

    STARTUPINFOEXW startupInfo = {};
    startupInfo.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    startupInfo.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.StartupInfo.hStdInput = childStandardInput.get();
    startupInfo.StartupInfo.hStdOutput = childStandardOutput.get();
    startupInfo.StartupInfo.hStdError = childStandardError;
    startupInfo.lpAttributeList = attributes;

    std::wstring commandLine = build_command_line(desc);
    std::vector<wchar_t> environmentBlock = build_environment_block(desc.environment);
    std::wstring workingDirectory = {};
    if (!desc.workingDirectory.empty()) {
        std::error_code ignored = {};
        workingDirectory = std::filesystem::absolute(desc.workingDirectory, ignored).wstring();
    }

    // Suspended so the process can be put into its job before it runs (and spawns) anything.
    const DWORD creationFlags = EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED
      | CREATE_NO_WINDOW;
    PROCESS_INFORMATION processInfo = {};
    const BOOL created = CreateProcessW(
      nullptr,
      commandLine.data(),
      nullptr,
      nullptr,
      TRUE,
      creationFlags,
      environmentBlock.empty() ? nullptr : static_cast<LPVOID>(environmentBlock.data()),
      workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
      &startupInfo.StartupInfo,
      &processInfo
    );
    const DWORD createError = GetLastError();
    DeleteProcThreadAttributeList(attributes);
    if (!created) {
        return make_error(
          detail::error_code_from_win32(createError),
          "cannot start '{}': {}",
          desc.executable,
          detail::win32_error_message(createError)
        );
    }

    const unique_handle mainThread(processInfo.hThread);
    launched.processHandle.reset(processInfo.hProcess);
    launched.processId = processInfo.dwProcessId;

    // The job makes kill() and the timeout take the whole process tree down and kills the tree
    // if this process dies while holding the last handle to the job.
    unique_handle job(CreateJobObjectW(nullptr, nullptr));
    bool inJob = false;
    if (job.is_valid()) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        const BOOL limited = SetInformationJobObject(
          job.get(),
          JobObjectExtendedLimitInformation,
          &limits,
          sizeof(limits)
        );
        inJob = limited && AssignProcessToJobObject(job.get(), processInfo.hProcess);
    }
    if (inJob) {
        launched.jobHandle = std::move(job);
    } else {
        CONJURE_LOG_WARN(
          LOG_CORE,
          "process {}: no job object ({}); kill() will not reach grandchildren",
          processInfo.dwProcessId,
          detail::win32_error_message(GetLastError())
        );
    }

    // Close this side's copies of the child's handles so the pipes report end-of-file when the
    // child exits. childStandardError aliases or is owned by childStandardErrorOwned.
    childStandardInput.reset();
    childStandardOutput.reset();
    childStandardErrorOwned.reset();

    // The state starts the readers and the clock; only then does the child get to run.
    process started = {};
    started.mState = std::make_unique<state>(std::move(launched));
    ResumeThread(mainThread.get());
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
