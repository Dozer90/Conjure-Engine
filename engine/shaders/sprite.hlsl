// Sprites (docs/design/02_renderer.md R12, 3.9): WORLD sprites are world-space quads projected with
// the view's clipFromWorld; SCREEN sprites are pixel-space quads (origin top-left, y down) projected
// with an orthographic matrix built from the viewport. Colours are premultiplied here; the pipeline
// blends One / InvSrcAlpha. gDrawFlags selects the space and the sampler (linear or point).

#include "gpu_types.hlsli"

Texture2D gSpriteTexture : register(t0, space0);
SamplerState gLinearSampler : register(s0, space0);
SamplerState gPointSampler : register(s1, space0);

struct sprite_vertex_input {
    float3 position : POSITION;
    float2 uv : TEXCOORD;
    uint colour : COLOR;
};

struct sprite_output {
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
    float4 colour : COLOR;
};

sprite_output vs_main(sprite_vertex_input vertex) {
    sprite_output output;
    if ((gDrawFlags & CONJURE_DRAW_FLAG_SCREEN_SPACE) != 0) {
        float2 ndc = float2(vertex.position.x * gView.viewportSize.z * 2.0 - 1.0, 1.0 - vertex.position.y * gView.viewportSize.w * 2.0);
        output.position = float4(ndc, 0.5, 1.0);
    } else {
        output.position = mul(float4(vertex.position, 1.0), gView.clipFromWorld);
    }
    output.uv = vertex.uv;
    output.colour = unpack_rgba8(vertex.colour);
    return output;
}

float4 ps_main(sprite_output input) : SV_Target0 {
    float4 texel = ((gDrawFlags & CONJURE_DRAW_FLAG_POINT_SAMPLER) != 0)
      ? gSpriteTexture.Sample(gPointSampler, input.uv)
      : gSpriteTexture.Sample(gLinearSampler, input.uv);
    float4 colour = texel * input.colour;
    return float4(colour.rgb * colour.a, colour.a);
}
