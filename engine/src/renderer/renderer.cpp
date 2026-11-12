#include "conjure/renderer/renderer.h"
#include "conjure/renderer/camera.h"
#include "conjure/renderer/log_category.h"
#include "conjure/renderer/renderer_error.h"
#include "renderer/culling.h"
#include "renderer/exposure_math.h"
#include "renderer/read_back_decode.h"
#include "renderer/render_target_impl.h"
#include "renderer/renderer_impl.h"
#include "renderer/shadows/cascade_builder.h"

#include "rhi/texture_format_mapping.h"
#include "conjure/core/assert.h"
#include "conjure/core/error.h"
#include "conjure/core/hash.h"
#include "conjure/core/jobs/parallel_for.h"
#include "conjure/core/platform_detect.h"
#include "conjure/rhi/rhi_error.h"

#include <nvrhi/utils.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <mutex>
#include <numeric>
#include <utility>

// The renderer (docs/design/02_renderer.md 3.1): begin_frame (acquire, uploads), prepare (resolve
// ids, cull, sort, fill instances, lights, shadow tiles), one render graph per view, ImGui, present.

namespace conjure::renderer {

namespace {
    constexpr u32 PARALLEL_FILL_THRESHOLD = 2048;
    constexpr u32 PARALLEL_FILL_GRAIN = 1024;
    constexpr f32 SPOT_FOV_MARGIN_RADIANS = 0.05f;
    constexpr f32 LOCAL_SHADOW_NEAR_PLANE = gpu::LOCAL_SHADOW_NEAR_PLANE;

    // An invalid near plane is clamped into this range; the upper bound keeps near + the default depth
    // range representable, so the far plane derived from it always lies beyond the near plane.
    constexpr f32 MIN_CAMERA_NEAR = 1.0e-4f;
    constexpr f32 MAX_CAMERA_NEAR = 1.0e6f;

    // An invalid perspective field of view is clamped to [CAMERA_FOV_MARGIN_RADIANS, PI - CAMERA_FOV_MARGIN_RADIANS].
    constexpr f32 CAMERA_FOV_MARGIN_RADIANS = 0.01f;

    /// <summary>
    /// Face directions and up vectors of the six point-light shadow projections (+X, -X, +Y, -Y, +Z, -Z).
    /// </summary>
    constexpr std::array<math::vec3, 6> CUBE_FACE_FORWARD = {
      math::vec3(1.0f, 0.0f, 0.0f),  math::vec3(-1.0f, 0.0f, 0.0f), math::vec3(0.0f, 1.0f, 0.0f),
      math::vec3(0.0f, -1.0f, 0.0f), math::vec3(0.0f, 0.0f, 1.0f),  math::vec3(0.0f, 0.0f, -1.0f),
    };
    constexpr std::array<math::vec3, 6> CUBE_FACE_UP = {
      math::vec3(0.0f, 1.0f, 0.0f), math::vec3(0.0f, 1.0f, 0.0f), math::vec3(0.0f, 0.0f, -1.0f),
      math::vec3(0.0f, 0.0f, 1.0f), math::vec3(0.0f, 1.0f, 0.0f), math::vec3(0.0f, 1.0f, 0.0f),
    };

    [[nodiscard]] u32 shadow_filter_bits(shadow_filter filter, shadow_filter fallback) noexcept {
        const shadow_filter resolved = filter == shadow_filter::DEFAULT ? fallback : filter;
        switch (resolved) {
            case shadow_filter::HARD:
                return 1;
            case shadow_filter::PCSS:
                return 3;
            case shadow_filter::PCF:
            case shadow_filter::DEFAULT:
                return 2;
        }
        return 2;
    }

    [[nodiscard]] f32 light_importance(const light &value, const math::vec3 &cameraPosition) noexcept {
        if (value.type == light_type::DIRECTIONAL) {
            return std::numeric_limits<f32>::max();
        }
        const math::vec3 offset = value.position - cameraPosition;
        const f32 distanceSquared = std::max(glm::dot(offset, offset), 1.0e-2f);
        const f32 importance = value.intensity * value.range * value.range / distanceSquared;
        // Intensity, range and position are user data and may be NaN, which compares neither greater
        // nor less than any value and would make the std::sort comparators below inconsistent (no
        // strict weak ordering, undefined behaviour). Such a light is the least important one.
        return std::isnan(importance) ? 0.0f : importance;
    }

    [[nodiscard]] draw_variant variant_of(alpha_mode mode) noexcept {
        switch (mode) {
            case alpha_mode::MASK:
                return draw_variant::MASKED;
            case alpha_mode::BLEND:
                return draw_variant::BLENDED;
            case alpha_mode::SOLID:
                break;
        }
        return draw_variant::OPAQUE_SURFACE;
    }

    [[nodiscard]] core::error device_lost_error() {
        return core::make_error(rhi::rhi_error::DEVICE_LOST, "the graphics device was lost");
    }

    /// <summary>
    /// True when clip_from_view accepts the camera (camera.cpp valid_camera, the aspect aside) and every
    /// field its projection uses is finite.
    /// </summary>
    [[nodiscard]] bool usable_camera(const camera &value) noexcept {
        const bool planes = std::isfinite(value.nearPlane) && std::isfinite(value.farPlane) && value.nearPlane > 0.0f
          && value.farPlane > value.nearPlane;
        if (!planes) {
            return false;
        }
        if (value.projection == projection_type::PERSPECTIVE) {
            return value.verticalFovRadians > 0.0f && value.verticalFovRadians < math::PI;
        }
        return std::isfinite(value.orthographicHeight) && value.orthographicHeight > 0.0f;
    }

    /// <summary>
    /// A usable camera unchanged; otherwise the camera with every field its projection uses clamped or
    /// reset to the default, so the result always passes usable_camera. The field the projection does
    /// not use is left untouched.
    /// </summary>
    [[nodiscard]] camera sanitised_camera(const camera &value) noexcept {
        if (usable_camera(value)) {
            return value;
        }
        const camera defaults = {};
        camera result = value;
        result.nearPlane = defaults.nearPlane;
        if (std::isfinite(value.nearPlane)) {
            result.nearPlane = math::clamp(value.nearPlane, MIN_CAMERA_NEAR, MAX_CAMERA_NEAR);
        }
        if (!std::isfinite(value.farPlane) || !(value.farPlane > result.nearPlane)) {
            result.farPlane = result.nearPlane + (defaults.farPlane - defaults.nearPlane);
        }

        // valid_camera treats every projection but PERSPECTIVE as orthographic.
        if (value.projection == projection_type::PERSPECTIVE) {
            result.verticalFovRadians = defaults.verticalFovRadians;
            if (std::isfinite(value.verticalFovRadians)) {
                const f32 maxFov = math::PI - CAMERA_FOV_MARGIN_RADIANS;
                result.verticalFovRadians = math::clamp(value.verticalFovRadians, CAMERA_FOV_MARGIN_RADIANS, maxFov);
            }
            return result;
        }
        if (!std::isfinite(value.orthographicHeight) || !(value.orthographicHeight > 0.0f)) {
            result.orthographicHeight = defaults.orthographicHeight;
        }
        return result;
    }

    /// <summary>
    /// The outer cone of a spot light as the light record and its shadow tile both use it: user data
    /// clamped to (0, PI / 2], NaN replaced by the default cone.
    /// </summary>
    [[nodiscard]] f32 spot_outer_cone(const light &value) noexcept {
        if (std::isnan(value.outerConeRadians)) {
            const light defaults = {};
            return defaults.outerConeRadians;
        }
        return math::clamp(value.outerConeRadians, 1.0e-3f, math::HALF_PI);
    }

    /// <summary>
    /// The HLSL-side light record of a scene light.
    /// </summary>
    [[nodiscard]] gpu::light make_gpu_light(const light &value, u32 tileIndex, shadow_filter defaultFilter) noexcept {
        gpu::light record = {};
        record.positionRange = math::vec4(value.position, std::max(value.range, 1.0e-3f));
        const math::vec3 direction = math::safe_normalize(value.direction, math::vec3(0.0f, -1.0f, 0.0f));
        const f32 outer = spot_outer_cone(value);
        const f32 inner = math::clamp(value.innerConeRadians, 0.0f, outer);
        record.directionCosOuter = math::vec4(direction, std::cos(outer));
        record.colourIntensity = math::vec4(value.colour, std::max(value.intensity, 0.0f));
        record.params = math::vec4(
          std::cos(inner),
          gpu::bits_as_float(static_cast<u32>(value.type)),
          gpu::bits_as_float(tileIndex),
          gpu::bits_as_float(shadow_filter_bits(value.filter, defaultFilter))
        );
        return record;
    }

