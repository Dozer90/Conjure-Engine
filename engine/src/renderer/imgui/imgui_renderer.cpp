#include "conjure/renderer/imgui_renderer.h"

#if defined(CONJURE_BUILD_EDITOR) && CONJURE_BUILD_EDITOR

#include "conjure/renderer/log_category.h"
#include "conjure/renderer/renderer_error.h"
#include "rhi/texture_format_mapping.h"

#include "conjure/core/containers/slot_map.h"
#include "conjure/core/error.h"

#include <imgui.h>
#include <nvrhi/nvrhi.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

// Dear ImGui backend (docs/design/02_renderer.md 3.11, R16). With ImGui >= 1.92 (IMGUI_HAS_TEXTURES)
// the backend serves the texture requests carried by the draw data; with the pinned 1.91 it uploads
// the font atlas and re-uploads it when the atlas pixels change. Vertices and indices of every draw
// list are concatenated into two growing buffers written once per frame.

using namespace conjure;
using namespace conjure::renderer;

namespace {
    constexpr u32 INITIAL_VERTEX_CAPACITY = 4096;
    constexpr u32 INITIAL_INDEX_CAPACITY = 8192;

    struct imgui_push_constants {
        f32 displayX = 0.0f;
        f32 displayY = 0.0f;
        f32 sizeX = 1.0f;
        f32 sizeY = 1.0f;
    };

    static_assert(sizeof(imgui_push_constants) == 16, "imgui push constants are 16 bytes");

    /// <summary>
    /// One texture the backend knows: owned (font atlas, protocol textures) or external (a renderer
    /// texture registered by the editor).
    /// </summary>
    struct imgui_texture {
        nvrhi::TextureHandle owned = nullptr;
        gpu_texture_id external = {};
        nvrhi::ITexture *bound = nullptr;           // the texture the binding set was built for
        nvrhi::BindingSetHandle bindingSet = nullptr;
        u32 width = 0;
        u32 height = 0;
    };

    [[nodiscard]] u32 grow(u32 current, u32 needed) noexcept {
        u32 capacity = std::max<u32>(current, 1);
        while (capacity < needed) {
            capacity *= 2;
        }
        return capacity;
    }
}

struct imgui_renderer::impl {
    nvrhi::IDevice *device = nullptr;
    rhi::texture_format targetFormat = rhi::texture_format::RGBA8_SRGB;
    imgui_texture_resolver resolver = {};
    nvrhi::BindingLayoutHandle layout = nullptr;
    nvrhi::InputLayoutHandle inputLayout = nullptr;
    nvrhi::GraphicsPipelineHandle pipeline = nullptr;
    nvrhi::SamplerHandle sampler = nullptr;
    nvrhi::BufferHandle vertices = nullptr;
    u32 vertexCapacity = 0;
    nvrhi::BufferHandle indices = nullptr;
    u32 indexCapacity = 0;
    core::slot_map<imgui_texture> textures = {};
    core::slot_key fontTexture = {};
    const void *fontPixels = nullptr;           // the atlas pixel pointer last uploaded (1.91 path)
    std::vector<ImDrawVert> vertexScratch = {};
    std::vector<ImDrawIdx> indexScratch = {};
    std::vector<std::byte> pixelScratch = {};
    imgui_render_stats stats = {};

