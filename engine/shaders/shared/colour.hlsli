// Colour helpers shared by the tonemap, exposure and bloom passes.

#ifndef CONJURE_COLOUR_HLSLI
#define CONJURE_COLOUR_HLSLI

float luminance(float3 colour) {
    return dot(colour, float3(0.2126, 0.7152, 0.0722));
}

float3 srgb_to_linear(float3 encoded) {
    float3 low = encoded / 12.92;
    float3 high = pow(max((encoded + 0.055) / 1.055, 0.0), 2.4);
    return float3(encoded.x <= 0.04045 ? low.x : high.x, encoded.y <= 0.04045 ? low.y : high.y, encoded.z <= 0.04045 ? low.z : high.z);
}

float3 linear_to_srgb(float3 colour) {
    float3 low = colour * 12.92;
    float3 high = 1.055 * pow(max(colour, 0.0), 1.0 / 2.4) - 0.055;
    return float3(colour.x <= 0.0031308 ? low.x : high.x, colour.y <= 0.0031308 ? low.y : high.y, colour.z <= 0.0031308 ? low.z : high.z);
}

// Karis average weight: 1 / (1 + luminance), used to suppress fireflies in the first bloom downsample.
float karis_weight(float3 colour) {
    return 1.0 / (1.0 + luminance(colour));
}

#endif // CONJURE_COLOUR_HLSLI
