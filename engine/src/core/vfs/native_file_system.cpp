#include "conjure/core/vfs/native_file_system.h"

#include "conjure/core/log.h"
#include "conjure/core/uuid.h"
#include "conjure/core/vfs/path_utils.h"
#include "core/vfs/native_platform.h"
#include "core/vfs/vfs_internal.h"

#include <algorithm>
#include <format>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::vfs;

/*===========================================================================*/
/*===========================================================================*/
// Helpers

namespace {
    using internal::not_found;

    error is_a_directory(std::string_view path) {
        return make_error(error_code::INVALID_ARGUMENT, std::format("'{}' is a directory", path));
    }

    error read_only_error() {
        return make_error(error_code::PERMISSION_DENIED, "the file system is read-only");
    }

    std::filesystem::path strip_trailing_separator(std::filesystem::path path) {
        std::filesystem::path normalized = path.lexically_normal();
        if (normalized.has_filename() || normalized == normalized.root_path()) {
            return normalized;
        }
        // "a/b/" normalises to "a/b/" (an empty filename); drop that so operator/ never doubles up.
        return normalized.parent_path();
    }

    result<file_stat> stat_native(const std::filesystem::path &nativePath, std::string_view displayPath) {
        // status() reports the not-found family through the file type (whether or not it also
        // leaves a code behind, which differs between standard libraries); any other code means the
        // operating system refused to look (permissions, I/O error).
        std::error_code code = {};
        const std::filesystem::file_status status = std::filesystem::status(nativePath, code);
        if (status.type() == std::filesystem::file_type::not_found) {
            return not_found(displayPath);
        }
        if (code) {
            if (internal::is_being_deleted(nativePath, code)) {
                return not_found(displayPath);
            }
            return internal::io_error("stat", displayPath, code);
        }
        if (!std::filesystem::exists(status)) {
            return not_found(displayPath);
        }

        file_stat info = {};
        info.isDirectory = std::filesystem::is_directory(status);
        info.isRegular = std::filesystem::is_regular_file(status);
        if (info.isRegular) {
            const std::uintmax_t size = std::filesystem::file_size(nativePath, code);
            if (!code) {
                info.size = static_cast<u64>(size);
            }
        }
        code.clear();
        const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(nativePath, code);
        if (!code) {
            info.writeTimeNanoseconds = internal::file_time_to_nanoseconds(writeTime);
        }
        return info;
    }

    file_stat stat_entry(const std::filesystem::directory_entry &entry) {
        file_stat info = {};
        std::error_code code = {};
        info.isDirectory = entry.is_directory(code);
        code.clear();
        info.isRegular = entry.is_regular_file(code);
        if (info.isRegular) {
            code.clear();
            const std::uintmax_t size = entry.file_size(code);
            if (!code) {
                info.size = static_cast<u64>(size);
            }
        }
        code.clear();
        const std::filesystem::file_time_type writeTime = entry.last_write_time(code);
        if (!code) {
            info.writeTimeNanoseconds = internal::file_time_to_nanoseconds(writeTime);
        }
        return info;
    }

    bool directory_is_empty(const std::filesystem::path &nativePath) {
        std::error_code code = {};
        const std::filesystem::directory_iterator iterator(nativePath, code);
        return !code && iterator == std::filesystem::directory_iterator();
    }

    /// <summary>
    /// A regular file opened for reading through the platform layer. Reads are positional, so
    /// several threads may call read_at at once, and the handle shares deletion: the file can be
    /// replaced or removed while it is open.
    /// </summary>
    class native_file final : public file {
        private:
            internal::read_handle mHandle = {};
            u64 mSize = 0;
            std::string mDisplayPath = {};

        public:
            native_file(internal::read_handle handle, u64 size, std::string displayPath) :
                mHandle(std::move(handle)),
                mSize(size),
                mDisplayPath(std::move(displayPath)) {}

            ~native_file() override {}

        public:
            [[nodiscard]] result<usize> read_at(u64 offset, std::span<std::byte> out) override {
                if (offset > mSize) {
                    return make_error(
                      error_code::OUT_OF_RANGE,
                      std::format("offset {} is past the end of '{}' ({} bytes)", offset, mDisplayPath, mSize)
                    );
                }

                const u64 available = mSize - offset;
                const usize toRead = static_cast<usize>(std::min<u64>(available, out.size()));
                if (toRead == 0) {
                    return usize{0};
                }
                return internal::read_at(mHandle, offset, out.first(toRead), mDisplayPath);
            }

