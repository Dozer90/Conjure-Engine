# renderer tests: one file per public header of conjure::renderer plus the topic files
# docs/design/02_renderer.md 10 names (culling, draw lists, cascades, the atlas allocator, the render
# graph, the transient pool, the GPU type mirror, the tonemap reference, exposure maths, the
# pipeline and resource caches, the warning latches, the null-device frame and the failure paths).
# Everything runs on the null device; the [gpu] cases create a WARP D3D12 device and skip (exit 77)
# when precondition::SOFTWARE_GPU_ADAPTER does not hold. The shader index conjure_shaders produces is
# read through CONJURE_TEST_SHADER_DIR; the HLSL sources (gpu_types.hlsli, tonemap.hlsli) through
# CONJURE_TEST_SHADER_SOURCE_DIR and CONJURE_TEST_TONEMAP_HLSLI. Private renderer headers
# (engine/src/renderer) are reachable because the topic tests exercise the internals directly.

if(NOT TARGET conjure::renderer)
    return()
endif()

set(CONJURE_RENDERER_TEST_SOURCES
    gltf_test_builder.cpp
    gltf_test_builder.h
    hlsl_shim.h
    renderer_test_support.cpp
    renderer_test_support.h
    test_camera.cpp
    test_cascade_builder.cpp
    test_components.cpp
    test_culling.cpp
    test_debug_draw.cpp
    test_draw_list_builder.cpp
    test_environment_asset.cpp
    test_exposure_math.cpp
    test_gltf_importer.cpp
    test_gpu_ids.cpp
    test_gpu_resource_cache.cpp
    test_gpu_types.cpp
    test_imgui_renderer.cpp
    test_jpeg_xl.cpp
    test_light.cpp
    test_limits.cpp
    test_log_category.cpp
    test_material_asset.cpp
    test_mesh_asset.cpp
    test_model_asset.cpp
    test_pipeline_cache.cpp
    test_render_graph.cpp
    test_render_scene.cpp
    test_render_settings.cpp
    test_render_target.cpp
    test_renderer.cpp
    test_renderer_error.cpp
    test_renderer_failures.cpp
    test_renderer_gpu.cpp
    test_renderer_null_frame.cpp
    test_shader_asset.cpp
    test_shadow_atlas_allocator.cpp
    test_sprite_batch.cpp
    test_texture_asset.cpp
    test_tonemap_reference.cpp
    test_transient_pool.cpp
    test_warn_latch.cpp
)

set(CONJURE_RENDERER_TEST_DEPS
    conjure::thirdparty::nvrhi
)
if(CONJURE_BUILD_EDITOR AND TARGET conjure::thirdparty::imgui)
    list(APPEND CONJURE_RENDERER_TEST_DEPS conjure::thirdparty::imgui)
endif()

conjure_add_test_module(renderer
    SOURCES
        ${CONJURE_RENDERER_TEST_SOURCES}
    DEPS
        ${CONJURE_RENDERER_TEST_DEPS}
    TIMEOUT 300
)

add_dependencies(conjure_renderer_tests conjure_shaders)
get_target_property(shaderOutputDir conjure_shaders CONJURE_SHADER_OUTPUT_DIR)
target_compile_definitions(conjure_renderer_tests PRIVATE
    CONJURE_TEST_SHADER_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${shaderOutputDir}>"
    CONJURE_TEST_SHADER_SOURCE_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${PROJECT_SOURCE_DIR}/engine/shaders>"
    CONJURE_TEST_TONEMAP_HLSLI="$<PATH:CMAKE_PATH,NORMALIZE,${PROJECT_SOURCE_DIR}/engine/shaders/shared/tonemap.hlsli>"
)
target_include_directories(conjure_renderer_tests PRIVATE "${PROJECT_SOURCE_DIR}/engine/src")

# Every public header of the module must compile on its own (build-only check, part of ALL).
conjure_header_check(conjure_renderer)
