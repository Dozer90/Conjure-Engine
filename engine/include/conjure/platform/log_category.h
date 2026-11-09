#pragma once

#include "conjure/core/log.h"

// The platform module's log category ("platform"), defined in engine/src/platform/platform.cpp.
// Every message of the window, input and backend code goes through it, including the text of every
// GLFW error callback.

namespace conjure::platform {
    CONJURE_DECLARE_LOG_CATEGORY(LOG_PLATFORM);
}
