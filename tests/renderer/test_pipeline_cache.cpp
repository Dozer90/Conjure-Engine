#include "conjure/testing/doctest.h"

#include "conjure/testing/temp_directory.h"
#include "renderer_test_support.h"

#include "renderer/binding_layouts.h"
#include "renderer/pipeline_cache.h"

#include "conjure/renderer/renderer_error.h"

#include "conjure/core/types.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/null_device_stats.h"
#include "conjure/rhi/shader_library.h"
#include "conjure/rhi/texture_format.h"

#include <nvrhi/nvrhi.h>

#include <memory>
#include <optional>
#include <string>

// The pipeline cache, the binding layouts and the binding set cache (docs/design/02_renderer.md
// R2, R7, 6, 10 "test_pipeline_cache"): everything is created at start-up, blits per target format
// on demand outside frames, a missing shader is reported with its key, and binding sets are reused.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

namespace {

    struct cache_fixture {
        std::unique_ptr<rhi::device> device = nullptr;
        std::unique_ptr<rhi::shader_library> shaders = nullptr;
        binding_layouts layouts = {};
        std::unique_ptr<pipeline_cache> pipelines = nullptr;

        cache_fixture() {
            device = renderer_support::make_device();
            shaders = renderer_support::open_shaders(*device);
            core::result<binding_layouts> created = binding_layouts::create(*device->native_device());
            CONJURE_REQUIRE_OK(created);
            layouts = std::move(created).value();
            core::result<std::unique_ptr<pipeline_cache>> cache = pipeline_cache::create(
              *device->native_device(),
              *shaders,
              layouts
            );
            CONJURE_REQUIRE_OK(cache);
            pipelines = std::move(cache).value();
        }

        ~cache_fixture() {
            pipelines.reset();
            layouts = binding_layouts();
        }

        cache_fixture(const cache_fixture &) = delete;
        cache_fixture &operator=(const cache_fixture &) = delete;
    };
}

