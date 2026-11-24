#pragma once

// errno text and error helpers shared by the POSIX backends of the system area, the counterpart of
// win32_utils.h. Each backend keeps its own mapping where a call needs more than these codes.

#include "conjure/core/error.h"

#include <string>

namespace conjure::core::detail {

    /// <summary>
    /// The system's message for an errno value.
    /// </summary>
    [[nodiscard]] std::string errno_message(int code);

    /// <summary>
    /// Maps the common errno values onto the generic error codes: ENOENT and ENOTDIR become
    /// NOT_FOUND, EACCES and EPERM become PERMISSION_DENIED, everything else BACKEND_FAILURE.
    /// </summary>
    [[nodiscard]] error_code error_code_from_errno(int code) noexcept;
}
