#include "rhi/backends/null/null_swapchain.h"

#include "rhi/backends/null/null_device.h"
#include "rhi/message_callback.h"
#include "rhi/texture_format_mapping.h"

#include "conjure/rhi/rhi_error.h"

#include "conjure/core/error.h"

#include <format>

using namespace conjure;
using namespace conjure::rhi;
using namespace conjure::rhi::null_backend;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

null_swapchain::null_swapchain(null_device &device, const swapchain_desc &desc) :
    mDevice(device),
    mDesc(desc) {
    create_back_buffers();
    mDevice.record([](null_device_stats &stats) {
        ++stats.swapchainsCreated;
    });
}

null_swapchain::~null_swapchain() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<u32> null_swapchain::acquire() {
    mDevice.record([](null_device_stats &stats) {
        ++stats.acquires;
    });
    return mCurrentIndex;
}

core::result<void> null_swapchain::present() {
    mDevice.record([](null_device_stats &stats) {
        ++stats.presents;
    });
    if (mDevice.consume_fault(null_fault::PRESENT)) {
        // Mirrors the D3D12 and Vulkan backends: the present that observes removal marks the device
        // lost and reports DEVICE_LOST; the image is not advanced.
        mDevice.messages().mark_lost();
        return core::make_error(rhi_error::DEVICE_LOST, "null present reported device loss (injected null_fault)");
    }
    mCurrentIndex = (mCurrentIndex + 1) % mDesc.bufferCount;
    return core::result<void>::success();
}

core::result<void> null_swapchain::resize(u32 width, u32 height) {
    if (width == 0 || height == 0) {
        return core::make_error(rhi_error::INVALID_DESC, "swapchain size must be non-zero, got {}x{}", width, height);
    }
    mDesc.width = width;
    mDesc.height = height;
    mBackBuffers.clear();
    create_back_buffers();
    mCurrentIndex = 0;
    mDevice.record([](null_device_stats &stats) {
        ++stats.resizes;
    });
    return core::result<void>::success();
}

nvrhi::ITexture *null_swapchain::back_buffer(u32 index) const noexcept {
    if (index >= mBackBuffers.size()) {
        return nullptr;
    }
    return mBackBuffers[index].Get();
}

u32 null_swapchain::buffer_count() const noexcept {
    // The number of back_buffer() indices that answer non-null, like every other backend.
    return static_cast<u32>(mBackBuffers.size());
}

u32 null_swapchain::current_index() const noexcept {
    return mCurrentIndex;
}

const swapchain_desc &null_swapchain::desc() const noexcept {
    return mDesc;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

void null_swapchain::create_back_buffers() {
    mBackBuffers.reserve(mDesc.bufferCount);
    for (u32 index = 0; index < mDesc.bufferCount; ++index) {
        nvrhi::TextureDesc textureDesc = {};
        textureDesc.width = mDesc.width;
        textureDesc.height = mDesc.height;
        textureDesc.format = to_nvrhi_format(mDesc.format);
        textureDesc.dimension = nvrhi::TextureDimension::Texture2D;
        textureDesc.isRenderTarget = true;
        textureDesc.isShaderResource = false;
        textureDesc.initialState = nvrhi::ResourceStates::Present;
        textureDesc.keepInitialState = true;
        textureDesc.debugName = std::format("swapchain buffer {}", index);
        mBackBuffers.emplace_back(
          mDevice.createHandleForNativeTexture(nvrhi::ObjectTypes::SharedHandle, nullptr, textureDesc)
        );
    }
}
