#include "rhi/backends/d3d12/device_d3d12.h"

#include "rhi/backends/d3d12/swapchain_d3d12.h"
#include "rhi/message_callback.h"

#include "conjure/rhi/log_category.h"
#include "conjure/rhi/rhi_error.h"

#include "conjure/core/error.h"
#include "conjure/core/log.h"
#include "conjure/core/string_utils.h"

#include <format>
#include <string>
#include <utility>

using namespace conjure;
using namespace conjure::rhi;
using namespace conjure::rhi::d3d12_backend;
using Microsoft::WRL::ComPtr;

namespace {
    constexpr D3D_FEATURE_LEVEL MINIMUM_FEATURE_LEVEL = D3D_FEATURE_LEVEL_11_0;

    core::error creation_failure(const char *what, HRESULT result) {
        return core::make_error(rhi_error::DEVICE_CREATION_FAILED, "{} failed with {}", what, describe_hresult(result));
    }

    core::result<ComPtr<ID3D12CommandQueue>> create_queue(
      ID3D12Device &device,
      D3D12_COMMAND_LIST_TYPE type,
      const wchar_t *name
    ) {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = type;
        queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.NodeMask = 0;

        ComPtr<ID3D12CommandQueue> queue = nullptr;
        const HRESULT created = device.CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue));
        if (FAILED(created)) {
            return creation_failure("ID3D12Device::CreateCommandQueue", created);
        }
        queue->SetName(name);
        return queue;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

d3d12_device_backend::d3d12_device_backend(message_callback &messages) :
    mMessages(messages) {
    mCapabilities.backend = graphics_backend::D3D12;
}

d3d12_device_backend::~d3d12_device_backend() {
    // The NVRHI device holds references to queues and the device; release it before the COM objects.
    // A removed device is never waited on, whether the removal was reported or is only visible to
    // GetDeviceRemovedReason (the same poll device::is_lost() performs).
    if (mNvrhiDevice != nullptr && !mMessages.is_lost() && !poll_device_removed()) {
        mNvrhiDevice->waitForIdle();
        mNvrhiDevice->runGarbageCollection();
    }
    mNvrhiDevice = nullptr;
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<void> d3d12_device_backend::initialise(const device_desc &desc) {
    UINT factoryFlags = 0;
    if (desc.enableDebugLayer) {
        ComPtr<ID3D12Debug> debug = nullptr;
        const HRESULT debugResult = D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
        if (SUCCEEDED(debugResult)) {
            debug->EnableDebugLayer();
            factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            if (desc.enableGpuValidation) {
                ComPtr<ID3D12Debug1> debug1 = nullptr;
                if (SUCCEEDED(debug.As(&debug1))) {
                    debug1->SetEnableGPUBasedValidation(TRUE);
                } else {
                    CONJURE_LOG_WARN(LOG_RHI, "d3d12: GPU-based validation is unavailable (ID3D12Debug1 missing)");
                }
            }
        } else {
            // The debug layer is an optional Windows feature; its absence is not a creation failure.
            CONJURE_LOG_WARN(
              LOG_RHI,
              "d3d12: debug layer requested but unavailable ({}); install 'Graphics Tools'",
              describe_hresult(debugResult)
            );
        }
    }

    const HRESULT factoryResult = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&mFactory));
    if (FAILED(factoryResult)) {
        return creation_failure("CreateDXGIFactory2", factoryResult);
    }

    ComPtr<IDXGIFactory5> factory5 = nullptr;
    if (SUCCEEDED(mFactory.As(&factory5))) {
        BOOL allowTearing = FALSE;
        const HRESULT tearingResult = factory5->CheckFeatureSupport(
          DXGI_FEATURE_PRESENT_ALLOW_TEARING,
          &allowTearing,
          sizeof(allowTearing)
        );
        if (SUCCEEDED(tearingResult)) {
            mAllowTearing = allowTearing != FALSE;
        }
    }

    CONJURE_TRY(select_adapter(desc));

    const HRESULT deviceResult = D3D12CreateDevice(mAdapter.Get(), MINIMUM_FEATURE_LEVEL, IID_PPV_ARGS(&mDevice));
    if (FAILED(deviceResult)) {
        return creation_failure("D3D12CreateDevice", deviceResult);
    }
    mDevice->SetName(L"conjure d3d12 device");

    CONJURE_TRY(create_queues());

    nvrhi::d3d12::DeviceDesc nvrhiDesc = {};
    nvrhiDesc.errorCB = &mMessages;
    nvrhiDesc.pDevice = mDevice.Get();
    nvrhiDesc.pGraphicsCommandQueue = mGraphicsQueue.Get();
    nvrhiDesc.pComputeCommandQueue = mComputeQueue.Get();
    nvrhiDesc.pCopyCommandQueue = mCopyQueue.Get();
    mNvrhiDevice = nvrhi::d3d12::createDevice(nvrhiDesc);
    if (mNvrhiDevice == nullptr) {
        return core::make_error(rhi_error::DEVICE_CREATION_FAILED, "nvrhi::d3d12::createDevice returned null");
    }

    mCapabilities.maxTextureDimension2d = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    mCapabilities.computeQueue = mNvrhiDevice->queryFeatureSupport(nvrhi::Feature::ComputeQueue);
    mCapabilities.timerQueries = true;
    return core::result<void>::success();
}

