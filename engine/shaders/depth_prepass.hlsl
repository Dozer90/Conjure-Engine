// Depth + normal prepass (docs/design/02_renderer.md R1, 3.3): reverse-Z depth and an octahedral
// view-space normal (RG16_SNORM) for the GTAO pass and the light culler.

#include "mesh_pass.hlsli"
#include "octahedral.hlsli"

struct prepass_output {
    float4 position : SV_Position;
    float3 worldNormal : NORMAL;
    float2 uv0 : TEXCOORD;
    float2 uv1 : LIGHTMAP;
    float4 colour : COLOR;
};

prepass_output vs_main(mesh_vertex vertex, uint instanceId : SV_InstanceID) {
    gpu_instance instance = gInstances[gDrawFirstInstance + instanceId];
    prepass_output output;
    float3 worldPosition = transform_point(instance, vertex.position);
    output.position = mul(float4(worldPosition, 1.0), gView.clipFromWorld);
    output.worldNormal = transform_normal(instance, vertex.normal.xyz);
    output.uv0 = vertex.uv0;
    output.uv1 = vertex.uv1;
    output.colour = vertex.colour;
    return output;
}

float2 ps_main(prepass_output input, bool isFrontFace : SV_IsFrontFace) : SV_Target0 {
#if ALPHA_MASK
    float alpha = material_base_colour(input.uv0, input.uv1, input.colour).a;
    if (alpha < material_alpha_cutoff()) {
        discard;
    }
#endif
    float3 normal = normalize(input.worldNormal);
    if (!isFrontFace) {
        normal = -normal;
    }
    float3 viewNormal = normalize(mul(float4(normal, 0.0), gView.viewFromWorld).xyz);
    return octahedral_encode(viewNormal);
}