    [[nodiscard]] nvrhi::IBindingSet *binding_set(imgui_texture &texture) {
        nvrhi::ITexture *current = texture.owned != nullptr ? texture.owned.Get() : nullptr;
        if (texture.external.is_valid() && resolver) {
            current = resolver(texture.external);
        }
        if (current == nullptr) {
            imgui_texture *font = textures.find(fontTexture);
            current = font != nullptr ? font->owned.Get() : nullptr;
        }
        if (current == nullptr) {
            return nullptr;
        }
        if (texture.bindingSet == nullptr || texture.bound != current) {
            nvrhi::BindingSetDesc desc = {};
            desc.addItem(nvrhi::BindingSetItem::PushConstants(1, sizeof(imgui_push_constants)));
            desc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, current));
            desc.addItem(nvrhi::BindingSetItem::Sampler(0, sampler));
            texture.bindingSet = device->createBindingSet(desc, layout);
            texture.bound = current;
        }
        return texture.bindingSet.Get();
    }

    [[nodiscard]] nvrhi::TextureHandle create_rgba8(u32 width, u32 height, const char *name) {
        nvrhi::TextureDesc desc = {};
        desc.width = width;
        desc.height = height;
        desc.format = nvrhi::Format::RGBA8_UNORM;
        desc.debugName = name;
        desc.isShaderResource = true;
        desc.initialState = nvrhi::ResourceStates::ShaderResource;
        desc.keepInitialState = true;
        return device->createTexture(desc);
    }

    void upload_rgba8(
      nvrhi::ICommandList &commandList,
      nvrhi::ITexture *texture,
      const void *pixels,
      u32 width,
      u32 height
    ) {
        commandList.writeTexture(
          texture,
          0,
          0,
          pixels,
          static_cast<usize>(width) * 4,
          static_cast<usize>(width) * 4 * height
        );
    }

    [[nodiscard]] bool ensure_buffers(u32 vertexCount, u32 indexCount) {
        if (vertices == nullptr || vertexCount > vertexCapacity) {
            const u32 capacity = grow(vertexCapacity, std::max(vertexCount, INITIAL_VERTEX_CAPACITY));
            nvrhi::BufferDesc desc = {};
            desc.byteSize = u64{capacity} * sizeof(ImDrawVert);
            desc.isVertexBuffer = true;
            desc.debugName = "imgui.vertices";
            desc.initialState = nvrhi::ResourceStates::VertexBuffer;
            desc.keepInitialState = true;
            vertices = device->createBuffer(desc);
            if (vertices == nullptr) {
                return false;
            }
            vertexCapacity = capacity;
        }
        if (indices == nullptr || indexCount > indexCapacity) {
            const u32 capacity = grow(indexCapacity, std::max(indexCount, INITIAL_INDEX_CAPACITY));
            nvrhi::BufferDesc desc = {};
            desc.byteSize = u64{capacity} * sizeof(ImDrawIdx);
            desc.isIndexBuffer = true;
            desc.debugName = "imgui.indices";
            desc.initialState = nvrhi::ResourceStates::IndexBuffer;
            desc.keepInitialState = true;
            indices = device->createBuffer(desc);
            if (indices == nullptr) {
                return false;
            }
            indexCapacity = capacity;
        }
        return true;
    }

#ifdef IMGUI_HAS_TEXTURES
    void update_texture(nvrhi::ICommandList &commandList, ImTextureData *texture) {
        if (texture->Status == ImTextureStatus_WantCreate) {
            imgui_texture record = {};
            record.width = static_cast<u32>(texture->Width);
            record.height = static_cast<u32>(texture->Height);
            record.owned = create_rgba8(record.width, record.height, "imgui.texture");
            if (record.owned == nullptr) {
                return;
            }
            upload_pixels(commandList, record.owned, texture);
            const core::slot_key key = textures.emplace(std::move(record));
            texture->SetTexID(static_cast<ImTextureID>(key.to_u64()));
            texture->SetStatus(ImTextureStatus_OK);
            ++stats.texturesCreated;
            return;
        }
        if (texture->Status == ImTextureStatus_WantUpdates) {
            imgui_texture *record = textures.find(core::slot_key::from_u64(static_cast<u64>(texture->TexID)));
            if (record != nullptr && record->owned != nullptr) {
                // NVRHI writes whole subresources: the full atlas is re-uploaded for any dirty rect.
                upload_pixels(commandList, record->owned, texture);
                ++stats.texturesUpdated;
            }
            texture->SetStatus(ImTextureStatus_OK);
            return;
        }
        if (texture->Status == ImTextureStatus_WantDestroy && texture->UnusedFrames > 0) {
            static_cast<void>(textures.erase(core::slot_key::from_u64(static_cast<u64>(texture->TexID))));
            texture->SetTexID(ImTextureID_Invalid);
            texture->SetStatus(ImTextureStatus_Destroyed);
            ++stats.texturesDestroyed;
        }
    }

    void upload_pixels(nvrhi::ICommandList &commandList, nvrhi::ITexture *target, ImTextureData *texture) {
        const u32 width = static_cast<u32>(texture->Width);
        const u32 height = static_cast<u32>(texture->Height);
        if (texture->Format == ImTextureFormat_RGBA32) {
            upload_rgba8(commandList, target, texture->GetPixels(), width, height);
            return;
        }
        // Alpha8 atlases expand to white with the alpha channel.
        pixelScratch.resize(static_cast<usize>(width) * height * 4);
        const unsigned char *source = static_cast<const unsigned char *>(texture->GetPixels());
        for (usize index = 0; index < static_cast<usize>(width) * height; ++index) {
            pixelScratch[index * 4] = std::byte{255};
            pixelScratch[index * 4 + 1] = std::byte{255};
            pixelScratch[index * 4 + 2] = std::byte{255};
            pixelScratch[index * 4 + 3] = static_cast<std::byte>(source[index]);
        }
        upload_rgba8(commandList, target, pixelScratch.data(), width, height);
    }
