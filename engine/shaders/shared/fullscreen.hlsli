// The fullscreen triangle of the post-processing and utility passes: three vertices from SV_VertexID,
// no vertex buffer. uv (0, 0) is the top-left corner of the target, so uv.y is texture v.

#ifndef CONJURE_FULLSCREEN_HLSLI
#define CONJURE_FULLSCREEN_HLSLI

struct fullscreen_output {
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

fullscreen_output fullscreen_vs(uint vertexId) {
    fullscreen_output output;
    output.uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.position = float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

#endif // CONJURE_FULLSCREEN_HLSLI
