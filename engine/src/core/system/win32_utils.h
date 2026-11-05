#pragma once

// Win32 text and error helpers shared by the Windows backends of the system area. The header
// stays free of <Windows.h>; DWORD is spelled unsigned long.

#include "conjure/core/error.h"

#include <string>
#include <string_view>

namespace conjure::core::detail {

    /// <summary>
    /// UTF-8 to UTF-16. Invalid sequences become U+FFFD.
    /// </summary>
    [[nodiscard]] std::wstring to_wide(std::string_view utf8);

    /// <summary>
    /// UTF-16 to UTF-8.
    /// </summary>
    [[nodiscard]] std::string from_wide(std::wstring_view wide);

    /// <summary>
    /// The system's message for a Win32 error code, trimmed of trailing whitespace. Falls back to
    /// "Win32 error N" when the system has no text.
    /// </summary>
    [[nodiscard]] std::string win32_error_message(unsigned long code);

    /// <summary>
    /// Maps the common Win32 error codes onto the generic error codes: file, path and module not
    /// found become NOT_FOUND, access denied becomes PERMISSION_DENIED, everything else
    /// BACKEND_FAILURE.
    /// </summary>
    [[nodiscard]] error_code error_code_from_win32(unsigned long code) noexcept;
}
