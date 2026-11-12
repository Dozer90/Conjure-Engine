// Skybox (docs/design/02_renderer.md 3.3): a fullscreen triangle at the far depth (0 under
// reverse-Z) tested GreaterOrEqual, so it fills what the geometry left; the sky cubemap is sampled
// in the rotated view direction and scaled by the environment intensity.

#include "gpu_types.hlsli"
#include "fullscreen.hlsli"

TextureCube gSky : register(t0, space0);
SamplerState gLinearClampSampler : register(s0, space0);

fullscreen_output vs_main(uint vertexId : SV_VertexID) {
    return fullscreen_vs(vertexId);
}

float4 ps_main(fullscreen_output input) : SV_Target0 {
    float2 ndc = float2(input.uv.x * 2.0 - 1.0, 1.0 - input.uv.y * 2.0);
    float4 farPoint = mul(float4(ndc, 0.0, 1.0), gView.worldFromClip);
    float4 nearPoint = mul(float4(ndc, 1.0, 1.0), gView.worldFromClip);
    float3 direction = normalize(farPoint.xyz / farPoint.w - nearPoint.xyz / nearPoint.w);
    float angle = -gView.environmentParams.y;
    float s = sin(angle);
    float c = cos(angle);
    float3 rotated = float3(direction.x * c + direction.z * s, direction.y, -direction.x * s + direction.z * c);
    float3 sky = gSky.SampleLevel(gLinearClampSampler, rotated, 0.0).rgb * gView.environmentParams.x;
    return float4(sky, 1.0);
}
