# conjure_rhi - NVRHI device, swapchain and shader library over the null, D3D12 and Vulkan
# backends (docs/design/02_renderer.md 1, 4.1, 7). NVRHI is a private dependency: public headers
# forward-declare nvrhi::IDevice / nvrhi::ITexture only (R15).
#
# The null backend is always compiled. D3D12 and Vulkan backend sources are listed only when their
# CONJURE_RHI_* option is on, so a backend that is off never reaches the compiler; device.cpp asks
# is_backend_compiled() and reports UNSUPPORTED instead. The shader build (engine/shaders,
# cmake/ConjureShaders.cmake) is added by engine/CMakeLists.txt; tests/rhi reads its index.json.

if(NOT CONJURE_WITH_RENDERER)
    return()
endif()

set(CONJURE_RHI_SOURCES
    backend.cpp
    device.cpp
    device_backend.cpp
    device_backend.h
    device_impl.h
    log_category.cpp
    message_callback.cpp
    message_callback.h
    native_window.cpp
    null_device_stats.cpp
    rhi_error.cpp
    shader_library.cpp
    swapchain.cpp
    texture_format.cpp
    texture_format_mapping.h
    backends/null/null_command_list.cpp
    backends/null/null_command_list.h
    backends/null/null_device.cpp
    backends/null/null_device.h
    backends/null/null_resources.cpp
    backends/null/null_resources.h
    backends/null/null_swapchain.cpp
    backends/null/null_swapchain.h
)

if(CONJURE_RHI_D3D12)
    list(APPEND CONJURE_RHI_SOURCES
        backends/d3d12/device_d3d12.cpp
        backends/d3d12/device_d3d12.h
        backends/d3d12/swapchain_d3d12.cpp
        backends/d3d12/swapchain_d3d12.h
    )
endif()

set(CONJURE_RHI_PRIVATE_DEPS conjure::thirdparty::nvrhi)
set(CONJURE_RHI_PRIVATE_DEFINES "")
if(CONJURE_RHI_VULKAN)
    list(APPEND CONJURE_RHI_SOURCES
        backends/vulkan/device_vulkan.cpp
        backends/vulkan/device_vulkan.h
        backends/vulkan/swapchain_vulkan.cpp
        backends/vulkan/swapchain_vulkan.h
        backends/vulkan/vulkan_common.h
        backends/vulkan/vulkan_loader.cpp
        backends/vulkan/vulkan_loader.h
    )
    # NVRHI links the Vulkan headers privately; the backend needs them too. Every entry point is
    # loaded at runtime through vulkan.hpp's dynamic dispatcher, so the C prototypes stay out and
    # nothing links against the Vulkan loader library.
    if(TARGET Vulkan::Headers)
        list(APPEND CONJURE_RHI_PRIVATE_DEPS Vulkan::Headers)
    elseif(TARGET Vulkan-Headers)
        list(APPEND CONJURE_RHI_PRIVATE_DEPS Vulkan-Headers)
    endif()
    list(APPEND CONJURE_RHI_PRIVATE_DEFINES VK_NO_PROTOTYPES)
endif()

conjure_add_module(rhi
    PUBLIC_HEADERS
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/rhi/backend.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/rhi/device.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/rhi/log_category.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/rhi/native_window.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/rhi/null_device_stats.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/rhi/rhi_error.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/rhi/shader_library.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/rhi/swapchain.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/rhi/texture_format.h
    SOURCES
        ${CONJURE_RHI_SOURCES}
    PUBLIC_DEPS
        conjure::core
    PRIVATE_DEPS
        ${CONJURE_RHI_PRIVATE_DEPS}
    PRIVATE_DEFINES
        ${CONJURE_RHI_PRIVATE_DEFINES}
)
