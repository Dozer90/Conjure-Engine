// Colour helpers shared by the exposure and bloom passes: luminance and the Karis weight. The sRGB
// transfer functions are not here because the hardware applies them (RGBA8_SRGB targets and views).

#ifndef CONJURE_COLOUR_HLSLI
#define CONJURE_COLOUR_HLSLI

float luminance(float3 colour) {
    return dot(colour, float3(0.2126, 0.7152, 0.0722));
}

// Karis average weight: 1 / (1 + luminance), used to suppress fireflies in the first bloom downsample.
float karis_weight(float3 colour) {
    return 1.0 / (1.0 + luminance(colour));
}

#endif // CONJURE_COLOUR_HLSLI
