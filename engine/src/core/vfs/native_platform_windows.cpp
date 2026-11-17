#include "core/vfs/native_platform.h"

#include "core/vfs/vfs_internal.h"

#include <Windows.h>

#include <AclAPI.h>

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

    // The extended-length ("\\?\") and device ("\\.\") forms, which Win32 takes literally.
    bool is_literal_form(const std::wstring &native) noexcept {
        constexpr std::wstring_view EXTENDED_PREFIX = L"\\\\?\\";
        constexpr std::wstring_view DEVICE_PREFIX = L"\\\\.\\";
        return native.starts_with(EXTENDED_PREFIX) || native.starts_with(DEVICE_PREFIX);
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

    // Writes all of 'data' at the handle's file position. Returns the Win32 error code, ERROR_SUCCESS
    // when every byte was taken.
    DWORD write_all(HANDLE handle, std::span<const std::byte> data) noexcept {
        // WriteFile moves at most a DWORD per call; large spans are written in pieces.
        constexpr usize MAX_CHUNK = usize{1} << 30;
        usize total = 0;
        while (total < data.size()) {
            const DWORD chunk = static_cast<DWORD>(std::min(data.size() - total, MAX_CHUNK));
            DWORD written = 0;
            if (WriteFile(handle, data.data() + total, chunk, &written, nullptr) == 0) {
                return GetLastError();
            }
            if (written == 0) {
                // A synchronous write to a disk file takes everything or fails; a call that claims
                // success without progress would otherwise spin here for ever.
                return ERROR_WRITE_FAULT;
            }
            total += written;
        }
        return ERROR_SUCCESS;
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
    // error code, 0 on success; the codes rename_falls_back_to_move accepts (ERROR_INVALID_FUNCTION,
    // ERROR_INVALID_PARAMETER, ERROR_NOT_SUPPORTED) mean the volume, its driver or the operating
    // system cannot do it and the caller should fall back to MoveFileExW.
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

    // Fills 'outTokenUser' with the TOKEN_USER block of the process: the account the process runs as.
    // Returns the Win32 error code, ERROR_SUCCESS on success.
    DWORD query_process_user(std::vector<std::byte> &outTokenUser) {
        HANDLE token = nullptr;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) == 0) {
            return GetLastError();
        }

        // The block ends in a security identifier of variable length: the first call reports the size.
        DWORD needed = 0;
        DWORD errorCode = ERROR_INVALID_DATA;
        if (GetTokenInformation(token, TokenUser, nullptr, 0, &needed) == 0) {
            errorCode = GetLastError();
        }
        if (errorCode == ERROR_INSUFFICIENT_BUFFER && needed >= sizeof(TOKEN_USER)) {
            outTokenUser.assign(needed, std::byte{0});
            const BOOL filled = GetTokenInformation(token, TokenUser, outTokenUser.data(), needed, &needed);
            errorCode = filled != 0 ? ERROR_SUCCESS : GetLastError();
        }
        CloseHandle(token);
        return errorCode;
    }

    // The security identifier inside a block query_process_user filled; it points into the block.
    PSID sid_of(const std::vector<std::byte> &tokenUser) noexcept {
        return reinterpret_cast<const TOKEN_USER *>(tokenUser.data())->User.Sid;
    }

    // True when the security descriptor leaves the file to 'user' alone: the user owns it (an owner may
    // always rewrite the list), the list exists (none at all grants everyone everything), is protected
    // from the folder's inheritable entries, and holds nothing but entries allowing that user.
    bool grants_user_only(PSECURITY_DESCRIPTOR descriptor, PSID owner, PACL list, PSID user) noexcept {
        if (owner == nullptr || list == nullptr || EqualSid(owner, user) == 0) {
            return false;
        }
        SECURITY_DESCRIPTOR_CONTROL control = 0;
        DWORD revision = 0;
        if (GetSecurityDescriptorControl(descriptor, &control, &revision) == 0) {
            return false;
        }
        if ((control & SE_DACL_PROTECTED) == 0) {
            return false;
        }
        for (DWORD index = 0; index < list->AceCount; ++index) {
            void *entry = nullptr;
            if (GetAce(list, index, &entry) == 0) {
                return false;
            }
            if (static_cast<const ACE_HEADER *>(entry)->AceType != ACCESS_ALLOWED_ACE_TYPE) {
                return false;
            }
            ACCESS_ALLOWED_ACE *const allowed = static_cast<ACCESS_ALLOWED_ACE *>(entry);
            if (EqualSid(reinterpret_cast<PSID>(&allowed->SidStart), user) == 0) {
                return false;
            }
        }
        return true;
    }

    // Decides whether the file behind 'handle' (opened with READ_CONTROL) can be opened by 'user' alone:
    // its volume keeps access control lists and its descriptor passes grants_user_only. Returns the Win32
    // error code of a query that failed, ERROR_SUCCESS when 'outPrivate' holds the answer.
    DWORD inspect_privacy(HANDLE handle, PSID user, bool &outPrivate) noexcept {
        outPrivate = false;
        DWORD volumeFlags = 0;
        if (GetVolumeInformationByHandleW(handle, nullptr, 0, nullptr, nullptr, &volumeFlags, nullptr, 0) == 0) {
            return GetLastError();
        }
        if ((volumeFlags & FILE_PERSISTENT_ACLS) == 0) {
            return ERROR_SUCCESS;
        }

        PSID owner = nullptr;
        PACL list = nullptr;
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        const SECURITY_INFORMATION wanted = OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION;
        const DWORD outcome = GetSecurityInfo(
          handle,
          SE_FILE_OBJECT,
          wanted,
          &owner,
          nullptr,
          &list,
          nullptr,
          &descriptor
        );
        if (outcome != ERROR_SUCCESS) {
            return outcome;
        }
        outPrivate = grants_user_only(descriptor, owner, list, user);
        LocalFree(descriptor);
        return ERROR_SUCCESS;
    }

    // Closes and removes a file create_private_file made and cannot keep.
    void discard_new_file(HANDLE handle, const std::filesystem::path &path) noexcept {
        CloseHandle(handle);
        DeleteFileW(path.c_str());
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
// Writing

result<void> internal::write_file_flushed(
  const std::filesystem::path &path,
  std::span<const std::byte> data,
  std::string_view displayPath
) {
    // Readers may look at the file while it is written (an indexer, a virus scanner); nobody else writes it.
    const HANDLE handle = CreateFileW(
      path.c_str(),
      GENERIC_WRITE,
      FILE_SHARE_READ,
      nullptr,
      CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL,
      nullptr
    );
    if (handle == INVALID_HANDLE_VALUE) {
        return io_failure("open for writing", displayPath, GetLastError());
    }

    const DWORD writeError = write_all(handle, data);
    if (writeError != ERROR_SUCCESS) {
        CloseHandle(handle);
        return io_failure("write", displayPath, writeError);
    }

    // The write above only reached the system cache. FlushFileBuffers returns once the file's data and
    // metadata are on the device and the device has been told to empty its own write cache.
    const DWORD flushError = FlushFileBuffers(handle) != 0 ? ERROR_SUCCESS : GetLastError();
    CloseHandle(handle);
    if (flushError == ERROR_SUCCESS) {
        note_flushed_file();
        return result<void>::success();
    }
    if (flush_is_unsupported(flushError)) {
        note_unflushable_volume(displayPath);
        return result<void>::success();
    }
    return io_failure("flush", displayPath, flushError);
}

bool internal::flush_is_unsupported(u32 platformError) noexcept {
    // What a driver without a flush routine answers (some RAM disks and redirectors). Everything else,
    // ERROR_ACCESS_DENIED and the device errors included, is a flush that was attempted and failed.
    return platformError == ERROR_INVALID_FUNCTION || platformError == ERROR_NOT_SUPPORTED;
}


/*===========================================================================*/
/*===========================================================================*/
// Private files

result<void> internal::create_private_file(
  const std::filesystem::path &path,
  std::span<const std::byte> data,
  std::string_view displayPath
) {
    std::vector<std::byte> tokenUser = {};
    const DWORD userError = query_process_user(tokenUser);
    if (userError != ERROR_SUCCESS) {
        return io_failure("identify the owner of", displayPath, userError);
    }
    const PSID user = sid_of(tokenUser);

    // One entry, the process user with every right. SE_DACL_PROTECTED keeps the folder's inheritable
    // entries (typically Users or Authenticated Users with read access) out of the list.
    const DWORD aclBytes = static_cast<DWORD>(sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD))
                           + GetLengthSid(user);
    std::vector<std::byte> aclStorage(aclBytes, std::byte{0});
    const PACL acl = reinterpret_cast<PACL>(aclStorage.data());
    SECURITY_DESCRIPTOR descriptor = {};
    const bool described = InitializeAcl(acl, aclBytes, ACL_REVISION) != 0
                           && AddAccessAllowedAce(acl, ACL_REVISION, FILE_ALL_ACCESS, user) != 0
                           && InitializeSecurityDescriptor(&descriptor, SECURITY_DESCRIPTOR_REVISION) != 0
                           && SetSecurityDescriptorOwner(&descriptor, user, FALSE) != 0
                           && SetSecurityDescriptorDacl(&descriptor, TRUE, acl, FALSE) != 0
                           && SetSecurityDescriptorControl(&descriptor, SE_DACL_PROTECTED, SE_DACL_PROTECTED) != 0;
    if (!described) {
        return io_failure("describe the access to", displayPath, GetLastError());
    }

    // CREATE_NEW refuses an entry that already exists, so the list above is the one the file is born with;
    // no sharing, so nobody opens the file before it is complete.
    SECURITY_ATTRIBUTES attributes = {};
    attributes.nLength = sizeof(attributes);
    attributes.lpSecurityDescriptor = &descriptor;
    attributes.bInheritHandle = FALSE;
    const HANDLE handle = CreateFileW(
      path.c_str(),
      GENERIC_WRITE | READ_CONTROL,
      0,
      &attributes,
      CREATE_NEW,
      FILE_ATTRIBUTE_NORMAL,
      nullptr
    );
    if (handle == INVALID_HANDLE_VALUE) {
        return io_failure("create", displayPath, GetLastError());
    }

    // A volume without access control lists (FAT, exFAT) takes the call and drops the list.
    bool isPrivate = false;
    const DWORD inspectError = inspect_privacy(handle, user, isPrivate);
    if (inspectError != ERROR_SUCCESS) {
        discard_new_file(handle, path);
        return io_failure("inspect", displayPath, inspectError);
    }
    if (!isPrivate) {
        discard_new_file(handle, path);
        return make_error(
          error_code::UNSUPPORTED,
          std::format(
            "'{}' cannot be restricted to its owner: the volume does not keep access control lists",
            displayPath
          )
        );
    }

    const DWORD writeError = write_all(handle, data);
    if (writeError != ERROR_SUCCESS) {
        discard_new_file(handle, path);
        return io_failure("write", displayPath, writeError);
    }
    CloseHandle(handle);
    return result<void>::success();
}