    [[nodiscard]] rhi::texture_format swapchain_format(const rhi::swapchain *swapchain) noexcept {
        return swapchain != nullptr ? swapchain->desc().format : rhi::texture_format::RGBA8_SRGB;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// render_stats

void render_stats::reset() noexcept {
    *this = render_stats();
}

bool render_stats::executed(std::string_view passName) const noexcept {
    return count(passName) > 0;
}

u32 render_stats::count(std::string_view passName) const noexcept {
    u32 total = 0;
    for (const char *name : passNames) {
        if (passName == name) {
            ++total;
        }
    }
    return total;
}

/*===========================================================================*/
/*===========================================================================*/
// Construction

renderer::renderer(create_tag) {}

renderer::~renderer() {
    if (mImpl == nullptr) {
        return;
    }
    // Everything recorded must have finished before the handles die (design 5).
    if (mImpl->desc.device != nullptr && !mImpl->lost) {
        mImpl->desc.device->wait_idle();
    }
    for (auto &[key, target] : mImpl->targets) {
        if (target != nullptr && mImpl->resources != nullptr) {
            const render_target::impl &targetState = renderer_access::state(*target);
            if (targetState.colourId.is_valid()) {
                mImpl->resources->unregister_external_texture(targetState.colourId);
            }
        }
    }
    mImpl->targets.clear();
}

core::result<std::unique_ptr<renderer>> renderer::create(const renderer_desc &desc) {
    if (desc.device == nullptr || desc.shaders == nullptr) {
        return core::make_error(
          core::error_code::INVALID_ARGUMENT,
          "renderer::create: a device and a shader library are required"
        );
    }
    if (desc.device->is_lost()) {
        return device_lost_error();
    }

    // The allocator's quadtree only grants tiles whose edge divides the atlas edge by a power of two,
    // and the allocator, the texture, the tile rectangles and the texel size must agree on one edge.
    const u32 atlasResolution = std::bit_floor(
      std::clamp<u32>(desc.atlasResolution, MIN_ATLAS_RESOLUTION, MAX_ATLAS_RESOLUTION)
    );
    if (atlasResolution != desc.atlasResolution) {
        CONJURE_LOG_WARN(
          LOG_RENDERER,
          "atlasResolution {} is not a power of two in [{}, {}]; using {}",
          desc.atlasResolution,
          MIN_ATLAS_RESOLUTION,
          MAX_ATLAS_RESOLUTION,
          atlasResolution
        );
    }
    std::unique_ptr<renderer> created = std::make_unique<renderer>(create_tag{});
    created->mImpl = std::make_unique<impl>(atlasResolution);
    impl &state = *created->mImpl;
    state.desc = desc;
    state.device = desc.device->native_device();
    state.renderThread = std::this_thread::get_id();
    nvrhi::IDevice &device = *state.device;

    state.commandList = device.createCommandList(nvrhi::CommandListParameters());
    if (state.commandList == nullptr) {
        return core::make_error(renderer_error::RESOURCE_CREATION_FAILED, "the device refused the frame command list");
    }
    CONJURE_TRY_ASSIGN(state.layouts, binding_layouts::create(device));
    CONJURE_TRY_ASSIGN(state.pipelines, pipeline_cache::create(device, *desc.shaders, state.layouts));
    state.framebuffers = std::make_unique<framebuffer_cache>(device);
    state.pool = std::make_unique<transient_pool>(device, state.framebuffers.get());
    state.bindingSets = std::make_unique<binding_set_cache>(device);
    CONJURE_TRY(state.frame.create(device, desc.cascadeResolution, atlasResolution));

    // Samplers of the view sets: comparison (NVRHI compares with Less; shadows.hlsli inverts the
    // result for reverse-Z), linear clamp, point clamp.
    nvrhi::SamplerDesc shadowSampler = {};
    shadowSampler.setAllFilters(true);
    shadowSampler.setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
    shadowSampler.reductionType = nvrhi::SamplerReductionType::Comparison;
    state.shadowSampler = device.createSampler(shadowSampler);
    nvrhi::SamplerDesc linearClamp = {};
    linearClamp.setAllFilters(true);
    linearClamp.setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
    state.linearClampSampler = device.createSampler(linearClamp);
    nvrhi::SamplerDesc pointClamp = {};
    pointClamp.setAllFilters(false);
    pointClamp.setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
    state.pointClampSampler = device.createSampler(pointClamp);
    if (state.shadowSampler == nullptr || state.linearClampSampler == nullptr || state.pointClampSampler == nullptr) {
        return core::make_error(renderer_error::RESOURCE_CREATION_FAILED, "the device refused a view sampler");
    }

    const u32 releaseDelay = desc.swapchain != nullptr ? desc.swapchain->desc().bufferCount + 1 : 3;
    state.commandList->open();
    core::result<std::unique_ptr<gpu_resource_cache>> resources =
      gpu_resource_cache::create(device, state.layouts, *state.commandList, releaseDelay);
    if (!resources.has_value()) {
        state.commandList->close();
        return resources.get_error();
    }
    state.resources = std::move(resources).value();

    // Exposure states start at EV 0 with the "first frame" marker so the adapt pass snaps to the scene.
    for (u32 viewIndex = 0; viewIndex < MAX_VIEWS; ++viewIndex) {
        gpu::exposure_state initial = {};
        initial.exposure = exposure::exposure_from_ev100(0.0f);
        state.commandList->writeBuffer(state.frame.view(viewIndex).exposureState, &initial, sizeof(initial));
    }

    // The split-sum BRDF lookup table, baked once (design R13).
    {
        nvrhi::FramebufferDesc lutFramebuffer = {};
        lutFramebuffer.addColorAttachment(state.frame.brdf_lut());
        nvrhi::IFramebuffer *framebuffer = state.framebuffers->get(lutFramebuffer);
        nvrhi::ITexture *white = state.resources->fallback_texture(material_texture::BASE_COLOUR).texture.Get();
        nvrhi::BindingSetDesc setDesc = {};
        setDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, state.frame.view_constants()));
        setDesc.addItem(nvrhi::BindingSetItem::PushConstants(1, gpu::DRAW_CONSTANTS_SIZE));
        setDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, white));
        setDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, white));
        setDesc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(2, state.frame.view(0).exposureState));
        setDesc.addItem(nvrhi::BindingSetItem::Sampler(0, state.linearClampSampler));
        setDesc.addItem(nvrhi::BindingSetItem::Sampler(1, state.pointClampSampler));
        nvrhi::IBindingSet *set = state.bindingSets->get(state.layouts.fullscreen, setDesc);
        if (framebuffer == nullptr || set == nullptr) {
            state.commandList->close();
            return core::make_error(
              renderer_error::RESOURCE_CREATION_FAILED,
              "the device refused the BRDF LUT framebuffer"
            );
        }
        const gpu::view_constants constants = {};
        state.commandList->writeBuffer(state.frame.view_constants(), &constants, sizeof(constants));
        nvrhi::GraphicsState graphics = {};
        graphics.pipeline = state.pipelines->graphics(graphics_variant::BRDF_LUT);
        graphics.framebuffer = framebuffer;
        graphics.viewport.addViewportAndScissorRect(
          nvrhi::Viewport(static_cast<f32>(BRDF_LUT_SIZE), static_cast<f32>(BRDF_LUT_SIZE))
        );
        graphics.addBindingSet(set);
        state.commandList->setGraphicsState(graphics);
        const gpu::draw_constants push = {};
        state.commandList->setPushConstants(&push, sizeof(push));
        nvrhi::DrawArguments arguments = {};
        arguments.vertexCount = 3;
        state.commandList->draw(arguments);
    }
    state.commandList->close();
    nvrhi::ICommandList *initialList = state.commandList.Get();
    device.executeCommandLists(&initialList, 1);

    if (desc.swapchain != nullptr) {
        CONJURE_TRY(state.pipelines->blit(swapchain_format(desc.swapchain)));
    }

    if (CONJURE_DEV_TOOLS_ENABLED && desc.device->capabilities().timerQueries) {
        state.timers = std::make_unique<timer_ring>(device);
    }

    // Pass objects (one per graph row, cascades and bloom mips enumerated).
    pass_shared &shared = state.shared;
    shared.device = state.device;
    shared.pipelines = state.pipelines.get();
    shared.resources = state.resources.get();
    shared.frame = &state.frame;
    shared.bindingSets = state.bindingSets.get();
    shared.layouts = &state.layouts;
    shared.shadowSampler = state.shadowSampler;
    shared.linearClampSampler = state.linearClampSampler;
    shared.pointClampSampler = state.pointClampSampler;
    for (u32 cascade = 0; cascade < MAX_SHADOW_CASCADES; ++cascade) {
        state.cascadePasses[cascade] = std::make_unique<shadow_cascade_pass>(shared, cascade);
    }
    state.atlasPass = std::make_unique<shadow_atlas_pass>(shared);
    state.prepass = std::make_unique<depth_prepass>(shared);
    state.lightCull = std::make_unique<light_cull_pass>(shared);
    state.ssao = std::make_unique<ssao_gtao_pass>(shared);
    state.ssaoBlurH = std::make_unique<ssao_blur_pass>(shared, false);
    state.ssaoBlurV = std::make_unique<ssao_blur_pass>(shared, true);
    state.forwardOpaque = std::make_unique<forward_pass>(shared, false);
    state.skybox = std::make_unique<skybox_pass>(shared);
    state.forwardBlend = std::make_unique<forward_pass>(shared, true);
    state.worldSprites = std::make_unique<sprite_pass>(shared, sprite_space::WORLD);
    state.debugDraw = std::make_unique<debug_draw_pass>(shared);
    state.exposureHistogram = std::make_unique<exposure_histogram_pass>(shared);
    state.exposureAdapt = std::make_unique<exposure_adapt_pass>(shared);
    for (u32 mip = 0; mip < BLOOM_MIP_COUNT; ++mip) {
        state.bloomDown[mip] = std::make_unique<bloom_down_pass>(shared, mip);
    }
    for (u32 mip = 0; mip + 1 < BLOOM_MIP_COUNT; ++mip) {
        state.bloomUp[mip] = std::make_unique<bloom_up_pass>(shared, mip);
    }
    state.tonemap = std::make_unique<tonemap_pass>(shared);
    state.screenSprites = std::make_unique<sprite_pass>(shared, sprite_space::SCREEN);
    state.blit = std::make_unique<blit_pass>(shared);

