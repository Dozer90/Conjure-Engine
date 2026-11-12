// GTAO (docs/design/02_renderer.md R10, 3.7): half-resolution horizon search with 2 slices x 4 steps
// per side, a 4x4 interleaved rotation pattern cycling over 16 frames, a thickness heuristic and a
// cosine-weighted visibility integral; the result is pow(visibility, intensity) in an R8 texture.

#include "gpu_types.hlsli"
#include "octahedral.hlsli"
#include "noise.hlsli"

Texture2D<float> gDepth : register(t0, space0);
Texture2D<float2> gViewNormals : register(t1, space0);
RWTexture2D<float> gAmbientOcclusion : register(u0, space0);
RWTexture2D<float> gViewDepthOut : register(u1, space0);    // linear view depth of the AO texel (bilateral upsample)
SamplerState gPointClampSampler : register(s0, space0);

#define CONJURE_GTAO_SLICES 2
#define CONJURE_GTAO_STEPS 4
#define CONJURE_GTAO_MAX_RADIUS_PIXELS 128.0

float3 view_position_at(float2 uv) {
    float depth = gDepth.SampleLevel(gPointClampSampler, uv, 0.0);
    float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    float4 view = mul(float4(ndc, depth, 1.0), gView.viewFromClip);
    return view.xyz / max(abs(view.w), 1e-6) * (view.w < 0.0 ? -1.0 : 1.0);
}

// stepUv is one step in uv per axis: the same pixel distance is a different uv distance along x and y
// on a target that is not square.
float horizon_cos(
  float3 position,
  float3 view,
  float2 uv,
  float2 direction,
  float2 stepUv,
  float thickness,
  float horizonInit
) {
    float maxCos = horizonInit;
    [unroll]
    for (int step = 1; step <= CONJURE_GTAO_STEPS; ++step) {
        float2 sampleUv = uv + direction * stepUv * float(step);
        if (sampleUv.x < 0.0 || sampleUv.y < 0.0 || sampleUv.x > 1.0 || sampleUv.y > 1.0) {
            break;
        }
        float3 samplePosition = view_position_at(sampleUv);
        float3 delta = samplePosition - position;
        float distance = length(delta);
        if (distance < 1e-5) {
            continue;
        }
        float cosHorizon = dot(delta / distance, view);
        // Thickness heuristic: distant samples fade out so thin objects do not occlude everything behind them.
        float falloff = saturate(1.0 - distance / max(gView.ssaoParams.x * (1.0 + thickness), 1e-4));
        cosHorizon = lerp(-1.0, cosHorizon, falloff);
        maxCos = max(maxCos, cosHorizon);
    }
    return maxCos;
}

[numthreads(8, 8, 1)]
void cs_main(uint3 threadId : SV_DispatchThreadID) {
    uint2 halfSize = max(uint2(gView.viewportSize.xy) / 2, uint2(1, 1));
    if (threadId.x >= halfSize.x || threadId.y >= halfSize.y) {
        return;
    }
    float2 uv = (float2(threadId.xy) + 0.5) / float2(halfSize);
    float depth = gDepth.SampleLevel(gPointClampSampler, uv, 0.0);
    if (depth <= 0.0) {
        gAmbientOcclusion[threadId.xy] = 1.0;
        gViewDepthOut[threadId.xy] = gView.nearFarExposure.y;
        return;
    }
    float3 position = view_position_at(uv);
    gViewDepthOut[threadId.xy] = -position.z;
    float3 normal = octahedral_decode(gViewNormals.SampleLevel(gPointClampSampler, uv, 0.0));
    float3 view = normalize(-position);

    // Projected radius in half-resolution pixels, clamped.
    float projectedScale = gView.clipFromView[1][1] * gView.viewportSize.y * 0.5 / max(-position.z, 1e-3);
    float radiusPixels = min(gView.ssaoParams.x * projectedScale * 0.5, CONJURE_GTAO_MAX_RADIUS_PIXELS);
    float2 stepUv = radiusPixels / float(CONJURE_GTAO_STEPS) / float2(halfSize);
    if (radiusPixels < 1.0) {
        gAmbientOcclusion[threadId.xy] = 1.0;
        return;
    }

    float frameRotation = float(gView.flags.z & 15) / 16.0;
    float noise = ordered_dither_4x4(threadId.xy) + frameRotation;
    float visibility = 0.0;
    [unroll]
    for (int slice = 0; slice < CONJURE_GTAO_SLICES; ++slice) {
        float angle = (float(slice) / float(CONJURE_GTAO_SLICES) + noise) * CONJURE_PI;
        float2 direction = float2(cos(angle), sin(angle));
        float3 sliceDirection = float3(direction.x, -direction.y, 0.0);

        // Project the normal into the slice plane for the cosine-weighted integral.
        float3 orthoDirection = sliceDirection - view * dot(sliceDirection, view);
        float3 axis = cross(orthoDirection, view);
        float3 projectedNormal = normal - axis * dot(normal, axis);
        float projectedLength = max(length(projectedNormal), 1e-4);
        float cosNormal = clamp(dot(projectedNormal / projectedLength, view), -1.0, 1.0);
        float signNormal = dot(orthoDirection, projectedNormal) < 0.0 ? -1.0 : 1.0;
        float normalAngle = signNormal * acos(cosNormal);

        float horizon0 = horizon_cos(position, view, uv, direction, stepUv, gView.ssaoParams.z, -1.0);
        float horizon1 = horizon_cos(position, view, uv, -direction, stepUv, gView.ssaoParams.z, -1.0);
        float h0 = -acos(clamp(horizon0, -1.0, 1.0));
        float h1 = acos(clamp(horizon1, -1.0, 1.0));
        h0 = normalAngle + max(h0 - normalAngle, -CONJURE_PI * 0.5);
        h1 = normalAngle + min(h1 - normalAngle, CONJURE_PI * 0.5);
        float integral0 = -cos(2.0 * h0 - normalAngle) + cosNormal + 2.0 * h0 * sin(normalAngle);
        float integral1 = -cos(2.0 * h1 - normalAngle) + cosNormal + 2.0 * h1 * sin(normalAngle);
        visibility += projectedLength * 0.25 * (integral0 + integral1);
    }
    visibility = saturate(visibility / float(CONJURE_GTAO_SLICES));
    gAmbientOcclusion[threadId.xy] = pow(visibility, gView.ssaoParams.y);
}
