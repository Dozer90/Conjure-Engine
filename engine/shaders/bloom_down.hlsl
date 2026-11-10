// Bloom downsample (docs/design/02_renderer.md R11, 3.8): 13-tap filter (Jimenez 2014) from the
// source texture into a half-size destination; the first level applies the Karis average
// (gDrawFlags & CONJURE_DRAW_FLAG_KARIS_AVERAGE) to suppress fireflies.

#include "gpu_types.hlsli"
#include "colour.hlsli"

Texture2D gSource : register(t0, space0);
RWTexture2D<float4> gDestination : register(u0, space0);
SamplerState gLinearClampSampler : register(s0, space0);

float3 fetch(float2 uv, float2 offset, float2 texel) {
    return gSource.SampleLevel(gLinearClampSampler, uv + offset * texel, 0.0).rgb;
}

float3 karis(float3 a, float3 b, float3 c, float3 d) {
    float wa = karis_weight(a);
    float wb = karis_weight(b);
    float wc = karis_weight(c);
    float wd = karis_weight(d);
    return (a * wa + b * wb + c * wc + d * wd) / max(wa + wb + wc + wd, 1e-4);
}

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

    float3 a = fetch(uv, float2(-2.0, -2.0), texel);
    float3 b = fetch(uv, float2(0.0, -2.0), texel);
    float3 c = fetch(uv, float2(2.0, -2.0), texel);
    float3 d = fetch(uv, float2(-2.0, 0.0), texel);
    float3 e = fetch(uv, float2(0.0, 0.0), texel);
    float3 f = fetch(uv, float2(2.0, 0.0), texel);
    float3 g = fetch(uv, float2(-2.0, 2.0), texel);
    float3 h = fetch(uv, float2(0.0, 2.0), texel);
    float3 i = fetch(uv, float2(2.0, 2.0), texel);
    float3 j = fetch(uv, float2(-1.0, -1.0), texel);
    float3 k = fetch(uv, float2(1.0, -1.0), texel);
    float3 l = fetch(uv, float2(-1.0, 1.0), texel);
    float3 m = fetch(uv, float2(1.0, 1.0), texel);

    float3 result;
    if ((gDrawFlags & CONJURE_DRAW_FLAG_KARIS_AVERAGE) != 0) {
        result = karis(j, k, l, m) * 0.5;
        result += karis(a, b, d, e) * 0.125;
        result += karis(b, c, e, f) * 0.125;
        result += karis(d, e, g, h) * 0.125;
        result += karis(e, f, h, i) * 0.125;
    } else {
        result = e * 0.125;
        result += (a + c + g + i) * 0.03125;
        result += (b + d + f + h) * 0.0625;
        result += (j + k + l + m) * 0.125;
    }
    gDestination[threadId.xy] = float4(result, 1.0);
}