#if defined(CONJURE_BUILD_EDITOR) && CONJURE_BUILD_EDITOR
    if (desc.enableImGui && desc.swapchain != nullptr) {
        gpu_resource_cache *cache = state.resources.get();
        imgui_texture_resolver resolver = [cache](gpu_texture_id id) -> nvrhi::ITexture * {
            if (!cache->is_texture_ready(id)) {
                return nullptr;
            }
            return cache->resolve_texture(id).texture.Get();
        };
        CONJURE_TRY_ASSIGN(
          state.imgui,
          imgui_renderer::create(*desc.device, *desc.shaders, swapchain_format(desc.swapchain), std::move(resolver))
        );
    }
#endif

    desc.device->run_garbage_collection();
    CONJURE_LOG_INFO(
      LOG_RENDERER,
      "renderer created on {} ({} pipelines, cascades {}^2, atlas {}^2, {})",
      rhi::to_string(desc.device->backend()),
      state.pipelines->pipeline_count(),
      state.frame.cascade_resolution(),
      state.frame.atlas_resolution(),
      desc.swapchain != nullptr ? "swapchain" : "headless"
    );
    return created;
}

/*===========================================================================*/
/*===========================================================================*/
// Uploads and lookups

gpu_mesh_id renderer::upload_mesh(const assets::asset_id &id, const mesh_asset &mesh) {
    return mImpl->resources->enqueue_mesh(id, mesh);
}

gpu_texture_id renderer::upload_texture(const assets::asset_id &id, const texture_asset &texture) {
    return mImpl->resources->enqueue_texture(id, texture);
}

gpu_material_id renderer::upload_material(const assets::asset_id &id, const material_asset &material) {
    return mImpl->resources->enqueue_material(id, material);
}

gpu_environment_id renderer::upload_environment(const assets::asset_id &id, const environment_asset &env) {
    return mImpl->resources->enqueue_environment(id, env);
}

void renderer::release_mesh(gpu_mesh_id id) {
    mImpl->resources->release_mesh(id, mImpl->frameIndex);
}

void renderer::release_texture(gpu_texture_id id) {
    mImpl->resources->release_texture(id, mImpl->frameIndex);
}

void renderer::release_material(gpu_material_id id) {
    mImpl->resources->release_material(id, mImpl->frameIndex);
}

void renderer::release_environment(gpu_environment_id id) {
    mImpl->resources->release_environment(id, mImpl->frameIndex);
}

gpu_mesh_id renderer::find_mesh(const assets::asset_id &id) const noexcept {
    return mImpl->resources->find_mesh(id);
}

gpu_texture_id renderer::find_texture(const assets::asset_id &id) const noexcept {
    return mImpl->resources->find_texture(id);
}

gpu_material_id renderer::find_material(const assets::asset_id &id) const noexcept {
    return mImpl->resources->find_material(id);
}

gpu_environment_id renderer::find_environment(const assets::asset_id &id) const noexcept {
    return mImpl->resources->find_environment(id);
}

/*===========================================================================*/
/*===========================================================================*/
// Render targets

core::result<render_target_id> renderer::create_render_target(const render_target_desc &desc) {
    if (desc.size.x == 0 || desc.size.y == 0) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "create_render_target: zero size");
    }
    if (!is_render_target_colour_format(desc.colourFormat)) {
        return core::make_error(
          core::error_code::INVALID_ARGUMENT,
          "create_render_target: {} is not a render target colour format",
          rhi::to_string(desc.colourFormat)
        );
    }
    if (mImpl->lost) {
        return device_lost_error();
    }
    CONJURE_TRY(mImpl->pipelines->blit(desc.colourFormat));

    std::unique_ptr<render_target> target = std::make_unique<render_target>();
    render_target::impl &targetState = renderer_access::state(*target);
    targetState.desc = desc;
    nvrhi::TextureDesc colour = {};
    colour.width = desc.size.x;
    colour.height = desc.size.y;
    colour.format = rhi::to_nvrhi_format(desc.colourFormat);
    colour.debugName = "render_target.colour";
    colour.isRenderTarget = true;
    colour.isShaderResource = true;
    colour.initialState = nvrhi::ResourceStates::ShaderResource;
    colour.keepInitialState = true;
    colour.useClearValue = true;
    colour.clearValue = nvrhi::Color(0.0f);
    targetState.colour = mImpl->device->createTexture(colour);
    if (targetState.colour == nullptr) {
        return core::make_error(
          renderer_error::RESOURCE_CREATION_FAILED,
          "the device refused the render target colour texture"
        );
    }
    if (desc.withDepth) {
        nvrhi::TextureDesc depth = {};
        depth.width = desc.size.x;
        depth.height = desc.size.y;
        depth.format = nvrhi::Format::D32;
        depth.debugName = "render_target.depth";
        depth.isRenderTarget = true;
        depth.isShaderResource = true;
        depth.initialState = nvrhi::ResourceStates::ShaderResource;
        depth.keepInitialState = true;
        depth.useClearValue = true;
        depth.clearValue = nvrhi::Color(0.0f);
        targetState.depth = mImpl->device->createTexture(depth);
        if (targetState.depth == nullptr) {
            return core::make_error(
              renderer_error::RESOURCE_CREATION_FAILED,
              "the device refused the render target depth texture"
            );
        }
    }
    targetState.colourId = mImpl->resources->register_external_texture(
      targetState.colour.Get(),
      desc.colourFormat,
      desc.size.x,
      desc.size.y
    );
    return mImpl->targets.emplace(std::move(target));
}

core::result<void> renderer::resize_render_target(render_target_id id, math::uvec2 size) {
    std::unique_ptr<render_target> *slot = mImpl->targets.find(id);
    if (slot == nullptr || *slot == nullptr) {
        return core::make_error(core::error_code::NOT_FOUND, "resize_render_target: unknown target");
    }
    if (size.x == 0 || size.y == 0) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "resize_render_target: zero size");
    }
    render_target::impl &targetState = renderer_access::state(**slot);
    render_target_desc desc = targetState.desc;
    desc.size = size;
    // Build the replacement first so a refusal leaves the old target intact.
    core::result<render_target_id> replacement = create_render_target(desc);
    if (!replacement.has_value()) {
        return replacement.get_error();
    }
    std::unique_ptr<render_target> *created = mImpl->targets.find(replacement.value());
    if (!mImpl->lost) {
        mImpl->desc.device->wait_idle();
    }
    mImpl->framebuffers->remove_texture(targetState.colour.Get());
    if (targetState.depth != nullptr) {
        mImpl->framebuffers->remove_texture(targetState.depth.Get());
    }
    mImpl->resources->unregister_external_texture(targetState.colourId);
    render_target::impl &createdState = renderer_access::state(**created);
    targetState = std::move(createdState);
    createdState = render_target::impl();
    static_cast<void>(mImpl->targets.erase(replacement.value()));
    return core::result<void>();
}

void renderer::destroy_render_target(render_target_id id) {
    std::unique_ptr<render_target> *slot = mImpl->targets.find(id);
    if (slot == nullptr || *slot == nullptr) {
        return;
    }
    render_target::impl &targetState = renderer_access::state(**slot);
    if (!mImpl->lost) {
        mImpl->desc.device->wait_idle();
    }
    mImpl->framebuffers->remove_texture(targetState.colour.Get());
    if (targetState.depth != nullptr) {
        mImpl->framebuffers->remove_texture(targetState.depth.Get());
    }
    mImpl->resources->unregister_external_texture(targetState.colourId);
    static_cast<void>(mImpl->targets.erase(id));
}

render_target *renderer::get_render_target(render_target_id id) noexcept {
    std::unique_ptr<render_target> *slot = mImpl->targets.find(id);
    return slot == nullptr ? nullptr : slot->get();
}

/*===========================================================================*/
/*===========================================================================*/
// Extraction

render_scene &renderer::begin_extract() {
    return mImpl->pair.begin_extract();
}

void renderer::end_extract() {
    mImpl->pair.end_extract();
}

core::result<void> renderer::render_frame() {
    return render(mImpl->pair.read());
}

/*===========================================================================*/
/*===========================================================================*/
// Frame

namespace {

