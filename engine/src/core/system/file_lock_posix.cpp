// POSIX backend of core::file_lock: flock(LOCK_EX | LOCK_NB) on an O_CLOEXEC descriptor. flock locks the open file
// description on Linux and macOS alike, so two file_locks conflict even inside one process, as Windows handles do;
// fcntl record locks belong to the process instead, never conflict within it and are dropped when any descriptor
// of the file closes. Anyone may unlink a locked file, so after locking, and again before removing, the backend
// checks that the path still names the locked inode.

#include "conjure/core/file_lock.h"

#include "core/system/posix_utils.h"
#include "core/system/system_internal.h"

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <initializer_list>
#include <string_view>

using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// Descriptors and errors

namespace {
    // How often try_acquire reopens under CREATE when the locked file was unlinked or replaced before the identity
    // check, before it gives up with IO_FAILURE.
    constexpr int MAX_CREATE_ATTEMPTS = 8;

    constexpr mode_t LOCK_FILE_MODE = 0644;

    // Several errno names share a value on some systems (EAGAIN and EWOULDBLOCK, ENOTSUP and EOPNOTSUPP on Linux),
    // so neither a switch nor a chain of == comparisons can name them all portably.
    bool is_one_of(int code, std::initializer_list<int> candidates) noexcept {
        return std::find(candidates.begin(), candidates.end(), code) != candidates.end();
    }

    bool is_held_elsewhere(int code) noexcept {
        return is_one_of(code, {EWOULDBLOCK, EAGAIN});
    }

    // The file_lock mapping on top of error_code_from_errno. EINTR is retried before anything gets here, and
    // EWOULDBLOCK from flock is not an error: another holder has the file.
    error_code error_code_from_lock_errno(int code) noexcept {
        if (is_one_of(code, {ELOOP, EISDIR})) {
            return error_code::PERMISSION_DENIED;
        }
        if (is_one_of(code, {ENOLCK, EOPNOTSUPP, ENOTSUP})) {
            return error_code::UNSUPPORTED;
        }
        return detail::error_code_from_errno(code);
    }

    error lock_error(int code, std::string_view action, const std::filesystem::path &path) {
        // O_NOFOLLOW reports a symbolic link in the last component as ELOOP, whose system text ("too many levels of
        // symbolic links") would mislead.
        if (code == ELOOP) {
            return make_error(
              error_code::PERMISSION_DENIED,
              "file_lock: cannot {} '{}': it is a symbolic link",
              action,
              detail::path_to_utf8(path)
            );
        }
        return make_error(
          error_code_from_lock_errno(code),
          "file_lock: cannot {} '{}': {}",
          action,
          detail::path_to_utf8(path),
          detail::errno_message(code)
        );
    }

    // Never O_TRUNC: an existing file keeps its contents. O_NOFOLLOW refuses a symbolic link, so nothing is created
    // or locked through one.
    int open_retrying(const std::filesystem::path &path, file_lock_open openMode) noexcept {
        const int creation = openMode == file_lock_open::CREATE ? O_CREAT : 0;
        const int flags = O_RDWR | O_CLOEXEC | O_NOFOLLOW | creation;
        int descriptor = -1;
        do {
            descriptor = ::open(path.c_str(), flags, LOCK_FILE_MODE);
        } while (descriptor < 0 && errno == EINTR);
        return descriptor;
    }

    // 0 when the lock was taken, otherwise the errno of the failed flock.
    int lock_retrying(int descriptor) noexcept {
        while (::flock(descriptor, LOCK_EX | LOCK_NB) != 0) {
            const int lockError = errno;
            if (lockError != EINTR) {
                return lockError;
            }
        }
        return 0;
    }

    // True when 'path' still names the file open on 'descriptor' (same device and inode), false when the name is
    // gone or names another file.
    result<bool> names_open_file(int descriptor, const std::filesystem::path &path) {
        struct stat opened = {};
        if (::fstat(descriptor, &opened) != 0) {
            const int statError = errno;
            return lock_error(statError, "inspect", path);
        }

        struct stat named = {};
        if (::lstat(path.c_str(), &named) != 0) {
            const int statError = errno;
            if (statError == ENOENT || statError == ENOTDIR) {
                return false;
            }
            return lock_error(statError, "inspect", path);
        }
        return opened.st_dev == named.st_dev && opened.st_ino == named.st_ino;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Backend

result<std::intptr_t> detail::file_lock_open_native(const std::filesystem::path &path, file_lock_open open) {
    // Nothing on any path below removes a file: try_acquire never does, even one it has just created. A file
    // system without locks therefore keeps its (empty) lock file and reports UNSUPPORTED.
    for (int attempt = 0; attempt < MAX_CREATE_ATTEMPTS; ++attempt) {
        const int descriptor = open_retrying(path, open);
        if (descriptor < 0) {
            const int openError = errno;
            return lock_error(openError, "open", path);
        }

        const int lockError = lock_retrying(descriptor);
        if (lockError != 0) {
            ::close(descriptor);
            if (is_held_elsewhere(lockError)) {
                return detail::FILE_LOCK_NOT_HELD;
            }
            return lock_error(lockError, "lock", path);
        }

        // Between the open and the flock another holder may have removed the file (release_and_remove) or
        // replaced it; the lock then guards an inode nobody else can reach.
        const result<bool> stillNamed = names_open_file(descriptor, path);
        if (!stillNamed.has_value()) {
            ::close(descriptor);
            return stillNamed.get_error();
        }
        if (*stillNamed) {
            return static_cast<std::intptr_t>(descriptor);
        }

        ::close(descriptor);
        if (open == file_lock_open::EXISTING) {
            return make_error(
              error_code::NOT_FOUND,
              "file_lock: '{}' was removed while it was being locked",
              detail::path_to_utf8(path)
            );
        }
    }

    return make_error(
      error_code::IO_FAILURE,
      "file_lock: '{}' was replaced {} times while it was being locked",
      detail::path_to_utf8(path),
      MAX_CREATE_ATTEMPTS
    );
}

void detail::file_lock_close_native(std::intptr_t native) noexcept {
    // Closing the last descriptor of the open file description drops the flock.
    ::close(static_cast<int>(native));
}

result<void> detail::file_lock_remove_native(std::intptr_t native, const std::filesystem::path &path) {
    // The check and the unlink are two calls. Every file_lock removes a lock file only while holding it, so between
    // them only a process that ignores the lock can replace the name.
    const int descriptor = static_cast<int>(native);
    const result<bool> stillNamed = names_open_file(descriptor, path);
    if (!stillNamed.has_value()) {
        ::close(descriptor);
        return stillNamed.get_error();
    }
    if (!*stillNamed) {
        ::close(descriptor);
        return make_error(
          error_code::NOT_FOUND,
          "file_lock: '{}' no longer names the held file; nothing removed",
          detail::path_to_utf8(path)
        );
    }

    if (::unlink(path.c_str()) != 0) {
        const int unlinkError = errno;
        ::close(descriptor);
        return lock_error(unlinkError, "remove", path);
    }

    ::close(descriptor);
    return {};
}
