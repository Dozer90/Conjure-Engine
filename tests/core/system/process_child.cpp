// Helper program spawned by the core::process tests. The first argument selects a behaviour:
//
//   echo <words...>        print the words separated by spaces, then a newline, to stdout
//   stderr <words...>      same, to stderr
//   both                   print "out" to stdout and "err" to stderr, each followed by a newline
//   exit <code>            exit with the given code
//   echo-exit <code> <words...>
//                          print the words as echo does, then exit with the given code
//   sleep <milliseconds>   sleep, then exit 0
//   args <values...>       print every remaining argument as "[value]" on its own line
//   env <name>             print the variable's value, or "<unset>"
//   envlist                print the name of every environment variable, one per line
//   cwd                    print the current directory (generic, forward slashes)
//   flood <bytes>          write <bytes> bytes to stdout and <bytes> bytes to stderr, interleaved
//   trickle <count> <ms>   print "line<i>" <count> times, flushing and sleeping <ms> between lines
//   stdin                  read stdin to end-of-file and print the number of bytes read
//   pid                    print the operating system's identifier of this process
//   signals                print "ignored" when SIGINT was inherited as ignored, else "default"
//   zero-write             print "before", perform a zero-length write to stdout, print "after"
//   spawn-sleeper <ms>     start this program in "sleep <ms>" mode with inherited standard
//                          streams, print "spawned" and exit at once, leaving the grandchild
//   fds                    print how many descriptors from 3 to 1023 are open (POSIX); print 0
//                          on Windows, where inheritance is limited to the standard handles
//   wait-for <path>        poll every 5 ms until <path> exists, then exit 0; exit 4 after 60 s
//                          so a child whose marker never appears does not linger
//
// Anything else prints a usage line to stderr and exits with 2.
//
// A copy of this program can stand in for another tool (a dotnet that has no SDK or an old one, in the engine's
// and the editor's tests): when a file named like the program as the parent spelt it (argv[0]) plus ".replay"
// exists, one of its lines replaces the arguments, a mode and its words separated by single spaces, whatever the
// parent passed. A line "when <word> <mode> <words...>" applies when the parent's first argument is <word> (a
// dotnet's "build" or "--version"); otherwise the first line that does not start with "when " does.
//
// This is a test program, not engine code: the pid, zero-write, spawn-sleeper and fds modes need
// the operating system directly, so the platform guard below is accepted here.

#include "conjure/core/platform_detect.h"

#if CONJURE_PLATFORM_WINDOWS
    #include <Windows.h>
#else
    #include <fcntl.h>
    #include <spawn.h>
    #include <unistd.h>
#endif

#include <algorithm>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if CONJURE_PLATFORM_WINDOWS
namespace {
    char **environment_entries() {
        return _environ;
    }
}
#else
extern char **environ;

namespace {
    char **environment_entries() {
        return environ;
    }
}
#endif

namespace {
    constexpr int USAGE_EXIT_CODE = 2;

    int usage(std::string_view mode) {
        std::cerr << "conjure_process_child: unknown mode '" << mode << "'\n";
        return USAGE_EXIT_CODE;
    }

    long long parse_number(std::string_view text) {
        long long value = 0;
        std::from_chars(text.data(), text.data() + text.size(), value);
        return value;
    }