    void append_region_instances(
      renderer_state &state,
      draw_region &region,
      std::span<const draw_item> items,
      std::span<const draw_batch> batches,
      const render_scene &scene
    ) {
        region.firstInstance = static_cast<u32>(state.instanceData.size());
        region.instanceCount = static_cast<u32>(items.size());
        state.instanceData.resize(state.instanceData.size() + items.size());
        gpu::instance *destination = state.instanceData.data() + region.firstInstance;
        const auto fill = [&scene, items, destination](usize begin, usize end) {
            for (usize index = begin; index < end; ++index) {
                const mesh_instance &source = scene.meshes[items[index].instanceIndex];
                destination[index] = gpu::make_instance(
                  source.worldFromLocal,
                  source.entityId,
                  source.receivesShadows ? gpu::INSTANCE_FLAG_RECEIVES_SHADOWS : 0u
                );
            }
        };
        core::jobs::job_system *jobs = state.desc.jobs;
        if (jobs != nullptr && jobs->worker_count() > 0 && items.size() > PARALLEL_FILL_THRESHOLD) {
            core::jobs::parallel_for_range(*jobs, items.size(), PARALLEL_FILL_GRAIN, fill);
        } else {
            fill(0, items.size());
        }
        region.batches.assign(batches.begin(), batches.end());
        region.meshes.clear();
        region.materials.clear();
        for (const draw_batch &batch : batches) {
            const draw_item &first = items[batch.firstItem];
            const mesh_instance &source = scene.meshes[first.instanceIndex];
            const gpu_mesh &mesh = state.resources->resolve_mesh(source.mesh);
            const u32 slot = std::min<u32>(first.materialSlot, MAX_MATERIAL_SLOTS - 1);
            const gpu_material &material = state.resources->resolve_material(source.materials[slot]);
            region.meshes.push_back(&mesh);
            region.materials.push_back(&material);
        }
    }

    /// <summary>
    /// Fills a region from the visible instances: one item per submesh, sorted and batched.
    /// </summary>
    void build_region(
      renderer_state &state,
      draw_region &region,
      std::span<const u32> visibleInstances,
      const render_scene &scene,
      const math::mat4 &viewFromWorld,
      f32 farPlane,
      bool shadowsOnly,
      u32 *opaqueBatchCount
    ) {
        draw_list_builder &builder = state.drawLists;
        builder.clear();
        for (const u32 instanceIndex : visibleInstances) {
            const mesh_instance &source = scene.meshes[instanceIndex];
            const gpu_mesh &mesh = state.resources->resolve_mesh(source.mesh);
            const bool meshLive = &mesh != &state.resources->fallback_mesh();
            // cull_instances rejects invalid bounds (box_intersects), so every visible instance has valid ones.
            CONJURE_ASSERT(source.worldBounds.is_valid(), "build_region: a visible instance has invalid bounds");
            const math::vec3 centre = source.worldBounds.center();
            const f32 viewDepth = -(viewFromWorld * math::vec4(centre, 1.0f)).z;
            for (u32 submeshIndex = 0; submeshIndex < mesh.submeshes.size(); ++submeshIndex) {
                const submesh &part = mesh.submeshes[submeshIndex];
                const u32 slot = std::min<u32>(part.materialSlot, MAX_MATERIAL_SLOTS - 1);
                const gpu_material_id materialId = source.materials[slot];
                const gpu_material &material = state.resources->resolve_material(materialId);
                const bool materialLive = &material != &state.resources->fallback_material();
                draw_item item = {};
                item.instanceIndex = instanceIndex;
                item.submesh = submeshIndex;
                item.materialSlot = slot;
                // Batches key on the slot index alone, and a released id frees its slot at once: a stale
                // id and the live id that reuses its slot would merge into one batch. Every id that
                // resolves to the fallback (stale, unknown or pending) shares the fallback's key instead.
                item.meshIndex = meshLive ? source.mesh.index : MAX_DRAW_KEY_INDEX;
                item.materialIndex = materialLive ? materialId.index : MAX_DRAW_KEY_INDEX;
                item.variant = variant_of(material.alphaMode);
                item.doubleSided = material.doubleSided;
                item.layer = 0;
                if (shadowsOnly && item.variant == draw_variant::BLENDED) {
                    continue;
                }
                builder.add(item, viewDepth, farPlane);
            }
        }
        builder.build();
        append_region_instances(state, region, builder.items(), builder.batches(), scene);
        if (opaqueBatchCount != nullptr) {
            u32 opaque = 0;
            for (const draw_batch &batch : region.batches) {
                if (batch.variant != draw_variant::BLENDED) {
                    ++opaque;
                }
            }
            *opaqueBatchCount = opaque;
        }
    }

    [[nodiscard]] math::mat4 local_light_clip_from_world(const light &value, u32 face, f32 fovRadians) noexcept {
        camera projection = {};
        projection.projection = projection_type::PERSPECTIVE;
        projection.verticalFovRadians = fovRadians;
        projection.nearPlane = LOCAL_SHADOW_NEAR_PLANE;
        projection.farPlane = std::max(value.range, LOCAL_SHADOW_NEAR_PLANE * 2.0f);
        math::vec3 forward = math::safe_normalize(value.direction, math::vec3(0.0f, -1.0f, 0.0f));
        math::vec3 up = math::absolute(forward.y) < 0.99f ? math::vec3(0.0f, 1.0f, 0.0f) : math::vec3(1.0f, 0.0f, 0.0f);
        if (value.type == light_type::POINT) {
            forward = CUBE_FACE_FORWARD[face];
            up = CUBE_FACE_UP[face];
        }
        const math::mat4 viewFromWorld = math::look_at(value.position, value.position + forward, up);
        return clip_from_view(projection, 1.0f) * viewFromWorld;
    }
}

