# The engine's shaders (docs/design/02_renderer.md 3.12). Every permutation is listed here as
# "source;entry;stage;defines" and compiled by conjure_add_shaders() into
# ${PROJECT_BINARY_DIR}/shaders with an index.json that rhi::shader_library reads. The fullscreen
# triangle is the pair that proves the pipeline on every CI platform; the rest are the renderer's
# pass shaders (engine/src/renderer/assets/shader_asset.cpp lists the same permutations, and
# engine/src/renderer/pipeline_cache.cpp creates a pipeline from each).

# Every shared .hlsli is listed as a DEPENDENCIES entry: generators without depfile support (the
# windows-vs preset) rebuild the blobs from this list, and sourceHash in index.json covers them.
conjure_add_shaders(
    TARGET conjure_shaders
    OUTPUT_DIR "${PROJECT_BINARY_DIR}/shaders"
    INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/shared"
    DEPENDENCIES
        "${CMAKE_CURRENT_SOURCE_DIR}/shared/brdf.hlsli"
        "${CMAKE_CURRENT_SOURCE_DIR}/shared/colour.hlsli"
        "${CMAKE_CURRENT_SOURCE_DIR}/shared/common.hlsli"
        "${CMAKE_CURRENT_SOURCE_DIR}/shared/fullscreen.hlsli"
        "${CMAKE_CURRENT_SOURCE_DIR}/shared/gpu_types.hlsli"
        "${CMAKE_CURRENT_SOURCE_DIR}/shared/mesh_pass.hlsli"
        "${CMAKE_CURRENT_SOURCE_DIR}/shared/noise.hlsli"
        "${CMAKE_CURRENT_SOURCE_DIR}/shared/octahedral.hlsli"
        "${CMAKE_CURRENT_SOURCE_DIR}/shared/shadows.hlsli"
        "${CMAKE_CURRENT_SOURCE_DIR}/shared/tonemap.hlsli"
    SHADERS
        "fullscreen_triangle.hlsl;vs_main;vertex;"
        "fullscreen_triangle.hlsl;ps_main;pixel;"
        "shadow_depth.hlsl;vs_main;vertex;ALPHA_MASK=0,1"
        "shadow_depth.hlsl;ps_main;pixel;ALPHA_MASK=1"
        "depth_prepass.hlsl;vs_main;vertex;"
        "depth_prepass.hlsl;ps_main;pixel;ALPHA_MASK=0,1"
        "forward_pbr.hlsl;vs_main;vertex;"
        "forward_pbr.hlsl;ps_main;pixel;ALPHA_MASK=0,1"
        "skybox.hlsl;vs_main;vertex;"
        "skybox.hlsl;ps_main;pixel;"
        "sprite.hlsl;vs_main;vertex;"
        "sprite.hlsl;ps_main;pixel;"
        "debug_draw.hlsl;vs_main;vertex;"
        "debug_draw.hlsl;ps_main;pixel;"
        "tonemap.hlsl;vs_main;vertex;"
        "tonemap.hlsl;ps_main;pixel;"
        "blit.hlsl;vs_main;vertex;"
        "blit.hlsl;ps_main;pixel;"
        "ibl_brdf_lut.hlsl;vs_main;vertex;"
        "ibl_brdf_lut.hlsl;ps_main;pixel;"
        "imgui.hlsl;vs_main;vertex;"
        "imgui.hlsl;ps_main;pixel;"
        "light_cull.hlsl;cs_main;compute;"
        "ssao_gtao.hlsl;cs_main;compute;"
        "ssao_blur.hlsl;cs_main;compute;"
        "exposure_histogram.hlsl;cs_main;compute;"
        "exposure_adapt.hlsl;cs_main;compute;"
        "bloom_down.hlsl;cs_main;compute;"
        "bloom_up.hlsl;cs_main;compute;"
)
