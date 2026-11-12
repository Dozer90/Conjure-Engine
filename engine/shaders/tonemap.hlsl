// Tonemap (docs/design/02_renderer.md R11, 3.8): colour = hdr * exposure + bloom * intensity, then
// the operator chosen by gView.postProcess.y. Exposure comes from the GPU exposure state in automatic
// mode and from the view constants in manual mode or with NO_POST_PROCESS (exposure 1, operator NONE).
// The LDR target is RGBA8_SRGB, so the hardware applies the sRGB OETF.

#include "gpu_types.hlsli"
#include "fullscreen.hlsli"
#include "tonemap.hlsli"

Texture2D gHdr : register(t0, space0);
Texture2D gBloom : register(t1, space0);
StructuredBuffer<gpu_exposure_state> gExposure : register(t2, space0);
SamplerState gLinearClampSampler : register(s0, space0);
SamplerState gPointClampSampler : register(s1, space0);

fullscreen_output vs_main(uint vertexId : SV_VertexID) {
    return fullscreen_vs(vertexId);
}

float4 ps_main(fullscreen_output input) : SV_Target0 {
    float4 hdr = gHdr.SampleLevel(gPointClampSampler, input.uv, 0.0);
    float exposure = gView.nearFarExposure.z;
    if ((gView.flags.x & CONJURE_VIEW_FLAG_AUTO_EXPOSURE) != 0) {
        exposure = gExposure[0].exposure;
    }
    float3 colour = hdr.rgb * exposure;
    if ((gView.flags.x & CONJURE_VIEW_FLAG_BLOOM) != 0) {
        colour += gBloom.SampleLevel(gLinearClampSampler, input.uv, 0.0).rgb * gView.postProcess.x * exposure;
    }
    float3 mapped = apply_tonemap(colour, gView.postProcess.y, gView.postProcess.z, gView.postProcess.w);
    return float4(mapped, hdr.a);
}
