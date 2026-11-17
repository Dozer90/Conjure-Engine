#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"
#include "renderer_test_support.h"

#include "conjure/renderer/imgui_renderer.h"

#include "conjure/core/platform_detect.h"
#include "conjure/core/types.h"

// imgui_renderer.h (docs/design/02_renderer.md 3.11, R16): the Dear ImGui backend, compiled in
// editor builds only. The ImGui context is created here because the engine's imgui_layer (which
// owns it in production) belongs to another module; <imgui.h> is the sanctioned third-party include
// of this one test file.

#if defined(CONJURE_BUILD_EDITOR) && CONJURE_BUILD_EDITOR

#include "conjure/renderer/gpu_ids.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/renderer.h"
#include "conjure/renderer/renderer_error.h"
#include "conjure/renderer/texture_asset.h"

#include "conjure/core/image.h"
#include "conjure/core/log.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/null_device_stats.h"
#include "conjure/rhi/shader_library.h"
#include "conjure/rhi/swapchain.h"
#include "conjure/rhi/texture_format.h"

#include <imgui.h>
#include <nvrhi/nvrhi.h>

#include <array>
#include <memory>
#include <optional>
#include <string>

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;
using namespace conjure::testing::renderer_support;

namespace {

    /// <summary>
    /// An ImGui context with a built font atlas, destroyed with the scope.
    /// </summary>
    class imgui_scope {
        private:
            ImGuiContext *mContext = nullptr;

        public:
            imgui_scope() {
                mContext = ImGui::CreateContext();
                ImGuiIO &io = ImGui::GetIO();
                io.DisplaySize = ImVec2(256.0f, 128.0f);
                io.DeltaTime = 1.0f / 60.0f;
                io.IniFilename = nullptr;
                io.LogFilename = nullptr;
                unsigned char *pixels = nullptr;
                int width = 0;
                int height = 0;
                io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
            }

            ~imgui_scope() {
                ImGui::DestroyContext(mContext);
            }

            imgui_scope(const imgui_scope &) = delete;
            imgui_scope &operator=(const imgui_scope &) = delete;

            /// <summary>
            /// Builds one frame with a window and a few widgets and returns its draw data.
            /// </summary>
            [[nodiscard]] static ImDrawData *frame(bool withWindow, ImTextureID image = 0) {
                ImGui::NewFrame();
                if (withWindow) {
                    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f));
                    ImGui::SetNextWindowSize(ImVec2(200.0f, 100.0f));
                    ImGui::Begin("test");
                    ImGui::Text("hello renderer");
                    ImGui::Button("press");
                    if (image != 0) {
                        ImGui::Image(image, ImVec2(32.0f, 32.0f));
                    }
                    ImGui::End();
                }
                ImGui::Render();
                return ImGui::GetDrawData();
            }
    };

    struct backend_fixture {
        std::unique_ptr<rhi::device> device = nullptr;
        std::unique_ptr<rhi::shader_library> shaders = nullptr;
        std::unique_ptr<imgui_renderer> backend = nullptr;
        nvrhi::TextureHandle target = nullptr;
        nvrhi::FramebufferHandle framebuffer = nullptr;
        nvrhi::CommandListHandle commandList = nullptr;
    };

    [[nodiscard]] backend_fixture make_backend(imgui_texture_resolver resolver) {
        backend_fixture fixture = {};
        fixture.device = make_device();
        fixture.shaders = open_shaders(*fixture.device);
        core::result<std::unique_ptr<imgui_renderer>> created =
          imgui_renderer::create(
            *fixture.device,
            *fixture.shaders,
            rhi::texture_format::RGBA8_SRGB,
            std::move(resolver)
          );
        CONJURE_REQUIRE_OK(created);
        fixture.backend = std::move(created).value();
        nvrhi::IDevice &native = *fixture.device->native_device();
        nvrhi::TextureDesc desc = {};
        desc.width = 256;
        desc.height = 128;
        desc.format = nvrhi::Format::SRGBA8_UNORM;
        desc.isRenderTarget = true;
        desc.initialState = nvrhi::ResourceStates::RenderTarget;
        desc.keepInitialState = true;
        desc.debugName = "imgui_test_target";
        fixture.target = native.createTexture(desc);
        REQUIRE(fixture.target != nullptr);
        nvrhi::FramebufferDesc framebufferDesc = {};
        framebufferDesc.addColorAttachment(fixture.target);
        fixture.framebuffer = native.createFramebuffer(framebufferDesc);
        REQUIRE(fixture.framebuffer != nullptr);
        fixture.commandList = native.createCommandList();
        REQUIRE(fixture.commandList != nullptr);
        return fixture;
    }

    void render_once(backend_fixture &fixture, ImDrawData *drawData) {
        fixture.commandList->open();
        fixture.backend->render(drawData, fixture.framebuffer, *fixture.commandList);
        fixture.commandList->close();
        nvrhi::ICommandList *list = fixture.commandList.Get();
        fixture.device->native_device()->executeCommandLists(&list, 1);
    }
}

