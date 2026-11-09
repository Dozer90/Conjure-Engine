#include "conjure/ecs/system_phase.h"

#include <string_view>

using namespace conjure;
using namespace conjure::ecs;

std::string_view conjure::ecs::to_string(system_phase phase) noexcept {
    switch (phase) {
        case system_phase::FRAME_BEGIN:
            return "FRAME_BEGIN";
        case system_phase::FIXED_UPDATE:
            return "FIXED_UPDATE";
        case system_phase::PHYSICS_STEP:
            return "PHYSICS_STEP";
        case system_phase::FIXED_END:
            return "FIXED_END";
        case system_phase::UPDATE:
            return "UPDATE";
        case system_phase::LATE_UPDATE:
            return "LATE_UPDATE";
        case system_phase::TRANSFORM:
            return "TRANSFORM";
        case system_phase::SYNC:
            return "SYNC";
        case system_phase::EXTRACT:
            return "EXTRACT";
        case system_phase::FRAME_END:
            return "FRAME_END";
        case system_phase::COUNT:
            break;
    }
    return "COUNT";
}
