#pragma once

#include "conjure/core/log.h"

// The log category of the engine-level test support library ("testing"), defined in
// tests/support/engine/src/log_category.cpp. The fixtures log through it when they take a decision a
// test author should see: a golden reference written in update mode, a worker count clamped to the
// engine's minimum, a scene file written into the fixture's scratch file system.

namespace conjure::testing {
    CONJURE_DECLARE_LOG_CATEGORY(LOG_TESTING);
}
