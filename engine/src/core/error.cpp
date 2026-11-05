#include "conjure/core/error.h"

#include <array>

using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// Free functions

namespace {
    constexpr std::array<std::string_view, static_cast<conjure::usize>(error_code::COUNT)> ERROR_CODE_NAMES = {
        "NONE",
        "UNKNOWN",
        "INVALID_ARGUMENT",
        "INVALID_STATE",
        "NOT_FOUND",
        "ALREADY_EXISTS",
        "OUT_OF_RANGE",
        "OUT_OF_MEMORY",
        "UNSUPPORTED",
        "NOT_IMPLEMENTED",
        "IO_FAILURE",
        "PARSE_FAILURE",
        "TIMEOUT",
        "PERMISSION_DENIED",
        "BACKEND_FAILURE",
        "DEPENDENCY_FAILURE",
        "CANCELLED",
    };
}

std::string_view conjure::core::to_string(error_code code) noexcept {
    const conjure::usize index = static_cast<conjure::usize>(code);
    if (index >= ERROR_CODE_NAMES.size()) {
        return "INVALID_ERROR_CODE";
    }
    return ERROR_CODE_NAMES[index];
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

error::error() {}

error::error(error_code code) :
    mCode(code) {}

error::error(error_code code, std::string message) :
    mCode(code),
    mMessage(std::move(message)) {}

error::~error() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

std::string error::to_string() const {
    if (mMessage.empty()) {
        return std::string(conjure::core::to_string(mCode));
    }
    return std::format("{}: {}", conjure::core::to_string(mCode), mMessage);
}
