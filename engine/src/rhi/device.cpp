#include "conjure/rhi/device.h"

#include "conjure/rhi/log_category.h"
#include "conjure/rhi/rhi_error.h"
#include "rhi/backends/null/null_device.h"
#include "rhi/device_backend.h"
#include "rhi/device_impl.h"
#include "rhi/message_callback.h"

#include "conjure/core/error.h"
#include "conjure/core/log.h"

#include <nvrhi/nvrhi.h>
#include <nvrhi/validation.h>

#include <utility>

#if CONJURE_RHI_D3D12
    #include "rhi/backends/d3d12/device_d3d12.h"
#endif
#if CONJURE_RHI_VULKAN
    #include "rhi/backends/vulkan/device_vulkan.h"
#endif

using namespace conjure;
using namespace conjure::rhi;

namespace {
    core::result<std::unique_ptr<device_backend>> create_backend(const device_desc &desc, message_callback &messages) {
        switch (desc.backend) {
            case graphics_backend::NULL_DEVICE: {
                return null_backend::create_null_backend(desc, messages);
            }
            case graphics_backend::D3D12: {
#if CONJURE_RHI_D3D12
                return d3d12_backend::create_d3d12_backend(desc, messages);
#else
                break;
#endif
            }
            case graphics_backend::VULKAN: {
#if CONJURE_RHI_VULKAN
                return vulkan_backend::create_vulkan_backend(desc, messages);
#else
                break;
#endif
            }
            case graphics_backend::COUNT: {
                break;   // rejected by device::create before this is reached
            }
        }
        return core::make_error(
          core::error_code::UNSUPPORTED,
          "graphics backend '{}' is not compiled into this build",
          to_string(desc.backend)
        );
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

device::device(create_tag) :
    mImpl(std::make_unique<impl>()) {}

device::~device() {
    if (mImpl == nullptr || mImpl->backend == nullptr) {
        return;
    }
    // Every NVRHI object the renderer created is referenced by command lists in flight; draining the
    // queue before the wrapper and the API objects go away keeps destruction order-independent. A
    // device the API removed since the last poll must not be waited on, so this is the polling is_lost().
    if (!is_lost()) {
        mImpl->nvrhiDevice->waitForIdle();
        mImpl->backend->flush_queues();
        mImpl->nvrhiDevice->runGarbageCollection();
    }
    CONJURE_LOG_INFO(LOG_RHI, "destroying {} device", to_string(mImpl->desc.backend));
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<std::unique_ptr<device>> device::create(const device_desc &desc) {
    if (desc.backend >= graphics_backend::COUNT) {
        return core::make_error(rhi_error::INVALID_DESC, "device_desc: backend is out of range");
    }
    if (desc.enableGpuValidation && !desc.enableDebugLayer) {
        return core::make_error(rhi_error::INVALID_DESC, "device_desc: enableGpuValidation requires enableDebugLayer");
    }
    if (!is_backend_compiled(desc.backend)) {
        return core::make_error(
          core::error_code::UNSUPPORTED,
          "graphics backend '{}' is not compiled into this build",
          to_string(desc.backend)
        );
    }

    std::unique_ptr<device> created = std::make_unique<device>(create_tag{});
    impl &state = *created->mImpl;
    state.desc = desc;
    state.messages = std::make_unique<message_callback>();

    CONJURE_TRY_ASSIGN(state.backend, create_backend(desc, *state.messages));

    // The NVRHI validation layer catches API misuse before the driver does; it is the natural
    // companion of the debug layer and costs nothing when off.
    nvrhi::IDevice *underlying = state.backend->nvrhi_device();
    if (desc.enableDebugLayer) {
        state.nvrhiDevice = nvrhi::validation::createValidationLayer(underlying);
    } else {
        state.nvrhiDevice = underlying;
    }

    const device_capabilities &capabilities = state.backend->capabilities();
    const char *debugLayer = "off";
    if (desc.enableDebugLayer) {
        debugLayer = "on";
    } else if (state.backend->debug_layer_inherited()) {
        debugLayer = "on (enabled by an earlier device of this process)";
    }
    CONJURE_LOG_INFO(
      LOG_RHI,
      "created {} device on '{}' ({} MiB dedicated video memory, compute queue {}, debug layer {})",
      to_string(desc.backend),
      capabilities.adapterName,
      capabilities.dedicatedVideoMemoryBytes / MIB,
      capabilities.computeQueue ? "yes" : "no",
      debugLayer
    );
    return created;
}

graphics_backend device::backend() const noexcept {
    return mImpl->desc.backend;
}

const device_desc &device::desc() const noexcept {
    return mImpl->desc;
}

const device_capabilities &device::capabilities() const noexcept {
    return mImpl->backend->capabilities();
}

bool device::is_lost() const noexcept {
    if (mImpl->messages->is_lost()) {
        return true;
    }
    if (mImpl->backend->poll_device_removed()) {
        mImpl->messages->mark_lost();
        return true;
    }
    return false;
}

nvrhi::IDevice *device::native_device() const noexcept {
    return mImpl->nvrhiDevice.Get();
}

void device::run_garbage_collection() {
    mImpl->nvrhiDevice->runGarbageCollection();
}

void device::wait_idle() {
    if (!mImpl->nvrhiDevice->waitForIdle()) {
        CONJURE_LOG_ERROR(LOG_RHI, "waitForIdle failed on the {} device; marking it lost", to_string(backend()));
        mImpl->messages->mark_lost();
        return;
    }
    // waitForIdle covers what NVRHI executed; the backend waits for what the API queued behind it (a present).
    mImpl->backend->flush_queues();
}

std::optional<null_device_stats> device::null_stats() const {
    null_backend::null_device *nullDevice = mImpl->backend->as_null();
    if (nullDevice == nullptr) {
        return std::nullopt;
    }
    return nullDevice->stats();
}

void device::reset_null_stats() {
    null_backend::null_device *nullDevice = mImpl->backend->as_null();
    if (nullDevice != nullptr) {
        nullDevice->reset_stats();
    }
}

void device::simulate_device_loss() {
    mImpl->messages->message(nvrhi::MessageSeverity::Fatal, "device loss simulated by device::simulate_device_loss()");
}

void device::inject_null_fault(null_fault fault) {
    null_backend::null_device *nullDevice = mImpl->backend->as_null();
    if (nullDevice != nullptr) {
        nullDevice->inject_fault(fault);
    }
}