namespace {

void prepare_view(renderer_state &state, frame_view_state &stateOfView, const render_scene &scene);

void prepare_frame(renderer_state &state, const render_scene &scene, nvrhi::ITexture *backBuffer) {
    state.preparedViewCount = 0;
    state.instanceData.clear();
    state.atlasTileData.clear();
    state.lightTileBase.assign(scene.lights.size(), INVALID_INDEX);

    // Views beyond the budget are dropped with a WARN once per change of count.
    const u32 viewCount = static_cast<u32>(std::min<usize>(scene.views.size(), MAX_VIEWS));
    const u32 viewOverflow = static_cast<u32>(scene.views.size()) - viewCount;
    if (viewOverflow != state.lastViewOverflow) {
        if (viewOverflow > 0) {
            CONJURE_LOG_WARN(
              LOG_RENDERER,
              "{} views exceed MAX_VIEWS ({}); the last {} are skipped",
              scene.views.size(),
              MAX_VIEWS,
              viewOverflow
            );
        }
        state.lastViewOverflow = viewOverflow;
    }
    state.stats.viewsSkipped += viewOverflow;

    // Sprites and debug lines are shared by every view: one upload each.
    state.sprites.build(scene.sprites);
    if (state.sprites.sprite_count() > 0) {
        const std::span<const sprite_vertex> vertices = state.sprites.vertices();
        const std::span<const u32> indices = state.sprites.indices();
        if (state.frame.ensure_sprite_capacity(static_cast<u32>(vertices.size()), static_cast<u32>(indices.size()))) {
            state.commandList->writeBuffer(state.frame.sprite_vertices(), vertices.data(), vertices.size_bytes());
            state.commandList->writeBuffer(state.frame.sprite_indices(), indices.data(), indices.size_bytes());
        }
    }
    bool worldSprites = false;
    bool screenSprites = false;
    for (const sprite_run &run : state.sprites.runs()) {
        worldSprites = worldSprites || run.space == sprite_space::WORLD;
        screenSprites = screenSprites || run.space == sprite_space::SCREEN;
    }
    const std::span<const debug_vertex> depthTested = scene.debugDraw.depth_tested();
    const std::span<const debug_vertex> overlay = scene.debugDraw.overlay();
    const u32 debugVertexCount = static_cast<u32>(depthTested.size() + overlay.size());
    if (debugVertexCount > 0 && state.frame.ensure_debug_capacity(debugVertexCount)) {
        if (!depthTested.empty()) {
            state.commandList->writeBuffer(
              state.frame.debug_vertices(),
              depthTested.data(),
              depthTested.size_bytes(),
              0
            );
        }
        if (!overlay.empty()) {
            state.commandList->writeBuffer(
              state.frame.debug_vertices(),
              overlay.data(),
              overlay.size_bytes(),
              depthTested.size_bytes()
            );
        }
    }

    // Shadow atlas: scene-wide, importance measured from the first view's camera, rendered by the
    // first view that draws with shadows (prepare_view).
    const math::vec3 primaryCamera = viewCount > 0 ? scene.views[0].cameraPosition : math::vec3(0.0f);
    state.atlasRequests.clear();
    state.atlasRequestLights.clear();
    state.atlasKeyOccurrences.clear();
    state.atlasRendered = false;
    const bool shadowsEnabled = scene.shadows.enabled;
    if (shadowsEnabled) {
        std::vector<u32> &casters = state.shadowCasters;
        casters.clear();
        for (u32 index = 0; index < scene.lights.size(); ++index) {
            const light &value = scene.lights[index];
            if (value.castsShadows && value.type != light_type::DIRECTIONAL) {
                casters.push_back(index);
            }
        }
        // std::sort with the scene index as the tie-break is the stable order without the temporary
        // buffer std::stable_sort allocates.
        std::sort(casters.begin(), casters.end(), [&](u32 left, u32 right) {
            const f32 leftImportance = light_importance(scene.lights[left], primaryCamera);
            const f32 rightImportance = light_importance(scene.lights[right], primaryCamera);
            if (leftImportance != rightImportance) {
                return leftImportance > rightImportance;
            }
            return left < right;
        });
        for (const u32 index : casters) {
            const light &value = scene.lights[index];
            atlas_request request = {};
            // The key identifies the light across frames (stable tile coordinates), never its
            // position in scene.lights: inserting a light must not re-key the ones after it.
            const u64 baseKey = atlas_light_key(value);
            u32 &occurrences = state.atlasKeyOccurrences.try_emplace(baseKey, 0u).first->second;
            const u32 occurrence = occurrences;
            ++occurrences;
            request.lightKey = atlas_light_key_occurrence(baseKey, occurrence);
            request.size = clamp_shadow_resolution(value.shadowResolution);
            request.tileCount = value.type == light_type::POINT ? 6 : 1;
            state.atlasRequests.push_back(request);
            state.atlasRequestLights.push_back(index);
        }
    }
    state.atlas.allocate(state.atlasRequests);
    state.stats.shadowTilesDropped = state.atlas.dropped_count();
    state.stats.shadowTilesReused = state.atlas.reused_count();
    const std::span<const atlas_allocation> allocations = state.atlas.allocations();
    const std::span<const atlas_tile> tiles = state.atlas.tiles();
    for (usize request = 0; request < allocations.size(); ++request) {
        const atlas_allocation &allocation = allocations[request];
        if (allocation.tileCount == 0) {
            continue;
        }
        const u32 lightIndex = state.atlasRequestLights[request];
        const light &value = scene.lights[lightIndex];
        state.lightTileBase[lightIndex] = static_cast<u32>(state.atlasTileData.size());
        state.stats.shadowTilesAllocated += allocation.tileCount;
        // The cone is sanitised exactly as in the light record, so a negative or NaN cone of user data
        // never reaches clip_from_view.
        const f32 fov = value.type == light_type::POINT ? math::HALF_PI : std::min(
          2.0f * spot_outer_cone(value) + SPOT_FOV_MARGIN_RADIANS,
          math::PI - 0.1f
        );
        for (u8 face = 0; face < allocation.tileCount; ++face) {
            const atlas_tile &tile = tiles[allocation.firstTile + face];
            gpu::shadow_tile record = {};
            record.clipFromWorld = local_light_clip_from_world(value, face, fov);
            record.atlasRect = tile.uv_rect(state.atlas.atlas_size());
            const f32 texelUv = 1.0f / static_cast<f32>(std::max<u32>(tile.size, 1));
            // y: the light's source radius in world units (PCSS); w: the world size of one texel
            // per unit of distance from the light (perspective tiles).
            record.params = math::vec4(
              texelUv,
              std::max(value.sourceRadius, 0.0f),
              gpu::bits_as_float(shadow_filter_bits(value.filter, scene.shadows.defaultFilter)),
              2.0f * std::tan(fov * 0.5f) * texelUv
            );
            state.atlasTileData.push_back(record);
        }
    }

    // Per view preparation. The view states persist across frames (next_view_state resets one in
    // place), so their draw regions keep the capacity of earlier frames.
    for (u32 viewIndex = 0; viewIndex < viewCount; ++viewIndex) {
        const render_view &view = scene.views[viewIndex];
        frame_view_state &stateOfView = state.next_view_state();
        stateOfView.view = &view;
        stateOfView.viewIndex = viewIndex;
        stateOfView.worldSprites = worldSprites;
        stateOfView.screenSprites = screenSprites;
        stateOfView.debugDepthTestedVertices = static_cast<u32>(depthTested.size());
        stateOfView.debugOverlayVertices = static_cast<u32>(overlay.size());

        // Target.
        if (view.target.is_valid()) {
            std::unique_ptr<render_target> *slot = state.targets.find(view.target);
            if (slot == nullptr || *slot == nullptr) {
                CONJURE_LOG_WARN(LOG_RENDERER, "view {} targets an unknown render target; skipped", viewIndex);
                ++state.stats.viewsSkipped;
                continue;
            }
            render_target::impl &targetState = renderer_access::state(**slot);
            stateOfView.targetTexture = targetState.colour.Get();
            stateOfView.targetSize = targetState.desc.size;
            stateOfView.targetFormat = targetState.desc.colourFormat;
            ++targetState.framesRendered;
        } else {
            if (backBuffer == nullptr) {
                if (!state.warnedNoSwapchain) {
                    CONJURE_LOG_WARN(
                      LOG_RENDERER,
                      "a view targets the swapchain but the renderer is headless; skipped"
                    );
                    state.warnedNoSwapchain = true;
                }
                ++state.stats.viewsSkipped;
                continue;
            }
            const rhi::swapchain_desc &swapchainDesc = state.desc.swapchain->desc();
            stateOfView.targetTexture = backBuffer;
            stateOfView.targetSize = math::uvec2(swapchainDesc.width, swapchainDesc.height);
            stateOfView.targetFormat = swapchainDesc.format;
        }
        math::urect viewport = view.viewport;
        if (viewport.is_empty()) {
            viewport = math::urect{0, 0, stateOfView.targetSize.x, stateOfView.targetSize.y};
        }
        viewport.x = std::min(viewport.x, stateOfView.targetSize.x - 1);
        viewport.y = std::min(viewport.y, stateOfView.targetSize.y - 1);
        viewport.width = std::min(viewport.width, stateOfView.targetSize.x - viewport.x);
        viewport.height = std::min(viewport.height, stateOfView.targetSize.y - viewport.y);
        stateOfView.viewport = viewport;

        prepare_view(state, stateOfView, scene);
        ++state.preparedViewCount;
    }

    // One instance upload for every region of every view.
    const u32 instanceCount = static_cast<u32>(state.instanceData.size());
    if (instanceCount > 0 && state.frame.ensure_instance_capacity(instanceCount)) {
        state.commandList->writeBuffer(
          state.frame.instances(),
          state.instanceData.data(),
          state.instanceData.size() * sizeof(gpu::instance)
        );
    }
}

void prepare_view(renderer_state &state, frame_view_state &stateOfView, const render_scene &scene) {
    const render_view &view = *stateOfView.view;

    // A bad camera in the scene data is clamped with one WARN per view until it is valid again
    // (renderer.h: a bad scene never aborts a frame); clip_from_view would assert on it.
    const camera cameraData = sanitised_camera(view.cameraData);
    const u32 viewBit = 1u << stateOfView.viewIndex;
    if (usable_camera(view.cameraData)) {
        state.invalidCameraViews &= ~viewBit;
    } else if ((state.invalidCameraViews & viewBit) == 0) {
        CONJURE_LOG_WARN(
          LOG_RENDERER,
          "view {}: invalid camera (near {}, far {}, fov {}, height {}) clamped",
          stateOfView.viewIndex,
          view.cameraData.nearPlane,
          view.cameraData.farPlane,
          view.cameraData.verticalFovRadians,
          view.cameraData.orthographicHeight
        );
        state.invalidCameraViews |= viewBit;
    }

    const f32 aspect = static_cast<f32>(
      stateOfView.viewport.width
    ) / static_cast<f32>(std::max<u32>(stateOfView.viewport.height, 1));
    const math::mat4 clipFromView = clip_from_view(cameraData, aspect);
    const math::mat4 clipFromWorld = clipFromView * view.viewFromWorld;
    const math::mat4 worldFromView = glm::inverse(view.viewFromWorld);
    stateOfView.frustum = frustum_from_clip(clipFromWorld);

    const bool noPostProcess = has_flag(view.flags, view_flags::NO_POST_PROCESS);
    stateOfView.postProcess = !noPostProcess;
    stateOfView.autoExposure = stateOfView.postProcess && scene.postProcess.exposure == exposure_mode::AUTOMATIC;
    stateOfView.bloom = stateOfView.postProcess && scene.postProcess.bloomEnabled;
    stateOfView.ssao = scene.ssao.enabled && !noPostProcess;
    stateOfView.shadows = scene.shadows.enabled && !has_flag(view.flags, view_flags::SKIP_SHADOWS);
    stateOfView.drawDebug = has_flag(view.flags, view_flags::DRAW_DEBUG);
    if (scene.environment.environment.is_valid()) {
        stateOfView.environment = &state.resources->resolve_environment(scene.environment.environment);
        stateOfView.hasEnvironment = true;
    } else {
        stateOfView.environment = nullptr;
        stateOfView.hasEnvironment = false;
    }

    // Visibility and the main region.
    cull_options options = {};
    options.layerMask = view.layerMask;
    cull_instances(scene.meshes, stateOfView.frustum, options, state.desc.jobs, state.cullScratch, state.visible);
    state.stats.instancesCulled += static_cast<u32>(scene.meshes.size() - state.visible.size());
    build_region(
      state,
      stateOfView.main,
      state.visible,
      scene,
      view.viewFromWorld,
      cameraData.farPlane,
      false,
      &stateOfView.opaqueBatchCount
    );

    // Lights: directional first, then local lights by importance, clamped per view.
    std::vector<u32> &order = state.lightOrder;
    order.resize(scene.lights.size());
    std::iota(order.begin(), order.end(), 0u);
    // Scene order breaks ties, which makes std::sort produce the stable order without the temporary
    // buffer std::stable_sort allocates.
    std::sort(order.begin(), order.end(), [&](u32 left, u32 right) {
        const bool leftDirectional = scene.lights[left].type == light_type::DIRECTIONAL;
        const bool rightDirectional = scene.lights[right].type == light_type::DIRECTIONAL;
        if (leftDirectional != rightDirectional) {
            return leftDirectional;
        }
        const f32 leftImportance = light_importance(scene.lights[left], view.cameraPosition);
        const f32 rightImportance = light_importance(scene.lights[right], view.cameraPosition);
        if (leftImportance != rightImportance) {
            return leftImportance > rightImportance;
        }
        return left < right;
    });
    const u32 lightCount = static_cast<u32>(std::min<usize>(order.size(), MAX_LIGHTS_PER_VIEW));
    const u32 lightOverflow = static_cast<u32>(order.size()) - lightCount;
    if (lightOverflow != state.lastLightOverflow) {
        if (lightOverflow > 0) {
            CONJURE_LOG_WARN(
              LOG_RENDERER,
              "{} lights exceed MAX_LIGHTS_PER_VIEW ({}); the least important {} are dropped",
              order.size(),
              MAX_LIGHTS_PER_VIEW,
              lightOverflow
            );
        }
        state.lastLightOverflow = lightOverflow;
    }
    state.stats.lightsDropped += lightOverflow;

    // Shadow tiles of this view: the scene's atlas tiles then the cascades.
    state.tileData = state.atlasTileData;
    u32 directionalShadowLight = INVALID_INDEX;
    u32 directionalShadowScene = INVALID_INDEX;
    state.lightData.clear();
    u32 directionalCount = 0;
    for (u32 slot = 0; slot < lightCount; ++slot) {
        const u32 sceneIndex = order[slot];
        const light &value = scene.lights[sceneIndex];
        u32 tileIndex = INVALID_INDEX;
        if (value.type == light_type::DIRECTIONAL) {
            ++directionalCount;
            if (stateOfView.shadows && value.castsShadows && directionalShadowLight == INVALID_INDEX) {
                directionalShadowLight = slot;
                directionalShadowScene = sceneIndex;
            }
        } else if (stateOfView.shadows) {
            tileIndex = state.lightTileBase[sceneIndex];
        }
        state.lightData.push_back(make_gpu_light(value, tileIndex, scene.shadows.defaultFilter));
    }
    stateOfView.lightCount = lightCount;
    stateOfView.directionalLightCount = directionalCount;
    stateOfView.localLightCount = lightCount - directionalCount;
    state.stats.lightsRendered += lightCount;

    // Cascades.
    stateOfView.cascadeCount = 0;
    f32 directionalSourceRadius = 0.0f;
    if (directionalShadowScene != INVALID_INDEX) {
        const light &sun = scene.lights[directionalShadowScene];
        directionalSourceRadius = std::max(sun.sourceRadius, 0.0f);
        const cascade_set cascades = build_cascades(
          cameraData,
          worldFromView,
          aspect,
          sun.direction,
          scene.shadows,
          state.frame.cascade_resolution()
        );
        stateOfView.cascadeCount = cascades.count;
        const u32 filterBits = shadow_filter_bits(sun.filter, scene.shadows.defaultFilter);
        for (u32 index = 0; index < cascades.count; ++index) {
            const cascade &slice = cascades.cascades[index];
            shadow_draw_list &list = stateOfView.cascades[index];
            list.tileIndex = static_cast<u32>(state.tileData.size());
            gpu::shadow_tile record = {};
            record.clipFromWorld = slice.clipFromWorld;
            record.atlasRect = math::vec4(static_cast<f32>(index), 0.0f, 1.0f, 1.0f);
            record.params = math::vec4(
              1.0f / static_cast<f32>(state.frame.cascade_resolution()),
              directionalSourceRadius,
              gpu::bits_as_float(filterBits),
              0.0f
            );
            state.tileData.push_back(record);
            stateOfView.constants.cascadeClipFromWorld[index] = slice.clipFromWorld;
            stateOfView.constants.cascadeSplits[static_cast<glm::length_t>(index)] = slice.farDistance;

            cull_options cascadeOptions = {};
            cascadeOptions.layerMask = view.layerMask;
            cascadeOptions.ignoreFarPlane = true;
            cascadeOptions.shadowCastersOnly = true;
            const math::frustum lightFrustum = frustum_from_clip(slice.clipFromWorld);
            cull_instances(
              scene.meshes,
              lightFrustum,
              cascadeOptions,
              state.desc.jobs,
              state.cullScratch,
              state.visible
            );
            build_region(
              state,
              list.region,
              state.visible,
              scene,
              slice.viewFromWorld,
              2.0f * slice.radius + scene.shadows.casterPadding,
              true,
              nullptr
            );
        }
        for (u32 index = cascades.count; index < MAX_SHADOW_CASCADES; ++index) {
            stateOfView.constants.cascadeSplits[static_cast<glm::length_t>(index)] = scene.shadows.distance;
        }
    }

    // Atlas tiles are rendered once per frame, by the first view that is actually prepared with
    // shadows enabled (the allocation is scene-wide; view 0 may skip shadows or be skipped itself).
    if (!state.atlasRendered && stateOfView.shadows && !state.atlasTileData.empty()) {
        state.atlasRendered = true;
        const std::span<const atlas_allocation> allocations = state.atlas.allocations();
        const std::span<const atlas_tile> tiles = state.atlas.tiles();
        for (usize request = 0; request < allocations.size(); ++request) {
            const atlas_allocation &allocation = allocations[request];
            if (allocation.tileCount == 0) {
                continue;
            }
            const u32 lightIndex = state.atlasRequestLights[request];
            const u32 base = state.lightTileBase[lightIndex];
            for (u8 face = 0; face < allocation.tileCount; ++face) {
                const atlas_tile &tile = tiles[allocation.firstTile + face];
                shadow_draw_list &list = stateOfView.push_atlas_tile();
                list.tileIndex = base + face;
                list.atlasX = tile.x;
                list.atlasY = tile.y;
                list.atlasSize = tile.size;
                cull_options tileOptions = {};
                tileOptions.layerMask = view.layerMask;
                tileOptions.shadowCastersOnly = true;
                const math::mat4 &clip = state.atlasTileData[base + face].clipFromWorld;
                const math::frustum lightFrustum = frustum_from_clip(clip);
                cull_instances(
                  scene.meshes,
                  lightFrustum,
                  tileOptions,
                  state.desc.jobs,
                  state.cullScratch,
                  state.visible
                );
                const light &value = scene.lights[lightIndex];
                const math::mat4 lightView = math::look_at(
                  value.position,
                  value.position + (
                    value.type == light_type::POINT ? CUBE_FACE_FORWARD[face] : math::safe_normalize(value.direction)
                  ),
                  CUBE_FACE_UP[face]
                );
                build_region(state, list.region, state.visible, scene, lightView, value.range, true, nullptr);
            }
        }
    }

    // GPU buffers of this view.
    view_resources &resourcesOfView = state.frame.view(stateOfView.viewIndex);
    if (!state.lightData.empty()) {
        state.commandList->writeBuffer(
          resourcesOfView.lights,
          state.lightData.data(),
          state.lightData.size() * sizeof(gpu::light)
        );
    }
    const u32 tileCount = static_cast<u32>(state.tileData.size());
    if (tileCount > 0 && state.frame.ensure_shadow_tile_capacity(stateOfView.viewIndex, tileCount)) {
        state.commandList->writeBuffer(
          resourcesOfView.shadowTiles,
          state.tileData.data(),
          state.tileData.size() * sizeof(gpu::shadow_tile)
        );
    }
    const u32 tilesX = (stateOfView.targetSize.x + LIGHT_TILE_SIZE - 1) / LIGHT_TILE_SIZE;
    const u32 tilesY = (stateOfView.targetSize.y + LIGHT_TILE_SIZE - 1) / LIGHT_TILE_SIZE;
    static_cast<void>(
      state.frame.ensure_tile_list_capacity(stateOfView.viewIndex, tilesX * tilesY * gpu::TILE_LIST_STRIDE)
    );

    // View constants.
    gpu::view_constants &constants = stateOfView.constants;
    constants.viewFromWorld = view.viewFromWorld;
    constants.clipFromView = clipFromView;
    constants.clipFromWorld = clipFromWorld;
    constants.worldFromClip = glm::inverse(clipFromWorld);
    constants.viewFromClip = glm::inverse(clipFromView);
    constants.cameraPosition = math::vec4(view.cameraPosition, 1.0f);
    constants.viewportSize = math::vec4(
      static_cast<f32>(stateOfView.targetSize.x),
      static_cast<f32>(stateOfView.targetSize.y),
      1.0f / static_cast<f32>(stateOfView.targetSize.x),
      1.0f / static_cast<f32>(stateOfView.targetSize.y)
    );
    f32 manualExposure = 1.0f;
    if (stateOfView.postProcess && scene.postProcess.exposure == exposure_mode::MANUAL) {
        manualExposure = exposure::exposure_from_ev100(
          scene.postProcess.manualEv100 + scene.postProcess.exposureCompensation
        );
    }
    constants.nearFarExposure = math::vec4(
      cameraData.nearPlane,
      cameraData.farPlane,
      manualExposure,
      scene.deltaSeconds
    );
    constants.counts = math::uvec4(tilesX, tilesY, lightCount, directionalCount);
    // Passes that need geometry (or an environment) are decided here so the view constants are
    // written exactly once per view (the volatile buffer holds one version per view and frame).
    const bool hasGeometry = !stateOfView.main.batches.empty();
    stateOfView.autoExposure = stateOfView.autoExposure && hasGeometry;
    stateOfView.bloom = stateOfView.bloom && (hasGeometry || stateOfView.hasEnvironment);
    u32 flags = 0;
    flags |= noPostProcess ? gpu::VIEW_FLAG_NO_POST_PROCESS : 0u;
    flags |= stateOfView.shadows ? gpu::VIEW_FLAG_SHADOWS : 0u;
    flags |= (stateOfView.ssao && stateOfView.opaqueBatchCount > 0) ? gpu::VIEW_FLAG_SSAO : 0u;
    flags |= stateOfView.hasEnvironment ? gpu::VIEW_FLAG_ENVIRONMENT : 0u;
    flags |= stateOfView.autoExposure ? gpu::VIEW_FLAG_AUTO_EXPOSURE : 0u;
    flags |= stateOfView.bloom ? gpu::VIEW_FLAG_BLOOM : 0u;
    constants.flags = math::uvec4(
      flags,
      stateOfView.cascadeCount,
      static_cast<u32>(state.frameIndex & 0xFFFFFFFFu),
      directionalShadowLight
    );
    constants.ssaoParams = math::vec4(scene.ssao.radius, scene.ssao.intensity, scene.ssao.thickness, 0.0f);
    const gpu_environment &environment = stateOfView.environment != nullptr ? *stateOfView.environment
      : state.resources->fallback_environment();
    constants.environmentParams = math::vec4(
      stateOfView.hasEnvironment ? scene.environment.intensity : 1.0f,
      scene.environment.rotationRadians,
      static_cast<f32>(std::max<u32>(environment.prefilteredMipCount, 1) - 1),
      0.0f
    );
    constants.shadowParams = math::vec4(
      1.0f / static_cast<f32>(state.frame.cascade_resolution()),
      scene.shadows.pcfRadiusTexels,
      scene.shadows.normalOffsetTexels,
      1.0f / static_cast<f32>(state.frame.atlas_resolution())
    );
    constants.shadowParams2 = math::vec4(directionalSourceRadius, 0.0f, 0.0f, 0.0f);
    const tonemap_operator operatorValue = noPostProcess ? tonemap_operator::NONE : scene.postProcess.tonemap;
    constants.postProcess = math::vec4(
      scene.postProcess.bloomIntensity,
      static_cast<f32>(static_cast<u8>(operatorValue)),
      scene.postProcess.reinhardWhitePoint,
      scene.postProcess.agxLook
    );
    // SH9: nine vec3 packed into seven vec4 (27 floats in order).
    std::array<f32, 28> packed = {};
    for (usize coefficient = 0; coefficient < 9; ++coefficient) {
        packed[coefficient * 3] = environment.irradiance[coefficient].x;
        packed[coefficient * 3 + 1] = environment.irradiance[coefficient].y;
        packed[coefficient * 3 + 2] = environment.irradiance[coefficient].z;
    }
    for (usize entry = 0; entry < 7; ++entry) {
        constants.shCoefficients[entry] = math::vec4(
          packed[entry * 4],
          packed[entry * 4 + 1],
          packed[entry * 4 + 2],
          packed[entry * 4 + 3]
        );
    }
    constants.exposureParams = math::vec4(
      scene.postProcess.exposureCompensation,
      scene.postProcess.adaptationSpeedUp,
      scene.postProcess.adaptationSpeedDown,
      scene.postProcess.minEv100
    );
    constants.exposureParams2 = math::vec4(scene.postProcess.maxEv100, 0.0f, 0.0f, 0.0f);
}

// A pass added to this graph must be counted in MAX_PASSES_PER_VIEW (limits.h), which sizes render_stats::passNames.
void render_view_graph(renderer_state &state, frame_view_state &stateOfView, const render_scene &scene) {
    pass_shared &shared = state.shared;
    shared.scene = &scene;
    shared.view = &stateOfView;
    shared.graph = view_graph_resources();
    shared.spriteRuns = state.sprites.runs();
    shared.frameIndex = state.frameIndex;
    stateOfView.hdrCleared = false;
    stateOfView.depthCleared = false;

    state.commandList->writeBuffer(state.frame.view_constants(), &stateOfView.constants, sizeof(stateOfView.constants));

    render_graph &graph = state.graph;
    graph.reset();
    view_resources &resourcesOfView = state.frame.view(stateOfView.viewIndex);
    shared.graph.target = graph.import_texture(stateOfView.targetTexture, "target");
    shared.graph.instances = graph.import_buffer(state.frame.instances(), "instances");
    shared.graph.lights = graph.import_buffer(resourcesOfView.lights, "lights");
    shared.graph.shadowTiles = graph.import_buffer(resourcesOfView.shadowTiles, "shadow_tiles");
    shared.graph.tileLists = graph.import_buffer(resourcesOfView.tileLists, "tile_lists");
    shared.graph.shadowCascades = graph.import_texture(state.frame.shadow_cascades(), "shadow_cascades");
    shared.graph.shadowAtlas = graph.import_texture(state.frame.shadow_atlas(), "shadow_atlas");
    shared.graph.histogram = graph.import_buffer(resourcesOfView.histogram, "histogram");
    shared.graph.exposureState = graph.import_buffer(resourcesOfView.exposureState, "exposure_state");
    shared.graph.spriteVertices = graph.import_buffer(state.frame.sprite_vertices(), "sprite_vertices");
    shared.graph.debugVertices = graph.import_buffer(state.frame.debug_vertices(), "debug_vertices");
    shared.graph.ldrIsTarget = stateOfView.targetFormat == rhi::texture_format::RGBA8_SRGB;

    const bool hasOpaque = stateOfView.opaqueBatchCount > 0;
    const bool hasBlended = stateOfView.main.batches.size() > stateOfView.opaqueBatchCount;

    for (u32 cascade = 0; cascade < stateOfView.cascadeCount; ++cascade) {
        graph.add_pass(*state.cascadePasses[cascade]);
    }
    if (stateOfView.atlasTileCount > 0) {
        graph.add_pass(*state.atlasPass);
    }
    if (hasOpaque) {
        graph.add_pass(*state.prepass);
        if (stateOfView.localLightCount > 0) {
            graph.add_pass(*state.lightCull);
        }
        if (stateOfView.ssao) {
            graph.add_pass(*state.ssao);
            graph.add_pass(*state.ssaoBlurH);
            graph.add_pass(*state.ssaoBlurV);
        }
        graph.add_pass(*state.forwardOpaque);
    }
    if (stateOfView.hasEnvironment) {
        graph.add_pass(*state.skybox);
    }
    if (hasBlended) {
        if (!hasOpaque) {
            // Blended geometry alone still needs a depth buffer and lights: the prepass lays down
            // nothing but clears depth, and the culler builds list B.
            graph.add_pass(*state.prepass);
            if (stateOfView.localLightCount > 0) {
                graph.add_pass(*state.lightCull);
            }
        }
        graph.add_pass(*state.forwardBlend);
    }
    if (stateOfView.worldSprites) {
        graph.add_pass(*state.worldSprites);
    }
    if (stateOfView.drawDebug && (stateOfView.debugDepthTestedVertices + stateOfView.debugOverlayVertices) > 0) {
        graph.add_pass(*state.debugDraw);
    }
    if (stateOfView.autoExposure) {
        graph.add_pass(*state.exposureHistogram);
        graph.add_pass(*state.exposureAdapt);
    }
    if (stateOfView.bloom) {
        for (u32 mip = 0; mip < BLOOM_MIP_COUNT; ++mip) {
            graph.add_pass(*state.bloomDown[mip]);
        }
        for (u32 mip = BLOOM_MIP_COUNT - 1; mip > 0; --mip) {
            graph.add_pass(*state.bloomUp[mip - 1]);
        }
    }
    graph.add_pass(*state.tonemap);
    if (stateOfView.screenSprites) {
        graph.add_pass(*state.screenSprites);
    }
    if (!shared.graph.ldrIsTarget) {
        core::result<nvrhi::IGraphicsPipeline *> blitPipeline = state.pipelines->blit(stateOfView.targetFormat);
        state.blit->set_pipeline(blitPipeline.has_value() ? blitPipeline.value() : nullptr);
        graph.add_pass(*state.blit);
    }

    graph.compile(*state.pool, state.frameIndex);
    graph.execute(*state.commandList, *state.framebuffers, stateOfView.constants, state.stats, state.timers.get());
    ++state.stats.viewsRendered;
}

}

