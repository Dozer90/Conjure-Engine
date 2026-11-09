#pragma once

#include "rhi/device_backend.h"
#include "rhi/message_callback.h"

#include "conjure/rhi/device.h"

#include <nvrhi/nvrhi.h>

#include <memory>

// The private state of rhi::device, shared with swapchain.cpp (a friend) so the swapchain can ask
// the backend for its API swapchain without the public header exposing device_backend.
//
// Destruction order matters: the validation wrapper (nvrhiDevice) releases its reference to the
// backend's device first, the backend then destroys the API objects, and the message callback
// outlives both because NVRHI may still report during teardown. Members are declared in reverse
// destruction order.

namespace conjure::rhi {

    struct device::impl {
        device_desc desc = {};
        std::unique_ptr<message_callback> messages = nullptr;
        std::unique_ptr<device_backend> backend = nullptr;
        nvrhi::DeviceHandle nvrhiDevice = nullptr;
    };
}
