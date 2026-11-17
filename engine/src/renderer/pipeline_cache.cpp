#include "renderer/pipeline_cache.h"
#include "renderer/gpu_types.h"
#include "renderer/shader_program_loader.h"
#include "conjure/renderer/debug_draw.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/renderer_error.h"
#include "conjure/renderer/sprite_batch.h"

#include "rhi/texture_format_mapping.h"
#include "conjure/core/error.h"

#include <cstddef>
#include <string_view>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    // Shadow map bias (design 3.4: 2 units and 2x slope), negated for reverse-Z: the rasteriser adds
    // the bias to the stored depth, and under near = 1 a stored occluder must move away from the
    // light (smaller depth) to stop self-shadowing, not toward it.
    constexpr int SHADOW_DEPTH_BIAS = -2;
    constexpr f32 SHADOW_SLOPE_SCALED_BIAS = -2.0f;

    [[nodiscard]] nvrhi::DepthStencilState depth_state(bool test, bool write, nvrhi::ComparisonFunc function) {
        nvrhi::DepthStencilState state = {};
        state.depthTestEnable = test;
        state.depthWriteEnable = write;
        state.depthFunc = function;
        state.stencilEnable = false;
        return state;
    }

    [[nodiscard]] nvrhi::BlendState::RenderTarget premultiplied_blend() {
        nvrhi::BlendState::RenderTarget target = {};
        target.blendEnable = true;
        target.srcBlend = nvrhi::BlendFactor::One;
        target.destBlend = nvrhi::BlendFactor::InvSrcAlpha;
        target.blendOp = nvrhi::BlendOp::Add;
        target.srcBlendAlpha = nvrhi::BlendFactor::One;
        target.destBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;
        target.blendOpAlpha = nvrhi::BlendOp::Add;
        return target;
    }

    [[nodiscard]] nvrhi::BlendState::RenderTarget alpha_blend() {
        nvrhi::BlendState::RenderTarget target = {};
        target.blendEnable = true;
        target.srcBlend = nvrhi::BlendFactor::SrcAlpha;
        target.destBlend = nvrhi::BlendFactor::InvSrcAlpha;
        target.blendOp = nvrhi::BlendOp::Add;
        target.srcBlendAlpha = nvrhi::BlendFactor::One;
        target.destBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;
        target.blendOpAlpha = nvrhi::BlendOp::Add;
        return target;
    }

    /// <summary>
    /// What shader_asset::find_built_in takes: the identity of one program of the built-in table.
    /// </summary>
    struct program_key {
        std::string_view name = "";
        std::string_view pixelDefines = "";
        std::string_view vertexDefines = "";
    };

    constexpr program_key BLIT_PROGRAM = {"blit", "", ""};

    [[nodiscard]] program_key key_of(graphics_variant variant) noexcept {
        switch (variant) {
            case graphics_variant::SHADOW_DEPTH:
                return {"shadow_depth", "", "ALPHA_MASK=0"};
            case graphics_variant::SHADOW_DEPTH_MASKED:
                return {"shadow_depth", "ALPHA_MASK=1", "ALPHA_MASK=1"};
            case graphics_variant::DEPTH_PREPASS:
                return {"depth_prepass", "ALPHA_MASK=0", ""};
            case graphics_variant::DEPTH_PREPASS_MASKED:
                return {"depth_prepass", "ALPHA_MASK=1", ""};
            case graphics_variant::FORWARD_OPAQUE:
            case graphics_variant::FORWARD_BLEND:
                return {"forward_pbr", "ALPHA_MASK=0", ""};
            case graphics_variant::FORWARD_MASKED:
                return {"forward_pbr", "ALPHA_MASK=1", ""};
            case graphics_variant::SKYBOX:
                return {"skybox", "", ""};
            case graphics_variant::WORLD_SPRITES:
            case graphics_variant::SCREEN_SPRITES:
                return {"sprite", "", ""};
            case graphics_variant::DEBUG_DEPTH_TESTED:
            case graphics_variant::DEBUG_OVERLAY:
                return {"debug_draw", "", ""};
            case graphics_variant::TONEMAP:
                return {"tonemap", "", ""};
            case graphics_variant::BRDF_LUT:
                return {"ibl_brdf_lut", "", ""};
            case graphics_variant::COUNT:
                break;
        }
        return {};
    }

    [[nodiscard]] program_key key_of(compute_variant variant) noexcept {
        switch (variant) {
            case compute_variant::LIGHT_CULL:
                return {"light_cull", "", ""};
            case compute_variant::SSAO_GTAO:
                return {"ssao_gtao", "", ""};
            case compute_variant::SSAO_BLUR:
                return {"ssao_blur", "", ""};
            case compute_variant::EXPOSURE_HISTOGRAM:
                return {"exposure_histogram", "", ""};
            case compute_variant::EXPOSURE_ADAPT:
                return {"exposure_adapt", "", ""};
            case compute_variant::BLOOM_DOWN:
                return {"bloom_down", "", ""};
            case compute_variant::BLOOM_UP:
                return {"bloom_up", "", ""};
            case compute_variant::COUNT:
                break;
        }
        return {};
    }

    /// <summary>
    /// The program of a key, checked whole against the shader library (resolve_built_in_program).
    /// </summary>
    [[nodiscard]] core::result<shader_asset> resolve(rhi::shader_library &shaders, const program_key &key) {
        return resolve_built_in_program(shaders, key.name, key.pixelDefines, key.vertexDefines);
    }
}