            [[nodiscard]] u64 size() const noexcept override {
                return mSize;
            }
    };
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

native_file_system::native_file_system(std::filesystem::path rootPath, bool readOnly) :
    mRoot(strip_trailing_separator(std::move(rootPath))),
    mReadOnly(readOnly) {}

native_file_system::~native_file_system() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

std::filesystem::path native_file_system::to_native(std::string_view path) const {
    std::string normalized = path_utils::normalize(path);
    if (normalized.starts_with('/')) {
        normalized.erase(0, 1);
    }
    if (normalized.empty()) {
        return internal::extend_native_path(mRoot);
    }
    // A project in a deep directory (or a cache under a test's temporary root) can pass the classic
    // Windows path limit; the extended form keeps every call below working (wave 4 integration).
    return internal::extend_native_path(mRoot / internal::path_from_utf8(normalized));
}

result<std::vector<std::byte>> native_file_system::read_file(std::string_view path) {
    CONJURE_TRY_ASSIGN(const std::filesystem::path nativePath, resolve(path));
    CONJURE_TRY_ASSIGN(const file_stat info, stat_native(nativePath, path));
    if (info.isDirectory) {
        return is_a_directory(path);
    }

    CONJURE_TRY_ASSIGN(const internal::read_handle handle, internal::open_read_handle(nativePath, path));

    // Size whatever the file holds now rather than trusting the stat: it may have changed since.
    CONJURE_TRY_ASSIGN(const u64 size, internal::read_handle_size(handle, path));
    std::vector<std::byte> bytes(static_cast<usize>(size));
    if (bytes.empty()) {
        return bytes;
    }

    // A file truncated between the two calls yields fewer bytes; the vector shrinks to match.
    CONJURE_TRY_ASSIGN(const usize read, internal::read_at(handle, 0, bytes, path));
    bytes.resize(read);
    return bytes;
}

result<void> native_file_system::write_file(std::string_view path, std::span<const std::byte> data) {
    CONJURE_TRY_ASSIGN(const std::filesystem::path nativePath, resolve_writable(path));
    return write_bytes(nativePath, path, data);
}

result<void> native_file_system::write_file_atomic(std::string_view path, std::span<const std::byte> data) {
    CONJURE_TRY_ASSIGN(const std::filesystem::path nativePath, resolve_writable(path));

    const std::string temporaryName = std::format(
      "{}.tmp-{}",
      internal::path_to_utf8(nativePath.filename()),
      uuid::generate().to_string()
    );
    // The destination came back from resolve_writable in the form its own length needs, but the
    // temporary is 41 characters longer: a target just under the classic Windows limit has a temporary
    // beyond it, which the standard library cannot open without the extended-length form (performance
    // round: the integration tests' scratch path in a build directory four characters longer failed
    // every meta write with "open for writing failed" while the shorter tree passed).
    const std::filesystem::path temporary =
      internal::extend_native_path(nativePath.parent_path() / internal::path_from_utf8(temporaryName));

    // Validate the destination before the temporary is created so a rejected call leaves
    // nothing behind.
    std::error_code ignored = {};
    const std::filesystem::file_status targetStatus = std::filesystem::status(nativePath, ignored);
    if (std::filesystem::is_directory(targetStatus)) {
        return is_a_directory(path);
    }

    const result<void> written = write_bytes(temporary, path, data);
    if (!written.has_value()) {
        std::filesystem::remove(temporary, ignored);
        return written.get_error();
    }

    const result<void> replaced = internal::replace_file(temporary, nativePath);
    if (!replaced.has_value()) {
        ignored.clear();
        std::filesystem::remove(temporary, ignored);
        if (ignored) {
            CONJURE_LOG_WARN(LOG_CORE, "vfs: could not remove temporary file '{}'", internal::path_to_utf8(temporary));
        }
    }
    return replaced;
}

result<file_stat> native_file_system::stat(std::string_view path) {
    CONJURE_TRY_ASSIGN(const std::filesystem::path nativePath, resolve(path));
    return stat_native(nativePath, path);
}

result<void> native_file_system::enumerate(
  std::string_view directory,
  enumerate_flags flags,
  const enumerate_callback &callback
) {
    CONJURE_TRY_ASSIGN(const std::string relative, internal::resolve_backend_path(directory));
    const std::filesystem::path nativePath = to_native(relative);
    CONJURE_TRY_ASSIGN(const file_stat info, stat_native(nativePath, directory));
    if (!info.isDirectory) {
        return make_error(error_code::INVALID_ARGUMENT, std::format("'{}' is not a directory", directory));
    }

    // The walk appends components below a root that may be short enough for the classic form while an
    // entry deep down is not: rooted in the extended form, one deeply nested asset cannot abort the scan.
    const std::filesystem::path walkRoot = internal::always_extended_native_path(nativePath);

    // Directory iteration order is unspecified, so entries are collected and sorted: every
    // caller sees the same order on every platform.
    std::vector<std::pair<std::string, file_stat>> entries = {};
    const auto collect = [&entries, &relative, &walkRoot, flags](const std::filesystem::directory_entry &entry) {
        const file_stat entryStat = stat_entry(entry);
        if (!internal::passes_kind_filter(flags, entryStat.isDirectory)) {
            return;
        }
        const std::string tail = internal::path_to_utf8(entry.path().lexically_relative(walkRoot));
        entries.emplace_back(path_utils::join(relative, tail), entryStat);
    };

    std::error_code code = {};
    constexpr std::filesystem::directory_options OPTIONS = std::filesystem::directory_options::skip_permission_denied;
    if (has_flag(flags, enumerate_flags::RECURSIVE)) {
        std::filesystem::recursive_directory_iterator iterator(walkRoot, OPTIONS, code);
        for (; !code && iterator != std::filesystem::recursive_directory_iterator(); iterator.increment(code)) {
            collect(*iterator);
        }
    } else {
        std::filesystem::directory_iterator iterator(walkRoot, OPTIONS, code);
        for (; !code && iterator != std::filesystem::directory_iterator(); iterator.increment(code)) {
            collect(*iterator);
        }
    }
    if (code) {
        return internal::io_error("enumerate", directory, code);
    }

    std::sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
        return left.first < right.first;
    });
    for (const auto &[entryPath, entryStat] : entries) {
        callback(entryPath, entryStat);
    }
    return result<void>::success();
}

