#pragma once

#include "conjure/core/api.h"
#include "conjure/core/error.h"
#include "conjure/core/types.h"

#include <string_view>

// Errors of the rhi module (docs/design/02_renderer.md 6).
//
// Generic conditions keep the core codes: a backend that is not compiled in fails with
// core::error_code::UNSUPPORTED, a missing shader with NOT_FOUND, a corrupt index.json with
// PARSE_FAILURE and an unreadable blob with IO_FAILURE. The codes below are the conditions only
// a graphics API can produce.

namespace conjure::rhi {

    enum class rhi_error : u16 {
        DEVICE_CREATION_FAILED = 1,     // the API refused to create the device; the message carries its text
        DEVICE_LOST,                    // removed: DXGI_ERROR_DEVICE_REMOVED, VK_ERROR_DEVICE_LOST or a Fatal message
        OUT_OF_DATE,                    // the swapchain no longer matches the surface and must be resized
        SWAPCHAIN_CREATION_FAILED,      // the API refused to create or resize the swapchain
        INVALID_DESC,                   // a device_desc or swapchain_desc field is invalid; the message names it
        PRESENT_FAILED,                 // present was refused for a reason other than device loss or OUT_OF_DATE
        COUNT
    };

    /// <summary>
    /// The enumerator name of an rhi error code ("DEVICE_LOST"), or "" for unknown values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view rhi_error_name(rhi_error code) noexcept;
}

CONJURE_DECLARE_ERROR_CATEGORY(conjure::rhi::rhi_error, "rhi", conjure::rhi::rhi_error_name);