const char *conjure::renderer::to_string(graphics_variant variant) noexcept {
    switch (variant) {
        case graphics_variant::SHADOW_DEPTH:
            return "SHADOW_DEPTH";
        case graphics_variant::SHADOW_DEPTH_MASKED:
            return "SHADOW_DEPTH_MASKED";
        case graphics_variant::DEPTH_PREPASS:
            return "DEPTH_PREPASS";
        case graphics_variant::DEPTH_PREPASS_MASKED:
            return "DEPTH_PREPASS_MASKED";
        case graphics_variant::FORWARD_OPAQUE:
            return "FORWARD_OPAQUE";
        case graphics_variant::FORWARD_MASKED:
            return "FORWARD_MASKED";
        case graphics_variant::FORWARD_BLEND:
            return "FORWARD_BLEND";
        case graphics_variant::SKYBOX:
            return "SKYBOX";
        case graphics_variant::WORLD_SPRITES:
            return "WORLD_SPRITES";
        case graphics_variant::SCREEN_SPRITES:
            return "SCREEN_SPRITES";
        case graphics_variant::DEBUG_DEPTH_TESTED:
            return "DEBUG_DEPTH_TESTED";
        case graphics_variant::DEBUG_OVERLAY:
            return "DEBUG_OVERLAY";
        case graphics_variant::TONEMAP:
            return "TONEMAP";
        case graphics_variant::BRDF_LUT:
            return "BRDF_LUT";
        case graphics_variant::COUNT:
            break;
    }
    return "COUNT";
}

const char *conjure::renderer::to_string(compute_variant variant) noexcept {
    switch (variant) {
        case compute_variant::LIGHT_CULL:
            return "LIGHT_CULL";
        case compute_variant::SSAO_GTAO:
            return "SSAO_GTAO";
        case compute_variant::SSAO_BLUR:
            return "SSAO_BLUR";
        case compute_variant::EXPOSURE_HISTOGRAM:
            return "EXPOSURE_HISTOGRAM";
        case compute_variant::EXPOSURE_ADAPT:
            return "EXPOSURE_ADAPT";
        case compute_variant::BLOOM_DOWN:
            return "BLOOM_DOWN";
        case compute_variant::BLOOM_UP:
            return "BLOOM_UP";
        case compute_variant::COUNT:
            break;
    }
    return "COUNT";
}

/*===========================================================================*/
/*===========================================================================*/
// pipeline_cache

