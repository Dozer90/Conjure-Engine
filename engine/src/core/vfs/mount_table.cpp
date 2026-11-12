#include "conjure/core/vfs/mount_table.h"

#include "conjure/core/log.h"
#include "conjure/core/vfs/path_utils.h"
#include "core/vfs/vfs_internal.h"

#include <algorithm>
#include <format>
#include <string>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::vfs;

/*===========================================================================*/
/*===========================================================================*/
// Helpers

namespace {
    constexpr std::string_view SCHEME_SEPARATOR = "://";

    bool is_scheme_character(char character) noexcept {
        const bool alpha = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
        const bool digit = character >= '0' && character <= '9';
        return alpha || digit || character == '-' || character == '_' || character == '.' || character == '+';
    }

    bool is_valid_scheme(std::string_view scheme) noexcept {
        if (scheme.empty()) {
            return false;
        }
        return std::all_of(scheme.begin(), scheme.end(), &is_scheme_character);
    }

    // Accepts "assets" and "assets://" alike.
    std::string_view strip_scheme_separator(std::string_view scheme) noexcept {
        if (scheme.ends_with(SCHEME_SEPARATOR)) {
            scheme.remove_suffix(SCHEME_SEPARATOR.size());
        }
        return scheme;
    }

    error invalid_scheme(std::string_view scheme) {
        return make_error(error_code::INVALID_ARGUMENT, std::format("'{}' is not a valid scheme", scheme));
    }

    std::string make_uri(std::string_view scheme, std::string_view path) {
        std::string uri(scheme);
        uri.append(SCHEME_SEPARATOR);
        uri.append(path);
        return uri;
    }

    bool failed_other_than_not_found(const result<file_stat> &outcome) noexcept {
        return !outcome.has_value() && outcome.get_error().generic_code() != error_code::NOT_FOUND;
    }

    // Every backend's own rename() refuses its root at either end with this code.
    error root_of_file_system(std::string_view uri) {
        return make_error(error_code::INVALID_ARGUMENT, std::format("'{}' is the root of a file system", uri));
    }

