#pragma once

#include "conjure/scripting/script_runtime.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/ecs/scheduler.h"

#include <string_view>

// The six EXCLUSIVE scripting systems (docs/design/03_scripting.md 3.2, R27). All managed execution
// happens inside them on the main thread, so scripts may mutate the world directly except inside a DOD
// query scope.

namespace conjure::scripting {

    class hot_reload_controller;

    inline constexpr std::string_view SYSTEM_FRAME_BEGIN = "scripting.frame_begin";
    inline constexpr std::string_view SYSTEM_COLLISION_DISPATCH = "scripting.collision_dispatch";
    inline constexpr std::string_view SYSTEM_FIXED_UPDATE = "scripting.fixed_update";
    inline constexpr std::string_view SYSTEM_UPDATE = "scripting.update";
    inline constexpr std::string_view SYSTEM_LATE_UPDATE = "scripting.late_update";
    inline constexpr std::string_view SYSTEM_FRAME_END = "scripting.frame_end";

    /// <summary>
    /// Registers scripting.frame_begin (FRAME_BEGIN: reload tick, flush), scripting.collision_dispatch and
    /// scripting.fixed_update (FIXED_UPDATE, dispatch before update), scripting.update (UPDATE),
    /// scripting.late_update (LATE_UPDATE) and scripting.frame_end (FRAME_END: flush). 'reload' may be
    /// null; when set, frame_begin ticks it with the frame time. The runtime and controller must outlive
    /// the scheduler. Call once per scheduler: a second call returns NAME_CONFLICT from its first system
    /// and adds nothing (01_ecs.md D13).
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<void> register_script_systems(
      ecs::scheduler &scheduler,
      script_runtime &runtime,
      hot_reload_controller *reload = nullptr
    );
}