pipeline_cache::pipeline_cache(create_tag) {}

pipeline_cache::~pipeline_cache() {}

nvrhi::FramebufferInfo pipeline_cache::framebuffer_info(graphics_variant variant) noexcept {
    nvrhi::FramebufferInfo info = {};
    switch (variant) {
        case graphics_variant::SHADOW_DEPTH:
        case graphics_variant::SHADOW_DEPTH_MASKED: {
            info.depthFormat = nvrhi::Format::D32;
            break;
        }
        case graphics_variant::DEPTH_PREPASS:
        case graphics_variant::DEPTH_PREPASS_MASKED: {
            info.addColorFormat(nvrhi::Format::RG16_SNORM);
            info.depthFormat = nvrhi::Format::D32;
            break;
        }
        case graphics_variant::FORWARD_OPAQUE:
        case graphics_variant::FORWARD_MASKED:
        case graphics_variant::FORWARD_BLEND:
        case graphics_variant::SKYBOX:
        case graphics_variant::WORLD_SPRITES:
        case graphics_variant::DEBUG_DEPTH_TESTED:
        case graphics_variant::DEBUG_OVERLAY: {
            info.addColorFormat(nvrhi::Format::RGBA16_FLOAT);
            info.depthFormat = nvrhi::Format::D32;
            break;
        }
        case graphics_variant::SCREEN_SPRITES:
        case graphics_variant::TONEMAP: {
            info.addColorFormat(nvrhi::Format::SRGBA8_UNORM);
            break;
        }
        case graphics_variant::BRDF_LUT: {
            info.addColorFormat(nvrhi::Format::RG16_FLOAT);
            break;
        }
        case graphics_variant::COUNT: {
            break;
        }
    }
    return info;
}

core::result<std::unique_ptr<pipeline_cache>> pipeline_cache::create(
  nvrhi::IDevice &device,
  rhi::shader_library &shaders,
  const binding_layouts &layouts
) {
    std::unique_ptr<pipeline_cache> cache = std::make_unique<pipeline_cache>(create_tag{});
    cache->mDevice = &device;
    cache->mShaders = &shaders;
    cache->mLayouts = &layouts;
    CONJURE_TRY(cache->create_input_layouts());
    for (usize index = 0; index < GRAPHICS_COUNT; ++index) {
        CONJURE_TRY(cache->create_graphics(static_cast<graphics_variant>(index)));
    }
    for (usize index = 0; index < COMPUTE_COUNT; ++index) {
        CONJURE_TRY(cache->create_compute(static_cast<compute_variant>(index)));
    }
    CONJURE_TRY_ASSIGN(const shader_asset blitProgram, resolve(shaders, BLIT_PROGRAM));
    CONJURE_TRY_ASSIGN(
      cache->mBlitVertex,
      create_program_shader(device, shaders, blitProgram, nvrhi::ShaderType::Vertex)
    );
    CONJURE_TRY_ASSIGN(
      cache->mBlitPixel,
      create_program_shader(device, shaders, blitProgram, nvrhi::ShaderType::Pixel)
    );
    return cache;
}

core::result<shader_asset> pipeline_cache::program_of(graphics_variant variant) {
    const program_key key = key_of(variant);
    return shader_asset::find_built_in(key.name, key.pixelDefines, key.vertexDefines);
}

core::result<shader_asset> pipeline_cache::program_of(compute_variant variant) {
    const program_key key = key_of(variant);
    return shader_asset::find_built_in(key.name, key.pixelDefines, key.vertexDefines);
}

core::result<shader_asset> pipeline_cache::blit_program() {
    return shader_asset::find_built_in(BLIT_PROGRAM.name, BLIT_PROGRAM.pixelDefines, BLIT_PROGRAM.vertexDefines);
}

core::result<nvrhi::ShaderHandle> pipeline_cache::create_vertex_shader(graphics_variant variant) {
    CONJURE_TRY_ASSIGN(const shader_asset program, resolve(*mShaders, key_of(variant)));
    return create_program_shader(*mDevice, *mShaders, program, nvrhi::ShaderType::Vertex);
}

