// Cook-Torrance BRDF (docs/design/02_renderer.md 3.5): GGX distribution, height-correlated Smith
// visibility, Schlick Fresnel, Lambert diffuse scaled by (1 - F) * (1 - metallic). All inputs linear.

#ifndef CONJURE_BRDF_HLSLI
#define CONJURE_BRDF_HLSLI

#include "common.hlsli"

#define CONJURE_MIN_ROUGHNESS 0.045

struct surface_params {
    float3 baseColour;
    float metallic;
    float roughness;        // perceptual, clamped to [0.045, 1]
    float3 f0;
    float3 diffuseColour;
};

surface_params make_surface(float3 baseColour, float metallic, float roughness) {
    surface_params surface;
    surface.baseColour = baseColour;
    surface.metallic = saturate(metallic);
    surface.roughness = clamp(roughness, CONJURE_MIN_ROUGHNESS, 1.0);
    surface.f0 = lerp(float3(0.04, 0.04, 0.04), baseColour, surface.metallic);
    surface.diffuseColour = baseColour * (1.0 - surface.metallic);
    return surface;
}

float distribution_ggx(float nDotH, float alpha) {
    float alpha2 = alpha * alpha;
    float denominator = nDotH * nDotH * (alpha2 - 1.0) + 1.0;
    return alpha2 / max(CONJURE_PI * denominator * denominator, 1e-7);
}

// Height-correlated Smith visibility, already divided by 4 * NdotL * NdotV.
float visibility_smith_ggx(float nDotV, float nDotL, float alpha) {
    float alpha2 = alpha * alpha;
    float lambdaV = nDotL * sqrt(nDotV * nDotV * (1.0 - alpha2) + alpha2);
    float lambdaL = nDotV * sqrt(nDotL * nDotL * (1.0 - alpha2) + alpha2);
    return 0.5 / max(lambdaV + lambdaL, 1e-7);
}

float3 fresnel_schlick(float3 f0, float vDotH) {
    float factor = pow(1.0 - vDotH, 5.0);
    return f0 + (1.0 - f0) * factor;
}

// Radiance reflected toward the viewer from one light direction with the given incoming radiance.
float3 evaluate_brdf(surface_params surface, float3 normal, float3 view, float3 lightDirection, float3 radiance) {
    float nDotL = saturate(dot(normal, lightDirection));
    if (nDotL <= 0.0) {
        return float3(0.0, 0.0, 0.0);
    }
    float3 halfVector = normalize(view + lightDirection);
    float nDotV = max(dot(normal, view), 1e-4);
    float nDotH = saturate(dot(normal, halfVector));
    float vDotH = saturate(dot(view, halfVector));
    float alpha = surface.roughness * surface.roughness;

    float3 fresnel = fresnel_schlick(surface.f0, vDotH);
    float3 specular = distribution_ggx(nDotH, alpha) * visibility_smith_ggx(nDotV, nDotL, alpha) * fresnel;
    float3 diffuse = surface.diffuseColour * (1.0 - fresnel) / CONJURE_PI;
    return (diffuse + specular) * radiance * nDotL;
}

// Punctual attenuation: inverse square windowed by (1 - (d / range)^4)^2.
float punctual_attenuation(float distance, float range) {
    float ratio = distance / max(range, 1e-3);
    float window = saturate(1.0 - ratio * ratio * ratio * ratio);
    return window * window / max(distance * distance, 1e-4);
}

float spot_attenuation(float3 lightToSurface, float3 spotDirection, float cosOuter, float cosInner) {
    float cosAngle = dot(lightToSurface, spotDirection);
    return smoothstep(cosOuter, max(cosInner, cosOuter + 1e-4), cosAngle);
}

// Split-sum ambient specular with Fdez-Aguera's multiple scattering compensation.
float3 ambient_specular(surface_params surface, float3 prefiltered, float2 lut) {
    float3 fssEss = surface.f0 * lut.x + lut.y;
    float ess = lut.x + lut.y;
    float ems = 1.0 - ess;
    float3 favg = surface.f0 + (1.0 - surface.f0) / 21.0;
    float3 fms = fssEss * favg / (1.0 - ems * favg);
    return prefiltered * (fssEss + fms * ems);
}

#endif // CONJURE_BRDF_HLSLI