result<bool> internal::is_private_file(const std::filesystem::path &path, std::string_view displayPath) {
    // FILE_FLAG_OPEN_REPARSE_POINT opens a link itself instead of the file it names; a link is never
    // private, whatever it points at.
    const DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    const DWORD flags = FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT;
    const HANDLE handle = CreateFileW(path.c_str(), READ_CONTROL, share, nullptr, OPEN_EXISTING, flags, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD errorCode = GetLastError();
        if (errorCode == ERROR_FILE_NOT_FOUND || errorCode == ERROR_PATH_NOT_FOUND) {
            return not_found(displayPath);
        }
        return io_failure("inspect", displayPath, errorCode);
    }

    BY_HANDLE_FILE_INFORMATION information = {};
    if (GetFileInformationByHandle(handle, &information) == 0) {
        const DWORD errorCode = GetLastError();
        CloseHandle(handle);
        return io_failure("inspect", displayPath, errorCode);
    }
    constexpr DWORD NOT_A_PLAIN_FILE = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT;
    if ((information.dwFileAttributes & NOT_A_PLAIN_FILE) != 0) {
        CloseHandle(handle);
        return false;
    }

    std::vector<std::byte> tokenUser = {};
    const DWORD userError = query_process_user(tokenUser);
    if (userError != ERROR_SUCCESS) {
        CloseHandle(handle);
        return io_failure("identify the owner of", displayPath, userError);
    }
    bool isPrivate = false;
    const DWORD inspectError = inspect_privacy(handle, sid_of(tokenUser), isPrivate);
    CloseHandle(handle);
    if (inspectError != ERROR_SUCCESS) {
        return io_failure("inspect", displayPath, inspectError);
    }
    return isPrivate;
}


