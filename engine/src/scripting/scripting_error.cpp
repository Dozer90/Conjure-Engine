#include "conjure/scripting/scripting_error.h"

using namespace conjure::scripting;

std::string_view conjure::scripting::scripting_error_name(scripting_error code) noexcept {
    switch (code) {
        case scripting_error::RUNTIME_NOT_FOUND:
            return "RUNTIME_NOT_FOUND";
        case scripting_error::HOSTFXR_INCOMPATIBLE:
            return "HOSTFXR_INCOMPATIBLE";
        case scripting_error::RUNTIME_CONFIG_INVALID:
            return "RUNTIME_CONFIG_INVALID";
        case scripting_error::MANAGED_SDK_MISSING:
            return "MANAGED_SDK_MISSING";
        case scripting_error::HOST_ALREADY_RUNNING:
            return "HOST_ALREADY_RUNNING";
        case scripting_error::ABI_MISMATCH:
            return "ABI_MISMATCH";
        case scripting_error::ASSEMBLY_LOAD_FAILED:
            return "ASSEMBLY_LOAD_FAILED";
        case scripting_error::ASSEMBLY_LEAKED:
            return "ASSEMBLY_LEAKED";
        case scripting_error::CLASS_NOT_FOUND:
            return "CLASS_NOT_FOUND";
        case scripting_error::FIELD_NOT_FOUND:
            return "FIELD_NOT_FOUND";
        case scripting_error::FIELD_BLOB_INVALID:
            return "FIELD_BLOB_INVALID";
        case scripting_error::INSTANCE_NOT_FOUND:
            return "INSTANCE_NOT_FOUND";
        case scripting_error::WRONG_THREAD:
            return "WRONG_THREAD";
        case scripting_error::NOT_IN_PHASE:
            return "NOT_IN_PHASE";
        case scripting_error::BUILD_FAILED:
            return "BUILD_FAILED";
        case scripting_error::NOT_INITIALISED:
            return "NOT_INITIALISED";
        case scripting_error::NOT_SUPPORTED:
            return "NOT_SUPPORTED";
        case scripting_error::COUNT:
            break;
    }
    return "";
}
