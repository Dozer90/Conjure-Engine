// Tonemap operators (docs/design/02_renderer.md R11, 3.8). This file is written in the subset of
// HLSL that tests/renderer/hlsl_shim.h maps onto glm (float, float3, float3x3, saturate, pow, exp2,
// log2, max, min, dot, mul, lerp, abs), so the curves compile natively and are unit tested without
// a GPU. Keep it free of textures, samplers, swizzles beyond .x/.y/.z and HLSL-only intrinsics.

#ifndef CONJURE_TONEMAP_HLSLI
#define CONJURE_TONEMAP_HLSLI

// ACES filmic approximation (Stephen Hill's fit of the RRT + ODT).
float3 tonemap_aces(float3 colour) {
    float3x3 inputMatrix = float3x3(
      0.59719, 0.35458, 0.04823,
      0.07600, 0.90834, 0.01566,
      0.02840, 0.13383, 0.83777
    );
    float3x3 outputMatrix = float3x3(
      1.60475, -0.53108, -0.07367,
      -0.10208, 1.10813, -0.00605,
      -0.00327, -0.07276, 1.07602
    );
    float3 v = mul(inputMatrix, colour);
    float3 a = v * (v + 0.0245786) - 0.000090537;
    float3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    float3 fitted = a / b;
    return saturate(mul(outputMatrix, fitted));
}

// Extended Reinhard: reaches exactly 1 at the white point.
float3 tonemap_reinhard_extended(float3 colour, float whitePoint) {
    float white = max(whitePoint, 1.0);
    float3 numerator = colour * (1.0 + colour / (white * white));
    return saturate(numerator / (1.0 + colour));
}

// AgX (Troy Sobotka) with the sigmoid approximation by Benjamin Wrensch; look 0 = default,
// 1 = golden, 2 = punchy. The inset / outset literals are in the column order of the reference
// GLSL, so they are applied as mul(vector, matrix).
float3 agx_default_contrast(float3 x) {
    float3 x2 = x * x;
    float3 x4 = x2 * x2;
    return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}

float3 tonemap_agx(float3 colour, float look) {
    float3x3 inset = float3x3(
      0.842479062253094, 0.0423282422610123, 0.0423756549057051,
      0.0784335999999992, 0.878468636469772, 0.0784336,
      0.0792237451477643, 0.0791661274605434, 0.879142973793104
    );
    float3x3 outset = float3x3(
      1.19687900512017, -0.0528968517574562, -0.0529716355144438,
      -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
      -0.0990297440797205, -0.0989611768448433, 1.15107367264116
    );
    float minEv = -12.47393;
    float maxEv = 4.026069;
    float3 v = mul(max(colour, 1e-10), inset);
    v = log2(v);
    v = (v - minEv) / (maxEv - minEv);
    v = saturate(v);
    v = agx_default_contrast(v);

    // Looks: a per-channel power around the luminance with saturation control.
    float3 slope = float3(1.0, 1.0, 1.0);
    float3 power = float3(1.0, 1.0, 1.0);
    float sat = 1.0;
    if (look > 0.5 && look < 1.5) {
        slope = float3(1.0, 0.9, 0.5);
        power = float3(0.8, 0.8, 0.8);
        sat = 0.8;
    } else if (look >= 1.5) {
        slope = float3(1.0, 1.0, 1.0);
        power = float3(1.35, 1.35, 1.35);
        sat = 1.4;
    }
    float luma = dot(v, float3(0.2126, 0.7152, 0.0722));
    v = pow(max(v * slope, 0.0), power);
    v = luma + sat * (v - luma);

    v = mul(v, outset);
    v = pow(max(v, 0.0), 2.2);
    return saturate(v);
}

// Operator selection shared by tonemap.hlsl and the native reference test.
float3 apply_tonemap(float3 colour, float operatorIndex, float whitePoint, float look) {
    if (operatorIndex < 0.5) {
        return tonemap_aces(colour);
    }
    if (operatorIndex < 1.5) {
        return tonemap_reinhard_extended(colour, whitePoint);
    }
    if (operatorIndex < 2.5) {
        return tonemap_agx(colour, look);
    }
    return saturate(colour);
}

#endif // CONJURE_TONEMAP_HLSLI
