#include "engine/subsystems/rhi_subsystem.h"

#include "conjure/engine/engine_error.h"
#include "conjure/engine/log_category.h"

#include "conjure/core/log.h"
#include "conjure/platform/backend.h"
#include "conjure/platform/window.h"
#include "conjure/rhi/swapchain.h"

#include <format>
#include <utility>

using namespace conjure;
using namespace conjure::engine;

namespace {

    [[nodiscard]] rhi::native_window_kind to_native_kind(platform::native_handle_kind kind) noexcept {
        switch (kind) {
            case platform::native_handle_kind::WINDOWS:
                return rhi::native_window_kind::WINDOWS;
            case platform::native_handle_kind::COCOA:
                return rhi::native_window_kind::COCOA;
            case platform::native_handle_kind::X11:
                return rhi::native_window_kind::X11;
            case platform::native_handle_kind::WAYLAND:
                return rhi::native_window_kind::WAYLAND;
            case platform::native_handle_kind::NONE:
                break;
        }
        return rhi::native_window_kind::NONE;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

rhi_subsystem::rhi_subsystem(engine_services &services) :
    mServices(&services) {}

rhi_subsystem::~rhi_subsystem() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

std::string_view rhi_subsystem::name() const noexcept {
    return "rhi";
}

core::result<void> rhi_subsystem::startup(engine_context &context) {
    const engine_config &config = *context.config;
    mSkipped = !config.rendering.enabled;
    if (mSkipped) {
        CONJURE_LOG_INFO(LOG_ENGINE, "rhi: rendering disabled, no device created");
        return core::result<void>::success();
    }

    const bool realWindow =
      context.window != nullptr && context.window->backend() != platform::backend_kind::NULL_BACKEND;
    rhi::device_desc desc = {};
    // DEBUG_LAYER and GPU_BASED both need the debug layer; only GPU_BASED adds GPU-based validation, which the
    // D3D12 backend alone acts on (code sweep F458).
    desc.enableDebugLayer = config.rendering.gpuValidation != gpu_validation_level::OFF;
    desc.enableGpuValidation = config.rendering.gpuValidation == gpu_validation_level::GPU_BASED;
    if (realWindow) {
        desc.window = to_native_window(context.window->native_handle());
    }

    core::error lastError = {};
    for (const rhi::graphics_backend backend : candidate_backends(config.rendering.backend)) {
        desc.backend = backend;
        core::result<std::unique_ptr<rhi::device>> created = rhi::device::create(desc);
        if (created.has_value()) {
            mServices->device = std::move(created).value();
            break;
        }
        lastError = created.get_error();
        CONJURE_LOG_WARN(
          LOG_ENGINE,
          "rhi: backend {} unavailable: {}",
          rhi::to_string(backend),
          lastError.to_string()
        );
    }
    if (mServices->device == nullptr) {
        return core::make_error(
          engine_error::DEVICE_CREATE_FAILED,
          std::format("no graphics device could be created: {}", lastError.to_string())
        );
    }

    if (realWindow) {
        const math::uvec2 size = context.window->framebuffer_size();
        rhi::swapchain_desc swapchainDesc = {};
        swapchainDesc.width = size.x;
        swapchainDesc.height = size.y;
        swapchainDesc.vsync = config.window.vsync;
        core::result<std::unique_ptr<rhi::swapchain>> swapchain =
          rhi::swapchain::create(*mServices->device, desc.window, swapchainDesc);
        if (!swapchain.has_value()) {
            const core::error error = swapchain.get_error();
            mServices->device.reset();
            return core::make_error(
              engine_error::DEVICE_CREATE_FAILED,
              std::format("swapchain creation failed: {}", error.to_string())
            );
        }
        mServices->swapchain = std::move(swapchain).value();
    }

    context.device = mServices->device.get();
    CONJURE_LOG_INFO(
      LOG_ENGINE,
      "rhi: {} device{}",
      rhi::to_string(mServices->device->backend()),
      mServices->swapchain != nullptr ? " with swapchain" : " (no swapchain)"
    );
    return core::result<void>::success();
}

void rhi_subsystem::shutdown(engine_context &context) {
    context.device = nullptr;
    if (mServices->device != nullptr) {
        mServices->device->wait_idle();
    }
    mServices->swapchain.reset();
    if (mServices->device != nullptr) {
        mServices->device->run_garbage_collection();
    }
    mServices->device.reset();
    mSkipped = false;
}

std::vector<rhi::graphics_backend> rhi_subsystem::candidate_backends(rhi_backend_choice choice) {
    std::vector<rhi::graphics_backend> candidates = {};
    switch (choice) {
        case rhi_backend_choice::NULL_DEVICE:
            candidates.push_back(rhi::graphics_backend::NULL_DEVICE);
            break;
        case rhi_backend_choice::D3D12:
            candidates.push_back(rhi::graphics_backend::D3D12);
            break;
        case rhi_backend_choice::VULKAN:
            candidates.push_back(rhi::graphics_backend::VULKAN);
            break;
        case rhi_backend_choice::AUTO: {
            const rhi::graphics_backend preferred = rhi::default_graphics_backend();
            candidates.push_back(preferred);
            const rhi::graphics_backend other = preferred == rhi::graphics_backend::D3D12
              ? rhi::graphics_backend::VULKAN
              : rhi::graphics_backend::D3D12;
            if (other != preferred && rhi::is_backend_compiled(other)) {
                candidates.push_back(other);
            }
            break;
        }
    }
    return candidates;
}

rhi::native_window rhi_subsystem::to_native_window(const platform::native_window_handle &handle) noexcept {
    rhi::native_window window = {};
    window.kind = to_native_kind(handle.kind);
    window.window = handle.window;
    window.display = handle.display;
    window.x11Window = handle.x11Window;
    return window;
}
