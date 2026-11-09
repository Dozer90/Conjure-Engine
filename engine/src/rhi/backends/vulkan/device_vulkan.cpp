#include "rhi/backends/vulkan/device_vulkan.h"

#include "rhi/backends/vulkan/swapchain_vulkan.h"
#include "rhi/message_callback.h"

#include "conjure/rhi/log_category.h"
#include "conjure/rhi/rhi_error.h"

#include "conjure/core/error.h"
#include "conjure/core/log.h"
#include "conjure/core/platform_detect.h"

#include <algorithm>
#include <cstring>
#include <string_view>
#include <utility>

using namespace conjure;
using namespace conjure::rhi;
using namespace conjure::rhi::vulkan_backend;

namespace {
    constexpr u32 REQUIRED_API_VERSION = VK_API_VERSION_1_2;
    constexpr const char *VALIDATION_LAYER = "VK_LAYER_KHRONOS_validation";
    constexpr const char *PORTABILITY_ENUMERATION_EXTENSION = "VK_KHR_portability_enumeration";
    constexpr const char *PORTABILITY_SUBSET_EXTENSION = "VK_KHR_portability_subset";
    // VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR, spelled out because the portability
    // enumeration header is not part of the core Vulkan headers.
    constexpr VkInstanceCreateFlags PORTABILITY_ENUMERATION_FLAG = 0x00000001;

    core::error creation_failure(const char *what, VkResult result) {
        return core::make_error(
          rhi_error::DEVICE_CREATION_FAILED,
          "{} failed with {}",
          what,
          describe_vk_result(result)
        );
    }

    bool has_extension(const std::vector<VkExtensionProperties> &available, std::string_view name) noexcept {
        for (const VkExtensionProperties &extension : available) {
            if (name == extension.extensionName) {
                return true;
            }
        }
        return false;
    }

    bool has_layer(const std::vector<VkLayerProperties> &available, std::string_view name) noexcept {
        for (const VkLayerProperties &layer : available) {
            if (name == layer.layerName) {
                return true;
            }
        }
        return false;
    }

    /// <summary>
    /// Ranks device types: the preferred kind first, CPU devices last (they are only chosen when
    /// preferSoftwareAdapter is set).
    /// </summary>
    int device_type_rank(VkPhysicalDeviceType type, bool preferSoftware) noexcept {
        if (preferSoftware) {
            return type == VK_PHYSICAL_DEVICE_TYPE_CPU ? 0 : 1;
        }
        switch (type) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: {
                return 0;
            }
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: {
                return 1;
            }
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: {
                return 2;
            }
            case VK_PHYSICAL_DEVICE_TYPE_OTHER: {
                return 3;
            }
            default: {
                return 4;
            }
        }
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(
      VkDebugUtilsMessageSeverityFlagBitsEXT severity,
      VkDebugUtilsMessageTypeFlagsEXT,
      const VkDebugUtilsMessengerCallbackDataEXT *data,
      void *userData
    ) {
        message_callback *messages = static_cast<message_callback *>(userData);
        if (messages == nullptr || data == nullptr) {
            return VK_FALSE;
        }
        const char *text = data->pMessage != nullptr ? data->pMessage : "";
        if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) {
            messages->message(nvrhi::MessageSeverity::Error, text);
        } else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0) {
            messages->message(nvrhi::MessageSeverity::Warning, text);
        } else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0) {
            messages->message(nvrhi::MessageSeverity::Info, text);
        }
        return VK_FALSE;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

vulkan_device_backend::vulkan_device_backend(message_callback &messages) :
    mMessages(messages) {
    mCapabilities.backend = graphics_backend::VULKAN;
}

