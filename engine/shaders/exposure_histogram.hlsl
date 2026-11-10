// Auto-exposure histogram (docs/design/02_renderer.md R11, 3.8): 256 bins over log2 luminance in
// [minEv, maxEv], accumulated through groupshared atomics, one 16x16 group per 16x16 pixels of the
// view's viewport. The viewport rectangle comes through the push constants: origin in
// gDrawFirstInstance / gDrawFlags, size as the bit patterns of gDrawParam0 / gDrawParam1.

#include "gpu_types.hlsli"
#include "colour.hlsli"

Texture2D gHdr : register(t0, space0);
RWStructuredBuffer<uint> gHistogram : register(u0, space0);

groupshared uint sBins[CONJURE_EXPOSURE_HISTOGRAM_BINS];

[numthreads(16, 16, 1)]
void cs_main(uint3 threadId : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex) {
    sBins[groupIndex] = 0;
    GroupMemoryBarrierWithGroupSync();

    uint2 origin = uint2(gDrawFirstInstance, gDrawFlags);
    uint2 size = uint2(asuint(gDrawParam0), asuint(gDrawParam1));
    if (threadId.x < size.x && threadId.y < size.y) {
        float3 colour = gHdr.Load(int3(origin + threadId.xy, 0)).rgb;
        float lum = max(luminance(colour), 1e-6);
        float minEv = gView.exposureParams.w;
        float maxEv = gView.exposureParams2.x;
        float normalised = saturate((log2(lum) - minEv) / max(maxEv - minEv, 1e-3));
        uint bin = uint(normalised * float(CONJURE_EXPOSURE_HISTOGRAM_BINS - 1) + 0.5);
        InterlockedAdd(sBins[bin], 1);
    }
    GroupMemoryBarrierWithGroupSync();

    uint count = sBins[groupIndex];
    if (count > 0) {
        InterlockedAdd(gHistogram[groupIndex], count);
    }
}
