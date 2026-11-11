#include "conjure/engine/engine_version.h"

using namespace conjure;
using namespace conjure::engine;

std::string_view conjure::engine::engine_version_string() noexcept {
    return CONJURE_VERSION_STRING;
}

std::string_view conjure::engine::engine_name() noexcept {
    return CONJURE_ENGINE_NAME;
}
