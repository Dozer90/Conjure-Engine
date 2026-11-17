# conjure_renderer - render graph over the rhi, render_scene contract, PBR / IBL / shadows / SSAO / HDR
# chain, 2D sprites, debug draw, the renderer asset types with their importers, and the ImGui backend
# (docs/design/02_renderer.md). Depends publicly on conjure::core, conjure::rhi and conjure::assets
# (never on ecs, R7); NVRHI, cgltf, stb, libjxl (JPEG XL, every configuration: a runtime that runs a project from
# its sources imports textures too) and ImGui are private. The ImGui backend is compiled only in
# editor builds (CONJURE_BUILD_EDITOR, never Distribution). The shaders are built by conjure_shaders
# (engine/shaders), which this target depends on so index.json exists before the renderer is used.

if(NOT CONJURE_WITH_RENDERER)
    message(STATUS "renderer: CONJURE_WITH_RENDERER is OFF, module skipped")
    return()
endif()
if(NOT TARGET conjure::rhi OR NOT TARGET conjure::assets)
    message(STATUS "renderer: conjure::rhi or conjure::assets is not available, module skipped")
    return()
endif()

set(CONJURE_RENDERER_PUBLIC_HEADERS
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/camera.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/components.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/debug_draw.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/environment_asset.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/gltf_importer.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/gpu_ids.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/imgui_renderer.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/jpeg_xl.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/light.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/limits.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/log_category.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/material_asset.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/mesh_asset.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/model_asset.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/render_scene.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/render_settings.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/render_target.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/renderer.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/renderer_error.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/shader_asset.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/sprite_batch.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/renderer/texture_asset.h
)

set(CONJURE_RENDERER_SOURCES
    binding_layouts.cpp
    binding_layouts.h
    camera.cpp
    components.cpp
    culling.cpp
    culling.h
    debug_draw.cpp
    draw_list_builder.cpp
    draw_list_builder.h
    exposure_math.cpp
    exposure_math.h
    framebuffer_cache.cpp
    framebuffer_cache.h
    frame_resources.cpp
    frame_resources.h
    gpu_resource_cache.cpp
    gpu_resource_cache.h
    gpu_types.cpp
    gpu_types.h
    light.cpp
    log_category.cpp
    pipeline_cache.cpp
    pipeline_cache.h
    read_back_decode.cpp
    read_back_decode.h
    render_graph.cpp
    render_graph.h
    render_scene.cpp
    render_settings.cpp
    render_target.cpp
    render_target_impl.h
    renderer.cpp
    renderer_error.cpp
    renderer_impl.h
    shader_program_loader.cpp
    shader_program_loader.h
    sprite_batcher.cpp
    timer_ring.cpp
    timer_ring.h
    transient_pool.cpp
    transient_pool.h
    warn_latch.h
    assets/asset_registration.cpp
    assets/blob_io.h
    assets/environment_asset.cpp
    assets/material_asset.cpp
    assets/mesh_asset.cpp
    assets/model_asset.cpp
    assets/shader_asset.cpp
    assets/texture_asset.cpp
    import/gltf_importer.cpp
    import/ibl_baker.cpp
    import/file_bytes.h
    import/ibl_baker.h
    import/image_loader.cpp
    import/image_loader.h
    import/jpeg_xl_codec.h
    import/jpeg_xl_decoder.cpp
    import/jpeg_xl_encoder.cpp
    import/jpeg_xl_hooks.cpp
    import/jpeg_xl_hooks.h
    import/mip_generator.cpp
    import/mip_generator.h
    import/tangent_generator.cpp
    import/tangent_generator.h
    passes/blit_pass.cpp
    passes/pass_shared.cpp
    passes/bloom_pass.cpp
    passes/debug_draw_pass.cpp
    passes/depth_prepass.cpp
    passes/exposure_pass.cpp
    passes/forward_pass.cpp
    passes/light_cull_pass.cpp
    passes/passes.h
    passes/shadow_passes.cpp
    passes/skybox_pass.cpp
    passes/sprite_pass.cpp
    passes/ssao_pass.cpp
    passes/tonemap_pass.cpp
    shadows/cascade_builder.cpp
    shadows/cascade_builder.h
    shadows/shadow_atlas_allocator.cpp
    shadows/shadow_atlas_allocator.h
)

set(CONJURE_RENDERER_PRIVATE_DEPS
    conjure::thirdparty::nvrhi
    conjure::thirdparty::cgltf
    conjure::thirdparty::stb
    conjure::thirdparty::jxl
)
if(CONJURE_BUILD_EDITOR AND TARGET conjure::thirdparty::imgui)
    list(APPEND CONJURE_RENDERER_SOURCES imgui/imgui_renderer.cpp)
    list(APPEND CONJURE_RENDERER_PRIVATE_DEPS conjure::thirdparty::imgui)
endif()

conjure_add_module(renderer
    PUBLIC_HEADERS
        ${CONJURE_RENDERER_PUBLIC_HEADERS}
    SOURCES
        ${CONJURE_RENDERER_SOURCES}
    PUBLIC_DEPS
        conjure::core
        conjure::rhi
        conjure::assets
    PRIVATE_DEPS
        ${CONJURE_RENDERER_PRIVATE_DEPS}
)

# The HLSL is compiled by conjure_shaders (added by engine/CMakeLists.txt after the modules); the
# renderer reads the result through rhi::shader_library at run time, so the dependency only orders
# the build. The shader target is declared later in the configure, hence the deferred call.
cmake_language(DEFER DIRECTORY "${PROJECT_SOURCE_DIR}/engine" CALL add_dependencies conjure_renderer conjure_shaders)
