#include "rhi/message_callback.h"

#include "conjure/rhi/log_category.h"

#include "conjure/core/log.h"

using namespace conjure;
using namespace conjure::rhi;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

message_callback::message_callback() {}

message_callback::~message_callback() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void message_callback::message(nvrhi::MessageSeverity severity, const char *messageText) {
    const char *text = messageText != nullptr ? messageText : "";
    switch (severity) {
        case nvrhi::MessageSeverity::Info: {
            CONJURE_LOG_INFO(LOG_RHI, "nvrhi: {}", text);
            return;
        }
        case nvrhi::MessageSeverity::Warning: {
            CONJURE_LOG_WARN(LOG_RHI, "nvrhi: {}", text);
            return;
        }
        case nvrhi::MessageSeverity::Error: {
            CONJURE_LOG_ERROR(LOG_RHI, "nvrhi: {}", text);
            return;
        }
        case nvrhi::MessageSeverity::Fatal: {
            // A Fatal message is NVRHI's device-removal report; the device is unusable from here on.
            mark_lost();
            CONJURE_LOG_ERROR(LOG_RHI, "nvrhi fatal (device lost): {}", text);
            return;
        }
    }
    CONJURE_LOG_WARN(LOG_RHI, "nvrhi (unknown severity): {}", text);
}
