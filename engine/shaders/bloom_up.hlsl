// Bloom upsample (docs/design/02_renderer.md R11, 3.8): 3x3 tent filter of the smaller mip added
// into the larger one.

#include "gpu_types.hlsli"

Texture2D gSource : register(t0, space0);
RWTexture2D<float4> gDestination : register(u0, space0);
SamplerState gLinearClampSampler : register(s0, space0);

[numthreads(8, 8, 1)]
void cs_main(uint3 threadId : SV_DispatchThreadID) {
    uint destinationWidth;
    uint destinationHeight;
    gDestination.GetDimensions(destinationWidth, destinationHeight);
    if (threadId.x >= destinationWidth || threadId.y >= destinationHeight) {
        return;
    }
    uint sourceWidth;
    uint sourceHeight;
    gSource.GetDimensions(sourceWidth, sourceHeight);
    float2 texel = 1.0 / float2(max(sourceWidth, 1), max(sourceHeight, 1));
    float2 uv = (float2(threadId.xy) + 0.5) / float2(destinationWidth, destinationHeight);

    float3 sum = float3(0.0, 0.0, 0.0);
    float weights[3] = {1.0, 2.0, 1.0};
    [unroll]
    for (int y = -1; y <= 1; ++y) {
        [unroll]
        for (int x = -1; x <= 1; ++x) {
            float weight = weights[x + 1] * weights[y + 1] / 16.0;
            sum += gSource.SampleLevel(gLinearClampSampler, uv + float2(x, y) * texel, 0.0).rgb * weight;
        }
    }
    float4 existing = gDestination[threadId.xy];
    gDestination[threadId.xy] = float4(existing.rgb + sum, 1.0);
}