/*===========================================================================*/
/*===========================================================================*/
// Replacing

std::filesystem::path internal::extend_native_path(const std::filesystem::path &path) {
    // MAX_PATH (260) counts the terminating character and CreateDirectoryW keeps 12 characters for a
    // short file name, so the classic limit for a directory component is 248; everything from there on
    // goes through the extended-length form, which Win32 accepts up to about 32,767 characters.
    constexpr usize CLASSIC_LIMIT = 248;
    if (path.native().size() < CLASSIC_LIMIT && !is_literal_form(path.native())) {
        return path;
    }
    return always_extended_native_path(path);
}

std::filesystem::path internal::always_extended_native_path(const std::filesystem::path &path) {
    if (is_literal_form(path.native())) {
        // Already a literal form (a user-supplied root, or one derived from native_path() of a long path).
        // Win32 does not translate '/' inside it, so the components the VFS appended with forward slashes
        // must still be normalised (review 4: an extended root failed every stat and read of a nested path).
        return path.lexically_normal().make_preferred();
    }

    // The extended form is taken literally: it must be absolute, use backslashes and hold no "." or
    // ".." components, which lexically_normal and make_preferred guarantee.
    constexpr std::wstring_view EXTENDED_PREFIX = L"\\\\?\\";
    constexpr std::wstring_view UNC_PREFIX = L"\\\\";
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

    if (rename_falls_back_to_move(errorCode)) {
        // Volumes without POSIX rename (FAT and exFAT drivers, SMB and NAS redirectors that answer
        // ERROR_INVALID_FUNCTION) take the classic route.
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

bool internal::rename_falls_back_to_move(u32 platformError) noexcept {
    // ERROR_ACCESS_DENIED stays out on purpose: it also means an ACL refusal or a delete-pending target,
    // which MoveFileExW would refuse in the same way.
    return platformError == ERROR_INVALID_FUNCTION || platformError == ERROR_INVALID_PARAMETER
           || platformError == ERROR_NOT_SUPPORTED;
}
