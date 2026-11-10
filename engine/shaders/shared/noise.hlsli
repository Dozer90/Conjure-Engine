// Screen-space noise patterns: interleaved gradient noise (Jimenez 2014) drives the rotated Poisson
// disk of the PCF filter and the GTAO slice rotation.

#ifndef CONJURE_NOISE_HLSLI
#define CONJURE_NOISE_HLSLI

float interleaved_gradient_noise(float2 pixel) {
    float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
    return frac(magic.z * frac(dot(pixel, magic.xy)));
}

// A 4x4 ordered dither value in [0, 1) for the pixel, used by GTAO's interleaved rotation pattern.
float ordered_dither_4x4(uint2 pixel) {
    uint index = (pixel.x & 3) + ((pixel.y & 3) << 2);
    // Bayer 4x4 matrix stored as 16 nibbles.
    uint bayer = 0;
    uint table[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    bayer = table[index];
    return (float(bayer) + 0.5) / 16.0;
}

#endif // CONJURE_NOISE_HLSLI