result<void> native_file_system::rename(std::string_view from, std::string_view to) {
    CONJURE_TRY_ASSIGN(const std::string fromRelative, internal::resolve_backend_path(from));
    CONJURE_TRY_ASSIGN(const std::string toRelative, internal::resolve_backend_path(to));
    if (mReadOnly) {
        return read_only_error();
    }
    if (fromRelative.empty() || toRelative.empty()) {
        return make_error(error_code::INVALID_ARGUMENT, "the root of a file system cannot be renamed");
    }

    const std::filesystem::path source = to_native(fromRelative);
    const std::filesystem::path destination = to_native(toRelative);
    CONJURE_TRY_ASSIGN(const file_stat sourceStat, stat_native(source, from));
    if (fromRelative == toRelative) {
        return result<void>::success();
    }
    if (sourceStat.isDirectory && internal::is_same_or_below(toRelative, fromRelative)) {
        return make_error(error_code::INVALID_ARGUMENT, std::format("cannot move '{}' into itself", from));
    }

    std::error_code code = {};
    const std::filesystem::file_status destinationStatus = std::filesystem::status(destination, code);
    code.clear();
    if (std::filesystem::exists(destinationStatus) && std::filesystem::equivalent(source, destination, code)) {
        // The same entry under another spelling, which on a case-insensitive volume is a rename that
        // changes only the case: the plain rename does exactly that for files and directories alike.
        code.clear();
        std::filesystem::rename(source, destination, code);
        if (code) {
            return internal::io_error("rename", from, code);
        }
        return result<void>::success();
    }
    if (std::filesystem::is_directory(destinationStatus)) {
        return make_error(error_code::ALREADY_EXISTS, std::format("a directory exists at '{}'", to));
    }
    code.clear();
    if (!std::filesystem::is_directory(std::filesystem::status(destination.parent_path(), code))) {
        return make_error(error_code::NOT_FOUND, std::format("the parent of '{}' does not exist", to));
    }

    if (std::filesystem::exists(destinationStatus)) {
        // std::filesystem::rename replaces files on POSIX but not reliably on every Windows
        // configuration; the platform helper does on both.
        if (sourceStat.isDirectory) {
            return make_error(error_code::ALREADY_EXISTS, std::format("a file exists at '{}'", to));
        }
        return internal::replace_file(source, destination);
    }

    code.clear();
    std::filesystem::rename(source, destination, code);
    if (code) {
        return internal::io_error("rename", from, code);
    }
    return result<void>::success();
}

result<void> native_file_system::remove(std::string_view path) {
    CONJURE_TRY_ASSIGN(const std::string relative, internal::resolve_backend_path(path));
    if (mReadOnly) {
        return read_only_error();
    }
    if (relative.empty()) {
        return make_error(error_code::INVALID_ARGUMENT, "the root of a file system cannot be removed");
    }

    const std::filesystem::path nativePath = to_native(relative);
    CONJURE_TRY_ASSIGN(const file_stat info, stat_native(nativePath, path));
    if (info.isDirectory && !directory_is_empty(nativePath)) {
        return make_error(error_code::INVALID_STATE, std::format("directory '{}' is not empty", path));
    }

    std::error_code code = {};
    std::filesystem::remove(nativePath, code);
    if (code) {
        return internal::io_error("remove", path, code);
    }
    return result<void>::success();
}

