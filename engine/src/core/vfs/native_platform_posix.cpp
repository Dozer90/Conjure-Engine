#include "core/vfs/native_platform.h"

#include "core/vfs/vfs_internal.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <format>
#include <limits>
#include <string>
#include <system_error>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::vfs;

/*===========================================================================*/
/*===========================================================================*/
// Helpers

namespace {
    int to_descriptor(const internal::read_handle &handle) noexcept {
        return static_cast<int>(handle.value());
    }

    error io_failure(std::string_view operation, std::string_view displayPath, int errorNumber) {
        const std::error_code code(errorNumber, std::generic_category());
        return make_error(error_code::IO_FAILURE, std::format("{} '{}': {}", operation, displayPath, code.message()));
    }

    // Writes all of 'data' at the descriptor's file position. Returns the errno value, 0 when every
    // byte was taken.
    int write_all(int descriptor, std::span<const std::byte> data) noexcept {
        // write takes a size_t but may legally transfer less than asked; the loop also keeps every
        // call within the range SSIZE_MAX so the return value stays representable.
        constexpr usize MAX_CHUNK = static_cast<usize>(std::numeric_limits<ssize_t>::max());
        usize total = 0;
        while (total < data.size()) {
            const usize chunk = std::min(data.size() - total, MAX_CHUNK);
            const ssize_t written = ::write(descriptor, data.data() + total, chunk);
            if (written < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return errno;
            }
            if (written == 0) {
                // A regular file takes at least one byte or fails; a call that claims success without
                // progress would otherwise spin here for ever.
                return EIO;
            }
            total += static_cast<usize>(written);
        }
        return 0;
    }

    // Pushes the file behind the descriptor to its storage device with fsync, on every POSIX platform
    // (decision C11 of 10_core.md). Returns the errno value, 0 on success.
    int flush_to_device(int descriptor) noexcept {
        int outcome = -1;
        do {
            outcome = ::fsync(descriptor);
        } while (outcome != 0 && errno == EINTR);
        return outcome == 0 ? 0 : errno;
    }

    // Read and write for the owner, nothing for the group or for others.
    constexpr mode_t OWNER_ONLY_MODE = S_IRUSR | S_IWUSR;

    // Every permission bit of the group and of others.
    constexpr mode_t GROUP_AND_OTHERS_MODE = S_IRWXG | S_IRWXO;

    // True for a regular file of the effective user that carries no permission bit for its group or for
    // others.
    bool is_owner_only(const struct stat &info) noexcept {
        const bool regular = S_ISREG(info.st_mode);
        const bool closedToOthers = (info.st_mode & GROUP_AND_OTHERS_MODE) == 0;
        return regular && closedToOthers && info.st_uid == ::geteuid();
    }

    // Closes and removes a file create_private_file made and cannot keep.
    void discard_new_file(int descriptor, const std::filesystem::path &path) noexcept {
        ::close(descriptor);
        ::unlink(path.c_str());
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Reading

result<internal::read_handle> internal::open_read_handle(
  const std::filesystem::path &path,
  std::string_view displayPath
) {
    int descriptor = -1;
    do {
        descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    } while (descriptor < 0 && errno == EINTR);
    if (descriptor < 0) {
        // The file may have been removed between the caller's stat and this open; that is still
        // "missing", not an operating system refusal.
        const int errorNumber = errno;
        if (errorNumber == ENOENT || errorNumber == ENOTDIR) {
            return not_found(displayPath);
        }
        return io_failure("open", displayPath, errorNumber);
    }
    return read_handle(static_cast<std::intptr_t>(descriptor));
}

result<u64> internal::read_handle_size(const read_handle &handle, std::string_view displayPath) {
    struct stat info = {};
    if (::fstat(to_descriptor(handle), &info) != 0) {
        return io_failure("determine the size of", displayPath, errno);
    }
    if (info.st_size < 0) {
        return u64{0};
    }
    return static_cast<u64>(info.st_size);
}

result<usize> internal::read_at(
  const read_handle &handle,
  u64 offset,
  std::span<std::byte> out,
  std::string_view displayPath
) {
    // pread takes a size_t but may legally transfer less than asked; the loop also keeps every
    // call within the range SSIZE_MAX so the return value stays representable.
    constexpr usize MAX_CHUNK = static_cast<usize>(std::numeric_limits<ssize_t>::max());
    usize total = 0;
    while (total < out.size()) {
        const usize chunk = std::min(out.size() - total, MAX_CHUNK);
        const off_t position = static_cast<off_t>(offset + total);
        const ssize_t read = ::pread(to_descriptor(handle), out.data() + total, chunk, position);
        if (read < 0) {
            if (errno == EINTR) {
                continue;
            }
            return io_failure("read", displayPath, errno);
        }
        if (read == 0) {
            break;
        }
        total += static_cast<usize>(read);
    }
    return total;
}

internal::refused_lookup internal::classify_refused_lookup(
  const std::filesystem::path &,
  const std::error_code &
) noexcept {
    return refused_lookup::REFUSED;
}

void internal::close_read_handle(std::intptr_t value) noexcept {
    ::close(static_cast<int>(value));
}


/*===========================================================================*/
/*===========================================================================*/
// Writing

result<void> internal::write_file_flushed(
  const std::filesystem::path &path,
  std::span<const std::byte> data,
  std::string_view displayPath
) {
    // 0666 less the process umask: the permissions std::ofstream gives a new file.
    constexpr mode_t NEW_FILE_MODE = 0666;
    int descriptor = -1;
    do {
        descriptor = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, NEW_FILE_MODE);
    } while (descriptor < 0 && errno == EINTR);
    if (descriptor < 0) {
        return io_failure("open for writing", displayPath, errno);
    }

    const int writeError = write_all(descriptor, data);
    if (writeError != 0) {
        ::close(descriptor);
        return io_failure("write", displayPath, writeError);
    }

    // The write above only reached the page cache; the flush returns once the device holds the file.
    const int flushError = flush_to_device(descriptor);
    ::close(descriptor);
    if (flushError == 0) {
        note_flushed_file();
        return result<void>::success();
    }
    if (flush_is_unsupported(static_cast<u32>(flushError))) {
        note_unflushable_volume(displayPath);
        return result<void>::success();
    }
    return io_failure("flush", displayPath, flushError);
}

bool internal::flush_is_unsupported(u32 platformError) noexcept {
    // What fsync answers for a file system that has no synchronisation at all. Everything else, EIO
    // and ENOSPC included, is a flush that was attempted and failed.
    return platformError == static_cast<u32>(EINVAL) || platformError == static_cast<u32>(ENOTSUP);
}


/*===========================================================================*/
/*===========================================================================*/
// Private files

result<void> internal::create_private_file(
  const std::filesystem::path &path,
  std::span<const std::byte> data,
  std::string_view displayPath
) {
    // O_EXCL refuses an entry that already exists, a symbolic link included, so the file is always a new
    // one that was never readable by anyone else.
    int descriptor = -1;
    do {
        descriptor = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, OWNER_ONLY_MODE);
    } while (descriptor < 0 && errno == EINTR);
    if (descriptor < 0) {
        return io_failure("create", displayPath, errno);
    }

