// Tiled light culling (docs/design/02_renderer.md R1, 3.3): one 16x16 thread group per screen tile
// builds two lists of local lights: list A bounded by the tile's min / max depth (opaque shading) and
// list B unbounded (blended meshes). Directional lights are not listed; the forward shader loops
// over them directly.

#include "gpu_types.hlsli"

Texture2D<float> gDepth : register(t0, space0);
StructuredBuffer<gpu_light> gLights : register(t1, space0);
RWStructuredBuffer<uint> gTileLists : register(u0, space0);

groupshared uint sMinDepthBits;
groupshared uint sMaxDepthBits;
groupshared uint sCountA;
groupshared uint sCountB;
groupshared uint sListA[CONJURE_MAX_LIGHTS_PER_TILE];
groupshared uint sListB[CONJURE_MAX_LIGHTS_PER_TILE];

// View-space position of a pixel at a reverse-Z depth.
float3 view_position(float2 pixel, float depth) {
    float2 ndc = float2(pixel.x * gView.viewportSize.z * 2.0 - 1.0, 1.0 - pixel.y * gView.viewportSize.w * 2.0);
    float4 view = mul(float4(ndc, depth, 1.0), gView.viewFromClip);
    return view.xyz / max(abs(view.w), 1e-6) * (view.w < 0.0 ? -1.0 : 1.0);
}

// A plane through the camera origin and two points on the far plane (view space); normal points
// into the frustum.
float4 side_plane(float3 a, float3 b) {
    float3 normal = normalize(cross(a, b));
    return float4(normal, 0.0);
}

bool sphere_inside(float4 plane, float3 centre, float radius) {
    return dot(plane.xyz, centre) + plane.w >= -radius;
}

[numthreads(CONJURE_LIGHT_TILE_SIZE, CONJURE_LIGHT_TILE_SIZE, 1)]
void cs_main(uint3 groupId : SV_GroupID, uint3 threadId : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex) {
    if (groupIndex == 0) {
        sMinDepthBits = 0xFFFFFFFF;
        sMaxDepthBits = 0;
        sCountA = 0;
        sCountB = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    uint2 size = uint2(gView.viewportSize.xy);
    uint2 pixel = min(threadId.xy, size - 1);
    float depth = gDepth.Load(int3(pixel, 0));
    if (depth > 0.0) {
        // Reverse-Z: larger depth is closer. asuint keeps the order for positive floats.
        InterlockedMin(sMinDepthBits, asuint(depth));
        InterlockedMax(sMaxDepthBits, asuint(depth));
    }
    GroupMemoryBarrierWithGroupSync();

    float farDepth = asfloat(sMinDepthBits);
    float nearDepth = asfloat(sMaxDepthBits);
    bool hasGeometry = sMaxDepthBits != 0;
    float nearZ = hasGeometry ? -view_position(float2(pixel) + 0.5, nearDepth).z : gView.nearFarExposure.x;
    float farZ = hasGeometry ? -view_position(float2(pixel) + 0.5, farDepth).z : gView.nearFarExposure.y;

    // Tile frustum from the four corners on the far plane.
    float2 tileMin = float2(groupId.xy * CONJURE_LIGHT_TILE_SIZE);
    float2 tileMax = tileMin + CONJURE_LIGHT_TILE_SIZE;
    float3 corner00 = view_position(float2(tileMin.x, tileMin.y), 0.0);
    float3 corner10 = view_position(float2(tileMax.x, tileMin.y), 0.0);
    float3 corner01 = view_position(float2(tileMin.x, tileMax.y), 0.0);
    float3 corner11 = view_position(float2(tileMax.x, tileMax.y), 0.0);
    float4 planes[4];
    planes[0] = side_plane(corner01, corner00);   // left
    planes[1] = side_plane(corner10, corner11);   // right
    planes[2] = side_plane(corner00, corner10);   // top
    planes[3] = side_plane(corner11, corner01);   // bottom

    uint lightCount = gView.counts.z;
    uint directionalCount = gView.counts.w;
    for (uint lightIndex = directionalCount + groupIndex; lightIndex < lightCount; lightIndex += CONJURE_LIGHT_TILE_SIZE * CONJURE_LIGHT_TILE_SIZE) {
        gpu_light light = gLights[lightIndex];
        float3 centre = mul(float4(light.positionRange.xyz, 1.0), gView.viewFromWorld).xyz;
        float radius = light.positionRange.w;
        bool inside = true;
        [unroll]
        for (int plane = 0; plane < 4; ++plane) {
            inside = inside && sphere_inside(planes[plane], centre, radius);
        }
        if (!inside) {
            continue;
        }
        // Depth bounds in view space along -Z.
        float lightNear = -centre.z - radius;
        float lightFar = -centre.z + radius;
        bool insideDepth = lightFar >= nearZ && lightNear <= farZ;
        uint slot;
        if (insideDepth && hasGeometry) {
            InterlockedAdd(sCountA, 1, slot);
            if (slot < CONJURE_MAX_LIGHTS_PER_TILE) {
                sListA[slot] = lightIndex;
            }
        }
        if (lightFar >= gView.nearFarExposure.x) {
            InterlockedAdd(sCountB, 1, slot);
            if (slot < CONJURE_MAX_LIGHTS_PER_TILE) {
                sListB[slot] = lightIndex;
            }
        }
    }
    GroupMemoryBarrierWithGroupSync();

    uint tileIndex = groupId.y * gView.counts.x + groupId.x;
    uint base = tileIndex * CONJURE_TILE_LIST_STRIDE;
    uint countA = min(sCountA, CONJURE_MAX_LIGHTS_PER_TILE);
    uint countB = min(sCountB, CONJURE_MAX_LIGHTS_PER_TILE);
    if (groupIndex == 0) {
        gTileLists[base] = countA;
        gTileLists[base + CONJURE_TILE_LIST_B_OFFSET] = countB;
    }
    for (uint copy = groupIndex; copy < CONJURE_MAX_LIGHTS_PER_TILE; copy += CONJURE_LIGHT_TILE_SIZE * CONJURE_LIGHT_TILE_SIZE) {
        if (copy < countA) {
            gTileLists[base + 1 + copy] = sListA[copy];
        }
        if (copy < countB) {
            gTileLists[base + CONJURE_TILE_LIST_B_OFFSET + 1 + copy] = sListB[copy];
        }
    }
}
