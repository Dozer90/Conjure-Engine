#include "conjure/engine/exit_code.h"

using namespace conjure;
using namespace conjure::engine;

std::string_view conjure::engine::to_string(exit_code code) noexcept {
    switch (code) {
        case exit_code::OK:
            return "OK";
        case exit_code::INIT_FAILURE:
            return "INIT_FAILURE";
        case exit_code::RUNTIME_FAILURE:
            return "RUNTIME_FAILURE";
        case exit_code::BAD_ARGUMENTS:
            return "BAD_ARGUMENTS";
        case exit_code::DEVICE_LOST:
            return "DEVICE_LOST";
    }
    return "UNKNOWN";
}
