// Forward PBR shading (docs/design/02_renderer.md R1, R7, 3.3, 3.5): one shader for opaque, masked
// and blended meshes. Directional lights come first in the light list and use the cascades; local
// lights come from the tile list (A for opaque, B for blended, chosen by gDrawFlags); ambient is
// SH9 irradiance plus the split-sum prefiltered specular, both scaled by SSAO and material occlusion.

#include "mesh_pass.hlsli"
#include "brdf.hlsli"
#include "shadows.hlsli"

struct forward_output {
    float4 position : SV_Position;
    float3 worldPosition : WORLDPOS;
    float3 worldNormal : NORMAL;
    float4 worldTangent : TANGENT;      // xyz tangent, w sign
    float2 uv0 : TEXCOORD;
    float2 uv1 : LIGHTMAP;
    float4 colour : COLOR;
    float viewDepth : VIEWDEPTH;
    uint instanceFlags : INSTANCEFLAGS;
};

forward_output vs_main(mesh_vertex vertex, uint instanceId : SV_InstanceID) {
    gpu_instance instance = gInstances[gDrawFirstInstance + instanceId];
    forward_output output;
    output.worldPosition = transform_point(instance, vertex.position);
    output.position = mul(float4(output.worldPosition, 1.0), gView.clipFromWorld);
    output.worldNormal = transform_normal(instance, vertex.normal.xyz);
    output.worldTangent = float4(transform_direction(instance, vertex.tangent.xyz), vertex.normal.w);
    output.uv0 = vertex.uv0;
    output.uv1 = vertex.uv1;
    output.colour = vertex.colour;
    output.viewDepth = -mul(float4(output.worldPosition, 1.0), gView.viewFromWorld).z;
    output.instanceFlags = asuint(instance.normal1.w);
    return output;
}

float3 shading_normal(forward_output input, bool isFrontFace) {
    float3 normal = normalize(input.worldNormal);
    if (!isFrontFace) {
        normal = -normal;
    }
    if (!material_has_texture(CONJURE_TEXTURE_NORMAL)) {
        return normal;
    }
    float3 tangent = input.worldTangent.xyz;
    tangent = normalize(tangent - normal * dot(normal, tangent));
    float3 bitangent = cross(normal, tangent) * (input.worldTangent.w < 0.0 ? -1.0 : 1.0);
    float3 sampled = gNormalTexture.Sample(gMaterialSampler, material_uv(2, input.uv0, input.uv1)).xyz * 2.0 - 1.0;
    sampled.xy *= gMaterial.params.z;
    return normalize(tangent * sampled.x + bitangent * sampled.y + normal * sampled.z);
}

uint select_cascade(float viewDepth, out float blend) {
    uint cascadeCount = max(gView.flags.y, 1);
    uint cascade = cascadeCount - 1;
    blend = 0.0;
    for (uint index = 0; index < cascadeCount; ++index) {
        float split = gView.cascadeSplits[index];
        if (viewDepth <= split) {
            cascade = index;
            // 10% blend band at the far end of the cascade.
            float bandStart = split * 0.9;
            blend = saturate((viewDepth - bandStart) / max(split - bandStart, 1e-4));
            break;
        }
    }
    return cascade;
}

