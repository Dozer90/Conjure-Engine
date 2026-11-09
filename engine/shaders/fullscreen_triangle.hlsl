// Fullscreen triangle: the first shader pair compiled by conjure_add_shaders(), used by the rhi
// tests to prove the pipeline (index.json, DXIL and SPIR-V blobs) and by the GPU swapchain test.
// The vertex shader generates one triangle covering the viewport from SV_VertexID; the pixel
// shader writes a constant colour from push constants.

#include "common.hlsli"

CONJURE_PUSH_CONSTANTS cbuffer draw_constants : register(b1, space0) {
    float4 gColour;
};

struct vertex_output {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

vertex_output vs_main(uint vertexId : SV_VertexID) {
    vertex_output output;
    // Vertices (0, 0), (2, 0), (0, 2) in uv space cover the whole viewport with one triangle.
    output.uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.position = float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

float4 ps_main(vertex_output input) : SV_Target0 {
    return gColour;
}