    // The words of the line of '<self>.replay' that applies to the parent's first argument 'first' (see the top of
    // the file), split at single spaces; false when there is no such file or no line applies.
    bool read_replay(const char *self, std::string_view first, std::vector<std::string> &words) {
        std::ifstream stream(std::filesystem::path(std::string(self) + ".replay"), std::ios::binary);
        if (!stream.is_open()) {
            return false;
        }
        constexpr std::string_view WHEN = "when ";
        std::string chosen = {};
        bool chosenFound = false;
        std::string fallback = {};
        bool fallbackFound = false;
        std::string line = {};
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            const std::string_view text = line;
            if (!text.starts_with(WHEN)) {
                if (!fallbackFound) {
                    fallback = line;
                    fallbackFound = true;
                }
                continue;
            }
            const std::string_view rule = text.substr(WHEN.size());
            const std::size_t space = rule.find(' ');
            if (!chosenFound && space != std::string_view::npos && rule.substr(0, space) == first) {
                chosen = std::string(rule.substr(space + 1));
                chosenFound = true;
            }
        }
        if (!chosenFound) {
            if (!fallbackFound) {
                return false;
            }
            chosen = fallback;
        }
        std::size_t start = 0;
        while (start <= chosen.size()) {
            const std::size_t space = chosen.find(' ', start);
            const std::size_t end = space == std::string::npos ? chosen.size() : space;
            words.push_back(chosen.substr(start, end - start));
            start = end + 1;
        }
        return true;
    }

    void print_joined(std::ostream &stream, const std::vector<std::string_view> &words) {
        for (std::size_t index = 0; index < words.size(); ++index) {
            if (index > 0) {
                stream << ' ';
            }
            stream << words[index];
        }
        stream << '\n';
        stream.flush();
    }

    int flood(long long bytes) {
        // Alternate between the streams in chunks so a parent reading only one of them blocks.
        const std::string chunk(4096, 'x');
        long long writtenOut = 0;
        long long writtenErr = 0;
        while (writtenOut < bytes || writtenErr < bytes) {
            if (writtenOut < bytes) {
                const long long count = std::min<long long>(static_cast<long long>(chunk.size()), bytes - writtenOut);
                std::cout.write(chunk.data(), static_cast<std::streamsize>(count));
                writtenOut += count;
            }
            if (writtenErr < bytes) {
                const long long count = std::min<long long>(static_cast<long long>(chunk.size()), bytes - writtenErr);
                std::cerr.write(chunk.data(), static_cast<std::streamsize>(count));
                writtenErr += count;
            }
        }
        std::cout.flush();
        std::cerr.flush();
        return 0;
    }

    int trickle(long long count, long long milliseconds) {
        for (long long index = 0; index < count; ++index) {
            std::cout << "line" << index << '\n';
            std::cout.flush();
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        }
        return 0;
    }

    int read_stdin() {
        long long total = 0;
        char buffer[1024] = {};
        while (std::cin.read(buffer, sizeof(buffer)) || std::cin.gcount() > 0) {
            total += std::cin.gcount();
        }
        std::cout << total << '\n';
        return 0;
    }

    int print_environment_names() {
        for (char **cursor = environment_entries(); cursor != nullptr && *cursor != nullptr; ++cursor) {
            const std::string_view entry(*cursor);
            // Hidden drive-letter entries ("=C:=C:\dir") have no name worth reporting.
            if (entry.empty() || entry.front() == '=') {
                continue;
            }
            const std::size_t separator = entry.find('=');
            std::cout << entry.substr(0, separator) << '\n';
        }
        std::cout.flush();
        return 0;
    }

    int print_pid() {
#if CONJURE_PLATFORM_WINDOWS
        std::cout << GetCurrentProcessId() << '\n';
#else
        std::cout << getpid() << '\n';
#endif
        return 0;
    }

    int print_signal_disposition() {
        // The previous disposition is what this process inherited; the C library reports SIG_IGN
        // only when the parent left SIGINT ignored across the exec.
        const auto previous = std::signal(SIGINT, SIG_DFL);
        std::cout << (previous == SIG_IGN ? "ignored" : "default") << '\n';
        return 0;
    }

    int zero_write() {
        std::cout << "before\n";
        std::cout.flush();
#if CONJURE_PLATFORM_WINDOWS
        DWORD written = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), "", 0, &written, nullptr);
#else
        const ssize_t written = ::write(STDOUT_FILENO, "", 0);
        static_cast<void>(written);
