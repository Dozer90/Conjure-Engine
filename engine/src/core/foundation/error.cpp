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

    using core_traits = error_category_traits<error_code>;
}

std::string_view conjure::core::to_string(error_code code) noexcept {
    const conjure::usize index = static_cast<conjure::usize>(code);
    if (index >= ERROR_CODE_NAMES.size()) {
        return "INVALID_ERROR_CODE";
    }
    return ERROR_CODE_NAMES[index];
}

std::string_view conjure::core::detail::core_error_code_name(conjure::u32 code) noexcept {
    if (code >= static_cast<conjure::u32>(error_code::COUNT)) {
        return "";
    }
    return ERROR_CODE_NAMES[code];
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

error::error() :
    mCategory(core_traits::ID),
    mCode(static_cast<conjure::u32>(error_code::NONE)),
    mCategoryName(core_traits::NAME.data()),
    mCodeName(&core_traits::code_name) {}

error::error(error_code code) :
    mCategory(core_traits::ID),
    mCode(static_cast<conjure::u32>(code)),
    mCategoryName(core_traits::NAME.data()),
    mCodeName(&core_traits::code_name) {}

error::error(error_code code, std::string message) :
    mCategory(core_traits::ID),
    mCode(static_cast<conjure::u32>(code)),
    mCategoryName(core_traits::NAME.data()),
    mCodeName(&core_traits::code_name),
    mMessage(std::move(message)) {}

error::error(
  conjure::u32 category,
  conjure::u32 code,
  const char *categoryName,
  error_code_name_fn codeName,
  std::string message
) :
    mCategory(category),
    mCode(code),
    mCategoryName(categoryName == nullptr ? "" : categoryName),
    mCodeName(codeName),
    mMessage(std::move(message)) {}

error::~error() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

std::string_view error::code_name() const noexcept {
    if (mCodeName == nullptr) {
        return "";
    }
    return mCodeName(mCode);
}

error_code error::generic_code() const noexcept {
    if (mCategory != core_traits::ID || mCode >= static_cast<conjure::u32>(error_code::COUNT)) {
        return error_code::NONE;
    }
    return static_cast<error_code>(mCode);
}

std::string error::to_string() const {
    const std::string_view name = code_name();
    std::string head = {};
    if (name.empty()) {
        head = std::format("{}.{}", category_name(), mCode);
    } else {
        head = std::format("{}.{}", category_name(), name);
    }

    if (mMessage.empty()) {
        return head;
    }
    return std::format("{}: {}", head, mMessage);
}
