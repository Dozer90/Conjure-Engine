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

bool internal::is_being_deleted(const std::filesystem::path &, const std::error_code &) noexcept {
    return false;
}

void internal::close_read_handle(std::intptr_t value) noexcept {
    ::close(static_cast<int>(value));
}


/*===========================================================================*/
/*===========================================================================*/
// Replacing

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
