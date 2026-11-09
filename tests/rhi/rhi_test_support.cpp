#include "conjure/testing/doctest.h"

#include "conjure/testing/test_environment.h"
#include "rhi_test_support.h"

#include "conjure/rhi/backend.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/swapchain.h"
#include "conjure/rhi/texture_format.h"

#include "conjure/core/log.h"

#include <memory>

using namespace conjure;
using namespace conjure::rhi;
using namespace conjure::testing;

namespace {

    /// <summary>
    /// Tries the software adapter once per process; a failed attempt is final so a machine without
    /// WARP (or without the D3D12 backend) skips every [gpu] case in a few milliseconds.
    /// </summary>
    bool software_gpu_available() noexcept {
        static const bool available = []() noexcept {
            if (!is_backend_compiled(graphics_backend::D3D12)) {
                return false;
            }
            // The probe may run outside any log_capture; keep the console quiet while it works.
            const bool consoleWasEnabled = core::log::console_enabled();
            core::log::set_console_enabled(false);
            const core::result<std::unique_ptr<device>> probe = device::create(rhi_support::software_device_desc());
            core::log::set_console_enabled(consoleWasEnabled);
            return probe.has_value();
        }();
        return available;
    }

    struct probe_installer {
        probe_installer() {
            set_precondition_probe(precondition::SOFTWARE_GPU_ADAPTER, &software_gpu_available);
        }
    };

    const probe_installer sProbeInstaller = {};
}

device_desc conjure::testing::rhi_support::null_device_desc(bool enableDebugLayer) {
    device_desc desc = {};
    desc.backend = graphics_backend::NULL_DEVICE;
    desc.enableDebugLayer = enableDebugLayer;
    return desc;
}

device_desc conjure::testing::rhi_support::software_device_desc(bool enableDebugLayer) {
    device_desc desc = {};
    desc.backend = graphics_backend::D3D12;
    desc.preferSoftwareAdapter = true;
    desc.enableDebugLayer = enableDebugLayer;
    return desc;
}

std::unique_ptr<device> conjure::testing::rhi_support::make_null_device(bool enableDebugLayer) {
    core::result<std::unique_ptr<device>> created = device::create(null_device_desc(enableDebugLayer));
    CONJURE_REQUIRE_OK(created);
    return std::move(created).value();
}

std::unique_ptr<device> conjure::testing::rhi_support::make_software_device(bool enableDebugLayer) {
    core::result<std::unique_ptr<device>> created = device::create(software_device_desc(enableDebugLayer));
    CONJURE_REQUIRE_OK(created);
    return std::move(created).value();
}

swapchain_desc conjure::testing::rhi_support::small_swapchain_desc(u32 width, u32 height) {
    swapchain_desc desc = {};
    desc.width = width;
    desc.height = height;
    desc.bufferCount = 3;
    desc.format = texture_format::RGBA8_SRGB;
    desc.vsync = false;
    return desc;
}

native_window conjure::testing::rhi_support::headless_window() noexcept {
    return native_window();
}