core::result<void> renderer::render(const render_scene &scene) {
    impl &state = *mImpl;
    CONJURE_ASSERT(state.renderThread == std::this_thread::get_id(), "renderer::render from a foreign thread");
    // The draw data belongs to this render() alone: every exit (device lost, a skipped acquire, a drawn
    // frame) drops it, so a later frame never draws data ImGui has since freed.
    ImDrawData *const imguiDrawData = std::exchange(state.imguiDrawData, nullptr);
    if (state.lost || state.desc.device->is_lost()) {
        state.lost = true;
        return device_lost_error();
    }
    nvrhi::IDevice &device = *state.device;
    rhi::swapchain *swapchain = state.desc.swapchain;
    ++state.frameIndex;
    state.stats.reset();
    state.resources->reset_counters();
    state.pool->reset_counters();
    if (state.timers != nullptr) {
        state.timers->begin_frame();
    }

    // 1. begin_frame.
    nvrhi::ITexture *backBuffer = nullptr;
    if (swapchain != nullptr) {
        core::result<u32> acquired = swapchain->acquire();
        if (!acquired.has_value()) {
            if (acquired.get_error().is(rhi::rhi_error::DEVICE_LOST)) {
                state.lost = true;
                return device_lost_error();
            }
            CONJURE_LOG_WARN(LOG_RENDERER, "frame {} skipped: {}", state.frameIndex, acquired.get_error().message());
            state.stats.viewsSkipped = static_cast<u32>(scene.views.size());
            return core::result<void>();
        }
        backBuffer = swapchain->back_buffer(acquired.value());
    }
    state.desc.device->run_garbage_collection();
    state.commandList->open();
    const upload_report uploads = state.resources->flush_uploads(
      *state.commandList,
      state.frameIndex,
      UPLOAD_BUDGET_BYTES_PER_FRAME
    );
    state.stats.uploadsCompleted = uploads.completed;
    state.stats.uploadsPending = uploads.pending;
    state.stats.uploadBytes = uploads.bytes;
    state.resources->collect(state.frameIndex);

    // 2. prepare and 3. the graphs. No cache lock is held here: resolve_* lock per call and the
    //    references they return live until the next collect(), so jobs the render thread helps with
    //    inside parallel_for may call upload_* / release_* without deadlocking (design 5).
    prepare_frame(state, scene, backBuffer);
    for (frame_view_state &view : state.prepared_views()) {
        render_view_graph(state, view, scene);
    }

    // 4. ImGui into the swapchain.
#if defined(CONJURE_BUILD_EDITOR) && CONJURE_BUILD_EDITOR
    if (state.imgui != nullptr && imguiDrawData != nullptr && backBuffer != nullptr) {
        nvrhi::FramebufferDesc desc = {};
        desc.addColorAttachment(backBuffer);
        nvrhi::IFramebuffer *framebuffer = state.framebuffers->get(desc);
        if (framebuffer != nullptr) {
            state.commandList->beginMarker("imgui");
            state.imgui->render(imguiDrawData, framebuffer, *state.commandList);
            state.commandList->endMarker();
            state.stats.drawCalls += state.imgui->stats().drawCalls;
        }
    }
#else
    CONJURE_UNUSED(imguiDrawData);
#endif

    // 5. end_frame.
    state.commandList->close();
    nvrhi::ICommandList *frameList = state.commandList.Get();
    device.executeCommandLists(&frameList, 1);
    if (swapchain != nullptr) {
        core::result<void> presented = swapchain->present();
        if (!presented.has_value()) {
            if (presented.get_error().is(rhi::rhi_error::DEVICE_LOST)) {
                state.lost = true;
                return device_lost_error();
            }
            CONJURE_LOG_WARN(LOG_RENDERER, "present failed: {}", presented.get_error().message());
        }
    }
    if (state.desc.device->is_lost()) {
        state.lost = true;
        return device_lost_error();
    }
    state.pool->end_frame(state.frameIndex);
    state.bindingSets->end_frame(state.frameIndex);
    state.stats.transientTexturesLive = state.pool->live_texture_count();
    state.stats.transientTexturesCreated = state.pool->textures_created();
    state.stats.fallbacksUsed = state.resources->fallbacks_used();
    if (state.timers != nullptr) {
        for (const pass_timing &timing : state.timers->resolved()) {
            if (state.stats.passTimings.size() < state.stats.passTimings.INLINE_CAPACITY) {
                state.stats.passTimings.push_back(timing);
            }
        }
    }
    return core::result<void>();
}

