#include "core/vfs/native_platform.h"

#include "core/vfs/vfs_internal.h"

#include <Windows.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <format>
#include <string>
#include <string_view>
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
    std::string last_error_text(DWORD errorCode) {
        wchar_t *buffer = nullptr;
        const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
        wchar_t *const bufferTarget = reinterpret_cast<wchar_t *>(&buffer);
        const DWORD length = FormatMessageW(flags, nullptr, errorCode, 0, bufferTarget, 0, nullptr);
        if (length == 0 || buffer == nullptr) {
            return std::format("error {}", errorCode);
        }

        std::wstring_view wide(buffer, length);
        while (!wide.empty() && (wide.back() == L'\r' || wide.back() == L'\n' || wide.back() == L' ')) {
            wide.remove_suffix(1);
        }

        std::string text = {};
        const int wideLength = static_cast<int>(wide.size());
        const int required = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLength, nullptr, 0, nullptr, nullptr);
        if (required > 0) {
            text.resize(static_cast<usize>(required));
            WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLength, text.data(), required, nullptr, nullptr);
        }
        LocalFree(buffer);

        if (text.empty()) {
            return std::format("error {}", errorCode);
        }
        return std::format("{} (error {})", text, errorCode);
    }

    // A delete-pending file refuses even an open that asks for no access at all, whereas a file
    // protected by its ACL still grants that (only the parent's traverse right is needed).
    bool is_delete_pending(const std::filesystem::path &path) noexcept {
        const DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
        const DWORD flags = FILE_FLAG_BACKUP_SEMANTICS;
        const HANDLE probe = CreateFileW(path.c_str(), 0, share, nullptr, OPEN_EXISTING, flags, nullptr);
        if (probe != INVALID_HANDLE_VALUE) {
            CloseHandle(probe);
            return false;
        }
        const DWORD errorCode = GetLastError();
        const bool gone = errorCode == ERROR_FILE_NOT_FOUND || errorCode == ERROR_PATH_NOT_FOUND;
        return errorCode == ERROR_ACCESS_DENIED || gone;
    }

    HANDLE to_handle(const internal::read_handle &handle) noexcept {
        return reinterpret_cast<HANDLE>(handle.value());
    }

    error io_failure(std::string_view operation, std::string_view displayPath, DWORD errorCode) {
        return make_error(
          error_code::IO_FAILURE,
          std::format("{} '{}': {}", operation, displayPath, last_error_text(errorCode))
        );
    }

    // The NT object-manager spelling of an absolute Win32 path: "C:\x" -> "\??\C:\x",
    // "\\server\share\x" -> "\??\UNC\server\share\x", "\\?\C:\x" -> "\??\C:\x".
    std::wstring to_nt_path(const std::filesystem::path &path) {
        std::error_code ignored = {};
        std::filesystem::path absolute = std::filesystem::absolute(path, ignored);
        if (ignored) {
            absolute = path;
        }
        const std::wstring native = absolute.lexically_normal().native();

        constexpr std::wstring_view EXTENDED_PREFIX = L"\\\\?\\";
        constexpr std::wstring_view UNC_PREFIX = L"\\\\";
        if (native.starts_with(EXTENDED_PREFIX)) {
            return L"\\??\\" + native.substr(EXTENDED_PREFIX.size());
        }
        if (native.starts_with(UNC_PREFIX)) {
            return L"\\??\\UNC\\" + native.substr(UNC_PREFIX.size());
        }
        return L"\\??\\" + native;
    }

    // Renames through SetFileInformationByHandle with POSIX semantics, which succeeds even while
    // another handle (opened with delete sharing) still refers to the target. Returns the Win32
    // error code, 0 on success; ERROR_INVALID_PARAMETER / ERROR_NOT_SUPPORTED mean the volume or
    // the operating system cannot do it and the caller should fall back to MoveFileExW.
    DWORD rename_posix(const std::filesystem::path &source, const std::filesystem::path &target) {
        const DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
        const HANDLE sourceHandle = CreateFileW(
          source.c_str(),
          DELETE | SYNCHRONIZE,
          share,
          nullptr,
          OPEN_EXISTING,
          FILE_FLAG_BACKUP_SEMANTICS,
          nullptr
        );
        if (sourceHandle == INVALID_HANDLE_VALUE) {
            return GetLastError();
        }

        const std::wstring targetName = to_nt_path(target);
        const usize nameBytes = targetName.size() * sizeof(wchar_t);
        const usize headerBytes = offsetof(FILE_RENAME_INFO, FileName);
        std::vector<std::byte> buffer(headerBytes + nameBytes + sizeof(wchar_t), std::byte{0});

        // The structure ends in a flexible array, so it is assembled in a byte buffer: the fixed
        // header is copied in, then the wide name right behind it.
        FILE_RENAME_INFO header = {};
        header.Flags = FILE_RENAME_FLAG_REPLACE_IF_EXISTS | FILE_RENAME_FLAG_POSIX_SEMANTICS;
        header.RootDirectory = nullptr;
        header.FileNameLength = static_cast<DWORD>(nameBytes);
        std::memcpy(buffer.data(), &header, headerBytes);
        std::memcpy(buffer.data() + headerBytes, targetName.data(), nameBytes);

        const DWORD bufferSize = static_cast<DWORD>(buffer.size());
        const BOOL renamed = SetFileInformationByHandle(sourceHandle, FileRenameInfoEx, buffer.data(), bufferSize);
        const DWORD errorCode = renamed != 0 ? 0 : GetLastError();
        CloseHandle(sourceHandle);
        return errorCode;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Reading

result<internal::read_handle> internal::open_read_handle(
  const std::filesystem::path &path,
  std::string_view displayPath
) {
    // FILE_SHARE_DELETE is what std::ifstream lacks: without it every rename over or removal of
    // the file fails for as long as the handle is open.
    const DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    const HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ, share, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        // The file may have been removed between the caller's stat and this open; that is still
        // "missing", not an operating system refusal.
        const DWORD errorCode = GetLastError();
        if (errorCode == ERROR_FILE_NOT_FOUND || errorCode == ERROR_PATH_NOT_FOUND) {
            return not_found(displayPath);
        }
        if (errorCode == ERROR_ACCESS_DENIED && is_delete_pending(path)) {
            return not_found(displayPath);
        }
        return io_failure("open", displayPath, errorCode);
    }
    return read_handle(reinterpret_cast<std::intptr_t>(handle));
}