TEST_SUITE("renderer/imgui_renderer") {

    TEST_CASE("imgui_renderer - create builds the pipeline for the target format and reports the protocol") {
        log_capture logs = {};
        imgui_scope imgui = {};
        backend_fixture fixture = make_backend(nullptr);
        CHECK(fixture.backend->texture_count() == 0);
        CHECK(fixture.backend->stats().drawCalls == 0);
#ifdef IMGUI_HAS_TEXTURES
        CHECK(imgui_renderer::uses_texture_protocol());
        CHECK((ImGui::GetIO().BackendFlags & ImGuiBackendFlags_RendererHasTextures) != 0);
#else
        CHECK_FALSE(imgui_renderer::uses_texture_protocol());
#endif
        CHECK((ImGui::GetIO().BackendFlags & ImGuiBackendFlags_RendererHasVtxOffset) != 0);
        CHECK(ImGui::GetIO().BackendRendererName != nullptr);
        const std::optional<rhi::null_device_stats> stats = fixture.device->null_stats();
        REQUIRE(stats.has_value());
        CHECK(stats->graphicsPipelinesCreated == 1);
        CHECK(stats->inputLayoutsCreated == 1);
        CHECK(stats->samplersCreated >= 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("imgui_renderer - a null or empty frame draws nothing while a window draws and uploads the font atlas") {
        imgui_scope imgui = {};
        backend_fixture fixture = make_backend(nullptr);
        render_once(fixture, nullptr);
        CHECK(fixture.backend->stats().drawCalls == 0);
        CHECK(fixture.backend->stats().vertices == 0);

        render_once(fixture, imgui_scope::frame(false));
        CHECK(fixture.backend->stats().drawCalls == 0);
        CHECK(fixture.backend->texture_count() == 1);   // the font atlas exists after the first frame

        fixture.device->reset_null_stats();
        render_once(fixture, imgui_scope::frame(true));
        const imgui_render_stats &stats = fixture.backend->stats();
        CHECK(stats.drawCalls >= 1);
        CHECK(stats.vertices > 0);
        CHECK(stats.indices > 0);
        CHECK(stats.commandsClipped == 0);
        CHECK(fixture.backend->texture_count() == 1);
        const std::optional<rhi::null_device_stats> nullStats = fixture.device->null_stats();
        REQUIRE(nullStats.has_value());
        CHECK(nullStats->draws == stats.drawCalls);
        CHECK(nullStats->bufferWrites >= 2);
        // The font atlas is uploaded once, not every frame.
        fixture.device->reset_null_stats();
        render_once(fixture, imgui_scope::frame(true));
        CHECK(fixture.device->null_stats()->textureWrites == 0);
        CHECK(fixture.backend->stats().texturesCreated == 0);
        CHECK(fixture.backend->stats().texturesUpdated == 0);
    }

    TEST_CASE("regression - invalidate_font_atlas re-uploads an atlas rebuilt at the same address") {
        imgui_scope imgui = {};
        backend_fixture fixture = make_backend(nullptr);
        render_once(fixture, imgui_scope::frame(true));
        fixture.device->reset_null_stats();
        render_once(fixture, imgui_scope::frame(true));
        CHECK(fixture.device->null_stats()->textureWrites == 0);
#ifdef IMGUI_HAS_TEXTURES
        // The texture protocol carries atlas changes itself; the call is a harmless no-op.
        fixture.backend->invalidate_font_atlas();
        fixture.device->reset_null_stats();
        render_once(fixture, imgui_scope::frame(true));
        CHECK(fixture.device->null_stats()->textureWrites == 0);
#else
        // Same pixels, same size, same TexID: only the explicit invalidation can tell a rebuild.
        fixture.backend->invalidate_font_atlas();
        fixture.device->reset_null_stats();
        render_once(fixture, imgui_scope::frame(true));
        CHECK(fixture.device->null_stats()->textureWrites == 1);
        CHECK(fixture.backend->stats().texturesUpdated == 1);
        CHECK(fixture.backend->stats().texturesCreated == 0);
        CHECK(fixture.backend->texture_count() == 1);
        // Resetting the TexID after a rebuild has the same effect.
        ImGui::GetIO().Fonts->SetTexID(0);
        fixture.device->reset_null_stats();
        render_once(fixture, imgui_scope::frame(true));
        CHECK(fixture.device->null_stats()->textureWrites == 1);
        CHECK(ImGui::GetIO().Fonts->TexID != 0);
        // And a quiet frame uploads nothing again.
        fixture.device->reset_null_stats();
        render_once(fixture, imgui_scope::frame(true));
        CHECK(fixture.device->null_stats()->textureWrites == 0);
#endif
    }

    TEST_CASE("imgui_renderer - registered renderer textures draw through the resolver and stale ones fall back") {
        imgui_scope imgui = {};
        headless_renderer renderer = make_headless_renderer();
        const gpu_texture_id uploaded = renderer->upload_texture(
          test_asset_id(1),
          texture_asset::solid_colour(core::rgba8{255, 0, 0, 255}, texture_colour_space::SRGB, 4, 4)
        );
        CONJURE_REQUIRE_OK(renderer->render(render_scene()));
        u32 resolved = 0;
        u32 unresolved = 0;
        imgui_texture_resolver resolver = [&](gpu_texture_id id) -> nvrhi::ITexture * {
            if (id == uploaded) {
                ++resolved;
                return nullptr;   // the null device texture is private to the renderer; count the lookup
            }
            ++unresolved;
            return nullptr;
        };
        backend_fixture fixture = make_backend(resolver);
        const u64 handle = fixture.backend->register_texture(uploaded);
        CHECK(handle != 0);
        CHECK(fixture.backend->texture_count() == 1);
        gpu_texture_id stale = {};
        stale.index = 77;
        stale.generation = 3;
        const u64 staleHandle = fixture.backend->register_texture(stale);
        CHECK(staleHandle != handle);
        CHECK(fixture.backend->texture_count() == 2);

        render_once(fixture, imgui_scope::frame(true, static_cast<ImTextureID>(handle)));
        CHECK(fixture.backend->stats().drawCalls >= 1);
        CHECK(resolved >= 1);
        render_once(fixture, imgui_scope::frame(true, static_cast<ImTextureID>(staleHandle)));
        CHECK(fixture.backend->stats().drawCalls >= 1);
        CHECK(unresolved >= 1);
        // An unknown handle draws the font atlas texel instead of crashing.
        render_once(fixture, imgui_scope::frame(true, static_cast<ImTextureID>(0x7777777700000001ull)));
        CHECK(fixture.backend->stats().drawCalls >= 1);

        fixture.backend->unregister_texture(staleHandle);
        CHECK(fixture.backend->texture_count() == 2);   // the font atlas and the live registration
        fixture.backend->unregister_texture(staleHandle);
        fixture.backend->unregister_texture(0);
        fixture.backend->unregister_texture(handle);
        CHECK(fixture.backend->texture_count() == 1);
        render_once(fixture, imgui_scope::frame(true));
        CHECK(fixture.backend->stats().drawCalls >= 1);
    }

    TEST_CASE("imgui_renderer - without a resolver registered textures never resolve and the frame still draws") {
        imgui_scope imgui = {};
        backend_fixture fixture = make_backend(nullptr);
        gpu_texture_id id = {};
        id.index = 1;
        id.generation = 1;
        const u64 handle = fixture.backend->register_texture(id);
        render_once(fixture, imgui_scope::frame(true, static_cast<ImTextureID>(handle)));
        CHECK(fixture.backend->stats().drawCalls >= 1);
        CHECK(fixture.backend->texture_count() == 2);
    }

    TEST_CASE("imgui_renderer - draw data whose scissors fall outside the target is clipped, not drawn") {
        imgui_scope imgui = {};
        backend_fixture fixture = make_backend(nullptr);
        // A primitive whose clip rectangle lies entirely outside the 256 x 128 target.
        ImGui::NewFrame();
        ImDrawList *background = ImGui::GetBackgroundDrawList();
        background->PushClipRect(ImVec2(1000.0f, 1000.0f), ImVec2(1050.0f, 1050.0f), false);
        background->AddRectFilled(ImVec2(1000.0f, 1000.0f), ImVec2(1050.0f, 1050.0f), IM_COL32(255, 0, 0, 255));
        background->PopClipRect();
        ImGui::Render();
        render_once(fixture, ImGui::GetDrawData());
        CHECK(fixture.backend->stats().drawCalls == 0);
        CHECK(fixture.backend->stats().commandsClipped >= 1);
        // The same primitive inside the target draws.
        ImGui::NewFrame();
        ImDrawList *visible = ImGui::GetBackgroundDrawList();
        visible->PushClipRect(ImVec2(10.0f, 10.0f), ImVec2(60.0f, 60.0f), false);
        visible->AddRectFilled(ImVec2(10.0f, 10.0f), ImVec2(60.0f, 60.0f), IM_COL32(255, 0, 0, 255));
        visible->PopClipRect();
        ImGui::Render();
        render_once(fixture, ImGui::GetDrawData());
        CHECK(fixture.backend->stats().drawCalls == 1);
        CHECK(fixture.backend->stats().commandsClipped == 0);
    }

    TEST_CASE(
      "imgui_renderer - the renderer owns a backend only with a swapchain in editor builds and draws it after the views"
    ) {
        imgui_scope imgui = {};
        std::unique_ptr<rhi::device> device = make_device();
        std::unique_ptr<rhi::shader_library> shaders = open_shaders(*device);
        rhi::swapchain_desc swapchainDesc = {};
        swapchainDesc.width = 256;
        swapchainDesc.height = 128;
        core::result<std::unique_ptr<rhi::swapchain>> swapchain = rhi::swapchain::create(
          *device,
          rhi::native_window(),
          swapchainDesc
        );
        CONJURE_REQUIRE_OK(swapchain);
        renderer_desc desc = headless_desc(*device, *shaders);
        desc.swapchain = swapchain.value().get();
        desc.enableImGui = true;
        core::result<std::unique_ptr<renderer_type>> created = renderer_type::create(desc);
        CONJURE_REQUIRE_OK(created);
        renderer_type &instance = *created.value();
        REQUIRE(instance.imgui() != nullptr);
        instance.set_imgui_draw_data(imgui_scope::frame(true));
        CONJURE_REQUIRE_OK(instance.render(render_scene()));
        CHECK(instance.stats().drawCalls >= 1);
        CHECK(instance.imgui()->stats().drawCalls >= 1);
        // The draw data is consumed: the next frame without new data draws nothing.
        CONJURE_REQUIRE_OK(instance.render(render_scene()));
        CHECK(instance.stats().drawCalls == 0);
        // A render target's colour texture can be shown through the backend.
        render_target_desc targetDesc = {};
        targetDesc.size = math::uvec2(16, 16);
        core::result<render_target_id> target = instance.create_render_target(targetDesc);
        CONJURE_REQUIRE_OK(target);
        const gpu_texture_id colour = instance.get_render_target(target.value())->colour_texture();
        const u64 handle = instance.imgui()->register_texture(colour);
        instance.set_imgui_draw_data(imgui_scope::frame(true, static_cast<ImTextureID>(handle)));
        CONJURE_REQUIRE_OK(instance.render(render_scene()));
        CHECK(instance.stats().drawCalls >= 1);
        instance.imgui()->unregister_texture(handle);

        renderer_desc disabled = desc;
        disabled.enableImGui = false;
        core::result<std::unique_ptr<renderer_type>> plain = renderer_type::create(disabled);
        CONJURE_REQUIRE_OK(plain);
        CHECK(plain.value()->imgui() == nullptr);
        plain.value().reset();
        created.value().reset();
    }

    TEST_CASE("imgui_renderer - a missing imgui shader fails creation with SHADER_NOT_FOUND") {
        imgui_scope imgui = {};
        temp_directory directory("imgui_shaders");
        std::unique_ptr<rhi::device> device = make_device();
        std::unique_ptr<rhi::shader_library> empty = open_written_shaders(*device, directory);
        const core::result<std::unique_ptr<imgui_renderer>> created =
          imgui_renderer::create(*device, *empty, rhi::texture_format::RGBA8_SRGB, nullptr);
        CONJURE_CHECK_ERROR(created, renderer_error::SHADER_NOT_FOUND);
        const std::string message = created.get_error().message();
        CHECK(message.find("shader 'imgui'") != std::string::npos);
        CHECK(message.find("entry 'vs_main'") != std::string::npos);
    }

    TEST_CASE("regression - code sweep F395 imgui_renderer checks its whole program before it creates a shader") {
        // The index carries the vertex stage of the imgui program but not the pixel stage. A backend that
        // asked the library for its two blobs by name would create the vertex shader and then fail;
        // resolved through shader_asset's table, the program is refused before any device object exists.
        imgui_scope imgui = {};
        temp_directory directory("partial_imgui_shaders");
        std::unique_ptr<rhi::device> device = make_device();
        const std::array<index_stage, 1> stages = {{{"imgui", "vs_main", "vertex", ""}}};
        std::unique_ptr<rhi::shader_library> partial = open_written_shaders(*device, directory, stages);
        REQUIRE(partial->contains({"imgui", "vs_main", ""}));
        const core::result<std::unique_ptr<imgui_renderer>> created =
          imgui_renderer::create(*device, *partial, rhi::texture_format::RGBA8_SRGB, nullptr);
        CONJURE_CHECK_ERROR(created, renderer_error::SHADER_NOT_FOUND);
        const std::string message = created.get_error().message();
        CHECK(message.find("shader 'imgui'") != std::string::npos);
        CHECK(message.find("entry 'ps_main'") != std::string::npos);
        const std::optional<rhi::null_device_stats> stats = device->null_stats();
        REQUIRE(stats.has_value());
        CHECK(stats->shadersCreated == 0);
        CHECK(stats->bindingLayoutsCreated == 0);
        CHECK(partial->loaded_blob_count() == 0);
    }
}

#else

TEST_SUITE("renderer/imgui_renderer") {

    TEST_CASE("imgui_renderer - the header compiles to nothing outside editor builds") {
        CHECK(true);
    }
}

#endif