result<void> native_file_system::remove_all(std::string_view path) {
    CONJURE_TRY_ASSIGN(const std::string relative, internal::resolve_backend_path(path));
    if (mReadOnly) {
        return read_only_error();
    }
    if (relative.empty()) {
        return make_error(error_code::INVALID_ARGUMENT, "the root of a file system cannot be removed");
    }

    const std::filesystem::path nativePath = to_native(relative);
    CONJURE_TRY(stat_native(nativePath, path));

    // remove_all walks the tree like enumerate does, so it starts from the extended form for the same reason.
    std::error_code code = {};
    std::filesystem::remove_all(internal::always_extended_native_path(nativePath), code);
    if (code) {
        return internal::io_error("remove_all", path, code);
    }
    return result<void>::success();
}

result<void> native_file_system::create_directories(std::string_view path) {
    CONJURE_TRY_ASSIGN(const std::string relative, internal::resolve_backend_path(path));
    if (mReadOnly) {
        return read_only_error();
    }

    const std::filesystem::path nativePath = to_native(relative);
    std::error_code code = {};
    const std::filesystem::file_status status = std::filesystem::status(nativePath, code);
    if (std::filesystem::is_directory(status)) {
        return result<void>::success();
    }
    if (std::filesystem::exists(status)) {
        return make_error(error_code::ALREADY_EXISTS, std::format("a file exists at '{}'", path));
    }

    code.clear();
    std::filesystem::create_directories(nativePath, code);
    if (code) {
        return internal::io_error("create directories", path, code);
    }
    return result<void>::success();
}

result<std::unique_ptr<file>> native_file_system::open_read(std::string_view path) {
    CONJURE_TRY_ASSIGN(const std::filesystem::path nativePath, resolve(path));
    CONJURE_TRY_ASSIGN(const file_stat info, stat_native(nativePath, path));
    if (info.isDirectory) {
        return is_a_directory(path);
    }

    CONJURE_TRY_ASSIGN(internal::read_handle handle, internal::open_read_handle(nativePath, path));
    CONJURE_TRY_ASSIGN(const u64 size, internal::read_handle_size(handle, path));
    std::unique_ptr<file> opened = std::make_unique<native_file>(std::move(handle), size, std::string(path));
    return opened;
}

bool native_file_system::is_read_only() const noexcept {
    return mReadOnly;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

result<std::filesystem::path> native_file_system::resolve(std::string_view path) const {
    CONJURE_TRY_ASSIGN(const std::string relative, internal::resolve_backend_path(path));
    return to_native(relative);
}

result<std::filesystem::path> native_file_system::resolve_writable(std::string_view path) const {
    CONJURE_TRY_ASSIGN(const std::string relative, internal::resolve_backend_path(path));
    if (mReadOnly) {
        return read_only_error();
    }
    if (relative.empty()) {
        return is_a_directory(path);
    }
    return to_native(relative);
}

result<void> native_file_system::write_bytes(
  const std::filesystem::path &nativePath,
  std::string_view displayPath,
  std::span<const std::byte> data
) const {
    // The display path is the caller's VFS path: the native one may carry the extended-length prefix on
    // Windows, whose root name differs from mRoot's, so lexically_relative would yield nothing (review 4).
    std::error_code code = {};
    const std::filesystem::file_status status = std::filesystem::status(nativePath, code);
    if (std::filesystem::is_directory(status)) {
        return is_a_directory(displayPath);
    }

    code.clear();
    const std::filesystem::path parent = nativePath.parent_path();
    const std::filesystem::file_status parentStatus = std::filesystem::status(parent, code);
    if (std::filesystem::exists(parentStatus) && !std::filesystem::is_directory(parentStatus)) {
        return make_error(error_code::IO_FAILURE, std::format("write '{}': a parent is not a directory", displayPath));
    }
    if (!std::filesystem::exists(parentStatus)) {
        code.clear();
        std::filesystem::create_directories(parent, code);
        if (code) {
            return internal::io_error("create the parent directories of", displayPath, code);
        }
    }

    std::ofstream stream(nativePath, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        return make_error(error_code::IO_FAILURE, std::format("open '{}' for writing failed", displayPath));
    }
    if (!data.empty()) {
        stream.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
    }
    stream.close();
    if (stream.fail()) {
        return make_error(error_code::IO_FAILURE, std::format("write '{}' failed", displayPath));
    }
    return result<void>::success();
}