core::result<void> pipeline_cache::create_input_layouts() {
    // The input layout needs a vertex shader for reflection on some backends; each layout takes the
    // one of a variant that draws with it. The forward vertex shader covers every mesh attribute.
    nvrhi::ShaderHandle meshVertex = nullptr;
    CONJURE_TRY_ASSIGN(meshVertex, create_vertex_shader(graphics_variant::FORWARD_OPAQUE));
    const std::array<nvrhi::VertexAttributeDesc, 6> meshAttributes = {
      nvrhi::VertexAttributeDesc()
        .setName("POSITION")
        .setFormat(nvrhi::Format::RGB32_FLOAT)
        .setBufferIndex(0)
        .setOffset(0)
        .setElementStride(sizeof(math::vec3)),
      nvrhi::VertexAttributeDesc()
        .setName("NORMAL")
        .setFormat(nvrhi::Format::RGBA16_SNORM)
        .setBufferIndex(1)
        .setOffset(offsetof(packed_vertex_attributes, normal))
        .setElementStride(sizeof(packed_vertex_attributes)),
      nvrhi::VertexAttributeDesc()
        .setName("TANGENT")
        .setFormat(nvrhi::Format::RGBA16_SNORM)
        .setBufferIndex(1)
        .setOffset(offsetof(packed_vertex_attributes, tangent))
        .setElementStride(sizeof(packed_vertex_attributes)),
      nvrhi::VertexAttributeDesc()
        .setName("TEXCOORD")
        .setFormat(nvrhi::Format::RG32_FLOAT)
        .setBufferIndex(1)
        .setOffset(offsetof(packed_vertex_attributes, uv0))
        .setElementStride(sizeof(packed_vertex_attributes)),
      nvrhi::VertexAttributeDesc()
        .setName("LIGHTMAP")
        .setFormat(nvrhi::Format::RG16_FLOAT)
        .setBufferIndex(1)
        .setOffset(offsetof(packed_vertex_attributes, uv1))
        .setElementStride(sizeof(packed_vertex_attributes)),
      nvrhi::VertexAttributeDesc()
        .setName("COLOR")
        .setFormat(nvrhi::Format::RGBA8_UNORM)
        .setBufferIndex(1)
        .setOffset(offsetof(packed_vertex_attributes, colour))
        .setElementStride(sizeof(packed_vertex_attributes)),
    };
    mMeshLayout = mDevice->createInputLayout(
      meshAttributes.data(),
      static_cast<u32>(meshAttributes.size()),
      meshVertex.Get()
    );
    if (mMeshLayout == nullptr) {
        return core::make_error(
          renderer_error::PIPELINE_CREATION_FAILED,
          "the mesh input layout was refused by the device"
        );
    }

    nvrhi::ShaderHandle shadowVertex = nullptr;
    CONJURE_TRY_ASSIGN(shadowVertex, create_vertex_shader(graphics_variant::SHADOW_DEPTH));
    mPositionLayout = mDevice->createInputLayout(meshAttributes.data(), 1, shadowVertex.Get());
    if (mPositionLayout == nullptr) {
        return core::make_error(
          renderer_error::PIPELINE_CREATION_FAILED,
          "the position-only input layout was refused by the device"
        );
    }

    nvrhi::ShaderHandle spriteVertex = nullptr;
    CONJURE_TRY_ASSIGN(spriteVertex, create_vertex_shader(graphics_variant::WORLD_SPRITES));
    const std::array<nvrhi::VertexAttributeDesc, 3> spriteAttributes = {
      nvrhi::VertexAttributeDesc()
        .setName("POSITION")
        .setFormat(nvrhi::Format::RGB32_FLOAT)
        .setOffset(offsetof(sprite_vertex, position))
        .setElementStride(sizeof(sprite_vertex)),
      nvrhi::VertexAttributeDesc()
        .setName("TEXCOORD")
        .setFormat(nvrhi::Format::RG32_FLOAT)
        .setOffset(offsetof(sprite_vertex, uv))
        .setElementStride(sizeof(sprite_vertex)),
      nvrhi::VertexAttributeDesc()
        .setName("COLOR")
        .setFormat(nvrhi::Format::R32_UINT)
        .setOffset(offsetof(sprite_vertex, colour))
        .setElementStride(sizeof(sprite_vertex)),
    };
    mSpriteLayout = mDevice->createInputLayout(
      spriteAttributes.data(),
      static_cast<u32>(spriteAttributes.size()),
      spriteVertex.Get()
    );
    if (mSpriteLayout == nullptr) {
        return core::make_error(
          renderer_error::PIPELINE_CREATION_FAILED,
          "the sprite input layout was refused by the device"
        );
    }

    nvrhi::ShaderHandle debugVertex = nullptr;
    CONJURE_TRY_ASSIGN(debugVertex, create_vertex_shader(graphics_variant::DEBUG_DEPTH_TESTED));
    const std::array<nvrhi::VertexAttributeDesc, 2> debugAttributes = {
      nvrhi::VertexAttributeDesc()
        .setName("POSITION")
        .setFormat(nvrhi::Format::RGB32_FLOAT)
        .setOffset(offsetof(debug_vertex, position))
        .setElementStride(sizeof(debug_vertex)),
      nvrhi::VertexAttributeDesc()
        .setName("COLOR")
        .setFormat(nvrhi::Format::R32_UINT)
        .setOffset(offsetof(debug_vertex, colour))
        .setElementStride(sizeof(debug_vertex)),
    };
    mDebugLayout = mDevice->createInputLayout(
      debugAttributes.data(),
      static_cast<u32>(debugAttributes.size()),
      debugVertex.Get()
    );
    if (mDebugLayout == nullptr) {
        return core::make_error(
          renderer_error::PIPELINE_CREATION_FAILED,
          "the debug input layout was refused by the device"
        );
    }
    return core::result<void>();
}

