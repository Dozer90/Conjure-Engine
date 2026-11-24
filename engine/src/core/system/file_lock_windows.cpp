// Windows backend of core::file_lock. The file is opened with DELETE access and without delete sharing, so a
// second file_lock's open fails with ERROR_SHARING_VIOLATION and nobody can delete or rename the file while it is
// held: the share mode is the lock. LockFileEx over the whole file is a second layer for tools that open the
// file without DELETE access and for SMB servers, which enforce byte-range locks across machines.

#include "conjure/core/file_lock.h"

#include "core/system/system_internal.h"
#include "core/system/win32_utils.h"

#include <Windows.h>

#include <string_view>

using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// Handles and errors

namespace {
    HANDLE to_handle(std::intptr_t native) noexcept {
        return reinterpret_cast<HANDLE>(native);
    }

    std::intptr_t to_native(HANDLE handle) noexcept {
        return reinterpret_cast<std::intptr_t>(handle);
    }

    // The file_lock mapping on top of error_code_from_win32. ERROR_SHARING_VIOLATION from the open and
    // ERROR_LOCK_VIOLATION from LockFileEx never reach it: they mean another holder has the file.
    error_code error_code_from_lock_error(DWORD code) noexcept {
        switch (code) {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
            case ERROR_DELETE_PENDING: {
                return error_code::NOT_FOUND;
            }
            case ERROR_ACCESS_DENIED: {
                return error_code::PERMISSION_DENIED;
            }
            default: {
                return detail::error_code_from_win32(code);
            }
        }
    }

    error lock_error(DWORD code, std::string_view action, const std::filesystem::path &path) {
        return make_error(
          error_code_from_lock_error(code),
          "file_lock: cannot {} '{}': {}",
          action,
          detail::path_to_utf8(path),
          detail::win32_error_message(code)
        );
    }

    // Closing the handle drops its locks too, but Windows may do that a moment later; the explicit unlock makes
    // the release immediate. It fails harmlessly (ERROR_NOT_LOCKED) where LockFileEx was unsupported.
    void unlock_and_close(HANDLE handle) noexcept {
        OVERLAPPED wholeFile = {};
        UnlockFileEx(handle, 0, MAXDWORD, MAXDWORD, &wholeFile);
        CloseHandle(handle);
    }

    // Marks the open file for deletion. The POSIX-semantics disposition removes the name at once (Windows 10 1809
    // and later, NTFS); elsewhere the classic disposition removes it once the last handle to the file has closed.
    BOOL mark_for_deletion(HANDLE handle) noexcept {
        FILE_DISPOSITION_INFO_EX immediate = {};
        immediate.Flags = FILE_DISPOSITION_FLAG_DELETE | FILE_DISPOSITION_FLAG_POSIX_SEMANTICS;
        const DWORD immediateSize = static_cast<DWORD>(sizeof(immediate));
        if (SetFileInformationByHandle(handle, FileDispositionInfoEx, &immediate, immediateSize)) {
            return TRUE;
        }

        const DWORD code = GetLastError();
        if (code != ERROR_INVALID_PARAMETER && code != ERROR_NOT_SUPPORTED && code != ERROR_INVALID_FUNCTION) {
            SetLastError(code);
            return FALSE;
        }

        FILE_DISPOSITION_INFO onClose = {};
        onClose.DeleteFile = TRUE;
        const DWORD onCloseSize = static_cast<DWORD>(sizeof(onClose));
        return SetFileInformationByHandle(handle, FileDispositionInfo, &onClose, onCloseSize);
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Backend

result<std::intptr_t> detail::file_lock_open_native(const std::filesystem::path &path, file_lock_open open) {
    // OPEN_ALWAYS creates a missing file and never truncates an existing one. The null security attributes make
    // the handle non-inheritable, so no child keeps the file open, however it was spawned.
    const DWORD disposition = open == file_lock_open::CREATE ? OPEN_ALWAYS : OPEN_EXISTING;
    const HANDLE handle = CreateFileW(
      path.c_str(),
      GENERIC_READ | GENERIC_WRITE | DELETE,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      nullptr,
      disposition,
      FILE_ATTRIBUTE_NORMAL,
      nullptr
    );
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD openError = GetLastError();
        if (openError == ERROR_SHARING_VIOLATION) {
            return detail::FILE_LOCK_NOT_HELD;
        }
        return lock_error(openError, "open", path);
    }

    OVERLAPPED wholeFile = {};
    if (LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, MAXDWORD, MAXDWORD, &wholeFile)) {
        return to_native(handle);
    }

    // A file system without byte-range locks: the handle stays and the file is held, because the share mode
    // already keeps every other file_lock out. Closing it here would leave the caller's folder without a held
    // lock file.
    const DWORD lockError = GetLastError();
    if (lockError == ERROR_NOT_SUPPORTED || lockError == ERROR_INVALID_FUNCTION) {
        return to_native(handle);
    }

    // The file stays: try_acquire never removes a file, even one it has just created.
    CloseHandle(handle);
    if (lockError == ERROR_LOCK_VIOLATION) {
        return detail::FILE_LOCK_NOT_HELD;
    }
    return lock_error(lockError, "lock", path);
}

void detail::file_lock_close_native(std::intptr_t native) noexcept {
    unlock_and_close(to_handle(native));
}

result<void> detail::file_lock_remove_native(std::intptr_t native, const std::filesystem::path &path) {
    const HANDLE handle = to_handle(native);
    if (!mark_for_deletion(handle)) {
        const DWORD removeError = GetLastError();
        unlock_and_close(handle);
        return lock_error(removeError, "remove", path);
    }

    unlock_and_close(handle);
    return {};
}
