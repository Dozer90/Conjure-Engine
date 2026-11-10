// Blit (docs/design/02_renderer.md 3.3): copies the LDR texture into the swapchain back buffer or the
// render target's colour texture with a point sample.

#include "gpu_types.hlsli"

Texture2D gSource : register(t0, space0);
SamplerState gPointClampSampler : register(s0, space0);

struct fullscreen_output {
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

fullscreen_output vs_main(uint vertexId : SV_VertexID) {
    fullscreen_output output;
    output.uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.position = float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

float4 ps_main(fullscreen_output input) : SV_Target0 {
    return gSource.SampleLevel(gPointClampSampler, input.uv, 0.0);
}
