#pragma once

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <string>
#include <string_view>

// Graphics backends of the rhi module (docs/design/02_renderer.md 4.1).
//
// NULL_DEVICE is always compiled: it is a complete nvrhi::IDevice that stores descriptors, hands
// out valid handles and records what command lists do (null_device_stats.h), so every renderer
// test runs without a GPU. D3D12 and VULKAN exist when CONJURE_RHI_D3D12 / CONJURE_RHI_VULKAN are
// enabled; asking for a backend that is not compiled fails with core::error_code::UNSUPPORTED.

namespace conjure::rhi {

    enum class graphics_backend : u8 {
        NULL_DEVICE = 0,
        D3D12,
        VULKAN,
        COUNT
    };

    /// <summary>
    /// What the created device can do; filled by device::create from the adapter.
    /// </summary>
    struct device_capabilities {
        graphics_backend backend = graphics_backend::NULL_DEVICE;
        std::string adapterName = {};
        u32 maxTextureDimension2d = 16384;
        u64 dedicatedVideoMemoryBytes = 0;
        bool computeQueue = false;
        bool timerQueries = false;
        bool softwareAdapter = false;       // WARP, lavapipe or the null device
    };

    /// <summary>
    /// The lower-case name of a backend: "null", "d3d12" or "vulkan"; "" for an out-of-range value.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(graphics_backend backend) noexcept;

    /// <summary>
    /// Parses a backend name case-insensitively, ignoring surrounding whitespace ("null", "d3d12",
    /// "vulkan"). Fails with core::error_code::INVALID_ARGUMENT for anything else.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<graphics_backend> parse_graphics_backend(std::string_view name) noexcept;

    /// <summary>
    /// True when the backend was compiled into this build. NULL_DEVICE is always true.
    /// </summary>
    [[nodiscard]] CONJURE_API bool is_backend_compiled(graphics_backend backend) noexcept;

    /// <summary>
    /// The backend the engine tries first: D3D12 when compiled, otherwise VULKAN when compiled,
    /// otherwise NULL_DEVICE.
    /// </summary>
    [[nodiscard]] CONJURE_API graphics_backend default_graphics_backend() noexcept;
}
