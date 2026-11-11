#include "conjure/engine/engine_phase.h"

using namespace conjure;
using namespace conjure::engine;

std::string_view conjure::engine::to_string(ecs::system_phase phase) noexcept {
    return ecs::to_string(phase);
}
