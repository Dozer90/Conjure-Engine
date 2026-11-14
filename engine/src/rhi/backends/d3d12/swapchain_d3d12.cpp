#include "rhi/backends/d3d12/swapchain_d3d12.h"

#include "rhi/backends/d3d12/device_d3d12.h"
#include "rhi/message_callback.h"
#include "rhi/texture_format_mapping.h"

#include "conjure/rhi/log_category.h"
#include "conjure/rhi/rhi_error.h"

#include "conjure/core/error.h"
#include "conjure/core/log.h"

#include <format>

using namespace conjure;
using namespace conjure::rhi;
using namespace conjure::rhi::d3d12_backend;
using Microsoft::WRL::ComPtr;

namespace {
    /// <summary>
    /// The DXGI format of the swapchain buffers: flip model rejects *_SRGB, so sRGB formats are
    /// created as UNORM and viewed as sRGB through the NVRHI texture description.
    /// </summary>
    DXGI_FORMAT buffer_format(texture_format format) noexcept {
        switch (format) {
            case texture_format::RGBA8_UNORM:
            case texture_format::RGBA8_SRGB: {
                return DXGI_FORMAT_R8G8B8A8_UNORM;
            }
            case texture_format::BGRA8_SRGB: {
                return DXGI_FORMAT_B8G8R8A8_UNORM;
            }
            case texture_format::RGBA16_FLOAT: {
                return DXGI_FORMAT_R16G16B16A16_FLOAT;
            }
            default: {
                return DXGI_FORMAT_UNKNOWN;
            }
        }
    }

    bool is_device_removed(HRESULT result) noexcept {
        return result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET ||
          result == DXGI_ERROR_DEVICE_HUNG || result == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    }