#else
    void update_font_atlas(nvrhi::ICommandList &commandList) {
        if (ImGui::GetCurrentContext() == nullptr) {
            return;
        }
        ImGuiIO &io = ImGui::GetIO();
        unsigned char *pixels = nullptr;
        int width = 0;
        int height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        if (pixels == nullptr || width <= 0 || height <= 0) {
            return;
        }
        imgui_texture *font = textures.find(fontTexture);
        const bool sizeChanged = font == nullptr || font->width != static_cast<u32>(
          width
        ) || font->height != static_cast<u32>(height);
        const bool pixelsChanged = fontPixels != pixels;
        const bool idCurrent = io.Fonts->TexID == static_cast<ImTextureID>(fontTexture.to_u64());
        if (font != nullptr && !sizeChanged && !pixelsChanged && idCurrent) {
            return;
        }
        if (font == nullptr || sizeChanged) {
            if (font != nullptr) {
                static_cast<void>(textures.erase(fontTexture));
                ++stats.texturesDestroyed;
            }
            imgui_texture record = {};
            record.width = static_cast<u32>(width);
            record.height = static_cast<u32>(height);
            record.owned = create_rgba8(record.width, record.height, "imgui.font_atlas");
            if (record.owned == nullptr) {
                return;
            }
            fontTexture = textures.emplace(std::move(record));
            font = textures.find(fontTexture);
            ++stats.texturesCreated;
        } else {
            ++stats.texturesUpdated;
        }
        upload_rgba8(commandList, font->owned, pixels, static_cast<u32>(width), static_cast<u32>(height));
        fontPixels = pixels;
        io.Fonts->SetTexID(static_cast<ImTextureID>(fontTexture.to_u64()));
    }
#endif
};

/*===========================================================================*/
/*===========================================================================*/
// imgui_renderer

imgui_renderer::imgui_renderer(create_tag) :
    mImpl(std::make_unique<impl>()) {}

imgui_renderer::~imgui_renderer() {
#ifdef IMGUI_HAS_TEXTURES
    if (ImGui::GetCurrentContext() != nullptr) {
        for (ImTextureData *texture : ImGui::GetPlatformIO().Textures) {
            if (texture->RefCount == 1) {
                texture->SetTexID(ImTextureID_Invalid);
                texture->SetStatus(ImTextureStatus_Destroyed);
            }
        }
    }
#else
    if (ImGui::GetCurrentContext() != nullptr && mImpl->textures.contains(mImpl->fontTexture)) {
        ImGui::GetIO().Fonts->SetTexID(0);
    }
#endif
}

bool imgui_renderer::uses_texture_protocol() noexcept {
#ifdef IMGUI_HAS_TEXTURES
    return true;
#else
    return false;
#endif
}

