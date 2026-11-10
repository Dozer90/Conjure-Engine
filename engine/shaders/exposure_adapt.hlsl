// Auto-exposure adaptation (docs/design/02_renderer.md R11, 3.8): the mean log luminance of the
// 50th-90th percentile gives the target EV100 (plus compensation); the stored EV100 moves toward it
// with separate up / down speeds and exposure = 1 / (1.2 * 2^EV100). One group of 256 threads.

#include "gpu_types.hlsli"

StructuredBuffer<uint> gHistogram : register(t0, space0);
RWStructuredBuffer<gpu_exposure_state> gExposure : register(u0, space0);

groupshared uint sCounts[CONJURE_EXPOSURE_HISTOGRAM_BINS];
groupshared uint sPrefix[CONJURE_EXPOSURE_HISTOGRAM_BINS];

[numthreads(CONJURE_EXPOSURE_HISTOGRAM_BINS, 1, 1)]
void cs_main(uint groupIndex : SV_GroupIndex) {
    sCounts[groupIndex] = gHistogram[groupIndex];
    GroupMemoryBarrierWithGroupSync();

    // Inclusive prefix sum (Hillis-Steele) over the 256 bins.
    sPrefix[groupIndex] = sCounts[groupIndex];
    GroupMemoryBarrierWithGroupSync();
    for (uint offset = 1; offset < CONJURE_EXPOSURE_HISTOGRAM_BINS; offset <<= 1) {
        uint addend = groupIndex >= offset ? sPrefix[groupIndex - offset] : 0;
        GroupMemoryBarrierWithGroupSync();
        sPrefix[groupIndex] += addend;
        GroupMemoryBarrierWithGroupSync();
    }
    if (groupIndex != 0) {
        return;
    }

    uint total = sPrefix[CONJURE_EXPOSURE_HISTOGRAM_BINS - 1];
    float minEv = gView.exposureParams.w;
    float maxEv = gView.exposureParams2.x;
    gpu_exposure_state state = gExposure[0];
    if (total == 0) {
        // Nothing rendered: keep the previous exposure.
        gExposure[0] = state;
        return;
    }
    uint lowCount = total / 2;
    uint highCount = (total * 9) / 10;
    float weightedSum = 0.0;
    float weightSum = 0.0;
    uint runningBefore = 0;
    for (uint bin = 0; bin < CONJURE_EXPOSURE_HISTOGRAM_BINS; ++bin) {
        uint count = sCounts[bin];
        uint runningAfter = runningBefore + count;
        // The part of this bin that falls inside [lowCount, highCount].
        uint clippedLow = max(runningBefore, lowCount);
        uint clippedHigh = min(runningAfter, highCount);
        if (clippedHigh > clippedLow) {
            float weight = float(clippedHigh - clippedLow);
            float ev = minEv + (float(bin) / float(CONJURE_EXPOSURE_HISTOGRAM_BINS - 1)) * (maxEv - minEv);
            weightedSum += ev * weight;
            weightSum += weight;
        }
        runningBefore = runningAfter;
    }
    float averageLogLuminance = weightSum > 0.0 ? weightedSum / weightSum : 0.0;
    float averageLuminance = exp2(averageLogLuminance);
    float targetEv100 = clamp(log2(averageLuminance * 100.0 / 12.5) + gView.exposureParams.x, minEv, maxEv);

    float deltaSeconds = max(gView.nearFarExposure.w, 0.0);
    float speed = targetEv100 > state.ev100 ? gView.exposureParams.y : gView.exposureParams.z;
    float blend = 1.0 - exp2(-deltaSeconds * speed * 1.4426950408889634);
    float ev100 = (state.lastTargetEv100 == 0.0 && state.ev100 == 0.0) ? targetEv100 : lerp(state.ev100, targetEv100, blend);
    state.ev100 = ev100;
    state.lastTargetEv100 = targetEv100;
    state.exposure = 1.0 / (1.2 * exp2(ev100));
    gExposure[0] = state;
}
