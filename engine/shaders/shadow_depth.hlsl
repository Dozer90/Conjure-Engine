// Shadow map rendering (docs/design/02_renderer.md 3.3, 3.4): depth only into one cascade slice or
// one atlas tile. The tile's projection comes from gShadowTiles[gDrawFlags]; ALPHA_MASK=1 binds
// stream 1 as well so the pixel shader can discard masked texels. ALPHA_MASK=0 has no pixel shader.

#include "mesh_pass.hlsli"

#if ALPHA_MASK
struct shadow_output {
    float4 position : SV_Position;
    float2 uv0 : TEXCOORD;
    float2 uv1 : LIGHTMAP;
    float4 colour : COLOR;
};

shadow_output vs_main(mesh_vertex vertex, uint instanceId : SV_InstanceID) {
    gpu_instance instance = gInstances[gDrawFirstInstance + instanceId];
    gpu_shadow_tile tile = gShadowTiles[gDrawFlags];
    shadow_output output;
    output.position = mul(float4(transform_point(instance, vertex.position), 1.0), tile.clipFromWorld);
    output.uv0 = vertex.uv0;
    output.uv1 = vertex.uv1;
    output.colour = vertex.colour;
    return output;
}

void ps_main(shadow_output input) {
    float alpha = material_base_colour(input.uv0, input.uv1, input.colour).a;
    if (alpha < material_alpha_cutoff()) {
        discard;
    }
}
#else
float4 vs_main(mesh_vertex_position vertex, uint instanceId : SV_InstanceID) : SV_Position {
    gpu_instance instance = gInstances[gDrawFirstInstance + instanceId];
    gpu_shadow_tile tile = gShadowTiles[gDrawFlags];
    return mul(float4(transform_point(instance, vertex.position), 1.0), tile.clipFromWorld);
}
#endif