core::result<std::unique_ptr<imgui_renderer>> imgui_renderer::create(
  rhi::device &device,
  rhi::shader_library &shaders,
  rhi::texture_format targetFormat,
  imgui_texture_resolver resolver
) {
    std::unique_ptr<imgui_renderer> created = std::make_unique<imgui_renderer>(create_tag{});
    impl &state = *created->mImpl;
    state.device = device.native_device();
    state.targetFormat = targetFormat;
    state.resolver = std::move(resolver);
    nvrhi::IDevice &native = *state.device;

    const auto loadShader = [&](std::string_view entry, nvrhi::ShaderType type) -> core::result<nvrhi::ShaderHandle> {
        rhi::shader_key key = {};
        key.name = "imgui";
        key.entry = entry;
        core::result<rhi::shader_blob> blob = shaders.get(key);
        if (!blob.has_value()) {
            return core::make_error(
              renderer_error::SHADER_NOT_FOUND,
              "shader 'imgui' entry '{}': {}",
              entry,
              blob.get_error().message()
            );
        }
        nvrhi::ShaderDesc desc = {};
        desc.shaderType = type;
        desc.entryName = std::string(entry);
        desc.debugName = "imgui." + std::string(entry);
        nvrhi::ShaderHandle shader = native.createShader(desc, blob.value().bytes.data(), blob.value().bytes.size());
        if (shader == nullptr) {
            return core::make_error(
              renderer_error::PIPELINE_CREATION_FAILED,
              "the device refused the imgui {} shader",
              entry
            );
        }
        return shader;
    };
    nvrhi::ShaderHandle vertex = nullptr;
    nvrhi::ShaderHandle pixel = nullptr;
    CONJURE_TRY_ASSIGN(vertex, loadShader("vs_main", nvrhi::ShaderType::Vertex));
    CONJURE_TRY_ASSIGN(pixel, loadShader("ps_main", nvrhi::ShaderType::Pixel));

    nvrhi::BindingLayoutDesc layout = {};
    layout.visibility = nvrhi::ShaderType::All;
    layout.registerSpace = 0;
    layout.registerSpaceIsDescriptorSet = true;
    layout.addItem(nvrhi::BindingLayoutItem::PushConstants(1, sizeof(imgui_push_constants)));
    layout.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
    layout.addItem(nvrhi::BindingLayoutItem::Sampler(0));
    state.layout = native.createBindingLayout(layout);
    if (state.layout == nullptr) {
        return core::make_error(
          renderer_error::PIPELINE_CREATION_FAILED,
          "the device refused the imgui binding layout"
        );
    }

    const std::array<nvrhi::VertexAttributeDesc, 3> attributes = {
      nvrhi::VertexAttributeDesc()
        .setName("POSITION")
        .setFormat(nvrhi::Format::RG32_FLOAT)
        .setOffset(offsetof(ImDrawVert, pos))
        .setElementStride(sizeof(ImDrawVert)),
      nvrhi::VertexAttributeDesc()
        .setName("TEXCOORD")
        .setFormat(nvrhi::Format::RG32_FLOAT)
        .setOffset(offsetof(ImDrawVert, uv))
        .setElementStride(sizeof(ImDrawVert)),
      nvrhi::VertexAttributeDesc()
        .setName("COLOR")
        .setFormat(nvrhi::Format::R32_UINT)
        .setOffset(offsetof(ImDrawVert, col))
        .setElementStride(sizeof(ImDrawVert)),
    };
    state.inputLayout = native.createInputLayout(attributes.data(), static_cast<u32>(attributes.size()), vertex);
    if (state.inputLayout == nullptr) {
        return core::make_error(renderer_error::PIPELINE_CREATION_FAILED, "the device refused the imgui input layout");
    }

    nvrhi::GraphicsPipelineDesc pipeline = {};
    pipeline.VS = vertex;
    pipeline.PS = pixel;
    pipeline.inputLayout = state.inputLayout;
    pipeline.primType = nvrhi::PrimitiveType::TriangleList;
    pipeline.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
    pipeline.renderState.rasterState.scissorEnable = true;
    pipeline.renderState.depthStencilState.depthTestEnable = false;
    pipeline.renderState.depthStencilState.depthWriteEnable = false;
    pipeline.renderState.depthStencilState.stencilEnable = false;
    nvrhi::BlendState::RenderTarget &blend = pipeline.renderState.blendState.targets[0];
    blend.blendEnable = true;
    blend.srcBlend = nvrhi::BlendFactor::One;
    blend.destBlend = nvrhi::BlendFactor::InvSrcAlpha;
    blend.srcBlendAlpha = nvrhi::BlendFactor::One;
    blend.destBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;
    pipeline.bindingLayouts.push_back(state.layout);
    nvrhi::FramebufferInfo info = {};
    info.addColorFormat(rhi::to_nvrhi_format(targetFormat));
    state.pipeline = native.createGraphicsPipeline(pipeline, info);
    if (state.pipeline == nullptr) {
        return core::make_error(renderer_error::PIPELINE_CREATION_FAILED, "the device refused the imgui pipeline");
    }

    nvrhi::SamplerDesc samplerDesc = {};
    samplerDesc.setAllFilters(true);
    samplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
    state.sampler = native.createSampler(samplerDesc);
    if (state.sampler == nullptr) {
        return core::make_error(renderer_error::RESOURCE_CREATION_FAILED, "the device refused the imgui sampler");
    }
    if (!state.ensure_buffers(INITIAL_VERTEX_CAPACITY, INITIAL_INDEX_CAPACITY)) {
        return core::make_error(renderer_error::RESOURCE_CREATION_FAILED, "the device refused the imgui buffers");
    }

    if (ImGui::GetCurrentContext() != nullptr) {
        ImGuiIO &io = ImGui::GetIO();
        io.BackendRendererName = "conjure_renderer";
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
#ifdef IMGUI_HAS_TEXTURES
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
#endif
    }
    return created;
}