result<u64> internal::read_handle_size(const read_handle &handle, std::string_view displayPath) {
    LARGE_INTEGER size = {};
    if (GetFileSizeEx(to_handle(handle), &size) == 0) {
        return io_failure("determine the size of", displayPath, GetLastError());
    }
    return static_cast<u64>(size.QuadPart);
}

result<usize> internal::read_at(
  const read_handle &handle,
  u64 offset,
  std::span<std::byte> out,
  std::string_view displayPath
) {
    // ReadFile moves at most a DWORD per call; large spans are read in pieces.
    constexpr usize MAX_CHUNK = usize{1} << 30;
    usize total = 0;
    while (total < out.size()) {
        const u64 position = offset + total;
        const DWORD chunk = static_cast<DWORD>(std::min(out.size() - total, MAX_CHUNK));

        OVERLAPPED overlapped = {};
        overlapped.Offset = static_cast<DWORD>(position & 0xFFFF'FFFFull);
        overlapped.OffsetHigh = static_cast<DWORD>(position >> 32);

        DWORD read = 0;
        if (ReadFile(to_handle(handle), out.data() + total, chunk, &read, &overlapped) == 0) {
            const DWORD errorCode = GetLastError();
            if (errorCode == ERROR_HANDLE_EOF) {
                break;
            }
            return io_failure("read", displayPath, errorCode);
        }
        if (read == 0) {
            break;
        }
        total += read;
    }
    return total;
}

bool internal::is_being_deleted(const std::filesystem::path &path, const std::error_code &code) noexcept {
    if (code != std::errc::permission_denied) {
        return false;
    }
    return is_delete_pending(path);
}

void internal::close_read_handle(std::intptr_t value) noexcept {
    CloseHandle(reinterpret_cast<HANDLE>(value));
}


/*===========================================================================*/
/*===========================================================================*/
// Replacing

std::filesystem::path internal::extend_native_path(const std::filesystem::path &path) {
    // MAX_PATH (260) counts the terminating character and CreateDirectoryW keeps 12 characters for a
    // short file name, so the classic limit for a directory component is 248; everything from there on
    // goes through the extended-length form, which Win32 accepts up to about 32,767 characters.
    constexpr usize CLASSIC_LIMIT = 248;
    constexpr std::wstring_view EXTENDED_PREFIX = L"\\\\?\\";
    constexpr std::wstring_view DEVICE_PREFIX = L"\\\\.\\";
    constexpr std::wstring_view UNC_PREFIX = L"\\\\";

    const std::wstring &native = path.native();
    if (native.starts_with(EXTENDED_PREFIX) || native.starts_with(DEVICE_PREFIX)) {
        // Already a literal form (a user-supplied root, or one derived from native_path() of a long path).
        // Win32 does not translate '/' inside it, so the components the VFS appended with forward slashes
        // must still be normalised (review 4: an extended root failed every stat and read of a nested path).
        return path.lexically_normal().make_preferred();
    }
    if (native.size() < CLASSIC_LIMIT) {
        return path;
    }

    // The extended form is taken literally: it must be absolute, use backslashes and hold no "." or
    // ".." components, which lexically_normal and make_preferred guarantee.
    std::error_code ignored = {};
    std::filesystem::path absolute = std::filesystem::absolute(path, ignored);
    if (ignored) {
        return path;
    }
    absolute = absolute.lexically_normal().make_preferred();
    std::wstring extended = absolute.native();
    if (extended.starts_with(UNC_PREFIX)) {
        extended.replace(0, UNC_PREFIX.size(), L"\\\\?\\UNC\\");
    } else {
        extended.insert(0, EXTENDED_PREFIX);
    }
    return std::filesystem::path(std::move(extended));
}

result<void> internal::replace_file(const std::filesystem::path &source, const std::filesystem::path &target) {
    DWORD errorCode = rename_posix(source, target);
    if (errorCode == 0) {
        return result<void>::success();
    }

    if (errorCode == ERROR_INVALID_PARAMETER || errorCode == ERROR_NOT_SUPPORTED) {
        // Volumes without POSIX rename (FAT, some network shares) take the classic route.
        // MOVEFILE_WRITE_THROUGH makes the call return only once the rename has reached the disk.
        const DWORD flags = MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH;
        if (MoveFileExW(source.c_str(), target.c_str(), flags) != 0) {
            return result<void>::success();
        }
        errorCode = GetLastError();
    }

    return make_error(
      error_code::IO_FAILURE,
      std::format("replace '{}' with '{}': {}", path_to_utf8(target), path_to_utf8(source), last_error_text(errorCode))
    );
}
