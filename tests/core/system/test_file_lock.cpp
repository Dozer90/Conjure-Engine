#include "conjure/testing/doctest.h"

#include "conjure/testing/temp_directory.h"

#include "system_test_support.h"

#include "conjure/core/clock.h"
#include "conjure/core/file_lock.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/process.h"
#include "core/system/system_internal.h"

// The inheritance case spawns a child through the operating system directly, the way a spawner that inherits
// every inheritable handle or descriptor does; core::process would hide the difference. The foreign-handle case
// opens and locks the file directly, the way a tool that knows nothing of file_lock does.
#if CONJURE_PLATFORM_WINDOWS
    #include <Windows.h>
#else
    #include <fcntl.h>
    #include <signal.h>
    #include <spawn.h>
    #include <sys/file.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

#include <array>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if !CONJURE_PLATFORM_WINDOWS
extern char **environ;
#endif

using namespace conjure;
using namespace conjure::core;

using process_child_support::child;
using process_child_support::child_path;
using process_child_support::without_carriage_returns;

namespace {
    constexpr std::array<file_lock_open, 2> OPEN_MODES = {file_lock_open::CREATE, file_lock_open::EXISTING};

    constexpr f64 CHILD_READY_TIMEOUT_SECONDS = 30.0;

    // The operating system releases a departed holder's lock, but Windows documents that it may take a moment, so
    // a case that expects to take over such a lock polls this long before failing.
    constexpr f64 RELEASE_TIMEOUT_SECONDS = 5.0;

    // Measured on core::clock ticks: tests may not read the std::chrono clocks (tools/check_style.py), which only
    // test_process.cpp is excused from, for its wall-clock timeout contracts.
    f64 seconds_since(u64 startTicks) {
        return ticks_to_seconds(ticks_now() - startTicks);
    }