void imgui_renderer::render(ImDrawData *drawData, nvrhi::IFramebuffer *target, nvrhi::ICommandList &commandList) {
    impl &state = *mImpl;
    state.stats = imgui_render_stats();
    if (drawData == nullptr || target == nullptr) {
        return;
    }

#ifdef IMGUI_HAS_TEXTURES
    if (drawData->Textures != nullptr) {
        for (ImTextureData *texture : *drawData->Textures) {
            if (texture->Status != ImTextureStatus_OK) {
                state.update_texture(commandList, texture);
            }
        }
    }
#else
    state.update_font_atlas(commandList);
#endif

    if (drawData->CmdListsCount == 0 || drawData->TotalVtxCount == 0 || drawData->TotalIdxCount == 0) {
        return;
    }
    const u32 vertexCount = static_cast<u32>(drawData->TotalVtxCount);
    const u32 indexCount = static_cast<u32>(drawData->TotalIdxCount);
    if (!state.ensure_buffers(vertexCount, indexCount)) {
        return;
    }
    state.vertexScratch.clear();
    state.indexScratch.clear();
    state.vertexScratch.reserve(vertexCount);
    state.indexScratch.reserve(indexCount);
    for (int listIndex = 0; listIndex < drawData->CmdListsCount; ++listIndex) {
        const ImDrawList *list = drawData->CmdLists[listIndex];
        state.vertexScratch.insert(
          state.vertexScratch.end(),
          list->VtxBuffer.Data,
          list->VtxBuffer.Data + list->VtxBuffer.Size
        );
        state.indexScratch.insert(
          state.indexScratch.end(),
          list->IdxBuffer.Data,
          list->IdxBuffer.Data + list->IdxBuffer.Size
        );
    }
    commandList.writeBuffer(
      state.vertices,
      state.vertexScratch.data(),
      state.vertexScratch.size() * sizeof(ImDrawVert)
    );
    commandList.writeBuffer(state.indices, state.indexScratch.data(), state.indexScratch.size() * sizeof(ImDrawIdx));
    state.stats.vertices = vertexCount;
    state.stats.indices = indexCount;

    const nvrhi::FramebufferDesc &targetDesc = target->getDesc();
    if (targetDesc.colorAttachments.empty() || targetDesc.colorAttachments[0].texture == nullptr) {
        return;
    }
    const nvrhi::TextureDesc &colourDesc = targetDesc.colorAttachments[0].texture->getDesc();
    const f32 framebufferWidth = static_cast<f32>(colourDesc.width);
    const f32 framebufferHeight = static_cast<f32>(colourDesc.height);

    imgui_push_constants constants = {};
    constants.displayX = drawData->DisplayPos.x;
    constants.displayY = drawData->DisplayPos.y;
    constants.sizeX = std::max(drawData->DisplaySize.x, 1.0f);
    constants.sizeY = std::max(drawData->DisplaySize.y, 1.0f);
    const ImVec2 clipOffset = drawData->DisplayPos;
    const ImVec2 clipScale = drawData->FramebufferScale;

    u32 globalVertexOffset = 0;
    u32 globalIndexOffset = 0;
    for (int listIndex = 0; listIndex < drawData->CmdListsCount; ++listIndex) {
        const ImDrawList *list = drawData->CmdLists[listIndex];
        for (int commandIndex = 0; commandIndex < list->CmdBuffer.Size; ++commandIndex) {
            const ImDrawCmd &command = list->CmdBuffer[commandIndex];
            if (command.UserCallback != nullptr) {
                if (command.UserCallback != ImDrawCallback_ResetRenderState) {
                    command.UserCallback(list, &command);
                }
                continue;
            }
            if (command.ElemCount == 0) {
                continue;
            }
            const f32 clipMinX = std::max((command.ClipRect.x - clipOffset.x) * clipScale.x, 0.0f);
            const f32 clipMinY = std::max((command.ClipRect.y - clipOffset.y) * clipScale.y, 0.0f);
            const f32 clipMaxX = std::min((command.ClipRect.z - clipOffset.x) * clipScale.x, framebufferWidth);
            const f32 clipMaxY = std::min((command.ClipRect.w - clipOffset.y) * clipScale.y, framebufferHeight);
            if (clipMaxX <= clipMinX || clipMaxY <= clipMinY) {
                ++state.stats.commandsClipped;
                continue;
            }
            imgui_texture *texture = state.textures.find(
              core::slot_key::from_u64(static_cast<u64>(command.GetTexID()))
            );
            if (texture == nullptr) {
                texture = state.textures.find(state.fontTexture);
            }
            nvrhi::IBindingSet *set = texture != nullptr ? state.binding_set(*texture) : nullptr;
            if (set == nullptr) {
                continue;
            }

            nvrhi::GraphicsState graphics = {};
            graphics.pipeline = state.pipeline;
            graphics.framebuffer = target;
            graphics.viewport.addViewport(nvrhi::Viewport(framebufferWidth, framebufferHeight));
            graphics.viewport.addScissorRect(nvrhi::Rect(
              static_cast<int>(clipMinX),
              static_cast<int>(std::ceil(clipMaxX)),
              static_cast<int>(clipMinY),
              static_cast<int>(std::ceil(clipMaxY))
            ));
            graphics.addBindingSet(set);
            const u64 vertexOffset = u64{globalVertexOffset} * sizeof(ImDrawVert);
            nvrhi::VertexBufferBinding vertexBinding = {};
            vertexBinding.setBuffer(state.vertices).setSlot(0).setOffset(vertexOffset);
            graphics.addVertexBuffer(vertexBinding);
            graphics.setIndexBuffer(nvrhi::IndexBufferBinding()
                                      .setBuffer(state.indices)
                                      .setFormat(
                                        sizeof(ImDrawIdx) == 2 ? nvrhi::Format::R16_UINT : nvrhi::Format::R32_UINT
                                      )
                                      .setOffset(globalIndexOffset * static_cast<u32>(sizeof(ImDrawIdx))));
            commandList.setGraphicsState(graphics);
            commandList.setPushConstants(&constants, sizeof(constants));
            nvrhi::DrawArguments arguments = {};
            arguments.vertexCount = command.ElemCount;
            arguments.instanceCount = 1;
            arguments.startIndexLocation = command.IdxOffset;
            arguments.startVertexLocation = command.VtxOffset;
            commandList.drawIndexed(arguments);
            ++state.stats.drawCalls;
        }
        globalVertexOffset += static_cast<u32>(list->VtxBuffer.Size);
        globalIndexOffset += static_cast<u32>(list->IdxBuffer.Size);
    }
}

u64 imgui_renderer::register_texture(gpu_texture_id texture) {
    imgui_texture record = {};
    record.external = texture;
    return mImpl->textures.emplace(std::move(record)).to_u64();
}

void imgui_renderer::unregister_texture(u64 textureId) {
    const core::slot_key key = core::slot_key::from_u64(textureId);
    if (key == mImpl->fontTexture) {
        return;
    }
    static_cast<void>(mImpl->textures.erase(key));
}

void imgui_renderer::invalidate_font_atlas() noexcept {
    // 1.91: forgetting the uploaded pixel pointer forces update_font_atlas to upload again even when
    // the rebuilt atlas reuses the old allocation. 1.92: ImGui carries the request in the draw data.
    mImpl->fontPixels = nullptr;
}

u32 imgui_renderer::texture_count() const noexcept {
    return static_cast<u32>(mImpl->textures.size());
}

const imgui_render_stats &imgui_renderer::stats() const noexcept {
    return mImpl->stats;
}

#endif