TEST_SUITE("renderer/pipeline_cache") {

    TEST_CASE("binding_layouts - create builds every layout of the design's register map") {
        std::unique_ptr<rhi::device> device = renderer_support::make_device();
        core::result<binding_layouts> created = binding_layouts::create(*device->native_device());
        CONJURE_REQUIRE_OK(created);
        const binding_layouts &layouts = created.value();
        CHECK(layouts.meshView != nullptr);
        CHECK(layouts.material != nullptr);
        CHECK(layouts.fullscreen != nullptr);
        CHECK(layouts.lightCull != nullptr);
        CHECK(layouts.ssaoGtao != nullptr);
        CHECK(layouts.ssao != nullptr);
        CHECK(layouts.exposureHistogram != nullptr);
        CHECK(layouts.exposureAdapt != nullptr);
        CHECK(layouts.bloom != nullptr);
        CHECK(layouts.imgui != nullptr);
        CHECK(layouts.meshView->getDesc()->registerSpaceIsDescriptorSet);
        CHECK(layouts.material->getDesc()->registerSpace == 1);
        CHECK(layouts.meshView->getDesc()->registerSpace == 0);
        // The mesh view set carries the volatile CB at b0 and the 16 byte push constants at b1.
        bool sawConstants = false;
        bool sawPush = false;
        for (const nvrhi::BindingLayoutItem &item : layouts.meshView->getDesc()->bindings) {
            sawConstants = sawConstants || (item.type == nvrhi::ResourceType::VolatileConstantBuffer && item.slot == 0);
            sawPush = sawPush || (item.type == nvrhi::ResourceType::PushConstants && item.slot == 1 && item.size == 16);
        }
        CHECK(sawConstants);
        CHECK(sawPush);
        CHECK(device->null_stats()->bindingLayoutsCreated == 10);
    }

    TEST_CASE(
      "pipeline_cache - create builds every graphics variant (twice for mesh passes) and every compute variant"
    ) {
        cache_fixture fixture = {};
        CHECK(fixture.pipelines->pipeline_count() == pipeline_cache::expected_pipeline_count());
        CHECK(pipeline_cache::expected_pipeline_count() == 14 + 7 + 7);
        for (u8 index = 0; index < static_cast<u8>(graphics_variant::COUNT); ++index) {
            const graphics_variant variant = static_cast<graphics_variant>(index);
            CAPTURE(to_string(variant));
            CHECK(fixture.pipelines->graphics(variant) != nullptr);
            if (is_mesh_variant(variant)) {
                CHECK(fixture.pipelines->graphics(variant, true) != nullptr);
                CHECK(fixture.pipelines->graphics(variant, true) != fixture.pipelines->graphics(variant, false));
                CHECK(
                  fixture.pipelines->graphics(
                    variant,
                    true
                  )->getDesc().renderState.rasterState.cullMode == nvrhi::RasterCullMode::None
                );
                CHECK(
                  fixture.pipelines->graphics(
                    variant,
                    false
                  )->getDesc().renderState.rasterState.cullMode == nvrhi::RasterCullMode::Back
                );
            } else {
                CHECK(fixture.pipelines->graphics(variant, true) == fixture.pipelines->graphics(variant, false));
            }
        }
        for (u8 index = 0; index < static_cast<u8>(compute_variant::COUNT); ++index) {
            const compute_variant variant = static_cast<compute_variant>(index);
            CAPTURE(to_string(variant));
            CHECK(fixture.pipelines->compute(variant) != nullptr);
        }
        CHECK(fixture.pipelines->graphics(graphics_variant::COUNT) == nullptr);
        CHECK(fixture.pipelines->compute(compute_variant::COUNT) == nullptr);
        const std::optional<rhi::null_device_stats> stats = fixture.device->null_stats();
        REQUIRE(stats.has_value());
        CHECK(stats->graphicsPipelinesCreated == 21);
        CHECK(stats->computePipelinesCreated == 7);
        CHECK(stats->inputLayoutsCreated == 4);
        CHECK(std::string(to_string(graphics_variant::FORWARD_OPAQUE)) == "FORWARD_OPAQUE");
        CHECK(std::string(to_string(compute_variant::LIGHT_CULL)) == "LIGHT_CULL");
    }

    TEST_CASE("pipeline_cache - the mesh and post pipelines carry the reverse-Z and blend states the design fixes") {
        cache_fixture fixture = {};
        const nvrhi::GraphicsPipelineDesc &prepass = fixture.pipelines->graphics(
          graphics_variant::DEPTH_PREPASS
        )->getDesc();
        CHECK(prepass.renderState.depthStencilState.depthTestEnable);
        CHECK(prepass.renderState.depthStencilState.depthWriteEnable);
        CHECK(prepass.renderState.depthStencilState.depthFunc == nvrhi::ComparisonFunc::GreaterOrEqual);
        const nvrhi::GraphicsPipelineDesc &forward = fixture.pipelines->graphics(
          graphics_variant::FORWARD_OPAQUE
        )->getDesc();
        CHECK(forward.renderState.depthStencilState.depthTestEnable);
        CHECK_FALSE(forward.renderState.depthStencilState.depthWriteEnable);
        CHECK(forward.renderState.depthStencilState.depthFunc == nvrhi::ComparisonFunc::Equal);
        CHECK_FALSE(forward.renderState.blendState.targets[0].blendEnable);
        const nvrhi::GraphicsPipelineDesc &blend = fixture.pipelines->graphics(
          graphics_variant::FORWARD_BLEND
        )->getDesc();
        CHECK(blend.renderState.blendState.targets[0].blendEnable);
        CHECK(blend.renderState.depthStencilState.depthFunc == nvrhi::ComparisonFunc::GreaterOrEqual);
        CHECK_FALSE(blend.renderState.depthStencilState.depthWriteEnable);
        const nvrhi::GraphicsPipelineDesc &shadow = fixture.pipelines->graphics(
          graphics_variant::SHADOW_DEPTH
        )->getDesc();
        CHECK(shadow.PS == nullptr);
        CHECK(shadow.renderState.depthStencilState.depthFunc == nvrhi::ComparisonFunc::GreaterOrEqual);
        CHECK(fixture.pipelines->graphics(graphics_variant::SHADOW_DEPTH_MASKED)->getDesc().PS != nullptr);
        const nvrhi::GraphicsPipelineDesc &skybox = fixture.pipelines->graphics(graphics_variant::SKYBOX)->getDesc();
        CHECK(skybox.renderState.depthStencilState.depthTestEnable);
        CHECK_FALSE(skybox.renderState.depthStencilState.depthWriteEnable);
        const nvrhi::GraphicsPipelineDesc &tonemap = fixture.pipelines->graphics(graphics_variant::TONEMAP)->getDesc();
        CHECK_FALSE(tonemap.renderState.depthStencilState.depthTestEnable);
        CHECK(tonemap.inputLayout == nullptr);
        const nvrhi::GraphicsPipelineDesc &overlay = fixture.pipelines->graphics(
          graphics_variant::DEBUG_OVERLAY
        )->getDesc();
        CHECK_FALSE(overlay.renderState.depthStencilState.depthTestEnable);
        CHECK(overlay.primType == nvrhi::PrimitiveType::LineList);
        CHECK(
          fixture.pipelines->graphics(
            graphics_variant::DEBUG_DEPTH_TESTED
          )->getDesc().renderState.depthStencilState.depthTestEnable
        );
        // Framebuffer formats per variant.
        const nvrhi::FramebufferInfo forwardInfo = pipeline_cache::framebuffer_info(graphics_variant::FORWARD_OPAQUE);
        CHECK(forwardInfo.colorFormats.size() == 1);
        CHECK(forwardInfo.colorFormats[0] == nvrhi::Format::RGBA16_FLOAT);
        CHECK(forwardInfo.depthFormat == nvrhi::Format::D32);
        const nvrhi::FramebufferInfo shadowInfo = pipeline_cache::framebuffer_info(graphics_variant::SHADOW_DEPTH);
        CHECK(shadowInfo.colorFormats.empty());
        CHECK(shadowInfo.depthFormat == nvrhi::Format::D32);
        const nvrhi::FramebufferInfo tonemapInfo = pipeline_cache::framebuffer_info(graphics_variant::TONEMAP);
        CHECK(tonemapInfo.colorFormats.size() == 1);
        CHECK(tonemapInfo.colorFormats[0] == nvrhi::Format::SRGBA8_UNORM);
        CHECK(tonemapInfo.depthFormat == nvrhi::Format::UNKNOWN);
        const nvrhi::FramebufferInfo lutInfo = pipeline_cache::framebuffer_info(graphics_variant::BRDF_LUT);
        CHECK(lutInfo.colorFormats[0] == nvrhi::Format::RG16_FLOAT);
    }

    TEST_CASE("pipeline_cache - blit pipelines are created once per target format on request") {
        cache_fixture fixture = {};
        const u32 before = fixture.pipelines->pipeline_count();
        CHECK_FALSE(fixture.pipelines->has_blit(rhi::texture_format::RGBA8_UNORM));
        core::result<nvrhi::IGraphicsPipeline *> first = fixture.pipelines->blit(rhi::texture_format::RGBA8_UNORM);
        CONJURE_REQUIRE_OK(first);
        CHECK(first.value() != nullptr);
        CHECK(fixture.pipelines->has_blit(rhi::texture_format::RGBA8_UNORM));
        CHECK(fixture.pipelines->pipeline_count() == before + 1);
        core::result<nvrhi::IGraphicsPipeline *> again = fixture.pipelines->blit(rhi::texture_format::RGBA8_UNORM);
        CONJURE_REQUIRE_OK(again);
        CHECK(again.value() == first.value());
        CHECK(fixture.pipelines->pipeline_count() == before + 1);
        core::result<nvrhi::IGraphicsPipeline *> other = fixture.pipelines->blit(rhi::texture_format::RGBA16_FLOAT);
        CONJURE_REQUIRE_OK(other);
        CHECK(other.value() != first.value());
        CHECK(fixture.pipelines->pipeline_count() == before + 2);
        CHECK(other.value()->getFramebufferInfo().colorFormats[0] == nvrhi::Format::RGBA16_FLOAT);
        CHECK_FALSE(other.value()->getDesc().renderState.depthStencilState.depthTestEnable);
        CONJURE_CHECK_OK(fixture.pipelines->blit(rhi::texture_format::BGRA8_SRGB));
        CONJURE_CHECK_OK(fixture.pipelines->blit(rhi::texture_format::R11G11B10_FLOAT));
        CHECK(fixture.pipelines->pipeline_count() == before + 4);
    }

    TEST_CASE("pipeline_cache - a missing shader fails creation with SHADER_NOT_FOUND naming the key") {
        temp_directory directory("pipeline_shaders");
        CONJURE_REQUIRE_OK(directory.write_text("index.json", "{ \"version\": 1, \"shaders\": [] }"));
        std::unique_ptr<rhi::device> device = renderer_support::make_device();
        core::result<std::unique_ptr<rhi::shader_library>> empty = rhi::shader_library::open(*device, directory.path());
        CONJURE_REQUIRE_OK(empty);
        core::result<binding_layouts> layouts = binding_layouts::create(*device->native_device());
        CONJURE_REQUIRE_OK(layouts);
        const core::result<std::unique_ptr<pipeline_cache>> created = pipeline_cache::create(
          *device->native_device(),
          *empty.value(),
          layouts.value()
        );
        CONJURE_CHECK_ERROR(created, renderer_error::SHADER_NOT_FOUND);
        const std::string message = created.get_error().message();
        CHECK(message.find("shader 'forward_pbr'") != std::string::npos);
        CHECK(message.find("entry 'vs_main'") != std::string::npos);
        CHECK(message.find("defines ''") != std::string::npos);
        CHECK(device->null_stats()->graphicsPipelinesCreated == 0);
    }

    TEST_CASE(
      "binding_set_cache - equal descriptions share one set, different ones do not, and idle sets are evicted"
    ) {
        cache_fixture fixture = {};
        nvrhi::IDevice &native = *fixture.device->native_device();
        nvrhi::BufferDesc constantsDesc = {};
        constantsDesc.byteSize = 1024;
        constantsDesc.isConstantBuffer = true;
        constantsDesc.isVolatile = true;
        constantsDesc.maxVersions = 16;
        constantsDesc.debugName = "constants";
        nvrhi::BufferHandle constants = native.createBuffer(constantsDesc);
        REQUIRE(constants != nullptr);
        // A structured view needs a buffer with a stride (NVRHI's validation layer refuses a constant buffer).
        nvrhi::BufferDesc structuredDesc = {};
        structuredDesc.byteSize = 256;
        structuredDesc.structStride = 16;
        structuredDesc.debugName = "structured";
        nvrhi::BufferHandle structured = native.createBuffer(structuredDesc);
        REQUIRE(structured != nullptr);
        nvrhi::TextureDesc textureDesc = {};
        textureDesc.width = 4;
        textureDesc.height = 4;
        textureDesc.format = nvrhi::Format::RGBA8_UNORM;
        textureDesc.debugName = "texture";
        nvrhi::TextureHandle textureA = native.createTexture(textureDesc);
        nvrhi::TextureHandle textureB = native.createTexture(textureDesc);
        REQUIRE(textureA != nullptr);
        REQUIRE(textureB != nullptr);
        nvrhi::SamplerHandle sampler = native.createSampler(nvrhi::SamplerDesc());
        REQUIRE(sampler != nullptr);

        const auto describe = [&](nvrhi::ITexture *texture) {
            nvrhi::BindingSetDesc desc = {};
            desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, constants));
            desc.addItem(nvrhi::BindingSetItem::PushConstants(1, 16));
            desc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, texture));
            desc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, texture));
            desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(2, structured));
            desc.addItem(nvrhi::BindingSetItem::Sampler(0, sampler));
            desc.addItem(nvrhi::BindingSetItem::Sampler(1, sampler));
            return desc;
        };
        binding_set_cache cache(native);
        nvrhi::IBindingSet *first = cache.get(fixture.layouts.fullscreen, describe(textureA));
        REQUIRE(first != nullptr);
        CHECK(cache.get(fixture.layouts.fullscreen, describe(textureA)) == first);
        CHECK(cache.size() == 1);
        CHECK(cache.hits() == 1);
        CHECK(cache.misses() == 1);
        nvrhi::IBindingSet *second = cache.get(fixture.layouts.fullscreen, describe(textureB));
        CHECK(second != first);
        CHECK(cache.size() == 2);
        CHECK(
          binding_set_cache::hash(
            fixture.layouts.fullscreen,
            describe(textureA)
          ) != binding_set_cache::hash(fixture.layouts.fullscreen, describe(textureB))
        );
        CHECK(
          binding_set_cache::hash(
            fixture.layouts.fullscreen,
            describe(textureA)
          ) == binding_set_cache::hash(fixture.layouts.fullscreen, describe(textureA))
        );
        CHECK(
          binding_set_cache::hash(
            fixture.layouts.fullscreen,
            describe(textureA)
          ) != binding_set_cache::hash(fixture.layouts.imgui, describe(textureA))
        );
        // Sets stay while used; an idle one is evicted after the window.
        cache.end_frame(1);
        CHECK(cache.get(fixture.layouts.fullscreen, describe(textureA)) == first);
        for (u64 frame = 2; frame < 60; ++frame) {
            cache.end_frame(frame);
        }
        CHECK(cache.size() == 2);
        for (u64 frame = 60; frame < 130; ++frame) {
            cache.end_frame(frame);
        }
        CHECK(cache.size() == 0);
        nvrhi::IBindingSet *recreated = cache.get(fixture.layouts.fullscreen, describe(textureA));
        CHECK(recreated != nullptr);
        CHECK(cache.misses() == 3);
        cache.clear();
        CHECK(cache.size() == 0);
    }
}