    std::string read_file(const std::filesystem::path &path) {
        std::ifstream stream(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    // Waits until 'running' created 'marker' (a bounded spin that yields between looks). False when the child exited
    // first or the time ran out.
    bool wait_for_marker(const std::filesystem::path &marker, const process &running) {
        const u64 start = ticks_now();
        while (seconds_since(start) < CHILD_READY_TIMEOUT_SECONDS) {
            std::error_code ignored = {};
            if (std::filesystem::exists(marker, ignored)) {
                return true;
            }
            if (!running.is_running()) {
                return std::filesystem::exists(marker, ignored);
            }
            std::this_thread::yield();
        }
        return false;
    }

    // Tries until the lock on the existing 'path' is held, for RELEASE_TIMEOUT_SECONDS at most (a bounded spin that
    // yields between tries); fails the case, naming the path, when it stays busy or a try fails.
    file_lock acquire_eventually(const std::filesystem::path &path) {
        const u64 start = ticks_now();
        while (true) {
            result<file_lock> attempt = file_lock::try_acquire(path, file_lock_open::EXISTING);
            CONJURE_REQUIRE_OK(attempt);
            if (attempt->held()) {
                return std::move(*attempt);
            }
            if (seconds_since(start) >= RELEASE_TIMEOUT_SECONDS) {
                FAIL("'" << detail::path_to_utf8(path) << "' is still locked " << RELEASE_TIMEOUT_SECONDS
                         << " s after its holder let go");
            }
            std::this_thread::yield();
        }
    }

    // Starts "lock-hold" on 'lockPath' and waits until the child holds it, checking that the parent's own try is
    // then not held. The child releases the lock and exits 0 once the file "release" exists in 'directory'.
    process start_holder(const testing::temp_directory &directory, const std::filesystem::path &lockPath) {
        const std::filesystem::path ready = directory.file("ready");
        const std::filesystem::path release = directory.file("release");
        result<process> holder = process::start(child(
          {"lock-hold", detail::path_to_utf8(lockPath), detail::path_to_utf8(ready), detail::path_to_utf8(release)}
        ));
        CONJURE_REQUIRE_OK(holder);
        REQUIRE(wait_for_marker(ready, *holder));

        const result<file_lock> whileHeld = file_lock::try_acquire(lockPath, file_lock_open::EXISTING);
        CONJURE_REQUIRE_OK(whileHeld);
        CHECK_FALSE(whileHeld->held());
        return std::move(*holder);
    }

    // How foreign_handle opens the file.
    enum class foreign_open : u8 {
        LOCK_WHOLE_FILE,        // share everything, then lock the whole file exclusively (LockFileEx / flock)
        NO_DELETE_SHARING       // Windows only: read access, sharing reads and writes but not deletion
    };

    // A file opened by the operating system directly, as a tool that knows nothing of file_lock opens it.
    // Destruction unlocks and closes it.
    class foreign_handle {
        private:
#if CONJURE_PLATFORM_WINDOWS
            HANDLE mHandle = INVALID_HANDLE_VALUE;
#else
            int mDescriptor = -1;
#endif

        public:
            foreign_handle() = default;

            ~foreign_handle() {
                close();
            }

            foreign_handle(const foreign_handle &) = delete;
            foreign_handle &operator=(const foreign_handle &) = delete;
            foreign_handle(foreign_handle &&) = delete;
            foreign_handle &operator=(foreign_handle &&) = delete;

        public:
            // Opens the existing 'path'; false when the open or the lock failed. NO_DELETE_SHARING has no POSIX
            // counterpart, since every POSIX open lets others unlink the file, and is false there.
            bool open(const std::filesystem::path &path, foreign_open how) {
#if CONJURE_PLATFORM_WINDOWS
                const bool lockWholeFile = how == foreign_open::LOCK_WHOLE_FILE;
                const DWORD access = lockWholeFile ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ;
                const DWORD everyShare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
                const DWORD sharing = lockWholeFile ? everyShare : FILE_SHARE_READ | FILE_SHARE_WRITE;
                const DWORD attributes = FILE_ATTRIBUTE_NORMAL;
                mHandle = CreateFileW(path.c_str(), access, sharing, nullptr, OPEN_EXISTING, attributes, nullptr);
                if (mHandle == INVALID_HANDLE_VALUE) {
                    return false;
                }
                if (!lockWholeFile) {
                    return true;
                }

                OVERLAPPED wholeFile = {};
                const DWORD lockFlags = LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY;
                return LockFileEx(mHandle, lockFlags, 0, MAXDWORD, MAXDWORD, &wholeFile) != FALSE;
#else
                if (how != foreign_open::LOCK_WHOLE_FILE) {
                    return false;
                }
                mDescriptor = ::open(path.c_str(), O_RDWR | O_CLOEXEC);
                if (mDescriptor < 0) {
                    return false;
                }
                return ::flock(mDescriptor, LOCK_EX | LOCK_NB) == 0;
#endif
            }

            // Unlocks and closes; safe when nothing is open.
            void close() {
#if CONJURE_PLATFORM_WINDOWS
                if (mHandle == INVALID_HANDLE_VALUE) {
                    return;
                }
                // Closing drops the lock too, but Windows may do that a moment later; the unlock is immediate. It
                // fails harmlessly (ERROR_NOT_LOCKED) for a handle that locked nothing.
                OVERLAPPED wholeFile = {};
                UnlockFileEx(mHandle, 0, MAXDWORD, MAXDWORD, &wholeFile);
                CloseHandle(mHandle);
                mHandle = INVALID_HANDLE_VALUE;
#else
                if (mDescriptor < 0) {
                    return;
                }
                ::close(mDescriptor);
                mDescriptor = -1;
#endif
            }
    };

    // A conjure_process_child started by the operating system with every inheritable handle (Windows,
    // bInheritHandles without a handle list) or descriptor (POSIX, posix_spawn without file actions) passed on.
    // Destruction kills and reaps it.
    class inheriting_child {
        private:
#if CONJURE_PLATFORM_WINDOWS
            HANDLE mProcess = nullptr;
#else
            pid_t mProcessId = -1;
#endif

        public:
            inheriting_child() = default;

            ~inheriting_child() {
                kill_and_wait();
            }

            inheriting_child(const inheriting_child &) = delete;
            inheriting_child &operator=(const inheriting_child &) = delete;
            inheriting_child(inheriting_child &&) = delete;
            inheriting_child &operator=(inheriting_child &&) = delete;

        public:
            // Starts "conjure_process_child sleep <milliseconds>"; false when the spawn failed.
            bool start_sleeping(int milliseconds) {
                const std::string duration = std::to_string(milliseconds);
#if CONJURE_PLATFORM_WINDOWS
                std::wstring commandLine = L"\"" + child_path().native() + L"\" sleep ";
                commandLine.append(duration.begin(), duration.end());
                STARTUPINFOW startupInfo = {};
                startupInfo.cb = sizeof(startupInfo);
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
                    return false;
                }
                CloseHandle(processInfo.hThread);
                mProcess = processInfo.hProcess;
                return true;
#else
                std::string program = child_path().string();
                std::string mode = "sleep";
                std::string durationArgument = duration;
                std::array<char *, 4> argv = {program.data(), mode.data(), durationArgument.data(), nullptr};
                pid_t spawned = -1;
                if (::posix_spawn(&spawned, program.c_str(), nullptr, nullptr, argv.data(), environ) != 0) {
                    return false;
                }
                mProcessId = spawned;
                return true;
#endif
            }

            // True while the child runs.
            bool is_alive() {
#if CONJURE_PLATFORM_WINDOWS
                return mProcess != nullptr && WaitForSingleObject(mProcess, 0) == WAIT_TIMEOUT;
#else
                if (mProcessId < 0) {
                    return false;
                }
                int status = 0;
                const pid_t reaped = ::waitpid(mProcessId, &status, WNOHANG);
                if (reaped == mProcessId) {
                    mProcessId = -1;
                }
                return reaped == 0;
#endif
            }

            void kill_and_wait() {
#if CONJURE_PLATFORM_WINDOWS
                if (mProcess == nullptr) {
                    return;
                }
                constexpr DWORD KILL_WAIT_MILLISECONDS = 30000;
                TerminateProcess(mProcess, 1);
                WaitForSingleObject(mProcess, KILL_WAIT_MILLISECONDS);
                CloseHandle(mProcess);
                mProcess = nullptr;
#else
                if (mProcessId < 0) {
                    return;
                }
                ::kill(mProcessId, SIGKILL);
                int status = 0;
                while (::waitpid(mProcessId, &status, 0) < 0 && errno == EINTR) {
                }
                mProcessId = -1;
#endif
            }
    };
}

TEST_SUITE("core/file_lock") {

    TEST_CASE("file_lock - a default-constructed lock holds nothing and release is safe") {
        file_lock lock = {};
        CHECK_FALSE(lock.held());
        CHECK(lock.path().empty());

        lock.release();
        CHECK_FALSE(lock.held());
    }

    TEST_CASE("file_lock - CREATE makes an empty file and holds it") {
        const testing::temp_directory directory("core_file_lock_create");
        const std::filesystem::path lockPath = directory.file("instance.lock");

        const result<file_lock> lock = file_lock::try_acquire(lockPath, file_lock_open::CREATE);
        CONJURE_REQUIRE_OK(lock);
        CHECK(lock->held());
        CHECK(lock->path() == lockPath);
        REQUIRE(std::filesystem::exists(lockPath));
        CHECK(std::filesystem::file_size(lockPath) == 0);
    }

    TEST_CASE("file_lock - an existing file is held without being truncated") {
        const testing::temp_directory directory("core_file_lock_keep");
        const std::filesystem::path lockPath = directory.file("instance.lock");
        CONJURE_REQUIRE_OK(directory.write_text("instance.lock", "abc"));

        file_lock_open open = file_lock_open::CREATE;
        SUBCASE("CREATE") {
            open = file_lock_open::CREATE;
        }
        SUBCASE("EXISTING") {
            open = file_lock_open::EXISTING;
        }

        result<file_lock> lock = file_lock::try_acquire(lockPath, open);
        CONJURE_REQUIRE_OK(lock);
        CHECK(lock->held());
        CHECK(std::filesystem::file_size(lockPath) == 3);

        // Windows refuses reads inside a range another handle has locked, so the contents are read afterwards.
        lock->release();
        CHECK(read_file(lockPath) == "abc");
    }

    TEST_CASE("file_lock - a second lock on the same file in the same process is not held") {
        const testing::temp_directory directory("core_file_lock_second");
        const std::filesystem::path lockPath = directory.file("instance.lock");
        const result<file_lock> first = file_lock::try_acquire(lockPath, file_lock_open::CREATE);
        CONJURE_REQUIRE_OK(first);
        REQUIRE(first->held());

        file_lock_open open = file_lock_open::CREATE;
        SUBCASE("CREATE") {
            open = file_lock_open::CREATE;
        }
        SUBCASE("EXISTING") {
            open = file_lock_open::EXISTING;
        }

        const result<file_lock> second = file_lock::try_acquire(lockPath, open);
        CONJURE_REQUIRE_OK(second);
        CHECK_FALSE(second->held());
        CHECK(second->path().empty());
        CHECK(first->held());
        CHECK(std::filesystem::exists(lockPath));
    }

    TEST_CASE("file_lock - a foreign handle that locks or refuses the file keeps try_acquire out") {
        const testing::temp_directory directory("core_file_lock_foreign");
        const std::filesystem::path lockPath = directory.file("instance.lock");
        CONJURE_REQUIRE_OK(directory.write_text("instance.lock", "abc"));

        // On Windows a second file_lock never reaches LockFileEx (its open already fails on the share mode), so only
        // a foreign byte-range lock shows that layer.
        foreign_handle foreign = {};
        SUBCASE("an exclusive lock over the whole file") {
            REQUIRE(foreign.open(lockPath, foreign_open::LOCK_WHOLE_FILE));
        }
        SUBCASE("an open without delete sharing") {
            if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
                REQUIRE(foreign.open(lockPath, foreign_open::NO_DELETE_SHARING));
            } else {
                MESSAGE("not exercised on Linux or macOS, where every open lets others unlink the file; only a "
                        "foreign flock keeps file_lock out there");
                return;
            }
        }

        for (const file_lock_open open : OPEN_MODES) {
            CAPTURE(static_cast<int>(open));
            const result<file_lock> busy = file_lock::try_acquire(lockPath, open);
            CONJURE_REQUIRE_OK(busy);
            CHECK_FALSE(busy->held());
            CHECK(busy->path().empty());
            CHECK(std::filesystem::file_size(lockPath) == 3);
        }

        foreign.close();
        const file_lock taken = acquire_eventually(lockPath);
        CHECK(taken.held());
    }

