#pragma once

#include "conjure/core/log.h"

// The engine module's log category ("engine"), defined in engine/src/engine/log_category.cpp. Startup
// and shutdown steps, frame-loop warnings (dropped fixed time, frame arena overflow, render failures)
// and the export pipeline log through it.

namespace conjure::engine {
    CONJURE_DECLARE_ENGINE_LOG_CATEGORY(LOG_ENGINE);
}