core::result<void> pipeline_cache::create_graphics(graphics_variant variant) {
    CONJURE_TRY_ASSIGN(const shader_asset program, resolve(*mShaders, key_of(variant)));
    nvrhi::GraphicsPipelineDesc desc = {};
    CONJURE_TRY_ASSIGN(desc.VS, create_program_shader(*mDevice, *mShaders, program, nvrhi::ShaderType::Vertex));
    // A depth-only program (the unmasked shadow pass) has no pixel stage.
    if (!program.desc().pixelEntry.empty()) {
        CONJURE_TRY_ASSIGN(desc.PS, create_program_shader(*mDevice, *mShaders, program, nvrhi::ShaderType::Pixel));
    }
    desc.primType = nvrhi::PrimitiveType::TriangleList;
    desc.renderState.rasterState.frontCounterClockwise = true;
    desc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::Back;
    desc.renderState.rasterState.depthClipEnable = true;
    desc.renderState.rasterState.scissorEnable = true;
    desc.renderState.depthStencilState = depth_state(true, true, nvrhi::ComparisonFunc::GreaterOrEqual);

    const bool mesh = is_mesh_variant(variant);
    if (mesh) {
        desc.bindingLayouts.push_back(mLayouts->meshView);
        desc.bindingLayouts.push_back(mLayouts->material);
    } else {
        desc.bindingLayouts.push_back(mLayouts->fullscreen);
    }

    switch (variant) {
        case graphics_variant::SHADOW_DEPTH: {
            desc.inputLayout = mPositionLayout;
            desc.renderState.rasterState.depthBias = SHADOW_DEPTH_BIAS;
            desc.renderState.rasterState.slopeScaledDepthBias = SHADOW_SLOPE_SCALED_BIAS;
            break;
        }
        case graphics_variant::SHADOW_DEPTH_MASKED: {
            desc.inputLayout = mMeshLayout;
            desc.renderState.rasterState.depthBias = SHADOW_DEPTH_BIAS;
            desc.renderState.rasterState.slopeScaledDepthBias = SHADOW_SLOPE_SCALED_BIAS;
            break;
        }
        case graphics_variant::DEPTH_PREPASS:
        case graphics_variant::DEPTH_PREPASS_MASKED: {
            desc.inputLayout = mMeshLayout;
            break;
        }
        case graphics_variant::FORWARD_OPAQUE:
        case graphics_variant::FORWARD_MASKED: {
            desc.inputLayout = mMeshLayout;
            // Depth was laid down by the prepass: equal test, no write.
            desc.renderState.depthStencilState = depth_state(true, false, nvrhi::ComparisonFunc::Equal);
            break;
        }
        case graphics_variant::FORWARD_BLEND: {
            desc.inputLayout = mMeshLayout;
            desc.renderState.depthStencilState = depth_state(true, false, nvrhi::ComparisonFunc::GreaterOrEqual);
            desc.renderState.blendState.targets[0] = alpha_blend();
            break;
        }
        case graphics_variant::SKYBOX: {
            desc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
            desc.renderState.depthStencilState = depth_state(true, false, nvrhi::ComparisonFunc::GreaterOrEqual);
            break;
        }
        case graphics_variant::WORLD_SPRITES: {
            desc.inputLayout = mSpriteLayout;
            desc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
            desc.renderState.depthStencilState = depth_state(true, false, nvrhi::ComparisonFunc::GreaterOrEqual);
            desc.renderState.blendState.targets[0] = premultiplied_blend();
            break;
        }
        case graphics_variant::SCREEN_SPRITES: {
            desc.inputLayout = mSpriteLayout;
            desc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
            desc.renderState.depthStencilState = depth_state(false, false, nvrhi::ComparisonFunc::Always);
            desc.renderState.blendState.targets[0] = premultiplied_blend();
            break;
        }
        case graphics_variant::DEBUG_DEPTH_TESTED: {
            desc.inputLayout = mDebugLayout;
            desc.primType = nvrhi::PrimitiveType::LineList;
            desc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
            desc.renderState.depthStencilState = depth_state(true, false, nvrhi::ComparisonFunc::GreaterOrEqual);
            desc.renderState.blendState.targets[0] = alpha_blend();
            break;
        }
        case graphics_variant::DEBUG_OVERLAY: {
            desc.inputLayout = mDebugLayout;
            desc.primType = nvrhi::PrimitiveType::LineList;
            desc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
            desc.renderState.depthStencilState = depth_state(false, false, nvrhi::ComparisonFunc::Always);
            desc.renderState.blendState.targets[0] = alpha_blend();
            break;
        }
        case graphics_variant::TONEMAP:
        case graphics_variant::BRDF_LUT: {
            desc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
            desc.renderState.depthStencilState = depth_state(false, false, nvrhi::ComparisonFunc::Always);
            break;
        }
        case graphics_variant::COUNT: {
            break;
        }
    }

    const nvrhi::FramebufferInfo info = framebuffer_info(variant);
    const usize index = static_cast<usize>(variant);
    mGraphics[index] = mDevice->createGraphicsPipeline(desc, info);
    if (mGraphics[index] == nullptr) {
        return core::make_error(
          renderer_error::PIPELINE_CREATION_FAILED,
          "pipeline {} was refused by the device",
          to_string(variant)
        );
    }
    ++mPipelinesCreated;
    if (mesh) {
        desc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
        mGraphicsDoubleSided[index] = mDevice->createGraphicsPipeline(desc, info);
        if (mGraphicsDoubleSided[index] == nullptr) {
            return core::make_error(
              renderer_error::PIPELINE_CREATION_FAILED,
              "pipeline {} (double-sided) was refused by the device",
              to_string(variant)
            );
        }
        ++mPipelinesCreated;
    }
    return core::result<void>();
}

