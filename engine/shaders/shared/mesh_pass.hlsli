// Resources and vertex layout shared by the mesh passes (shadow_depth, depth_prepass, forward_pbr).
// Register space 0 is the per-view binding set (binding_layouts.cpp: mesh_view_layout), space 1 the
// per-material set cached on the GPU material (material_layout). Vertex semantics match the two
// stream layout of mesh_asset.h: stream 0 POSITION, stream 1 NORMAL TANGENT TEXCOORD LIGHTMAP COLOR.

#ifndef CONJURE_MESH_PASS_HLSLI
#define CONJURE_MESH_PASS_HLSLI

#include "gpu_types.hlsli"

StructuredBuffer<gpu_instance> gInstances : register(t0, space0);
StructuredBuffer<gpu_light> gLights : register(t1, space0);
StructuredBuffer<gpu_shadow_tile> gShadowTiles : register(t2, space0);
Texture2DArray<float> gShadowCascades : register(t3, space0);
Texture2D<float> gShadowAtlas : register(t4, space0);
StructuredBuffer<uint> gTileLists : register(t5, space0);
Texture2D<float> gAmbientOcclusion : register(t6, space0);
TextureCube gPrefiltered : register(t7, space0);
Texture2D<float2> gBrdfLut : register(t8, space0);
Texture2D<float> gAmbientOcclusionDepth : register(t9, space0);   // half-resolution linear view depth of the AO
SamplerComparisonState gShadowSampler : register(s1, space0);
SamplerState gLinearClampSampler : register(s2, space0);
SamplerState gPointClampSampler : register(s3, space0);

cbuffer material_constants : register(b0, space1) {
    gpu_material_constants gMaterial;
};
Texture2D gBaseColourTexture : register(t0, space1);
Texture2D gMetallicRoughnessTexture : register(t1, space1);
Texture2D gNormalTexture : register(t2, space1);
Texture2D gOcclusionTexture : register(t3, space1);
Texture2D gEmissiveTexture : register(t4, space1);
SamplerState gMaterialSampler : register(s0, space1);

struct mesh_vertex {
    float3 position : POSITION;
    float4 normal : NORMAL;         // xyz normal, w tangent sign
    float4 tangent : TANGENT;
    float2 uv0 : TEXCOORD;
    float2 uv1 : LIGHTMAP;
    float4 colour : COLOR;
};

struct mesh_vertex_position {
    float3 position : POSITION;
};

#define CONJURE_TEXTURE_BASE_COLOUR 1
#define CONJURE_TEXTURE_METALLIC_ROUGHNESS 2
#define CONJURE_TEXTURE_NORMAL 4
#define CONJURE_TEXTURE_OCCLUSION 8
#define CONJURE_TEXTURE_EMISSIVE 16

bool material_has_texture(uint bit) {
    return (gMaterial.flags.x & bit) != 0;
}

float2 material_uv(uint slot, float2 uv0, float2 uv1) {
    return ((gMaterial.flags.z >> slot) & 1) != 0 ? uv1 : uv0;
}

float material_alpha_cutoff() {
    return asfloat(gMaterial.flags.w);
}

// Base colour with the factor applied; alpha included.
float4 material_base_colour(float2 uv0, float2 uv1, float4 vertexColour) {
    float4 colour = gMaterial.baseColourFactor * vertexColour;
    if (material_has_texture(CONJURE_TEXTURE_BASE_COLOUR)) {
        colour *= gBaseColourTexture.Sample(gMaterialSampler, material_uv(0, uv0, uv1));
    }
    return colour;
}

#endif // CONJURE_MESH_PASS_HLSLI