    TEST_CASE("file_lock - release lets the next try_acquire hold it") {
        const testing::temp_directory directory("core_file_lock_release");
        const std::filesystem::path lockPath = directory.file("instance.lock");

        SUBCASE("release") {
            result<file_lock> first = file_lock::try_acquire(lockPath, file_lock_open::CREATE);
            CONJURE_REQUIRE_OK(first);
            REQUIRE(first->held());

            first->release();
            CHECK_FALSE(first->held());
            CHECK(first->path().empty());
            first->release();
        }
        SUBCASE("destructor") {
            const result<file_lock> first = file_lock::try_acquire(lockPath, file_lock_open::CREATE);
            CONJURE_REQUIRE_OK(first);
            REQUIRE(first->held());
        }

        const result<file_lock> next = file_lock::try_acquire(lockPath, file_lock_open::EXISTING);
        CONJURE_REQUIRE_OK(next);
        CHECK(next->held());
    }

    TEST_CASE("file_lock - EXISTING on a missing file is NOT_FOUND and creates nothing") {
        const testing::temp_directory directory("core_file_lock_missing");
        const std::filesystem::path lockPath = directory.file("instance.lock");

        const result<file_lock> lock = file_lock::try_acquire(lockPath, file_lock_open::EXISTING);
        CONJURE_CHECK_ERROR(lock, error_code::NOT_FOUND);
        REQUIRE_FALSE(lock.has_value());
        CHECK(lock.get_error().message().find(detail::path_to_utf8(lockPath)) != std::string::npos);
        CHECK_FALSE(std::filesystem::exists(lockPath));
    }

