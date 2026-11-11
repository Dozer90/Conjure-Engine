#pragma once

#include "conjure/core/api.h"

#include <string_view>

// Process exit codes of the executables (docs/design/06_engine_loop.md 2.12). application::run()
// returns one of these as an int; runtime/main.cpp and conjure_editor return them from main().

namespace conjure::engine {

    enum class exit_code : int {
        OK = 0,
        INIT_FAILURE = 1,       // initialise() failed
        RUNTIME_FAILURE = 2,    // step_frame() reported an unrecoverable error or a resize failed twice
        BAD_ARGUMENTS = 3,      // application_desc::from_command_line failed
        DEVICE_LOST = 4         // the graphics device was lost (no recovery in v1, R8)
    };

    /// <summary>
    /// The enumerator name of a code ("RUNTIME_FAILURE"); "UNKNOWN" for values outside the enumeration.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(exit_code code) noexcept;
}
