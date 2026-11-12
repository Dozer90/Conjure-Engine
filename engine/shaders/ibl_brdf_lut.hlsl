// Split-sum BRDF lookup table (docs/design/02_renderer.md R13, 3.6): 256^2 RG16_FLOAT with
// x = N.V and y = roughness, 1024 Hammersley samples, baked once at renderer creation. Texture v is
// the roughness (row 0 is smooth), matching the float2(N.V, roughness) lookup in forward_pbr.hlsl.

#include "gpu_types.hlsli"
#include "brdf.hlsli"
#include "fullscreen.hlsli"

fullscreen_output vs_main(uint vertexId : SV_VertexID) {
    return fullscreen_vs(vertexId);
}

float radical_inverse(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

float3 importance_sample_ggx(float2 xi, float alpha) {
    float phi = 2.0 * CONJURE_PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (alpha * alpha - 1.0) * xi.y));
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
    return float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

float2 ps_main(fullscreen_output input) : SV_Target0 {
    float nDotV = max(input.uv.x, 1e-3);
    float roughness = max(input.uv.y, CONJURE_MIN_ROUGHNESS);
    float alpha = roughness * roughness;
    float3 view = float3(sqrt(1.0 - nDotV * nDotV), 0.0, nDotV);
    float scale = 0.0;
    float bias = 0.0;
    const uint sampleCount = 1024;
    for (uint sample = 0; sample < sampleCount; ++sample) {
        float2 xi = float2(float(sample) / float(sampleCount), radical_inverse(sample));
        float3 halfVector = importance_sample_ggx(xi, alpha);
        float3 light = 2.0 * dot(view, halfVector) * halfVector - view;
        float nDotL = saturate(light.z);
        float nDotH = saturate(halfVector.z);
        float vDotH = saturate(dot(view, halfVector));
        if (nDotL > 0.0) {
            float visibility = visibility_smith_ggx(nDotV, nDotL, alpha) * 4.0 * nDotL * vDotH / max(nDotH, 1e-4);
            float fresnel = pow(1.0 - vDotH, 5.0);
            scale += (1.0 - fresnel) * visibility;
            bias += fresnel * visibility;
        }
    }
    return float2(scale, bias) / float(sampleCount);
}
