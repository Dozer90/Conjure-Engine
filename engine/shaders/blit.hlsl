// Blit (docs/design/02_renderer.md 3.3): copies the LDR texture into the swapchain back buffer or the
// render target's colour texture with a point sample.

#include "gpu_types.hlsli"
#include "fullscreen.hlsli"

Texture2D gSource : register(t0, space0);
SamplerState gPointClampSampler : register(s0, space0);

fullscreen_output vs_main(uint vertexId : SV_VertexID) {
    return fullscreen_vs(vertexId);
}

float4 ps_main(fullscreen_output input) : SV_Target0 {
    return gSource.SampleLevel(gPointClampSampler, input.uv, 0.0);
}
