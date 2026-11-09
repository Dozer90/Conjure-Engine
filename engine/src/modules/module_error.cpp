#include "conjure/modules/module_error.h"

using namespace conjure;
using namespace conjure::modules;

std::string_view conjure::modules::module_error_name(module_error code) noexcept {
    switch (code) {
        case module_error::NONE:
            return "NONE";
        case module_error::MANIFEST_NOT_FOUND:
            return "MANIFEST_NOT_FOUND";
        case module_error::MANIFEST_INVALID:
            return "MANIFEST_INVALID";
        case module_error::INVALID_NAME:
            return "INVALID_NAME";
        case module_error::DUPLICATE_NAME:
            return "DUPLICATE_NAME";
        case module_error::PLATFORM_UNSUPPORTED:
            return "PLATFORM_UNSUPPORTED";
        case module_error::ENGINE_VERSION_MISMATCH:
            return "ENGINE_VERSION_MISMATCH";
        case module_error::FEATURE_UNAVAILABLE:
            return "FEATURE_UNAVAILABLE";
        case module_error::DEPENDENCY_MISSING:
            return "DEPENDENCY_MISSING";
        case module_error::DEPENDENCY_VERSION:
            return "DEPENDENCY_VERSION";
        case module_error::DEPENDENCY_CYCLE:
            return "DEPENDENCY_CYCLE";
        case module_error::DEPENDENCY_FAILED:
            return "DEPENDENCY_FAILED";
        case module_error::MULTIPLE_GAME_MODULES:
            return "MULTIPLE_GAME_MODULES";
        case module_error::SHARED_LIBRARIES_DISABLED:
            return "SHARED_LIBRARIES_DISABLED";
        case module_error::BINARY_NOT_FOUND:
            return "BINARY_NOT_FOUND";
        case module_error::BINARY_COPY_FAILED:
            return "BINARY_COPY_FAILED";
        case module_error::BINARY_LOAD_FAILED:
            return "BINARY_LOAD_FAILED";
        case module_error::ENTRY_SYMBOL_MISSING:
            return "ENTRY_SYMBOL_MISSING";
        case module_error::ABI_MISMATCH:
            return "ABI_MISMATCH";
        case module_error::NAME_MISMATCH:
            return "NAME_MISMATCH";
        case module_error::ENTRY_FAILED:
            return "ENTRY_FAILED";
        case module_error::REGISTRATION_CONFLICT:
            return "REGISTRATION_CONFLICT";
        case module_error::SERVICE_MISSING:
            return "SERVICE_MISSING";
        case module_error::HAS_DEPENDENTS:
            return "HAS_DEPENDENTS";
        case module_error::IN_USE:
            return "IN_USE";
        case module_error::UNLOAD_BLOCKED:
            return "UNLOAD_BLOCKED";
        case module_error::NOT_LOADED:
            return "NOT_LOADED";
        case module_error::ALREADY_LOADED:
            return "ALREADY_LOADED";
        case module_error::INVALID_ID:
            return "INVALID_ID";
        case module_error::STALE_ID:
            return "STALE_ID";
        case module_error::FAULTED:
            return "FAULTED";
        case module_error::EXPORT_FAILED:
            return "EXPORT_FAILED";
        case module_error::COUNT:
            break;
    }
    return "";
}