core::result<void> pipeline_cache::create_compute(compute_variant variant) {
    CONJURE_TRY_ASSIGN(const shader_asset program, resolve(*mShaders, key_of(variant)));
    nvrhi::ComputePipelineDesc desc = {};
    CONJURE_TRY_ASSIGN(desc.CS, create_program_shader(*mDevice, *mShaders, program, nvrhi::ShaderType::Compute));
    switch (variant) {
        case compute_variant::LIGHT_CULL: {
            desc.bindingLayouts.push_back(mLayouts->lightCull);
            break;
        }
        case compute_variant::SSAO_GTAO: {
            desc.bindingLayouts.push_back(mLayouts->ssaoGtao);
            break;
        }
        case compute_variant::SSAO_BLUR: {
            desc.bindingLayouts.push_back(mLayouts->ssao);
            break;
        }
        case compute_variant::EXPOSURE_HISTOGRAM: {
            desc.bindingLayouts.push_back(mLayouts->exposureHistogram);
            break;
        }
        case compute_variant::EXPOSURE_ADAPT: {
            desc.bindingLayouts.push_back(mLayouts->exposureAdapt);
            break;
        }
        case compute_variant::BLOOM_DOWN:
        case compute_variant::BLOOM_UP: {
            desc.bindingLayouts.push_back(mLayouts->bloom);
            break;
        }
        case compute_variant::COUNT: {
            break;
        }
    }
    const usize index = static_cast<usize>(variant);
    mCompute[index] = mDevice->createComputePipeline(desc);
    if (mCompute[index] == nullptr) {
        return core::make_error(
          renderer_error::PIPELINE_CREATION_FAILED,
          "compute pipeline {} was refused by the device",
          to_string(variant)
        );
    }
    ++mPipelinesCreated;
    return core::result<void>();
}

