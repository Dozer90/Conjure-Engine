// Separable depth-aware blur of the half-resolution AO (docs/design/02_renderer.md R10): four taps
// along the axis given by gDrawFlags (0 horizontal, 1 vertical), weighted by depth proximity.

#include "gpu_types.hlsli"

Texture2D<float> gAmbientOcclusionIn : register(t0, space0);
Texture2D<float> gDepth : register(t1, space0);
RWTexture2D<float> gAmbientOcclusionOut : register(u0, space0);
SamplerState gPointClampSampler : register(s0, space0);

float linear_depth(float2 uv) {
    float depth = gDepth.SampleLevel(gPointClampSampler, uv, 0.0);
    float4 view = mul(float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0), gView.viewFromClip);
    return -view.z / max(abs(view.w), 1e-6);
}

[numthreads(8, 8, 1)]
void cs_main(uint3 threadId : SV_DispatchThreadID) {
    uint2 halfSize = max(uint2(gView.viewportSize.xy) / 2, uint2(1, 1));
    if (threadId.x >= halfSize.x || threadId.y >= halfSize.y) {
        return;
    }
    float2 texel = 1.0 / float2(halfSize);
    float2 uv = (float2(threadId.xy) + 0.5) * texel;
    float2 axis = (gDrawFlags & 1) != 0 ? float2(0.0, texel.y) : float2(texel.x, 0.0);
    float centreDepth = linear_depth(uv);
    float sum = 0.0;
    float weightSum = 0.0;
    float offsets[4] = {-1.5, -0.5, 0.5, 1.5};
    [unroll]
    for (int tap = 0; tap < 4; ++tap) {
        float2 sampleUv = uv + axis * offsets[tap];
        float sampleDepth = linear_depth(sampleUv);
        float weight = saturate(1.0 - abs(sampleDepth - centreDepth) / max(centreDepth * 0.1, 1e-3));
        sum += gAmbientOcclusionIn.SampleLevel(gPointClampSampler, sampleUv, 0.0) * weight;
        weightSum += weight;
    }
    gAmbientOcclusionOut[threadId.xy] = weightSum > 0.0 ? sum / weightSum : gAmbientOcclusionIn.SampleLevel(gPointClampSampler, uv, 0.0);
}
