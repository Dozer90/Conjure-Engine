#pragma once

#include "conjure/core/platform_detect.h"

// The Dear ImGui render backend (docs/design/02_renderer.md 3.11, R16). Compiled only in editor
// builds (CONJURE_BUILD_EDITOR, never in Distribution); the header is empty otherwise so it still
// compiles alone. The ImGui context belongs to engine::imgui_layer (R9): this class only draws.
// With ImGui >= 1.92 it implements the texture protocol (ImGuiBackendFlags_RendererHasTextures);
// with the pinned 1.91 it uploads the font atlas once and again when the atlas changed: a new pixel
// pointer, a new size, a TexID that is not the backend's (io.Fonts->SetTexID(0) after a rebuild), or
// an explicit invalidate_font_atlas(). The owner of the context must do one of the last two after
// io.Fonts->Build(): a rebuilt atlas of the same size can land at the same address.

#if defined(CONJURE_BUILD_EDITOR) && CONJURE_BUILD_EDITOR

#include "conjure/renderer/gpu_ids.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/shader_library.h"
#include "conjure/rhi/texture_format.h"

#include <functional>
#include <memory>

struct ImDrawData;

namespace nvrhi {
    class ICommandList;
    class IFramebuffer;
    class ITexture;
}

namespace conjure::renderer {

    /// <summary>
    /// Resolves a renderer texture id to its NVRHI texture (nullptr when stale); supplied by the renderer.
    /// </summary>
    using imgui_texture_resolver = std::function<nvrhi::ITexture *(gpu_texture_id)>;

    /// <summary>
    /// Statistics of the last render().
    /// </summary>
    struct imgui_render_stats {
        u32 drawCalls = 0;
        u32 vertices = 0;
        u32 indices = 0;
        u32 texturesCreated = 0;
        u32 texturesUpdated = 0;
        u32 texturesDestroyed = 0;
        u32 commandsClipped = 0;        // draw commands whose scissor was empty after clamping
    };

    class CONJURE_API imgui_renderer {
        private:
            struct impl;
            struct create_tag {};

            std::unique_ptr<impl> mImpl = nullptr;

        public:
            explicit imgui_renderer(create_tag);
            ~imgui_renderer();

            imgui_renderer(const imgui_renderer &) = delete;
            imgui_renderer &operator=(const imgui_renderer &) = delete;

        public:
            /// <summary>
            /// Builds the pipeline (imgui.hlsl: premultiplied alpha, scissor, no depth) for the given
            /// framebuffer colour format. Fails with renderer_error::SHADER_NOT_FOUND or
            /// PIPELINE_CREATION_FAILED. A null resolver makes register_texture always fail.
            /// </summary>
            [[nodiscard]] static core::result<std::unique_ptr<imgui_renderer>> create(
              rhi::device &device,
              rhi::shader_library &shaders,
              rhi::texture_format targetFormat,
              imgui_texture_resolver resolver
            );

            /// <summary>
            /// Draws one frame of ImGui output into 'target' (whose colour format matches the one given at
            /// creation). Handles the texture requests carried by the draw data first (ImGui >= 1.92) or
            /// the font atlas (1.91). A null drawData or an empty frame draws nothing.
            /// </summary>
            void render(ImDrawData *drawData, nvrhi::IFramebuffer *target, nvrhi::ICommandList &commandList);

            /// <summary>
            /// Makes a renderer texture usable as an ImTextureID (returned packed as a u64). The id stays
            /// valid until unregister_texture or destruction; a stale texture draws the font atlas white
            /// texel instead.
            /// </summary>
            [[nodiscard]] u64 register_texture(gpu_texture_id texture);

            /// <summary>
            /// Forgets a registered texture; unknown handles are ignored.
            /// </summary>
            void unregister_texture(u64 textureId);

            /// <summary>
            /// Marks the font atlas as changed so the next render() re-uploads it (ImGui 1.91: call it
            /// after io.Fonts->Build(), or reset io.Fonts->TexID to 0, which has the same effect). A
            /// no-op under the 1.92 texture protocol, where ImGui reports the change itself.
            /// </summary>
            void invalidate_font_atlas() noexcept;

            /// <summary>
            /// Number of textures the backend currently knows (font atlas and registered ones).
            /// </summary>
            [[nodiscard]] u32 texture_count() const noexcept;

            [[nodiscard]] const imgui_render_stats &stats() const noexcept;

            /// <summary>
            /// True when the running ImGui implements the 1.92 texture protocol.
            /// </summary>
            [[nodiscard]] static bool uses_texture_protocol() noexcept;
    };
}

#endif
