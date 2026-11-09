# The engine's shaders (docs/design/02_renderer.md 3.12). Every permutation is listed here as
# "source;entry;stage;defines" and compiled by conjure_add_shaders() into
# ${PROJECT_BINARY_DIR}/shaders with an index.json that rhi::shader_library reads. The renderer
# (wave 3) appends its pass shaders to this list; the fullscreen triangle is the pair that proves
# the pipeline on every CI platform.

# Every shared .hlsli is listed as a DEPENDENCIES entry: generators without depfile support (the
# windows-vs preset) rebuild the blobs from this list, and sourceHash in index.json covers them.
conjure_add_shaders(
    TARGET conjure_shaders
    OUTPUT_DIR "${PROJECT_BINARY_DIR}/shaders"
    INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/shared"
    DEPENDENCIES
        "${CMAKE_CURRENT_SOURCE_DIR}/shared/common.hlsli"
    SHADERS
        "fullscreen_triangle.hlsl;vs_main;vertex;"
        "fullscreen_triangle.hlsl;ps_main;pixel;"
)