    TEST_CASE("file_lock - an empty path is INVALID_ARGUMENT and a missing parent is NOT_FOUND") {
        const testing::temp_directory directory("core_file_lock_arguments");

        SUBCASE("empty path") {
            CONJURE_CHECK_ERROR(
              file_lock::try_acquire(std::filesystem::path(), file_lock_open::CREATE),
              error_code::INVALID_ARGUMENT
            );
        }
        SUBCASE("embedded NUL") {
            const std::filesystem::path withNul = directory.path() / std::string("lock\0file", 9);
            CONJURE_CHECK_ERROR(file_lock::try_acquire(withNul, file_lock_open::CREATE), error_code::INVALID_ARGUMENT);
            CHECK(std::filesystem::is_empty(directory.path()));
        }
        SUBCASE("open value outside file_lock_open") {
            const std::filesystem::path lockPath = directory.file("instance.lock");
            constexpr u8 UNKNOWN_OPEN = 7;
            CONJURE_CHECK_ERROR(
              file_lock::try_acquire(lockPath, static_cast<file_lock_open>(UNKNOWN_OPEN)),
              error_code::INVALID_ARGUMENT
            );
            CHECK_FALSE(std::filesystem::exists(lockPath));
        }
        SUBCASE("missing parent") {
            const std::filesystem::path parent = directory.file("missing");
            const std::filesystem::path lockPath = parent / "instance.lock";

            file_lock_open open = file_lock_open::CREATE;
            SUBCASE("CREATE") {
                open = file_lock_open::CREATE;
            }
            SUBCASE("EXISTING") {
                open = file_lock_open::EXISTING;
            }

            const result<file_lock> lock = file_lock::try_acquire(lockPath, open);
            CONJURE_CHECK_ERROR(lock, error_code::NOT_FOUND);
            CHECK_FALSE(std::filesystem::exists(parent));
        }
    }

