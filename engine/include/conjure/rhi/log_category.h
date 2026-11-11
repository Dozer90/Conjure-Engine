#pragma once

#include "conjure/core/log.h"

// Log category of the rhi module. Every NVRHI message routed through message_callback and every
// device, swapchain and shader library event is written here:
//
//   CONJURE_LOG_INFO(LOG_RHI, "created {} device on {}", to_string(backend), adapterName);

namespace conjure::rhi {
    CONJURE_DECLARE_ENGINE_LOG_CATEGORY(LOG_RHI);
}
