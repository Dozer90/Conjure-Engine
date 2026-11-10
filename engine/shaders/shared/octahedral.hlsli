// Octahedral unit-vector encoding into two components in [-1, 1] (RG16_SNORM), used for the
// view-space normals the depth prepass writes for the GTAO pass.

#ifndef CONJURE_OCTAHEDRAL_HLSLI
#define CONJURE_OCTAHEDRAL_HLSLI

float2 sign_not_zero(float2 value) {
    return float2(value.x >= 0.0 ? 1.0 : -1.0, value.y >= 0.0 ? 1.0 : -1.0);
}

float2 octahedral_encode(float3 direction) {
    float3 n = direction / (abs(direction.x) + abs(direction.y) + abs(direction.z));
    float2 projected = n.xy;
    if (n.z < 0.0) {
        projected = (1.0 - abs(n.yx)) * sign_not_zero(n.xy);
    }
    return projected;
}

float3 octahedral_decode(float2 encoded) {
    float3 n = float3(encoded.xy, 1.0 - abs(encoded.x) - abs(encoded.y));
    if (n.z < 0.0) {
        n.xy = (1.0 - abs(n.yx)) * sign_not_zero(n.xy);
    }
    return normalize(n);
}

#endif // CONJURE_OCTAHEDRAL_HLSLI