/*===========================================================================*/
/*===========================================================================*/
// Events

void renderer::on_swapchain_resized() {
    mImpl->framebuffers->clear();
    mImpl->bindingSets->clear();
}

void renderer::on_device_lost() {
    impl &state = *mImpl;
    state.lost = true;
    state.framebuffers->clear();
    state.bindingSets->clear();
    state.pool->clear();
    state.resources->drop_all();
    CONJURE_LOG_ERROR(LOG_RENDERER, "device lost: GPU mirrors dropped, rendering stopped");
}

void renderer::set_imgui_draw_data(ImDrawData *drawData) noexcept {
    mImpl->imguiDrawData = drawData;
}

/*===========================================================================*/
/*===========================================================================*/
// Readback

core::result<core::image> renderer::read_back(render_target_id target) {
    impl &state = *mImpl;
    std::unique_ptr<render_target> *slot = state.targets.find(target);
    if (slot == nullptr || *slot == nullptr) {
        return core::make_error(core::error_code::NOT_FOUND, "read_back: unknown render target");
    }
    if (state.lost || state.desc.device->is_lost()) {
        state.lost = true;
        return device_lost_error();
    }
    const render_target::impl &targetState = renderer_access::state(**slot);
    const math::uvec2 size = targetState.desc.size;
    const rhi::texture_format format = targetState.desc.colourFormat;

    nvrhi::TextureDesc stagingDesc = {};
    stagingDesc.width = size.x;
    stagingDesc.height = size.y;
    stagingDesc.format = targetState.colour->getDesc().format;
    stagingDesc.dimension = nvrhi::TextureDimension::Texture2D;
    stagingDesc.mipLevels = 1;
    stagingDesc.arraySize = 1;
    stagingDesc.debugName = "read_back.staging";
    nvrhi::StagingTextureHandle staging = state.device->createStagingTexture(stagingDesc, nvrhi::CpuAccessMode::Read);
    if (staging == nullptr) {
        return core::make_error(
          renderer_error::RESOURCE_CREATION_FAILED,
          "read_back: the device refused the staging texture"
        );
    }
    nvrhi::CommandListHandle commandList = state.device->createCommandList();
    if (commandList == nullptr) {
        return core::make_error(
          renderer_error::RESOURCE_CREATION_FAILED,
          "read_back: the device refused a command list"
        );
    }
    commandList->open();
    commandList->copyTexture(staging, nvrhi::TextureSlice(), targetState.colour, nvrhi::TextureSlice());
    commandList->close();
    nvrhi::ICommandList *copyList = commandList.Get();
    state.device->executeCommandLists(&copyList, 1);
    state.desc.device->wait_idle();
    if (state.desc.device->is_lost()) {
        state.lost = true;
        return device_lost_error();
    }

    size_t rowPitch = 0;
    const void *mapped = state.device->mapStagingTexture(
      staging,
      nvrhi::TextureSlice(),
      nvrhi::CpuAccessMode::Read,
      &rowPitch
    );
    // A blank image would read as a successful capture of a black frame.
    if (mapped == nullptr) {
        if (state.desc.device->is_lost()) {
            state.lost = true;
            return device_lost_error();
        }
        return core::make_error(
          renderer_error::RESOURCE_CREATION_FAILED,
          "read_back: the staging texture could not be mapped"
        );
    }

    core::image image(size.x, size.y);
    const std::byte *rows = static_cast<const std::byte *>(mapped);
    const u32 bytesPerPixel = rhi::bytes_per_pixel(format);
    for (u32 y = 0; y < size.y; ++y) {
        const std::byte *row = rows + static_cast<usize>(y) * rowPitch;
        for (u32 x = 0; x < size.x; ++x) {
            const std::byte *texel = row + static_cast<usize>(x) * bytesPerPixel;
            image.set_pixel(x, y, decode_read_back_texel(format, texel));
        }
    }
    state.device->unmapStagingTexture(staging);
    return image;
}

/*===========================================================================*/
/*===========================================================================*/
// Accessors

const render_stats &renderer::stats() const noexcept {
    return mImpl->stats;
}

imgui_renderer *renderer::imgui() noexcept {
#if defined(CONJURE_BUILD_EDITOR) && CONJURE_BUILD_EDITOR
    return mImpl->imgui.get();
#else
    return nullptr;
#endif
}

rhi::device &renderer::device() const noexcept {
    return *mImpl->desc.device;
}

const renderer_desc &renderer::desc() const noexcept {
    return mImpl->desc;
}

bool renderer::is_lost() const noexcept {
    return mImpl->lost;
}

u32 renderer::pending_upload_count() const {
    return mImpl->resources->pending_count();
}

u64 renderer::frame_count() const noexcept {
    return mImpl->frameIndex;
}

}
