// Debug lines (docs/design/02_renderer.md 3.10): world-space line vertices with RGBA8 colours, drawn
// depth-tested and again as an overlay.

#include "gpu_types.hlsli"

struct debug_vertex_input {
    float3 position : POSITION;
    uint colour : COLOR;
};

struct debug_output {
    float4 position : SV_Position;
    float4 colour : COLOR;
};

debug_output vs_main(debug_vertex_input vertex) {
    debug_output output;
    output.position = mul(float4(vertex.position, 1.0), gView.clipFromWorld);
    output.colour = unpack_rgba8(vertex.colour);
    return output;
}

float4 ps_main(debug_output input) : SV_Target0 {
    return input.colour;
}
