// Dear ImGui backend (docs/design/02_renderer.md 3.11, R16): pixel-space vertices projected by the
// orthographic matrix in the push constants (display position and size), premultiplied alpha
// blending, one texture per draw command.

#include "common.hlsli"

CONJURE_PUSH_CONSTANTS cbuffer imgui_constants : register(b1, space0) {
    float2 gDisplayPosition;
    float2 gDisplaySize;
};

Texture2D gImGuiTexture : register(t0, space0);
SamplerState gImGuiSampler : register(s0, space0);

struct imgui_vertex_input {
    float2 position : POSITION;
    float2 uv : TEXCOORD;
    uint colour : COLOR;
};

struct imgui_output {
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
    float4 colour : COLOR;
};

imgui_output vs_main(imgui_vertex_input vertex) {
    imgui_output output;
    float2 ndc = (vertex.position - gDisplayPosition) / max(gDisplaySize, 1.0) * float2(2.0, -2.0) + float2(-1.0, 1.0);
    output.position = float4(ndc, 0.0, 1.0);
    output.uv = vertex.uv;
    output.colour = float4(vertex.colour & 0xFF, (vertex.colour >> 8) & 0xFF, (vertex.colour >> 16) & 0xFF, (vertex.colour >> 24) & 0xFF) / 255.0;
    return output;
}

float4 ps_main(imgui_output input) : SV_Target0 {
    float4 colour = input.colour * gImGuiTexture.Sample(gImGuiSampler, input.uv);
    return float4(colour.rgb * colour.a, colour.a);
}