    // The mode given to open already left out everyone but the owner, and a umask can only take more away;
    // fchmod gives the owner back what a strict umask removed. A file system that ignores both (FAT, a
    // mount with fixed permissions) is caught by the check that follows.
    const int modeError = ::fchmod(descriptor, OWNER_ONLY_MODE) == 0 ? 0 : errno;
    struct stat info = {};
    if (::fstat(descriptor, &info) != 0) {
        const int statError = errno;
        discard_new_file(descriptor, path);
        return io_failure("inspect", displayPath, statError);
    }
    if (!is_owner_only(info)) {
        discard_new_file(descriptor, path);
        const std::string reason = modeError != 0
          ? std::error_code(modeError, std::generic_category()).message()
          : std::string("the file system does not keep owner-only permissions");
        return make_error(
          error_code::UNSUPPORTED,
          std::format("'{}' cannot be restricted to its owner: {}", displayPath, reason)
        );
    }

    const int writeError = write_all(descriptor, data);
    if (writeError != 0) {
        discard_new_file(descriptor, path);
        return io_failure("write", displayPath, writeError);
    }
    if (::close(descriptor) != 0) {
        // The descriptor is gone either way; a deferred write error (a full disk, a lost network mount)
        // surfaces here, and the file may be short.
        const int closeError = errno;
        ::unlink(path.c_str());
        return io_failure("write", displayPath, closeError);
    }
    return result<void>::success();
}

result<bool> internal::is_private_file(const std::filesystem::path &path, std::string_view displayPath) {
    struct stat info = {};
    if (::lstat(path.c_str(), &info) != 0) {
        const int errorNumber = errno;
        if (errorNumber == ENOENT || errorNumber == ENOTDIR) {
            return not_found(displayPath);
        }
        return io_failure("inspect", displayPath, errorNumber);
    }
    return is_owner_only(info);
}


/*===========================================================================*/
/*===========================================================================*/
// Replacing

std::filesystem::path internal::extend_native_path(const std::filesystem::path &path) {
    // No classic path limit here: PATH_MAX applies to single calls and is far above what the engine builds.
    return path;
}

std::filesystem::path internal::always_extended_native_path(const std::filesystem::path &path) {
    return path;
}

result<void> internal::replace_file(const std::filesystem::path &source, const std::filesystem::path &target) {
    // POSIX rename(2) replaces an existing destination file atomically.
    std::error_code code = {};
    std::filesystem::rename(source, target, code);
    if (code) {
        return make_error(
          error_code::IO_FAILURE,
          std::format("replace '{}' with '{}': {}", path_to_utf8(target), path_to_utf8(source), code.message())
        );
    }
    return result<void>::success();
}

bool internal::rename_falls_back_to_move(u32) noexcept {
    // rename(2) is the only route here; there is nothing to fall back to.
    return false;
}
