#include "conjure/rhi/swapchain.h"

#include "conjure/rhi/log_category.h"
#include "conjure/rhi/rhi_error.h"
#include "rhi/device_backend.h"
#include "rhi/device_impl.h"

#include "conjure/core/error.h"
#include "conjure/core/log.h"

#include <nvrhi/nvrhi.h>

#include <utility>

using namespace conjure;
using namespace conjure::rhi;

/*===========================================================================*/
/*===========================================================================*/
// impl

struct swapchain::impl {
    device *owner = nullptr;
    std::unique_ptr<swapchain_backend> backend = nullptr;
};

namespace {
    core::error device_lost_error(const device &target) {
        return core::make_error(rhi_error::DEVICE_LOST, "the {} device was lost", to_string(target.backend()));
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

swapchain::swapchain(create_tag) :
    mImpl(std::make_unique<impl>()) {}

swapchain::~swapchain() {
    if (mImpl == nullptr || mImpl->backend == nullptr) {
        return;
    }
    // The back-buffer textures may still be referenced by the last frame's command list.
    if (!mImpl->owner->is_lost()) {
        mImpl->owner->wait_idle();
        mImpl->owner->run_garbage_collection();
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<std::unique_ptr<swapchain>> swapchain::create(
  device &device,
  const native_window &window,
  const swapchain_desc &desc
) {
    CONJURE_TRY(validate_swapchain_desc(desc));
    if (device.is_lost()) {
        return device_lost_error(device);
    }

    std::unique_ptr<swapchain> created = std::make_unique<swapchain>(create_tag{});
    created->mImpl->owner = &device;
    CONJURE_TRY_ASSIGN(created->mImpl->backend, device.mImpl->backend->create_swapchain(window, desc));

    CONJURE_LOG_INFO(
      LOG_RHI,
      "created {} swapchain {}x{} ({} buffers, {}, vsync {}, window {})",
      to_string(device.backend()),
      desc.width,
      desc.height,
      desc.bufferCount,
      to_string(desc.format),
      desc.vsync ? "on" : "off",
      to_string(window.kind)
    );
    return created;
}

core::result<u32> swapchain::acquire() {
    if (mImpl->owner->is_lost()) {
        return device_lost_error(*mImpl->owner);
    }
    return mImpl->backend->acquire();
}

core::result<void> swapchain::present() {
    if (mImpl->owner->is_lost()) {
        return device_lost_error(*mImpl->owner);
    }
    core::result<void> presented = mImpl->backend->present();
    if (!presented.has_value() && presented.get_error().is(rhi_error::DEVICE_LOST)) {
        CONJURE_LOG_ERROR(LOG_RHI, "present failed: {}", presented.get_error().message());
    }
    return presented;
}

core::result<void> swapchain::resize(u32 width, u32 height) {
    if (width == 0 || height == 0) {
        return core::make_error(
          rhi_error::INVALID_DESC,
          "swapchain::resize: size must be non-zero, got {}x{}",
          width,
          height
        );
    }
    if (mImpl->owner->is_lost()) {
        return device_lost_error(*mImpl->owner);
    }
    CONJURE_TRY(mImpl->backend->resize(width, height));
    CONJURE_LOG_DEBUG(LOG_RHI, "resized swapchain to {}x{}", width, height);
    return core::result<void>::success();
}

nvrhi::ITexture *swapchain::back_buffer(u32 index) const noexcept {
    return mImpl->backend->back_buffer(index);
}

u32 swapchain::buffer_count() const noexcept {
    return mImpl->backend->buffer_count();
}

u32 swapchain::current_index() const noexcept {
    return mImpl->backend->current_index();
}

const swapchain_desc &swapchain::desc() const noexcept {
    return mImpl->backend->desc();
}

device &swapchain::owner() const noexcept {
    return *mImpl->owner;
}