    core::error unusable_error() {
        return core::make_error(
          rhi_error::OUT_OF_DATE,
          "the swapchain has no back buffers (the last resize did not complete); resize it again"
        );
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

d3d12_swapchain::d3d12_swapchain(d3d12_device_backend &backend, const swapchain_desc &desc) :
    mBackend(backend),
    mDesc(desc) {}

d3d12_swapchain::~d3d12_swapchain() {
    // The last present is still on the queue and references the buffers; DXGI gives them their final release
    // when the swapchain goes, which the debug layer reports as OBJECT_DELETED_WHILE_STILL_IN_USE (and breaks
    // on) unless the queue has drained first. flush_queues() returns at once on a removed device.
    mBackend.flush_queues();
    release_back_buffers();
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<void> d3d12_swapchain::initialise(const native_window &window) {
    if (window.kind != native_window_kind::NONE && window.kind != native_window_kind::WINDOWS) {
        return core::make_error(
          rhi_error::INVALID_DESC,
          "d3d12 swapchain needs a WINDOWS window handle or none, got {}",
          to_string(window.kind)
        );
    }
    if (window.kind == native_window_kind::WINDOWS && window.window == nullptr) {
        return core::make_error(rhi_error::INVALID_DESC, "d3d12 swapchain: the WINDOWS window handle is null");
    }

    mFlags = mBackend.allow_tearing() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
    swapchainDesc.Width = mDesc.width;
    swapchainDesc.Height = mDesc.height;
    swapchainDesc.Format = buffer_format(mDesc.format);
    swapchainDesc.Stereo = FALSE;
    swapchainDesc.SampleDesc.Count = 1;
    swapchainDesc.SampleDesc.Quality = 0;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchainDesc.BufferCount = mDesc.bufferCount;
    swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchainDesc.AlphaMode = window.is_headless() ? DXGI_ALPHA_MODE_IGNORE : DXGI_ALPHA_MODE_UNSPECIFIED;
    swapchainDesc.Flags = mFlags;

    ComPtr<IDXGISwapChain1> swapchain1 = nullptr;
    HRESULT created = E_FAIL;
    if (window.is_headless()) {
        created = mBackend.factory()->CreateSwapChainForComposition(
          mBackend.graphics_queue(),
          &swapchainDesc,
          nullptr,
          &swapchain1
        );
    } else {
        HWND handle = static_cast<HWND>(window.window);
        created = mBackend.factory()->CreateSwapChainForHwnd(
          mBackend.graphics_queue(),
          handle,
          &swapchainDesc,
          nullptr,
          nullptr,
          &swapchain1
        );
        if (SUCCEEDED(created)) {
            // The engine handles fullscreen as borderless windows (11_platform.md P12); Alt+Enter
            // exclusive mode would fight the swapchain.
            mBackend.factory()->MakeWindowAssociation(handle, DXGI_MWA_NO_ALT_ENTER);
        }
    }
    if (FAILED(created)) {
        return core::make_error(
          rhi_error::SWAPCHAIN_CREATION_FAILED,
          "IDXGIFactory2::CreateSwapChainFor{} failed with {}",
          window.is_headless() ? "Composition" : "Hwnd",
          describe_hresult(created)
        );
    }
    const HRESULT upgraded = swapchain1.As(&mSwapchain);
    if (FAILED(upgraded)) {
        return core::make_error(
          rhi_error::SWAPCHAIN_CREATION_FAILED,
          "IDXGISwapChain3 is unavailable ({})",
          describe_hresult(upgraded)
        );
    }

    CONJURE_TRY(wrap_back_buffers());
    mCurrentIndex = mSwapchain->GetCurrentBackBufferIndex();
    return core::result<void>::success();
}

core::result<u32> d3d12_swapchain::acquire() {
    // A resize that failed in ResizeBuffers or while wrapping left no (or only some) back buffers, so
    // the index DXGI reports would name a null back_buffer(); answer like Vulkan until a resize succeeds.
    if (!has_back_buffers()) {
        return unusable_error();
    }
    mCurrentIndex = mSwapchain->GetCurrentBackBufferIndex();
    return mCurrentIndex;
}

core::result<void> d3d12_swapchain::present() {
    if (!has_back_buffers()) {
        return unusable_error();
    }
    const UINT syncInterval = mDesc.vsync ? 1 : 0;
    const UINT presentFlags = (!mDesc.vsync && mBackend.allow_tearing()) ? DXGI_PRESENT_ALLOW_TEARING : 0;
    const HRESULT presented = mSwapchain->Present(syncInterval, presentFlags);
    if (is_device_removed(presented)) {
        mBackend.messages().mark_lost();
        return core::make_error(
          rhi_error::DEVICE_LOST,
          "IDXGISwapChain::Present reported {} (removed reason {})",
          describe_hresult(presented),
          describe_hresult(mBackend.d3d_device()->GetDeviceRemovedReason())
        );
    }
    // DXGI_STATUS_OCCLUDED is a success code: the window is hidden and the frame was dropped.
    if (FAILED(presented)) {
        return core::make_error(
          rhi_error::PRESENT_FAILED,
          "IDXGISwapChain::Present failed with {}",
          describe_hresult(presented)
        );
    }
    mCurrentIndex = mSwapchain->GetCurrentBackBufferIndex();
    return core::result<void>::success();
}

core::result<void> d3d12_swapchain::resize(u32 width, u32 height) {
    // Every buffer reference must be gone before ResizeBuffers: wait for the GPU (NVRHI's command lists,
    // then the present queued behind them, which waitForIdle does not cover), drop the NVRHI wrappers and
    // let NVRHI release what its finished command lists still hold. References held by the caller (a
    // framebuffer over a back buffer) are the caller's to drop first; ResizeBuffers refuses otherwise.
    nvrhi::IDevice *device = mBackend.nvrhi_device();
    device->waitForIdle();
    mBackend.flush_queues();
    release_back_buffers();
    device->runGarbageCollection();

    const HRESULT resized = mSwapchain->ResizeBuffers(
      mDesc.bufferCount,
      width,
      height,
      buffer_format(mDesc.format),
      mFlags
    );
    if (is_device_removed(resized)) {
        mBackend.messages().mark_lost();
        return core::make_error(
          rhi_error::DEVICE_LOST,
          "IDXGISwapChain::ResizeBuffers reported {}",
          describe_hresult(resized)
        );
    }
    if (FAILED(resized)) {
        return core::make_error(
          rhi_error::SWAPCHAIN_CREATION_FAILED,
          "IDXGISwapChain::ResizeBuffers({}x{}) failed with {}",
          width,
          height,
          describe_hresult(resized)
        );
    }
    mDesc.width = width;
    mDesc.height = height;
    CONJURE_TRY(wrap_back_buffers());
    mCurrentIndex = mSwapchain->GetCurrentBackBufferIndex();
    return core::result<void>::success();
}

nvrhi::ITexture *d3d12_swapchain::back_buffer(u32 index) const noexcept {
    if (index >= mBackBuffers.size()) {
        return nullptr;
    }
    return mBackBuffers[index].Get();
}

u32 d3d12_swapchain::buffer_count() const noexcept {
    // The number of back_buffer() indices that answer non-null: 0 after a resize() that failed in
    // ResizeBuffers or while wrapping, until the next resize() succeeds.
    return static_cast<u32>(mBackBuffers.size());
}

u32 d3d12_swapchain::current_index() const noexcept {
    return mCurrentIndex;
}

const swapchain_desc &d3d12_swapchain::desc() const noexcept {
    return mDesc;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

bool d3d12_swapchain::has_back_buffers() const noexcept {
    return mBackBuffers.size() == mDesc.bufferCount;
}

core::result<void> d3d12_swapchain::wrap_back_buffers() {
    // Every failure below drops the wrappers made so far, so a failed wrap leaves buffer_count() at 0
    // as swapchain.h promises instead of a partial list.
    mBackBuffers.reserve(mDesc.bufferCount);
    for (u32 index = 0; index < mDesc.bufferCount; ++index) {
        ComPtr<ID3D12Resource> resource = nullptr;
        const HRESULT fetched = mSwapchain->GetBuffer(index, IID_PPV_ARGS(&resource));
        if (FAILED(fetched)) {
            release_back_buffers();
            return core::make_error(
              rhi_error::SWAPCHAIN_CREATION_FAILED,
              "IDXGISwapChain::GetBuffer({}) failed with {}",
              index,
              describe_hresult(fetched)
            );
        }

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

        // NVRHI AddRefs the resource inside the wrapper, so the ComPtr may go out of scope here.
        nvrhi::TextureHandle texture = mBackend.nvrhi_device()->createHandleForNativeTexture(
          nvrhi::ObjectTypes::D3D12_Resource,
          nvrhi::Object(resource.Get()),
          textureDesc
        );
        if (texture == nullptr) {
            release_back_buffers();
            return core::make_error(
              rhi_error::SWAPCHAIN_CREATION_FAILED,
              "nvrhi refused to wrap swapchain buffer {}",
              index
            );
        }
        mBackBuffers.emplace_back(std::move(texture));
    }
    return core::result<void>::success();
}

void d3d12_swapchain::release_back_buffers() {
    mBackBuffers.clear();
}