#endif
        std::cout << "after\n";
        std::cout.flush();
        return 0;
    }

    // Starts this very program in "sleep" mode with the inherited standard streams, so the
    // grandchild keeps the parent's output pipe open after this process exits.
    int spawn_sleeper(const char *self, std::string_view milliseconds) {
        const std::string sleepMilliseconds(milliseconds);
#if CONJURE_PLATFORM_WINDOWS
        std::wstring commandLine = L"\"";
        const std::string selfText(self);
        commandLine.append(selfText.begin(), selfText.end());
        commandLine.append(L"\" sleep ");
        commandLine.append(sleepMilliseconds.begin(), sleepMilliseconds.end());

        STARTUPINFOW startupInfo = {};
        startupInfo.cb = sizeof(startupInfo);
        startupInfo.dwFlags = STARTF_USESTDHANDLES;
        startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        startupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        startupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        PROCESS_INFORMATION processInfo = {};
        const BOOL created = CreateProcessW(
          nullptr,
          commandLine.data(),
          nullptr,
          nullptr,
          TRUE,
          CREATE_NO_WINDOW,
          nullptr,
          nullptr,
          &startupInfo,
          &processInfo
        );
        if (!created) {
            std::cerr << "conjure_process_child: cannot spawn the sleeper (" << GetLastError() << ")\n";
            return 3;
        }
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
#else
        std::string selfText(self);
        std::string mode = "sleep";
        std::string duration = sleepMilliseconds;
        char *argv[] = {selfText.data(), mode.data(), duration.data(), nullptr};
        pid_t grandchild = -1;
        const int spawnError = ::posix_spawn(&grandchild, self, nullptr, nullptr, argv, environment_entries());
        if (spawnError != 0) {
            std::cerr << "conjure_process_child: cannot spawn the sleeper (" << spawnError << ")\n";
            return 3;
        }
#endif
        std::cout << "spawned\n";
        std::cout.flush();
        return 0;
    }

    // Descriptors beyond the standard three that this process inherited (nothing here opens any).
    int count_open_descriptors() {
        int open = 0;
#if !CONJURE_PLATFORM_WINDOWS
        constexpr int FIRST_DESCRIPTOR = 3;
        constexpr int LAST_DESCRIPTOR = 1023;
        for (int descriptor = FIRST_DESCRIPTOR; descriptor <= LAST_DESCRIPTOR; ++descriptor) {
            if (::fcntl(descriptor, F_GETFD) != -1) {
                ++open;
            }
        }
#endif
        std::cout << open << '\n';
        std::cout.flush();
        return 0;
    }

    // A marker file is the portable "go" signal: standard input is always the null device.
    int wait_for(std::string_view path) {
        constexpr int GIVE_UP_EXIT_CODE = 4;
        constexpr std::chrono::seconds GIVE_UP_AFTER = std::chrono::seconds(60);

        // argv holds the narrow native encoding, which is what a std::string path is read as.
        const std::filesystem::path marker = std::filesystem::path(std::string(path));
        const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + GIVE_UP_AFTER;
        while (std::chrono::steady_clock::now() < deadline) {
            std::error_code ignored = {};
            if (std::filesystem::exists(marker, ignored)) {
                return 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return GIVE_UP_EXIT_CODE;
    }
}

int main(int argc, char **argv) {
    std::vector<std::string_view> arguments = {};
    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }
    std::vector<std::string> replayed = {};
    const std::string_view firstArgument = arguments.empty() ? std::string_view() : arguments.front();
    if (argc > 0 && read_replay(argv[0], firstArgument, replayed)) {
        arguments.assign(replayed.begin(), replayed.end());
    }
    if (arguments.empty()) {
        return usage("");
    }

    const std::string_view mode = arguments.front();
    const std::vector<std::string_view> rest(arguments.begin() + 1, arguments.end());

    if (mode == "echo") {
        print_joined(std::cout, rest);
        return 0;
    }
    if (mode == "stderr") {
        print_joined(std::cerr, rest);
        return 0;
    }
    if (mode == "both") {
        std::cout << "out\n";
        std::cout.flush();
        std::cerr << "err\n";
        std::cerr.flush();
        return 0;
    }
    if (mode == "exit") {
        return rest.empty() ? 0 : static_cast<int>(parse_number(rest.front()));
    }
    if (mode == "echo-exit") {
        if (rest.empty()) {
            return usage(mode);
        }
        print_joined(std::cout, std::vector<std::string_view>(rest.begin() + 1, rest.end()));
        return static_cast<int>(parse_number(rest.front()));
    }
    if (mode == "sleep") {
        const long long milliseconds = rest.empty() ? 0 : parse_number(rest.front());
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        return 0;
    }
    if (mode == "args") {
        for (const std::string_view value : rest) {
            std::cout << '[' << value << "]\n";
        }
        std::cout.flush();
        return 0;
    }
    if (mode == "env") {
        const char *value = rest.empty() ? nullptr : std::getenv(std::string(rest.front()).c_str());
        std::cout << (value != nullptr ? value : "<unset>") << '\n';
        return 0;
    }
    if (mode == "envlist") {
        return print_environment_names();
    }
    if (mode == "cwd") {
        std::error_code ignored = {};
        std::cout << std::filesystem::current_path(ignored).generic_string() << '\n';
        return 0;
    }
    if (mode == "flood") {
        return flood(rest.empty() ? 0 : parse_number(rest.front()));
    }
    if (mode == "trickle") {
        const long long count = rest.empty() ? 0 : parse_number(rest[0]);
        const long long milliseconds = rest.size() < 2 ? 0 : parse_number(rest[1]);
        return trickle(count, milliseconds);
    }
    if (mode == "stdin") {
        return read_stdin();
    }
    if (mode == "pid") {
        return print_pid();
    }
    if (mode == "signals") {
        return print_signal_disposition();
    }
    if (mode == "zero-write") {
        return zero_write();
    }
    if (mode == "spawn-sleeper") {
        return spawn_sleeper(argv[0], rest.empty() ? "60000" : rest.front());
    }
    if (mode == "fds") {
        return count_open_descriptors();
    }
    if (mode == "wait-for") {
        return rest.empty() ? usage(mode) : wait_for(rest.front());
    }
    return usage(mode);
}
