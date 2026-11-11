#pragma once

#include "conjure/core/log.h"

// The bindings module's log category ("bindings"): table filling, slot overrides, rate-limited entry
// failures and residency bookkeeping.

namespace conjure::bindings {

    CONJURE_DECLARE_ENGINE_LOG_CATEGORY(LOG_BINDINGS);
}
