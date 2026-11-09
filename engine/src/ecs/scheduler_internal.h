#pragma once

#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/system_phase.h"

#include "conjure/core/types.h"

#include <vector>

// Private record types of the scheduler, shared by scheduler.cpp (execution) and
// scheduler_stages.cpp (ordering and packing).

namespace conjure::ecs {

    struct scheduler::system_record {
        system_desc desc = {};
        u32 generation = 0;
        u32 registrationOrder = 0;
        bool alive = false;
        u64 sinceVersion = 0;
        u64 lastMicroseconds = 0;
        command_buffer commands;        // no default: bound to the world in the constructor
        system_scratch scratch = {};

        explicit system_record(world &target) :
            commands(target) {}
    };

    struct scheduler::phase_state {
        std::vector<stage_info> stages = {};
        std::vector<system_timing> timings = {};
        bool dirty = true;
    };
}
