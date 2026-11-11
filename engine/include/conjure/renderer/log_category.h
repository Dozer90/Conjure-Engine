#pragma once

#include "conjure/core/log.h"

// Log category of the renderer module: uploads, fallbacks, graph validation, device loss.
//
//   CONJURE_LOG_WARN(LOG_RENDERER, "mesh id {} is stale; drawing the fallback cube", id.index);

namespace conjure::renderer {
    CONJURE_DECLARE_ENGINE_LOG_CATEGORY(LOG_RENDERER);
}