nvrhi::IDevice *d3d12_device_backend::nvrhi_device() const noexcept {
    return mNvrhiDevice.Get();
}

const device_capabilities &d3d12_device_backend::capabilities() const noexcept {
    return mCapabilities;
}

core::result<std::unique_ptr<swapchain_backend>> d3d12_device_backend::create_swapchain(
  const native_window &window,
  const swapchain_desc &desc
) {
    std::unique_ptr<d3d12_swapchain> created = std::make_unique<d3d12_swapchain>(*this, desc);
    CONJURE_TRY(created->initialise(window));
    return std::unique_ptr<swapchain_backend>(std::move(created));
}

bool d3d12_device_backend::poll_device_removed() noexcept {
    if (mDevice == nullptr) {
        return false;
    }
    return mDevice->GetDeviceRemovedReason() != S_OK;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

core::result<void> d3d12_device_backend::select_adapter(const device_desc &desc) {
    if (desc.preferSoftwareAdapter) {
        const HRESULT warpResult = mFactory->EnumWarpAdapter(IID_PPV_ARGS(&mAdapter));
        if (FAILED(warpResult)) {
            return creation_failure("IDXGIFactory4::EnumWarpAdapter", warpResult);
        }
    } else {
        // Hardware adapters in GPU-preference order when the OS offers it (Windows 10 1803+),
        // enumeration order otherwise; software adapters are skipped so adapterIndex counts GPUs.
        ComPtr<IDXGIFactory6> factory6 = nullptr;
        const bool hasPreference = SUCCEEDED(mFactory.As(&factory6));
        u32 hardwareIndex = 0;
        for (UINT index = 0;; ++index) {
            ComPtr<IDXGIAdapter1> candidate = nullptr;
            HRESULT enumerated = E_FAIL;
            if (hasPreference) {
                enumerated = factory6->EnumAdapterByGpuPreference(
                  index,
                  DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                  IID_PPV_ARGS(&candidate)
                );
            } else {
                enumerated = mFactory->EnumAdapters1(index, &candidate);
            }
            if (enumerated == DXGI_ERROR_NOT_FOUND) {
                break;
            }
            if (FAILED(enumerated)) {
                return creation_failure("IDXGIFactory::EnumAdapters", enumerated);
            }
            DXGI_ADAPTER_DESC1 adapterDesc = {};
            candidate->GetDesc1(&adapterDesc);
            if ((adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
                continue;
            }
            if (FAILED(D3D12CreateDevice(candidate.Get(), MINIMUM_FEATURE_LEVEL, __uuidof(ID3D12Device), nullptr))) {
                continue;
            }
            if (hardwareIndex == desc.adapterIndex) {
                mAdapter = candidate;
                break;
            }
            ++hardwareIndex;
        }
        if (mAdapter == nullptr) {
            return core::make_error(
              rhi_error::DEVICE_CREATION_FAILED,
              "no D3D12-capable hardware adapter at index {} ({} found)",
              desc.adapterIndex,
              hardwareIndex
            );
        }
    }

    DXGI_ADAPTER_DESC1 adapterDesc = {};
    mAdapter->GetDesc1(&adapterDesc);
    mCapabilities.adapterName = core::str::from_wide(adapterDesc.Description);
    mCapabilities.dedicatedVideoMemoryBytes = adapterDesc.DedicatedVideoMemory;
    mCapabilities.softwareAdapter = (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
    return core::result<void>::success();
}

core::result<void> d3d12_device_backend::create_queues() {
    CONJURE_TRY_ASSIGN(
      mGraphicsQueue,
      create_queue(*mDevice.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT, L"conjure graphics queue")
    );
    CONJURE_TRY_ASSIGN(
      mComputeQueue,
      create_queue(*mDevice.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE, L"conjure compute queue")
    );
    CONJURE_TRY_ASSIGN(mCopyQueue, create_queue(*mDevice.Get(), D3D12_COMMAND_LIST_TYPE_COPY, L"conjure copy queue"));
    return core::result<void>::success();
}


/*===========================================================================*/
/*===========================================================================*/
// Free functions

core::result<std::unique_ptr<device_backend>> conjure::rhi::d3d12_backend::create_d3d12_backend(
  const device_desc &desc,
  message_callback &messages
) {
    std::unique_ptr<d3d12_device_backend> backend = std::make_unique<d3d12_device_backend>(messages);
    CONJURE_TRY(backend->initialise(desc));
    return std::unique_ptr<device_backend>(std::move(backend));
}

std::string conjure::rhi::d3d12_backend::describe_hresult(long result) {
    const HRESULT code = static_cast<HRESULT>(result);
    const char *name = "";
    switch (code) {
        case E_OUTOFMEMORY: {
            name = "E_OUTOFMEMORY";
            break;
        }
        case E_INVALIDARG: {
            name = "E_INVALIDARG";
            break;
        }
        case E_NOINTERFACE: {
            name = "E_NOINTERFACE";
            break;
        }
        case E_NOTIMPL: {
            name = "E_NOTIMPL";
            break;
        }
        case DXGI_ERROR_NOT_FOUND: {
            name = "DXGI_ERROR_NOT_FOUND";
            break;
        }
        case DXGI_ERROR_UNSUPPORTED: {
            name = "DXGI_ERROR_UNSUPPORTED";
            break;
        }
        case DXGI_ERROR_DEVICE_REMOVED: {
            name = "DXGI_ERROR_DEVICE_REMOVED";
            break;
        }
        case DXGI_ERROR_DEVICE_RESET: {
            name = "DXGI_ERROR_DEVICE_RESET";
            break;
        }
        case DXGI_ERROR_DEVICE_HUNG: {
            name = "DXGI_ERROR_DEVICE_HUNG";
            break;
        }
        case DXGI_ERROR_DRIVER_INTERNAL_ERROR: {
            name = "DXGI_ERROR_DRIVER_INTERNAL_ERROR";
            break;
        }
        case DXGI_ERROR_INVALID_CALL: {
            name = "DXGI_ERROR_INVALID_CALL";
            break;
        }
        case DXGI_ERROR_SDK_COMPONENT_MISSING: {
            name = "DXGI_ERROR_SDK_COMPONENT_MISSING";
            break;
        }
        default: {
            break;
        }
    }
    if (name[0] == '\0') {
        return std::format("0x{:08X}", static_cast<unsigned long>(code));
    }
    return std::format("0x{:08X} ({})", static_cast<unsigned long>(code), name);
}
