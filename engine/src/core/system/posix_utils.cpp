#include "core/system/posix_utils.h"

#include <cerrno>
#include <string>
#include <system_error>

using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// Errors

std::string conjure::core::detail::errno_message(int code) {
    return std::system_category().message(code);
}

error_code conjure::core::detail::error_code_from_errno(int code) noexcept {
    switch (code) {
        case ENOENT:
        case ENOTDIR: {
            return error_code::NOT_FOUND;
        }
        case EACCES:
        case EPERM: {
            return error_code::PERMISSION_DENIED;
        }
        default: {
            return error_code::BACKEND_FAILURE;
        }
    }
}
