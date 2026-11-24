// The platform-independent half of core::file_lock: argument checks, ownership and moves. The operating
// system calls live in file_lock_windows.cpp and file_lock_posix.cpp.

#include "conjure/core/file_lock.h"

#include "core/system/system_internal.h"

#include <utility>

using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

file_lock::file_lock() {}

file_lock::~file_lock() {
    release();
}

file_lock::file_lock(file_lock &&other) noexcept :
    mNative(std::exchange(other.mNative, INVALID_NATIVE)),
    mPath(std::move(other.mPath)) {
    other.mPath.clear();
}

file_lock &file_lock::operator=(file_lock &&other) noexcept {
    if (this == &other) {
        return *this;
    }

    release();
    mNative = std::exchange(other.mNative, INVALID_NATIVE);
    mPath = std::move(other.mPath);
    other.mPath.clear();
    return *this;
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

result<file_lock> file_lock::try_acquire(const std::filesystem::path &path, file_lock_open open) {
    if (path.empty()) {
        return error(error_code::INVALID_ARGUMENT, "file_lock: the path is empty");
    }
    if (path.native().find(std::filesystem::path::value_type{}) != std::filesystem::path::string_type::npos) {
        return make_error(
          error_code::INVALID_ARGUMENT,
          "file_lock: the path '{}' contains a NUL character",
          detail::path_to_utf8(path)
        );
    }
    if (open != file_lock_open::CREATE && open != file_lock_open::EXISTING) {
        return make_error(
          error_code::INVALID_ARGUMENT,
          "file_lock: unknown open mode {} for '{}'",
          static_cast<u32>(open),
          detail::path_to_utf8(path)
        );
    }

    result<std::intptr_t> native = detail::file_lock_open_native(path, open);
    if (!native.has_value()) {
        return std::move(native).get_error();
    }

    // Held elsewhere is a success that holds nothing.
    static_assert(INVALID_NATIVE == detail::FILE_LOCK_NOT_HELD, "the backend's 'not held' must be INVALID_NATIVE");
    file_lock lock = {};
    if (*native == INVALID_NATIVE) {
        return lock;
    }
    lock.mNative = *native;
    lock.mPath = path;
    return lock;
}

bool file_lock::held() const noexcept {
    return mNative != INVALID_NATIVE;
}

const std::filesystem::path &file_lock::path() const noexcept {
    return mPath;
}

void file_lock::release() noexcept {
    if (!held()) {
        return;
    }

    detail::file_lock_close_native(std::exchange(mNative, INVALID_NATIVE));
    mPath.clear();
}

result<void> file_lock::release_and_remove() {
    if (!held()) {
        return error(error_code::INVALID_STATE, "file_lock::release_and_remove: nothing is held");
    }

    // The object holds nothing from here on, whatever the backend reports: file_lock_remove_native closes the
    // native handle or descriptor on every outcome.
    const std::intptr_t native = std::exchange(mNative, INVALID_NATIVE);
    const std::filesystem::path heldPath = std::exchange(mPath, std::filesystem::path());
    return detail::file_lock_remove_native(native, heldPath);
}
