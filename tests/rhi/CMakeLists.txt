# rhi tests: one file per public header plus the topic files docs/design/02_renderer.md 10 names
# (test_null_device, test_device_lost, test_null_swapchain, test_shader_library). Everything runs on
# the null device; the [gpu] cases create a WARP D3D12 device and skip (exit 77) when
# precondition::SOFTWARE_GPU_ADAPTER does not hold. The shader library tests read the index.json
# conjure_shaders produces, so the test executable depends on it and knows where it lands.

if(NOT TARGET conjure::rhi)
    return()
endif()

conjure_add_test_module(rhi
    SOURCES
        rhi_test_support.cpp
        rhi_test_support.h
        test_backend.cpp
        test_device.cpp
        test_device_lost.cpp
        test_log_category.cpp
        test_native_window.cpp
        test_null_device.cpp
        test_null_device_stats.cpp
        test_null_swapchain.cpp
        test_rhi_error.cpp
        test_shader_library.cpp
        test_swapchain.cpp
        test_texture_format.cpp
    DEPS
        conjure::thirdparty::nvrhi
)

add_dependencies(conjure_rhi_tests conjure_shaders)
get_target_property(shaderOutputDir conjure_shaders CONJURE_SHADER_OUTPUT_DIR)
target_compile_definitions(conjure_rhi_tests PRIVATE
    CONJURE_TEST_SHADER_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${shaderOutputDir}>"
)

conjure_header_check(conjure_rhi)