    // A copy between backends must fail where a real rename would. write_file_atomic reports a
    // directory at the destination as INVALID_ARGUMENT and creates missing parents, so both are
    // checked before anything is written; the root, which stat reports as a directory, is
    // INVALID_ARGUMENT as in a real rename. 'path' is relative to 'backend', which must not be a
    // mount table. Returns whether a file already exists at the destination.
    result<bool> check_copy_destination(file_system &backend, std::string_view path, std::string_view uri) {
        CONJURE_TRY_ASSIGN(const std::string normalized, internal::resolve_backend_path(path));
        if (normalized.empty()) {
            return root_of_file_system(uri);
        }

        const result<file_stat> existing = backend.stat(path);
        if (existing.has_value() && existing->isDirectory) {
            return make_error(error_code::ALREADY_EXISTS, std::format("a directory exists at '{}'", uri));
        }
        if (failed_other_than_not_found(existing)) {
            return existing.get_error();
        }

        const std::string_view parentPath = path_utils::parent(normalized);
        if (parentPath.empty()) {
            return existing.has_value();
        }

        const result<file_stat> parentStat = backend.stat(parentPath);
        if (failed_other_than_not_found(parentStat)) {
            return parentStat.get_error();
        }
        if (!parentStat.has_value() || !parentStat->isDirectory) {
            return make_error(error_code::NOT_FOUND, std::format("the parent of '{}' does not exist", uri));
        }
        return existing.has_value();
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

mount_table::mount_table() {}

mount_table::~mount_table() {}


/*===========================================================================*/
/*===========================================================================*/
// Mount management

result<void> mount_table::mount(std::string_view scheme, std::shared_ptr<file_system> backend) {
    const std::string_view name = strip_scheme_separator(scheme);
    if (!is_valid_scheme(name)) {
        return invalid_scheme(scheme);
    }
    if (backend == nullptr) {
        return make_error(error_code::INVALID_ARGUMENT, std::format("no backend given for scheme '{}'", name));
    }
    if (backend.get() == this || contains(*backend)) {
        return make_error(error_code::INVALID_ARGUMENT, "a mount table cannot mount itself, directly or indirectly");
    }

    const std::scoped_lock lock(mMutex);
    const bool inserted = mMounts.try_emplace(std::string(name), std::move(backend)).second;
    if (!inserted) {
        return make_error(error_code::ALREADY_EXISTS, std::format("scheme '{}' is already mounted", name));
    }
    return result<void>::success();
}

result<void> mount_table::unmount(std::string_view scheme) {
    const std::string_view name = strip_scheme_separator(scheme);

    // The backend is released outside the lock: its destructor may be arbitrarily expensive.
    std::shared_ptr<file_system> released = nullptr;
    {
        const std::scoped_lock lock(mMutex);
        const auto found = mMounts.find(name);
        if (found == mMounts.end()) {
            return make_error(error_code::NOT_FOUND, std::format("scheme '{}' is not mounted", name));
        }
        released = std::move(found->second);
        mMounts.erase(found);
    }
    return result<void>::success();
}

file_system *mount_table::find_mount(std::string_view scheme) const noexcept {
    const std::scoped_lock lock(mMutex);
    const auto found = mMounts.find(strip_scheme_separator(scheme));
    if (found == mMounts.end()) {
        return nullptr;
    }
    return found->second.get();
}

std::vector<std::string> mount_table::schemes() const {
    const std::scoped_lock lock(mMutex);
    std::vector<std::string> names = {};
    names.reserve(mMounts.size());
    for (const auto &[name, backend] : mMounts) {
        names.push_back(name);
    }
    return names;
}

result<std::pair<std::string_view, std::string_view>> mount_table::split(std::string_view uri) {
    const usize separator = uri.find(SCHEME_SEPARATOR);
    if (separator == std::string_view::npos) {
        return make_error(
          error_code::INVALID_ARGUMENT,
          std::format("'{}' has no scheme (expected scheme://path)", uri)
        );
    }

    const std::string_view scheme = uri.substr(0, separator);
    if (!is_valid_scheme(scheme)) {
        return invalid_scheme(scheme);
    }
    return std::pair<std::string_view, std::string_view>(scheme, uri.substr(separator + SCHEME_SEPARATOR.size()));
}


/*===========================================================================*/
/*===========================================================================*/
// Routed file_system methods

result<std::vector<std::byte>> mount_table::read_file(std::string_view uri) {
    CONJURE_TRY_ASSIGN(const route_target target, route(uri));
    return target.backend->read_file(target.path);
}

result<std::string> mount_table::read_text_file(std::string_view uri) {
    CONJURE_TRY_ASSIGN(const route_target target, route(uri));
    return target.backend->read_text_file(target.path);
}

result<void> mount_table::write_file(std::string_view uri, std::span<const std::byte> data) {
    CONJURE_TRY_ASSIGN(const route_target target, route(uri));
    return target.backend->write_file(target.path, data);
}

result<void> mount_table::write_file_atomic(std::string_view uri, std::span<const std::byte> data) {
    CONJURE_TRY_ASSIGN(const route_target target, route(uri));
    return target.backend->write_file_atomic(target.path, data);
}

result<file_stat> mount_table::stat(std::string_view uri) {
    CONJURE_TRY_ASSIGN(const route_target target, route(uri));
    return target.backend->stat(target.path);
}

bool mount_table::exists(std::string_view uri) {
    const result<route_target> target = route(uri);
    if (!target.has_value()) {
        return false;
    }
    return target->backend->exists(target->path);
}

result<void> mount_table::enumerate(
  std::string_view directory,
  enumerate_flags flags,
  const enumerate_callback &callback
) {
    CONJURE_TRY_ASSIGN(const route_target target, route(directory));

    const std::string_view scheme = target.scheme;
    const enumerate_callback prefixed = [&callback, scheme](std::string_view path, const file_stat &info) {
        callback(make_uri(scheme, path), info);
    };
    return target.backend->enumerate(target.path, flags, prefixed);
}

result<void> mount_table::rename(std::string_view from, std::string_view to) {
    CONJURE_TRY_ASSIGN(const route_target source, route(from));
    CONJURE_TRY_ASSIGN(const route_target destination, route(to));

    // Both sides are followed down to the backends that store them, so every check below asks the
    // backend that holds the file rather than a nested mount table in front of it.
    CONJURE_TRY_ASSIGN(const route_target origin, route_to_storage(source));
    CONJURE_TRY_ASSIGN(const route_target storage, route_to_storage(destination));

    // One backend may sit behind several schemes, directly or through nested mount tables, so the
    // decision is by backend, not by scheme: copy-and-remove on the same backend would delete a file
    // renamed onto itself.
    if (origin.backend == storage.backend) {
        return origin.backend->rename(origin.path, storage.path);
    }

    // Across backends there is nothing to rename, so a file is copied and the original removed.
    CONJURE_TRY_ASSIGN(const std::string sourcePath, internal::resolve_backend_path(origin.path));
    if (sourcePath.empty()) {
        return root_of_file_system(from);
    }
    CONJURE_TRY_ASSIGN(const file_stat sourceStat, origin.backend->stat(origin.path));
    if (sourceStat.isDirectory) {
        return make_error(
          error_code::UNSUPPORTED,
          std::format("'{}' is a directory; directories cannot be moved between mounts", from)
        );
    }
    if (origin.backend->is_read_only()) {
        return make_error(error_code::PERMISSION_DENIED, std::format("the file system behind '{}' is read-only", from));
    }

    CONJURE_TRY_ASSIGN(const bool replacesFile, check_copy_destination(*storage.backend, storage.path, to));
    CONJURE_TRY_ASSIGN(const std::vector<std::byte> bytes, origin.backend->read_file(origin.path));
    CONJURE_TRY(storage.backend->write_file_atomic(storage.path, bytes));

    const result<void> removed = origin.backend->remove(origin.path);
    if (removed.has_value()) {
        return removed;
    }

    // A copy this call created is taken back so the failure leaves only the source. A file it
    // replaced cannot be restored; the copy stays in its place.
    if (!replacesFile) {
        const result<void> undone = storage.backend->remove(storage.path);
        if (!undone.has_value()) {
            CONJURE_LOG_WARN(
              LOG_CORE,
              "vfs: could not remove the copy at '{}' after '{}' could not be removed: {}",
              to,
              from,
              undone.get_error().message()
            );
        }
    }
    return removed;
}

result<void> mount_table::remove(std::string_view uri) {
    CONJURE_TRY_ASSIGN(const route_target target, route(uri));
    return target.backend->remove(target.path);
}

result<void> mount_table::remove_all(std::string_view uri) {
    CONJURE_TRY_ASSIGN(const route_target target, route(uri));
    return target.backend->remove_all(target.path);
}

result<void> mount_table::create_directories(std::string_view uri) {
    CONJURE_TRY_ASSIGN(const route_target target, route(uri));
    return target.backend->create_directories(target.path);
}

result<std::unique_ptr<file>> mount_table::open_read(std::string_view uri) {
    CONJURE_TRY_ASSIGN(const route_target target, route(uri));
    return target.backend->open_read(target.path);
}

bool mount_table::is_read_only() const noexcept {
    // The backends are asked after the lock is released so no table -> backend lock order exists.
    // Taking the mutex or growing the snapshot cannot fail in practice; if it does, terminating is
    // the intended outcome of this noexcept method.
    const std::vector<std::shared_ptr<file_system>> backends = snapshot_backends();
    if (backends.empty()) {
        return false;
    }
    return std::all_of(backends.begin(), backends.end(), [](const std::shared_ptr<file_system> &backend) {
        return backend->is_read_only();
    });
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

result<mount_table::route_target> mount_table::route(std::string_view uri) const {
    using split_result = std::pair<std::string_view, std::string_view>;
    CONJURE_TRY_ASSIGN(const split_result parts, split(uri));

    route_target target = {};
    target.backend = find_shared(parts.first);
    if (target.backend == nullptr) {
        return make_error(error_code::NOT_FOUND, std::format("scheme '{}' is not mounted", parts.first));
    }
    target.scheme = parts.first;
    target.path = parts.second;
    return target;
}

result<mount_table::route_target> mount_table::route_to_storage(route_target target) {
    // A mount table mounted in this one takes URIs, which path_utils cannot take apart and which
    // hide the backend behind them, so the target is followed down to the backend that stores it.
    const mount_table *nested = dynamic_cast<const mount_table *>(target.backend.get());
    while (nested != nullptr) {
        CONJURE_TRY_ASSIGN(target, nested->route(target.path));
        nested = dynamic_cast<const mount_table *>(target.backend.get());
    }
    return target;
}

std::shared_ptr<file_system> mount_table::find_shared(std::string_view scheme) const {
    const std::scoped_lock lock(mMutex);
    const auto found = mMounts.find(scheme);
    if (found == mMounts.end()) {
        return nullptr;
    }
    return found->second;
}

std::vector<std::shared_ptr<file_system>> mount_table::snapshot_backends() const {
    const std::scoped_lock lock(mMutex);
    std::vector<std::shared_ptr<file_system>> backends = {};
    backends.reserve(mMounts.size());
    for (const auto &[name, backend] : mMounts) {
        backends.push_back(backend);
    }
    return backends;
}

bool mount_table::contains(const file_system &backend) const {
    // Only another mount table can lead back here; its own snapshot is walked the same way.
    const mount_table *const nested = dynamic_cast<const mount_table *>(&backend);
    if (nested == nullptr) {
        return false;
    }
    const std::vector<std::shared_ptr<file_system>> inner = nested->snapshot_backends();
    for (const std::shared_ptr<file_system> &candidate : inner) {
        if (candidate.get() == this || contains(*candidate)) {
            return true;
        }
    }
    return false;
}
