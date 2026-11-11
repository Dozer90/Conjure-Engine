#pragma once

#include "conjure/core/api.h"
#include "conjure/core/types.h"
#include "conjure/ecs/system_phase.h"

#include <array>
#include <span>
#include <string_view>

// Helpers over the frame phases (docs/design/06_engine_loop.md 2.4, R1). The list itself lives in
// conjure/ecs/system_phase.h; the engine owns its order and the fixed-step grouping.

namespace conjure::engine {

    /// <summary>
    /// The three phases that run once per fixed step, in order (FIXED_UPDATE, PHYSICS_STEP, FIXED_END).
    /// </summary>
    inline constexpr std::array<ecs::system_phase, 3> FIXED_PHASE_LIST = {
        ecs::system_phase::FIXED_UPDATE,
        ecs::system_phase::PHYSICS_STEP,
        ecs::system_phase::FIXED_END,
    };

    /// <summary>
    /// The fixed-step phases as a span.
    /// </summary>
    inline constexpr std::span<const ecs::system_phase> FIXED_PHASES = FIXED_PHASE_LIST;

    /// <summary>
    /// The enumerator name of a phase ("UPDATE"); "COUNT" for COUNT and out-of-range values. Identical
    /// to ecs::to_string(system_phase), re-exported so engine code needs only this header.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(ecs::system_phase phase) noexcept;

    /// <summary>
    /// True for FIXED_UPDATE, PHYSICS_STEP and FIXED_END.
    /// </summary>
    [[nodiscard]] constexpr bool is_fixed_phase(ecs::system_phase phase) noexcept {
        return phase == ecs::system_phase::FIXED_UPDATE || phase == ecs::system_phase::PHYSICS_STEP
          || phase == ecs::system_phase::FIXED_END;
    }

    /// <summary>
    /// Index of a phase in the engine frame (0 = FRAME_BEGIN); SYSTEM_PHASE_COUNT for COUNT and
    /// out-of-range values.
    /// </summary>
    [[nodiscard]] constexpr u32 phase_index(ecs::system_phase phase) noexcept {
        const u32 index = static_cast<u32>(phase);
        return index < ecs::SYSTEM_PHASE_COUNT ? index : ecs::SYSTEM_PHASE_COUNT;
    }
}