vulkan_device_backend::~vulkan_device_backend() {
    // The same test device::is_lost() applies: the reported flag plus the backend poll (which Vulkan
    // cannot answer, see poll_device_removed), so both teardown paths agree.
    if (mNvrhiDevice != nullptr && !mMessages.is_lost() && !poll_device_removed()) {
        mNvrhiDevice->waitForIdle();
        mNvrhiDevice->runGarbageCollection();
    }
    mNvrhiDevice = nullptr;
    if (mDevice != VK_NULL_HANDLE) {
        dispatch().vkDestroyDevice(mDevice, nullptr);
        mDevice = VK_NULL_HANDLE;
    }
    if (mMessenger != VK_NULL_HANDLE && dispatch().vkDestroyDebugUtilsMessengerEXT != nullptr) {
        dispatch().vkDestroyDebugUtilsMessengerEXT(mInstance, mMessenger, nullptr);
        mMessenger = VK_NULL_HANDLE;
    }
    if (mInstance != VK_NULL_HANDLE) {
        dispatch().vkDestroyInstance(mInstance, nullptr);
        mInstance = VK_NULL_HANDLE;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<void> vulkan_device_backend::initialise(const device_desc &desc) {
    CONJURE_TRY(mLoader.load());
    CONJURE_TRY(create_instance(desc));
    CONJURE_TRY(select_physical_device(desc));
    CONJURE_TRY(create_device(desc));

    nvrhi::vulkan::DeviceDesc nvrhiDesc = {};
    nvrhiDesc.errorCB = &mMessages;
    nvrhiDesc.instance = mInstance;
    nvrhiDesc.physicalDevice = mPhysicalDevice;
    nvrhiDesc.device = mDevice;
    nvrhiDesc.graphicsQueue = mGraphicsQueue;
    nvrhiDesc.graphicsQueueIndex = static_cast<int>(mGraphicsFamily);
    nvrhiDesc.computeQueue = mComputeQueue;
    nvrhiDesc.computeQueueIndex = mComputeFamily == NO_QUEUE_FAMILY ? -1 : static_cast<int>(mComputeFamily);
    nvrhiDesc.transferQueue = mTransferQueue;
    nvrhiDesc.transferQueueIndex = mTransferFamily == NO_QUEUE_FAMILY ? -1 : static_cast<int>(mTransferFamily);
    nvrhiDesc.instanceExtensions = mInstanceExtensionPointers.data();
    nvrhiDesc.numInstanceExtensions = mInstanceExtensionPointers.size();
    nvrhiDesc.deviceExtensions = mDeviceExtensionPointers.data();
    nvrhiDesc.numDeviceExtensions = mDeviceExtensionPointers.size();
    nvrhiDesc.bufferDeviceAddressSupported = mBufferDeviceAddress;
    mNvrhiDevice = nvrhi::vulkan::createDevice(nvrhiDesc);
    if (mNvrhiDevice == nullptr) {
        return core::make_error(rhi_error::DEVICE_CREATION_FAILED, "nvrhi::vulkan::createDevice returned null");
    }

    fill_capabilities();
    return core::result<void>::success();
}

nvrhi::IDevice *vulkan_device_backend::nvrhi_device() const noexcept {
    return mNvrhiDevice.Get();
}

const device_capabilities &vulkan_device_backend::capabilities() const noexcept {
    return mCapabilities;
}

core::result<std::unique_ptr<swapchain_backend>> vulkan_device_backend::create_swapchain(
  const native_window &window,
  const swapchain_desc &desc
) {
    std::unique_ptr<vulkan_swapchain> created = std::make_unique<vulkan_swapchain>(*this, desc);
    CONJURE_TRY(created->initialise(window));
    return std::unique_ptr<swapchain_backend>(std::move(created));
}

bool vulkan_device_backend::poll_device_removed() noexcept {
    // Vulkan has no device-removed query; loss is observed by the queue operations (present,
    // waitForIdle), which mark the device through message_callback.
    return false;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

core::result<void> vulkan_device_backend::create_instance(const device_desc &desc) {
    vk::detail::DispatchLoaderDynamic &vulkan = dispatch();
    if (vulkan.vkEnumerateInstanceVersion == nullptr) {
        return core::make_error(
          rhi_error::DEVICE_CREATION_FAILED,
          "the Vulkan loader predates Vulkan 1.1; 1.2 is required"
        );
    }
    u32 loaderVersion = 0;
    const VkResult versionResult = vulkan.vkEnumerateInstanceVersion(&loaderVersion);
    if (versionResult != VK_SUCCESS) {
        return creation_failure("vkEnumerateInstanceVersion", versionResult);
    }
    if (loaderVersion < REQUIRED_API_VERSION) {
        return core::make_error(
          rhi_error::DEVICE_CREATION_FAILED,
          "the Vulkan loader supports {}.{} but 1.2 is required",
          VK_API_VERSION_MAJOR(loaderVersion),
          VK_API_VERSION_MINOR(loaderVersion)
        );
    }

    u32 extensionCount = 0;
    VkResult enumerated = vulkan.vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    if (enumerated != VK_SUCCESS) {
        return creation_failure("vkEnumerateInstanceExtensionProperties", enumerated);
    }
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    enumerated = vulkan.vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());
    if (enumerated != VK_SUCCESS) {
        return creation_failure("vkEnumerateInstanceExtensionProperties", enumerated);
    }
    availableExtensions.resize(extensionCount);

    u32 layerCount = 0;
    std::vector<VkLayerProperties> availableLayers = {};
    if (vulkan.vkEnumerateInstanceLayerProperties(&layerCount, nullptr) == VK_SUCCESS) {
        availableLayers.resize(layerCount);
        if (vulkan.vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()) != VK_SUCCESS) {
            availableLayers.clear();
        }
        availableLayers.resize(std::min<usize>(layerCount, availableLayers.size()));
    }

    mInstanceExtensions.clear();
    const char *surfaceExtension = surface_extension_for(desc.window.kind);
    if (surfaceExtension[0] != '\0') {
        if (!has_extension(availableExtensions, VK_KHR_SURFACE_EXTENSION_NAME) ||
            !has_extension(availableExtensions, surfaceExtension)) {
            return core::make_error(
              rhi_error::DEVICE_CREATION_FAILED,
              "the Vulkan instance lacks {} / {} needed to present to a {} window",
              VK_KHR_SURFACE_EXTENSION_NAME,
              surfaceExtension,
              to_string(desc.window.kind)
            );
        }
        mInstanceExtensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
        mInstanceExtensions.emplace_back(surfaceExtension);
    }
    VkInstanceCreateFlags createFlags = 0;
    if (has_extension(availableExtensions, PORTABILITY_ENUMERATION_EXTENSION)) {
        // MoltenVK devices are only enumerated with the portability flag; harmless elsewhere.
        mInstanceExtensions.emplace_back(PORTABILITY_ENUMERATION_EXTENSION);
        createFlags |= PORTABILITY_ENUMERATION_FLAG;
    }
    std::vector<const char *> layers = {};
    if (desc.enableDebugLayer) {
        if (has_extension(availableExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            mInstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        } else {
            CONJURE_LOG_WARN(
              LOG_RHI,
              "vulkan: {} is unavailable; API messages will not be routed",
              VK_EXT_DEBUG_UTILS_EXTENSION_NAME
            );
        }
        if (has_layer(availableLayers, VALIDATION_LAYER)) {
            layers.push_back(VALIDATION_LAYER);
        } else {
            CONJURE_LOG_WARN(
              LOG_RHI,
              "vulkan: {} is not installed; install the Vulkan SDK for validation",
              VALIDATION_LAYER
            );
        }
    }
    mInstanceExtensionPointers.clear();
    for (const std::string &extension : mInstanceExtensions) {
        mInstanceExtensionPointers.push_back(extension.c_str());
    }

    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "Conjure";
    applicationInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    applicationInfo.pEngineName = "Conjure";
    applicationInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    applicationInfo.apiVersion = REQUIRED_API_VERSION;

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.flags = createFlags;
    instanceInfo.pApplicationInfo = &applicationInfo;
    instanceInfo.enabledLayerCount = static_cast<u32>(layers.size());
    instanceInfo.ppEnabledLayerNames = layers.data();
    instanceInfo.enabledExtensionCount = static_cast<u32>(mInstanceExtensionPointers.size());
    instanceInfo.ppEnabledExtensionNames = mInstanceExtensionPointers.data();

    const VkResult instanceResult = vulkan.vkCreateInstance(&instanceInfo, nullptr, &mInstance);
    if (instanceResult != VK_SUCCESS) {
        mInstance = VK_NULL_HANDLE;
        return creation_failure("vkCreateInstance", instanceResult);
    }
    mLoader.init_instance(mInstance);

    if (desc.enableDebugLayer && vulkan.vkCreateDebugUtilsMessengerEXT != nullptr) {
        VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {};
        messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        messengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        messengerInfo.pfnUserCallback = &debug_messenger_callback;
        messengerInfo.pUserData = &mMessages;
        const VkResult messengerResult = vulkan.vkCreateDebugUtilsMessengerEXT(
          mInstance,
          &messengerInfo,
          nullptr,
          &mMessenger
        );
        if (messengerResult != VK_SUCCESS) {
            mMessenger = VK_NULL_HANDLE;
            CONJURE_LOG_WARN(
              LOG_RHI,
              "vulkan: vkCreateDebugUtilsMessengerEXT failed with {}",
              describe_vk_result(messengerResult)
            );
        }
    }
    return core::result<void>::success();
}

core::result<void> vulkan_device_backend::select_physical_device(const device_desc &desc) {
    vk::detail::DispatchLoaderDynamic &vulkan = dispatch();
    u32 deviceCount = 0;
    VkResult enumerated = vulkan.vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
    if (enumerated != VK_SUCCESS) {
        return creation_failure("vkEnumeratePhysicalDevices", enumerated);
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    enumerated = vulkan.vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());
    if (enumerated != VK_SUCCESS) {
        return creation_failure("vkEnumeratePhysicalDevices", enumerated);
    }
    devices.resize(deviceCount);
    if (devices.empty()) {
        return core::make_error(rhi_error::DEVICE_CREATION_FAILED, "no Vulkan physical device is present");
    }

    struct candidate {
        VkPhysicalDevice device = VK_NULL_HANDLE;
        int rank = 0;
        u32 graphicsFamily = NO_QUEUE_FAMILY;
    };
    std::vector<candidate> candidates = {};
    for (VkPhysicalDevice physical : devices) {
        VkPhysicalDeviceProperties properties = {};
        vulkan.vkGetPhysicalDeviceProperties(physical, &properties);
        if (properties.apiVersion < REQUIRED_API_VERSION) {
            continue;
        }
        const bool isSoftware = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
        if (isSoftware != desc.preferSoftwareAdapter) {
            continue;
        }

        u32 familyCount = 0;
        vulkan.vkGetPhysicalDeviceQueueFamilyProperties(physical, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        vulkan.vkGetPhysicalDeviceQueueFamilyProperties(physical, &familyCount, families.data());
        u32 graphicsFamily = NO_QUEUE_FAMILY;
        for (u32 index = 0; index < familyCount; ++index) {
            const VkQueueFlags required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
            if ((families[index].queueFlags & required) == required && families[index].queueCount > 0) {
                graphicsFamily = index;
                break;
            }
        }
        if (graphicsFamily == NO_QUEUE_FAMILY) {
            continue;
        }

        VkPhysicalDeviceVulkan12Features features12 = {};
        features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        VkPhysicalDeviceFeatures2 features2 = {};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &features12;
        vulkan.vkGetPhysicalDeviceFeatures2(physical, &features2);
        if (features12.timelineSemaphore == VK_FALSE) {
            continue;   // NVRHI's queues are timeline semaphores
        }

        candidates.push_back(
          candidate{physical, device_type_rank(properties.deviceType, desc.preferSoftwareAdapter), graphicsFamily}
        );
    }
    std::stable_sort(candidates.begin(), candidates.end(), [](const candidate &left, const candidate &right) {
        return left.rank < right.rank;
    });

    if (desc.adapterIndex >= candidates.size()) {
        return core::make_error(
          rhi_error::DEVICE_CREATION_FAILED,
          "no usable Vulkan {} device at index {} ({} found)",
          desc.preferSoftwareAdapter ? "software" : "hardware",
          desc.adapterIndex,
          candidates.size()
        );
    }
    mPhysicalDevice = candidates[desc.adapterIndex].device;
    mGraphicsFamily = candidates[desc.adapterIndex].graphicsFamily;

    // Dedicated compute and transfer families when the device has them; NVRHI accepts null queues.
    u32 familyCount = 0;
    vulkan.vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vulkan.vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &familyCount, families.data());
    for (u32 index = 0; index < familyCount; ++index) {
        if (index == mGraphicsFamily || families[index].queueCount == 0) {
            continue;
        }
        const VkQueueFlags flags = families[index].queueFlags;
        const bool computeOnly = (flags & VK_QUEUE_COMPUTE_BIT) != 0 && (flags & VK_QUEUE_GRAPHICS_BIT) == 0;
        if (mComputeFamily == NO_QUEUE_FAMILY && computeOnly) {
            mComputeFamily = index;
            continue;
        }
        if (mTransferFamily == NO_QUEUE_FAMILY && (flags & VK_QUEUE_TRANSFER_BIT) != 0 &&
            (flags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == 0) {
            mTransferFamily = index;
        }
    }
    return core::result<void>::success();
}

core::result<void> vulkan_device_backend::create_device(const device_desc &desc) {
    vk::detail::DispatchLoaderDynamic &vulkan = dispatch();

    u32 extensionCount = 0;
    VkResult enumerated = vulkan.vkEnumerateDeviceExtensionProperties(
      mPhysicalDevice,
      nullptr,
      &extensionCount,
      nullptr
    );
    if (enumerated != VK_SUCCESS) {
        return creation_failure("vkEnumerateDeviceExtensionProperties", enumerated);
    }
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    enumerated = vulkan.vkEnumerateDeviceExtensionProperties(
      mPhysicalDevice,
      nullptr,
      &extensionCount,
      availableExtensions.data()
    );
    if (enumerated != VK_SUCCESS) {
        return creation_failure("vkEnumerateDeviceExtensionProperties", enumerated);
    }
    availableExtensions.resize(extensionCount);

    mDeviceExtensions.clear();
    mSwapchainSupported = has_extension(availableExtensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if (mSwapchainSupported) {
        mDeviceExtensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    } else if (!desc.window.is_headless()) {
        return core::make_error(
          rhi_error::DEVICE_CREATION_FAILED,
          "the Vulkan device lacks {}",
          VK_KHR_SWAPCHAIN_EXTENSION_NAME
        );
    }
    if (has_extension(availableExtensions, PORTABILITY_SUBSET_EXTENSION)) {
        mDeviceExtensions.emplace_back(PORTABILITY_SUBSET_EXTENSION);   // mandatory on MoltenVK when present
    }
    if (desc.enableDebugLayer && has_extension(availableExtensions, VK_EXT_DEBUG_MARKER_EXTENSION_NAME)) {
        mDeviceExtensions.emplace_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
    }
    mDeviceExtensionPointers.clear();
    for (const std::string &extension : mDeviceExtensions) {
        mDeviceExtensionPointers.push_back(extension.c_str());
    }

    VkPhysicalDeviceVulkan12Features supported12 = {};
    supported12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    VkPhysicalDeviceFeatures2 supported2 = {};
    supported2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    supported2.pNext = &supported12;
    vulkan.vkGetPhysicalDeviceFeatures2(mPhysicalDevice, &supported2);

    VkPhysicalDeviceVulkan12Features enabled12 = {};
    enabled12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    enabled12.timelineSemaphore = VK_TRUE;
    enabled12.bufferDeviceAddress = supported12.bufferDeviceAddress;
    enabled12.descriptorIndexing = supported12.descriptorIndexing;
    enabled12.runtimeDescriptorArray = supported12.runtimeDescriptorArray;
    enabled12.shaderSampledImageArrayNonUniformIndexing = supported12.shaderSampledImageArrayNonUniformIndexing;
    enabled12.descriptorBindingPartiallyBound = supported12.descriptorBindingPartiallyBound;
    enabled12.descriptorBindingVariableDescriptorCount = supported12.descriptorBindingVariableDescriptorCount;
    mBufferDeviceAddress = supported12.bufferDeviceAddress == VK_TRUE;

    VkPhysicalDeviceFeatures2 enabled2 = {};
    enabled2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    enabled2.pNext = &enabled12;
    enabled2.features.samplerAnisotropy = supported2.features.samplerAnisotropy;
    enabled2.features.fillModeNonSolid = supported2.features.fillModeNonSolid;
    enabled2.features.independentBlend = supported2.features.independentBlend;
    enabled2.features.textureCompressionBC = supported2.features.textureCompressionBC;
    enabled2.features.depthClamp = supported2.features.depthClamp;
    enabled2.features.fragmentStoresAndAtomics = supported2.features.fragmentStoresAndAtomics;
    enabled2.features.shaderInt64 = supported2.features.shaderInt64;

    const float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos = {};
    for (const u32 family : {mGraphicsFamily, mComputeFamily, mTransferFamily}) {
        if (family == NO_QUEUE_FAMILY) {
            continue;
        }
        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;
        queueInfos.push_back(queueInfo);
    }

    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = &enabled2;
    deviceInfo.queueCreateInfoCount = static_cast<u32>(queueInfos.size());
    deviceInfo.pQueueCreateInfos = queueInfos.data();
    deviceInfo.enabledExtensionCount = static_cast<u32>(mDeviceExtensionPointers.size());
    deviceInfo.ppEnabledExtensionNames = mDeviceExtensionPointers.data();

    const VkResult deviceResult = vulkan.vkCreateDevice(mPhysicalDevice, &deviceInfo, nullptr, &mDevice);
    if (deviceResult != VK_SUCCESS) {
        mDevice = VK_NULL_HANDLE;
        return creation_failure("vkCreateDevice", deviceResult);
    }
    mLoader.init_device(mDevice);

    vulkan.vkGetDeviceQueue(mDevice, mGraphicsFamily, 0, &mGraphicsQueue);
    if (mComputeFamily != NO_QUEUE_FAMILY) {
        vulkan.vkGetDeviceQueue(mDevice, mComputeFamily, 0, &mComputeQueue);
    }
    if (mTransferFamily != NO_QUEUE_FAMILY) {
        vulkan.vkGetDeviceQueue(mDevice, mTransferFamily, 0, &mTransferQueue);
    }
    return core::result<void>::success();
}

void vulkan_device_backend::fill_capabilities() {
    vk::detail::DispatchLoaderDynamic &vulkan = dispatch();
    VkPhysicalDeviceProperties properties = {};
    vulkan.vkGetPhysicalDeviceProperties(mPhysicalDevice, &properties);
    VkPhysicalDeviceMemoryProperties memory = {};
    vulkan.vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memory);

    u64 deviceLocalBytes = 0;
    for (u32 index = 0; index < memory.memoryHeapCount; ++index) {
        if ((memory.memoryHeaps[index].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
            deviceLocalBytes += memory.memoryHeaps[index].size;
        }
    }

    mCapabilities.backend = graphics_backend::VULKAN;
    mCapabilities.adapterName = properties.deviceName;
    mCapabilities.maxTextureDimension2d = properties.limits.maxImageDimension2D;
    mCapabilities.dedicatedVideoMemoryBytes = deviceLocalBytes;
    mCapabilities.computeQueue = mComputeQueue != VK_NULL_HANDLE;
    mCapabilities.timerQueries = properties.limits.timestampComputeAndGraphics == VK_TRUE;
    mCapabilities.softwareAdapter = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
}


/*===========================================================================*/
/*===========================================================================*/
// Free functions

const char *conjure::rhi::vulkan_backend::surface_extension_for(native_window_kind kind) noexcept {
    switch (kind) {
        case native_window_kind::WINDOWS: {
            return "VK_KHR_win32_surface";
        }
        case native_window_kind::COCOA: {
            return "VK_EXT_metal_surface";
        }
        case native_window_kind::X11: {
            return "VK_KHR_xlib_surface";
        }
        case native_window_kind::WAYLAND: {
            return "VK_KHR_wayland_surface";
        }
        case native_window_kind::NONE: {
            break;
        }
    }
    return "";
}

core::result<std::unique_ptr<device_backend>> conjure::rhi::vulkan_backend::create_vulkan_backend(
  const device_desc &desc,
  message_callback &messages
) {
    std::unique_ptr<vulkan_device_backend> backend = std::make_unique<vulkan_device_backend>(messages);
    CONJURE_TRY(backend->initialise(desc));
    return std::unique_ptr<device_backend>(std::move(backend));
}