    TEST_CASE("file_lock - a failed try_acquire removes nothing it found") {
        const testing::temp_directory directory("core_file_lock_refused");

        SUBCASE("a directory") {
            const std::filesystem::path folder = directory.file("folder");
            REQUIRE(std::filesystem::create_directory(folder));
            CONJURE_REQUIRE_OK(directory.write_text("folder/content.txt", "kept"));

            for (const file_lock_open open : OPEN_MODES) {
                CAPTURE(static_cast<int>(open));
                CONJURE_CHECK_ERROR(file_lock::try_acquire(folder, open), error_code::PERMISSION_DENIED);
                CHECK(std::filesystem::is_directory(folder));
                CHECK(read_file(folder / "content.txt") == "kept");
            }
        }
        SUBCASE("a symbolic link") {
            if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
                MESSAGE("not exercised on Windows, which opens the target of a symbolic link (and needs a privilege "
                        "to create one); Linux and macOS refuse the link");
            } else {
                const std::filesystem::path target = directory.file("target.txt");
                const std::filesystem::path link = directory.file("link.lock");
                CONJURE_REQUIRE_OK(directory.write_text("target.txt", "kept"));
                std::filesystem::create_symlink(target, link);

                for (const file_lock_open open : OPEN_MODES) {
                    CAPTURE(static_cast<int>(open));
                    CONJURE_CHECK_ERROR(file_lock::try_acquire(link, open), error_code::PERMISSION_DENIED);
                    CHECK(std::filesystem::is_symlink(std::filesystem::symlink_status(link)));
                    CHECK(read_file(target) == "kept");
                }
            }
        }
    }

    TEST_CASE("file_lock - a moved-from lock is not held and move assignment releases the replaced lock") {
        const testing::temp_directory directory("core_file_lock_move");
        const std::filesystem::path firstPath = directory.file("first.lock");
        const std::filesystem::path secondPath = directory.file("second.lock");

        result<file_lock> first = file_lock::try_acquire(firstPath, file_lock_open::CREATE);
        CONJURE_REQUIRE_OK(first);
        REQUIRE(first->held());

        SUBCASE("move construction") {
            file_lock moved = std::move(*first);
            CHECK(moved.held());
            CHECK(moved.path() == firstPath);
            CHECK_FALSE(first->held());
            CHECK(first->path().empty());

            const result<file_lock> again = file_lock::try_acquire(firstPath, file_lock_open::EXISTING);
            CONJURE_REQUIRE_OK(again);
            CHECK_FALSE(again->held());
        }
        SUBCASE("move assignment") {
            result<file_lock> second = file_lock::try_acquire(secondPath, file_lock_open::CREATE);
            CONJURE_REQUIRE_OK(second);
            REQUIRE(second->held());

            *first = std::move(*second);
            CHECK(first->held());
            CHECK(first->path() == secondPath);
            CHECK_FALSE(second->held());
            CHECK(second->path().empty());

            const result<file_lock> replaced = file_lock::try_acquire(firstPath, file_lock_open::EXISTING);
            CONJURE_REQUIRE_OK(replaced);
            CHECK(replaced->held());
            const result<file_lock> taken = file_lock::try_acquire(secondPath, file_lock_open::EXISTING);
            CONJURE_REQUIRE_OK(taken);
            CHECK_FALSE(taken->held());
        }
    }

    TEST_CASE("file_lock - release_and_remove deletes the file and a new CREATE holds a fresh one") {
        const testing::temp_directory directory("core_file_lock_remove");
        const std::filesystem::path lockPath = directory.file("instance.lock");

        result<file_lock> lock = file_lock::try_acquire(lockPath, file_lock_open::CREATE);
        CONJURE_REQUIRE_OK(lock);
        REQUIRE(lock->held());

        CONJURE_CHECK_OK(lock->release_and_remove());
        CHECK_FALSE(lock->held());
        CHECK(lock->path().empty());
        CHECK_FALSE(std::filesystem::exists(lockPath));

        const result<file_lock> fresh = file_lock::try_acquire(lockPath, file_lock_open::CREATE);
        CONJURE_REQUIRE_OK(fresh);
        CHECK(fresh->held());
        CHECK(std::filesystem::exists(lockPath));
    }

    TEST_CASE("file_lock - release_and_remove without a held lock is INVALID_STATE") {
        const testing::temp_directory directory("core_file_lock_remove_state");
        const std::filesystem::path lockPath = directory.file("instance.lock");
        const result<file_lock> holder = file_lock::try_acquire(lockPath, file_lock_open::CREATE);
        CONJURE_REQUIRE_OK(holder);
        REQUIRE(holder->held());

        SUBCASE("default-constructed") {
            file_lock lock = {};
            CONJURE_CHECK_ERROR(lock.release_and_remove(), error_code::INVALID_STATE);
        }
        SUBCASE("released") {
            result<file_lock> other = file_lock::try_acquire(directory.file("other.lock"), file_lock_open::CREATE);
            CONJURE_REQUIRE_OK(other);
            other->release();
            CONJURE_CHECK_ERROR(other->release_and_remove(), error_code::INVALID_STATE);
            CHECK(std::filesystem::exists(directory.file("other.lock")));
        }
        SUBCASE("held elsewhere") {
            result<file_lock> busy = file_lock::try_acquire(lockPath, file_lock_open::EXISTING);
            CONJURE_REQUIRE_OK(busy);
            REQUIRE_FALSE(busy->held());
            CONJURE_CHECK_ERROR(busy->release_and_remove(), error_code::INVALID_STATE);
        }

        CHECK(holder->held());
        CHECK(std::filesystem::exists(lockPath));
    }

    TEST_CASE("file_lock - a held file resists deletion on Windows and an unlinked one blocks nothing on POSIX") {
        const testing::temp_directory directory("core_file_lock_unlink");
        const std::filesystem::path lockPath = directory.file("instance.lock");
        result<file_lock> stale = file_lock::try_acquire(lockPath, file_lock_open::CREATE);
        CONJURE_REQUIRE_OK(stale);
        REQUIRE(stale->held());

        if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
            std::error_code removeError = {};
            CHECK_FALSE(std::filesystem::remove(lockPath, removeError));
            CHECK(static_cast<bool>(removeError));
            std::error_code renameError = {};
            std::filesystem::rename(lockPath, directory.file("renamed.lock"), renameError);
            CHECK(static_cast<bool>(renameError));
            CHECK(std::filesystem::exists(lockPath));
            CHECK(stale->held());
        } else {
            REQUIRE(std::filesystem::remove(lockPath));
            CONJURE_CHECK_ERROR(file_lock::try_acquire(lockPath, file_lock_open::EXISTING), error_code::NOT_FOUND);

            const result<file_lock> fresh = file_lock::try_acquire(lockPath, file_lock_open::CREATE);
            CONJURE_REQUIRE_OK(fresh);
            CHECK(fresh->held());

            // The stale holder's lock guards an inode nobody can reach: removing must leave the new file alone.
            CONJURE_CHECK_ERROR(stale->release_and_remove(), error_code::NOT_FOUND);
            CHECK_FALSE(stale->held());
            CHECK(std::filesystem::exists(lockPath));
            CHECK(fresh->held());
        }
    }

    TEST_CASE("file_lock - a path past MAX_PATH locks in the extended-length form") {
        if constexpr (CONJURE_PLATFORM_WINDOWS == 0) {
            MESSAGE("no extended-length form on Linux or macOS; only a path longer than 260 characters is locked");
        }

        const testing::temp_directory directory("core_file_lock_long");
        const std::filesystem::path folder = testing::long_directory(directory, testing::PAST_MAX_PATH);
        const std::filesystem::path lockPath = testing::extended_length_path(folder / "instance.lock");

        result<file_lock> lock = file_lock::try_acquire(lockPath, file_lock_open::CREATE);
        CONJURE_REQUIRE_OK(lock);
        CHECK(lock->held());
        CHECK(lock->path() == lockPath);
        CHECK(std::filesystem::exists(lockPath));

        const result<file_lock> second = file_lock::try_acquire(lockPath, file_lock_open::EXISTING);
        CONJURE_REQUIRE_OK(second);
        CHECK_FALSE(second->held());

        CONJURE_CHECK_OK(lock->release_and_remove());
        CHECK_FALSE(std::filesystem::exists(lockPath));
    }

    TEST_CASE("file_lock - a child holding the lock keeps the parent out until it releases") {
        const testing::temp_directory directory("core_file_lock_child_hold");
        const std::filesystem::path lockPath = directory.file("instance.lock");
        process holder = start_holder(directory, lockPath);

        CONJURE_REQUIRE_OK(directory.write_text("release", ""));
        const result<process_result> finished = holder.wait();
        CONJURE_REQUIRE_OK(finished);
        CHECK(finished->exitCode == 0);
        CHECK(without_carriage_returns(finished->standardOutput) == "held\n");

        const file_lock taken = acquire_eventually(lockPath);
        CHECK(taken.held());
    }

    TEST_CASE("file_lock - the lock is released when the holder is killed") {
        const testing::temp_directory directory("core_file_lock_child_killed");
        const std::filesystem::path lockPath = directory.file("instance.lock");
        process holder = start_holder(directory, lockPath);

        holder.kill();
        const result<process_result> finished = holder.wait();
        CONJURE_REQUIRE_OK(finished);
        CHECK(finished->exitCode == process::KILLED_EXIT_CODE);

        const file_lock taken = acquire_eventually(lockPath);
        CHECK(taken.held());
    }

    TEST_CASE("file_lock - the lock is released when the holder exits without running destructors") {
        const testing::temp_directory directory("core_file_lock_child_abandon");
        const std::filesystem::path lockPath = directory.file("instance.lock");
        const std::filesystem::path ready = directory.file("ready");

        result<process> holder = process::start(
          child({"lock-abandon", detail::path_to_utf8(lockPath), detail::path_to_utf8(ready)})
        );
        CONJURE_REQUIRE_OK(holder);
        const result<process_result> finished = holder->wait();
        CONJURE_REQUIRE_OK(finished);
        CHECK(finished->exitCode == 0);
        CHECK(without_carriage_returns(finished->standardOutput) == "held\n");
        CHECK(std::filesystem::exists(ready));

        const file_lock taken = acquire_eventually(lockPath);
        CHECK(taken.held());
    }

    TEST_CASE("file_lock - a child cannot take the parent's lock and can after the parent releases") {
        const testing::temp_directory directory("core_file_lock_child_try");
        const std::filesystem::path lockPath = directory.file("instance.lock");
        result<file_lock> parent = file_lock::try_acquire(lockPath, file_lock_open::CREATE);
        CONJURE_REQUIRE_OK(parent);
        REQUIRE(parent->held());

        const result<process_result> whileHeld = process::run(child({"lock-try", detail::path_to_utf8(lockPath)}));
        CONJURE_REQUIRE_OK(whileHeld);
        CHECK(whileHeld->exitCode == 0);
        CHECK(without_carriage_returns(whileHeld->standardOutput) == "busy\n");

        parent->release();
        const result<process_result> afterRelease = process::run(child({"lock-try", detail::path_to_utf8(lockPath)}));
        CONJURE_REQUIRE_OK(afterRelease);
        CHECK(afterRelease->exitCode == 0);
        CHECK(without_carriage_returns(afterRelease->standardOutput) == "acquired\n");
    }

    TEST_CASE("file_lock - the lock handle is not inheritable even by a spawner that inherits everything") {
        const testing::temp_directory directory("core_file_lock_inherit");
        const std::filesystem::path lockPath = directory.file("instance.lock");
        result<file_lock> parent = file_lock::try_acquire(lockPath, file_lock_open::CREATE);
        CONJURE_REQUIRE_OK(parent);
        REQUIRE(parent->held());

        // The child only sleeps; it is killed at the end. An inherited copy of the handle (Windows) or of the
        // descriptor, which shares the open file description and its flock (POSIX), would keep the file locked
        // after the parent's release for as long as the child lives.
        constexpr int CHILD_SLEEP_MILLISECONDS = 60000;
        inheriting_child sleeper = {};
        REQUIRE(sleeper.start_sleeping(CHILD_SLEEP_MILLISECONDS));

        parent->release();
        const result<file_lock> again = file_lock::try_acquire(lockPath, file_lock_open::EXISTING);
        const bool childAliveAfterTry = sleeper.is_alive();
        CONJURE_REQUIRE_OK(again);
        CHECK(childAliveAfterTry);
        CHECK(again->held());

        sleeper.kill_and_wait();
    }
}