float directional_shadow(float3 worldPosition, float3 normal, float viewDepth, float2 pixel, gpu_light light) {
    uint filterMode = asuint(light.params.w);
    float blend = 0.0;
    uint cascade = select_cascade(viewDepth, blend);
    float texelSize = gView.shadowParams.x;
    // World-space texel size of a cascade: the orthographic extent is encoded as the inverse scale.
    float4x4 clipFromWorld = gView.cascadeClipFromWorld[cascade];
    float texelWorld = 2.0 / max(length(float3(clipFromWorld[0][0], clipFromWorld[1][0], clipFromWorld[2][0])), 1e-5) * texelSize;
    float3 lightSpace = shadow_project(clipFromWorld, worldPosition, normal, texelWorld, gView.shadowParams.z);
    if (!inside_unit_square(lightSpace.xy)) {
        return 1.0;
    }
    float sourceRadius = gView.shadowParams2.x;
    float shadow = sample_cascade_filtered(gShadowCascades, gShadowSampler, gPointClampSampler, float(cascade), lightSpace, pixel,
      texelSize, texelWorld, orthographic_depth_range(clipFromWorld), sourceRadius, gView.shadowParams.y, filterMode);
    uint cascadeCount = max(gView.flags.y, 1);
    if (blend > 0.0 && cascade + 1 < cascadeCount) {
        float4x4 nextClip = gView.cascadeClipFromWorld[cascade + 1];
        float nextTexelWorld = 2.0 / max(length(float3(nextClip[0][0], nextClip[1][0], nextClip[2][0])), 1e-5) * texelSize;
        float3 nextSpace = shadow_project(nextClip, worldPosition, normal, nextTexelWorld, gView.shadowParams.z);
        if (inside_unit_square(nextSpace.xy)) {
            float next = sample_cascade_filtered(gShadowCascades, gShadowSampler, gPointClampSampler, float(cascade + 1), nextSpace,
              pixel, texelSize, nextTexelWorld, orthographic_depth_range(nextClip), sourceRadius, gView.shadowParams.y, filterMode);
            shadow = lerp(shadow, next, blend);
        }
    }
    return shadow;
}

uint point_light_face(float3 lightToSurface) {
    float3 a = abs(lightToSurface);
    if (a.x >= a.y && a.x >= a.z) {
        return lightToSurface.x > 0.0 ? 0 : 1;
    }
    if (a.y >= a.z) {
        return lightToSurface.y > 0.0 ? 2 : 3;
    }
    return lightToSurface.z > 0.0 ? 4 : 5;
}

float local_shadow(float3 worldPosition, float3 normal, float2 pixel, gpu_light light) {
    uint tileIndex = asuint(light.params.z);
    if (tileIndex == CONJURE_INVALID_INDEX) {
        return 1.0;
    }
    uint type = asuint(light.params.y);
    if (type == CONJURE_LIGHT_POINT) {
        tileIndex += point_light_face(worldPosition - light.positionRange.xyz);
    }
    gpu_shadow_tile tile = gShadowTiles[tileIndex];
    float distance = max(length(worldPosition - light.positionRange.xyz), 1e-3);
    // Perspective tiles: the world size of a texel grows linearly with distance (90 degree point
    // faces, spot cone for spots); the tile stores the per-unit-distance texel size in params.w.
    float texelWorld = tile.params.w * distance;
    float3 lightSpace = shadow_project(tile.clipFromWorld, worldPosition, normal, texelWorld, gView.shadowParams.z);
    if (!inside_unit_square(lightSpace.xy) || lightSpace.z <= 0.0) {
        return 1.0;
    }
    // The tile's projection reaches the light's range (renderer.cpp local_light_clip_from_world).
    float farPlane = max(light.positionRange.w, 2.0 * CONJURE_LOCAL_SHADOW_NEAR_PLANE);
    return sample_atlas_filtered(gShadowAtlas, gShadowSampler, gPointClampSampler, tile, lightSpace, pixel, farPlane,
      gView.shadowParams.y, asuint(light.params.w));
}

float3 shade_light(surface_params surface, float3 worldPosition, float3 normal, float3 view, gpu_light light, float shadow) {
    uint type = asuint(light.params.y);
    float3 radiance = light.colourIntensity.rgb * light.colourIntensity.w;
    float3 lightDirection;
    if (type == CONJURE_LIGHT_DIRECTIONAL) {
        lightDirection = -light.directionCosOuter.xyz;
    } else {
        float3 toLight = light.positionRange.xyz - worldPosition;
        float distance = length(toLight);
        lightDirection = toLight / max(distance, 1e-4);
        radiance *= punctual_attenuation(distance, light.positionRange.w);
        if (type == CONJURE_LIGHT_SPOT) {
            radiance *= spot_attenuation(-lightDirection, light.directionCosOuter.xyz, light.directionCosOuter.w, light.params.x);
        }
    }
    return evaluate_brdf(surface, normal, view, lightDirection, radiance * shadow);
}

