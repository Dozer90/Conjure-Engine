#include "rhi/backends/vulkan/vulkan_loader.h"

#include "conjure/rhi/log_category.h"
#include "conjure/rhi/rhi_error.h"

#include "conjure/core/error.h"
#include "conjure/core/log.h"
#include "conjure/core/platform_detect.h"

#include <array>
#include <string>
#include <string_view>

using namespace conjure;
using namespace conjure::rhi;
using namespace conjure::rhi::vulkan_backend;

namespace {
    // Backend implementation file: the loader library name is the one platform fact this module
    // cannot avoid (CONVENTIONS.md 3).
#if CONJURE_PLATFORM_WINDOWS
    constexpr std::array<std::string_view, 1> LIBRARY_NAMES = {"vulkan-1.dll"};
#elif CONJURE_PLATFORM_MACOS
    constexpr std::array<std::string_view, 3> LIBRARY_NAMES = {
        "libvulkan.1.dylib",
        "libvulkan.dylib",
        "libMoltenVK.dylib",
    };
#else
    constexpr std::array<std::string_view, 2> LIBRARY_NAMES = {"libvulkan.so.1", "libvulkan.so"};
#endif
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

vulkan_loader::vulkan_loader() {}

vulkan_loader::~vulkan_loader() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<void> vulkan_loader::load() {
    if (is_loaded()) {
        return core::result<void>::success();
    }

    std::string tried = {};
    for (const std::string_view name : LIBRARY_NAMES) {
        core::result<core::dynamic_library> loaded = core::dynamic_library::load(std::filesystem::path(name));
        if (!loaded.has_value()) {
            if (!tried.empty()) {
                tried.append(", ");
            }
            tried.append(name);
            continue;
        }
        mLibrary = std::move(loaded).value();
        mGetInstanceProcAddr = mLibrary.symbol_as<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        if (mGetInstanceProcAddr == nullptr) {
            CONJURE_LOG_WARN(LOG_RHI, "vulkan: '{}' has no vkGetInstanceProcAddr", name);
            mLibrary.unload();
            continue;
        }
        dispatch().init(mGetInstanceProcAddr);
        CONJURE_LOG_DEBUG(LOG_RHI, "vulkan: loaded '{}'", name);
        return core::result<void>::success();
    }
    return core::make_error(
      rhi_error::DEVICE_CREATION_FAILED,
      "the Vulkan loader is not installed (tried {})",
      tried.empty() ? std::string("no library") : tried
    );
}

void vulkan_loader::init_instance(VkInstance instance) {
    dispatch().init(vk::Instance(instance));
}

void vulkan_loader::init_device(VkDevice device) {
    dispatch().init(vk::Device(device));
}

PFN_vkVoidFunction vulkan_loader::instance_function(VkInstance instance, const char *name) const noexcept {
    if (mGetInstanceProcAddr == nullptr || name == nullptr) {
        return nullptr;
    }
    return mGetInstanceProcAddr(instance, name);
}
