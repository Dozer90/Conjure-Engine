#include "core/system/win32_utils.h"

#include <Windows.h>

#include <format>

using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// Text conversion

std::wstring conjure::core::detail::to_wide(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }

    const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }

    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);
    return wide;
}

std::string conjure::core::detail::from_wide(std::wstring_view wide) {
    if (wide.empty()) {
        return {};
    }

    const int length = WideCharToMultiByte(
      CP_UTF8,
      0,
      wide.data(),
      static_cast<int>(wide.size()),
      nullptr,
      0,
      nullptr,
      nullptr
    );
    if (length <= 0) {
        return {};
    }

    std::string utf8(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), length, nullptr, nullptr);
    return utf8;
}


/*===========================================================================*/
/*===========================================================================*/
// Errors

std::string conjure::core::detail::win32_error_message(unsigned long code) {
    wchar_t *buffer = nullptr;
    const DWORD length = FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr,
      code,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      reinterpret_cast<LPWSTR>(&buffer),
      0,
      nullptr
    );

    std::string message = {};
    if (length > 0 && buffer != nullptr) {
        message = from_wide(std::wstring_view(buffer, length));
    }
    if (buffer != nullptr) {
        LocalFree(buffer);
    }

    while (!message.empty() && (message.back() == '\r' || message.back() == '\n' || message.back() == ' ')) {
        message.pop_back();
    }
    if (message.empty()) {
        message = std::format("Win32 error {}", code);
    }
    return message;
}

error_code conjure::core::detail::error_code_from_win32(unsigned long code) noexcept {
    switch (code) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_MOD_NOT_FOUND:
        case ERROR_DIRECTORY: {
            return error_code::NOT_FOUND;
        }
        case ERROR_ACCESS_DENIED: {
            return error_code::PERMISSION_DENIED;
        }
        default: {
            return error_code::BACKEND_FAILURE;
        }
    }
}