float3 sh9_irradiance(float3 n) {
    float3 c0 = gView.shCoefficients[0].xyz;
    float3 c1 = float3(gView.shCoefficients[0].w, gView.shCoefficients[1].xy);
    float3 c2 = float3(gView.shCoefficients[1].zw, gView.shCoefficients[2].x);
    float3 c3 = gView.shCoefficients[2].yzw;
    float3 c4 = gView.shCoefficients[3].xyz;
    float3 c5 = float3(gView.shCoefficients[3].w, gView.shCoefficients[4].xy);
    float3 c6 = float3(gView.shCoefficients[4].zw, gView.shCoefficients[5].x);
    float3 c7 = gView.shCoefficients[5].yzw;
    float3 c8 = gView.shCoefficients[6].xyz;
    float a0 = CONJURE_PI;
    float a1 = 2.0 * CONJURE_PI / 3.0;
    float a2 = CONJURE_PI / 4.0;
    float3 irradiance = c0 * (a0 * 0.282095);
    irradiance += c1 * (a1 * 0.488603 * n.y);
    irradiance += c2 * (a1 * 0.488603 * n.z);
    irradiance += c3 * (a1 * 0.488603 * n.x);
    irradiance += c4 * (a2 * 1.092548 * n.x * n.y);
    irradiance += c5 * (a2 * 1.092548 * n.y * n.z);
    irradiance += c6 * (a2 * 0.315392 * (3.0 * n.z * n.z - 1.0));
    irradiance += c7 * (a2 * 1.092548 * n.x * n.z);
    irradiance += c8 * (a2 * 0.546274 * (n.x * n.x - n.y * n.y));
    return max(irradiance, 0.0);
}

float3 rotate_y(float3 v, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return float3(v.x * c + v.z * s, v.y, -v.x * s + v.z * c);
}

// Bilateral upsample of the half-resolution AO (design 3.7): the four nearest AO texels, each
// weighted by how close its linear view depth is to the shaded pixel's, so AO does not bleed across
// depth discontinuities. The offsets are half-resolution texels (twice the full-resolution ones).
float sample_ambient_occlusion(float2 pixel, float viewDepth) {
    if ((gView.flags.x & CONJURE_VIEW_FLAG_SSAO) == 0) {
        return 1.0;
    }
    float2 uv = pixel * gView.viewportSize.zw;
    float2 halfTexel = gView.viewportSize.zw * 2.0;
    float total = 0.0;
    float weightSum = 0.0;
    float2 offsets[4] = {float2(-0.5, -0.5), float2(0.5, -0.5), float2(-0.5, 0.5), float2(0.5, 0.5)};
    [unroll]
    for (int tap = 0; tap < 4; ++tap) {
        float2 sampleUv = uv + offsets[tap] * halfTexel;
        float ao = gAmbientOcclusion.SampleLevel(gPointClampSampler, sampleUv, 0.0);
        float sampleDepth = gAmbientOcclusionDepth.SampleLevel(gPointClampSampler, sampleUv, 0.0);
        float weight = saturate(1.0 - abs(sampleDepth - viewDepth) / max(viewDepth * 0.1, 1e-3));
        total += ao * weight;
        weightSum += weight;
    }
    if (weightSum <= 1e-4) {
        // Every neighbour lies across an edge: fall back to the nearest texel rather than to white.
        return gAmbientOcclusion.SampleLevel(gPointClampSampler, uv, 0.0);
    }
    return total / weightSum;
}

