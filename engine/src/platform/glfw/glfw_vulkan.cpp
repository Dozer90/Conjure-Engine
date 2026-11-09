#include "platform/glfw/glfw_native.h"
#include "conjure/platform/platform_error.h"

#include "conjure/core/types.h"

// glfw3.h declares glfwCreateWindowSurface only when the Vulkan core header has been seen
// (VK_VERSION_1_0). The engine does not depend on a Vulkan SDK at this layer: the rhi receives
// opaque pointers (window::create_vulkan_surface). The handle types below are the ABI-identical
// shapes vulkan_core.h uses on 64-bit targets (dispatchable and non-dispatchable handles are both
// pointers to incomplete structs, VkResult is an int-sized enumeration, the loader entry point is a
// plain function pointer) and exist only in this translation unit.
//
// Two caveats, both to be retired by including <vulkan/vulkan_core.h> (VK_NO_PROTOTYPES) from a
// pinned header-only Vulkan-Headers dependency once third_party/ carries one:
// - A program that also compiles the real header (the Vulkan rhi backend, NVRHI) then holds two
//   definitions of ::VkResult, one per translation unit: an ODR violation in the letter of the
//   standard, harmless in practice because both are int-sized and only VK_SUCCESS is compared.
// - Non-dispatchable handles such as VkSurfaceKHR are uint64_t, not pointers, on 32-bit targets,
//   so the pointer-shaped typedef below is only correct on 64-bit builds (asserted).

static_assert(sizeof(void *) == 8, "glfw_vulkan.cpp assumes 64-bit Vulkan handles (VkSurfaceKHR is a pointer)");

#define VK_VERSION_1_0 1
typedef struct VkInstance_T *VkInstance;
typedef struct VkPhysicalDevice_T *VkPhysicalDevice;
typedef struct VkSurfaceKHR_T *VkSurfaceKHR;
struct VkAllocationCallbacks;
enum class VkResult : int {
    VK_SUCCESS = 0
};
typedef void (*PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction (*PFN_vkGetInstanceProcAddr)(VkInstance instance, const char *name);

#include <GLFW/glfw3.h>

using namespace conjure;
using namespace conjure::platform;

core::result<void> conjure::platform::glfw::create_surface(GLFWwindow *window, void *instance, void **surface) {
    if (surface == nullptr) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "create_vulkan_surface: surface is null");
    }
    *surface = nullptr;
    if (instance == nullptr) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "create_vulkan_surface: instance is null");
    }
    if (glfwVulkanSupported() != GLFW_TRUE) {
        return core::make_error(platform_error::VULKAN_UNSUPPORTED, "GLFW found no Vulkan loader or ICD");
    }

    VkSurfaceKHR created = nullptr;
    const VkResult result = glfwCreateWindowSurface(static_cast<VkInstance>(instance), window, nullptr, &created);
    if (result != VkResult::VK_SUCCESS) {
        return core::make_error(
          platform_error::SURFACE_CREATE_FAILED,
          "glfwCreateWindowSurface failed with VkResult {}",
          static_cast<int>(result)
        );
    }

    *surface = static_cast<void *>(created);
    return core::result<void>::success();
}
