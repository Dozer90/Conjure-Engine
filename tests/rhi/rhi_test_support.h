#pragma once

#include "conjure/rhi/device.h"
#include "conjure/rhi/native_window.h"
#include "conjure/rhi/swapchain.h"

#include "conjure/core/types.h"

#include <memory>

// Helpers shared by the rhi tests.
//
//   std::unique_ptr<device> device = make_null_device();
//   CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);   // [gpu] cases, WARP through software_device_desc()
//
// rhi_test_support.cpp installs the SOFTWARE_GPU_ADAPTER probe (08_testing.md 3.2: "probe installed by
// rhi tests") from a static initialiser, so every test executable that links this file can skip
// GPU cases cleanly where no software adapter exists.

namespace conjure::testing::rhi_support {

    /// <summary>
    /// Description of a null device, optionally wrapped in the NVRHI validation layer.
    /// </summary>
    [[nodiscard]] rhi::device_desc null_device_desc(bool enableDebugLayer = false);

    /// <summary>
    /// Description of the software (WARP) D3D12 device the [gpu] cases use.
    /// </summary>
    [[nodiscard]] rhi::device_desc software_device_desc(bool enableDebugLayer = false);

    /// <summary>
    /// Creates a null device or fails the case with the error message.
    /// </summary>
    [[nodiscard]] std::unique_ptr<rhi::device> make_null_device(bool enableDebugLayer = false);

    /// <summary>
    /// Creates the software D3D12 device or fails the case; call after CONJURE_TEST_REQUIRE.
    /// </summary>
    [[nodiscard]] std::unique_ptr<rhi::device> make_software_device(bool enableDebugLayer = false);

    /// <summary>
    /// A valid swapchain description of the given size.
    /// </summary>
    [[nodiscard]] rhi::swapchain_desc small_swapchain_desc(u32 width = 64, u32 height = 32);

    /// <summary>
    /// The headless window handle (kind NONE).
    /// </summary>
    [[nodiscard]] rhi::native_window headless_window() noexcept;
}
