#include "conjure/scripting/scripting_error.h"

using namespace conjure::scripting;

std::string_view conjure::scripting::script_error_name(script_error code) noexcept {
    switch (code) {
        case script_error::RUNTIME_NOT_FOUND:
            return "RUNTIME_NOT_FOUND";
        case script_error::HOSTFXR_INCOMPATIBLE:
            return "HOSTFXR_INCOMPATIBLE";
        case script_error::RUNTIME_CONFIG_INVALID:
            return "RUNTIME_CONFIG_INVALID";
        case script_error::MANAGED_SDK_MISSING:
            return "MANAGED_SDK_MISSING";
        case script_error::HOST_ALREADY_RUNNING:
            return "HOST_ALREADY_RUNNING";
        case script_error::ABI_MISMATCH:
            return "ABI_MISMATCH";
        case script_error::ASSEMBLY_LOAD_FAILED:
            return "ASSEMBLY_LOAD_FAILED";
        case script_error::ASSEMBLY_LEAKED:
            return "ASSEMBLY_LEAKED";
        case script_error::CLASS_NOT_FOUND:
            return "CLASS_NOT_FOUND";
        case script_error::FIELD_NOT_FOUND:
            return "FIELD_NOT_FOUND";
        case script_error::FIELD_BLOB_INVALID:
            return "FIELD_BLOB_INVALID";
        case script_error::INSTANCE_NOT_FOUND:
            return "INSTANCE_NOT_FOUND";
        case script_error::WRONG_THREAD:
            return "WRONG_THREAD";
        case script_error::NOT_IN_PHASE:
            return "NOT_IN_PHASE";
        case script_error::BUILD_FAILED:
            return "BUILD_FAILED";
        case script_error::NOT_INITIALIZED:
            return "NOT_INITIALIZED";
        case script_error::NOT_SUPPORTED:
            return "NOT_SUPPORTED";
        case script_error::COUNT:
            break;
    }
    return "";
}