nvrhi::IGraphicsPipeline *pipeline_cache::graphics(graphics_variant variant, bool doubleSided) const noexcept {
    if (variant >= graphics_variant::COUNT) {
        return nullptr;
    }
    const usize index = static_cast<usize>(variant);
    if (doubleSided && is_mesh_variant(variant)) {
        return mGraphicsDoubleSided[index].Get();
    }
    return mGraphics[index].Get();
}

nvrhi::IComputePipeline *pipeline_cache::compute(compute_variant variant) const noexcept {
    if (variant >= compute_variant::COUNT) {
        return nullptr;
    }
    return mCompute[static_cast<usize>(variant)].Get();
}

bool pipeline_cache::has_blit(rhi::texture_format targetFormat) const noexcept {
    return mBlit.contains(static_cast<u8>(targetFormat));
}

core::result<nvrhi::IGraphicsPipeline *> pipeline_cache::blit(rhi::texture_format targetFormat) {
    const auto found = mBlit.find(static_cast<u8>(targetFormat));
    if (found != mBlit.end()) {
        return found->second.Get();
    }
    nvrhi::GraphicsPipelineDesc desc = {};
    desc.VS = mBlitVertex;
    desc.PS = mBlitPixel;
    desc.primType = nvrhi::PrimitiveType::TriangleList;
    desc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
    desc.renderState.rasterState.scissorEnable = true;
    desc.renderState.depthStencilState = depth_state(false, false, nvrhi::ComparisonFunc::Always);
    desc.bindingLayouts.push_back(mLayouts->fullscreen);
    nvrhi::FramebufferInfo info = {};
    info.addColorFormat(rhi::to_nvrhi_format(targetFormat));
    nvrhi::GraphicsPipelineHandle pipeline = mDevice->createGraphicsPipeline(desc, info);
    if (pipeline == nullptr) {
        return core::make_error(
          renderer_error::PIPELINE_CREATION_FAILED,
          "blit pipeline for {} was refused by the device",
          rhi::to_string(targetFormat)
        );
    }
    ++mPipelinesCreated;
    nvrhi::IGraphicsPipeline *raw = pipeline.Get();
    mBlit.emplace(static_cast<u8>(targetFormat), std::move(pipeline));
    return raw;
}