float4 ps_main(forward_output input, bool isFrontFace : SV_IsFrontFace) : SV_Target0 {
    float4 baseColour = material_base_colour(input.uv0, input.uv1, input.colour);
#if ALPHA_MASK
    if (baseColour.a < material_alpha_cutoff()) {
        discard;
    }
    baseColour.a = 1.0;
#endif
    // The ambient-occlusion debug view (render_view::debugMode AMBIENT_OCCLUSION): the screen-space term
    // alone, as grey, without the material's occlusion and before the unlit branch so every surface shows
    // it. The flag is the same for every pixel of a draw, so the branch is uniform and needs no permutation.
    if ((gView.flags.x & CONJURE_VIEW_FLAG_DEBUG_AO) != 0) {
        float occlusionTerm = sample_ambient_occlusion(input.position.xy, input.viewDepth);
        return float4(occlusionTerm, occlusionTerm, occlusionTerm, 1.0);
    }
    float3 emissive = gMaterial.emissive.rgb * gMaterial.emissive.w;
    if (material_has_texture(CONJURE_TEXTURE_EMISSIVE)) {
        emissive *= gEmissiveTexture.Sample(gMaterialSampler, material_uv(4, input.uv0, input.uv1)).rgb;
    }
    if ((gMaterial.flags.y & CONJURE_MATERIAL_UNLIT) != 0) {
        return float4(baseColour.rgb + emissive, baseColour.a);
    }

    float metallic = gMaterial.params.x;
    float roughness = gMaterial.params.y;
    if (material_has_texture(CONJURE_TEXTURE_METALLIC_ROUGHNESS)) {
        float4 packed = gMetallicRoughnessTexture.Sample(gMaterialSampler, material_uv(1, input.uv0, input.uv1));
        roughness *= packed.g;
        metallic *= packed.b;
    }
    float occlusion = 1.0;
    if (material_has_texture(CONJURE_TEXTURE_OCCLUSION)) {
        float sampled = gOcclusionTexture.Sample(gMaterialSampler, material_uv(3, input.uv0, input.uv1)).r;
        occlusion = lerp(1.0, sampled, gMaterial.params.w);
    }

    surface_params surface = make_surface(baseColour.rgb, metallic, roughness);
    float3 normal = shading_normal(input, isFrontFace);
    float3 view = normalize(gView.cameraPosition.xyz - input.worldPosition);
    float2 pixel = input.position.xy;
    bool receivesShadows = (input.instanceFlags & CONJURE_INSTANCE_RECEIVES_SHADOWS) != 0 && (gView.flags.x & CONJURE_VIEW_FLAG_SHADOWS) != 0;

    float3 colour = float3(0.0, 0.0, 0.0);
    uint directionalCount = min(gView.counts.w, gView.counts.z);
    for (uint directional = 0; directional < directionalCount; ++directional) {
        gpu_light light = gLights[directional];
        float shadow = 1.0;
        if (receivesShadows && directional == gView.flags.w) {
            shadow = directional_shadow(input.worldPosition, normal, input.viewDepth, pixel, light);
        }
        colour += shade_light(surface, input.worldPosition, normal, view, light, shadow);
    }

    uint2 tile = uint2(pixel) / CONJURE_LIGHT_TILE_SIZE;
    uint tileIndex = tile.y * gView.counts.x + tile.x;
    uint listBase = tileIndex * CONJURE_TILE_LIST_STRIDE + (((gDrawFlags & CONJURE_DRAW_FLAG_LIGHT_LIST_B) != 0) ? CONJURE_TILE_LIST_B_OFFSET : 0);
    // The tile lists are stale when no local light exists (the culling pass did not run).
    uint localCount = gView.counts.z > gView.counts.w ? min(gTileLists[listBase], CONJURE_MAX_LIGHTS_PER_TILE) : 0;
    for (uint local = 0; local < localCount; ++local) {
        uint lightIndex = gTileLists[listBase + 1 + local];
        gpu_light light = gLights[lightIndex];
        float shadow = receivesShadows ? local_shadow(input.worldPosition, normal, pixel, light) : 1.0;
        colour += shade_light(surface, input.worldPosition, normal, view, light, shadow);
    }

    // Ambient: SH9 diffuse and split-sum specular, scaled by SSAO and the material's occlusion.
    float ambientOcclusion = sample_ambient_occlusion(pixel, input.viewDepth) * occlusion;
    float3 skyNormal = rotate_y(normal, -gView.environmentParams.y);
    float3 irradiance = sh9_irradiance(skyNormal) * gView.environmentParams.x;
    float nDotV = max(dot(normal, view), 1e-4);
    float3 reflected = rotate_y(reflect(-view, normal), -gView.environmentParams.y);
    float3 prefiltered = gPrefiltered.SampleLevel(gLinearClampSampler, reflected, surface.roughness * gView.environmentParams.z).rgb *
      gView.environmentParams.x;
    float2 lut = gBrdfLut.SampleLevel(gLinearClampSampler, float2(nDotV, surface.roughness), 0.0);
    float3 ambient = surface.diffuseColour * irradiance / CONJURE_PI + ambient_specular(surface, prefiltered, lut);
    colour += ambient * ambientOcclusion;
    colour += emissive;
    return float4(colour, baseColour.a);
}
