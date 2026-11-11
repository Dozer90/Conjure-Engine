// Renderer scenarios (docs/design/08_testing.md 7.1, 02_renderer.md 10, ARCHITECTURE.md 11 "wave 3"):
// renderer.extract_10k writes 10,000 mesh instances through the public extract API (begin_extract, fill,
// end_extract) and culls them against the view frustum with the job system, half of them behind the
// camera, so extraction plus culling are measured without a frame; renderer.null_frame_10k renders one
// whole frame of the same layout on the null device with a shadow-casting sun, so culling, draw-list
// building, every pass of the graph and the command list submission are measured without a GPU.

#include "bench_support.h"

#include "conjure/assets/asset_types.h"
#include "conjure/bench/benchmark.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/math.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/renderer/camera.h"
#include "conjure/renderer/light.h"
#include "conjure/renderer/material_asset.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/render_target.h"
#include "conjure/renderer/renderer.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/shader_library.h"
#include "renderer/culling.h"

#include <format>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::bench;
using namespace conjure::bench::support;

namespace {

    constexpr u32 GRID_SIDE = 100;
    constexpr f32 GRID_SPACING = 2.0f;
    constexpr u32 TARGET_WIDTH = 1280;
    constexpr u32 TARGET_HEIGHT = 720;
    constexpr f32 MIN_VISIBLE_FRACTION = 0.4f;      // the design row says 50 % visible; the grid gives ~half
    constexpr f32 MAX_VISIBLE_FRACTION = 0.6f;
    const math::vec3 CAMERA_EYE = math::vec3(0.0f, 20.0f, 0.0f);
    const math::vec3 CAMERA_TARGET = math::vec3(0.0f, 0.0f, -60.0f);

    /// <summary>
    /// A null device, the shader index conjure_shaders built and a headless renderer over them, in
    /// destruction order.
    /// </summary>
    struct headless_renderer {
        std::unique_ptr<rhi::device> device = nullptr;
        std::unique_ptr<rhi::shader_library> shaders = nullptr;
        std::unique_ptr<renderer::renderer> instance = nullptr;
        std::string failure = {};

        headless_renderer() = default;

        ~headless_renderer() {
            instance.reset();
            shaders.reset();
            device.reset();
        }

        headless_renderer(const headless_renderer &) = delete;
        headless_renderer &operator=(const headless_renderer &) = delete;

        /// <summary>
        /// Creates the three objects; on failure 'failure' names the step.
        /// </summary>
        [[nodiscard]] bool create(core::jobs::job_system &jobs) {
            if (shader_directory().empty()) {
                failure = "no shader index: the renderer was built without conjure_shaders";
                return false;
            }
            rhi::device_desc deviceDesc = {};
            deviceDesc.backend = rhi::graphics_backend::NULL_DEVICE;
            core::result<std::unique_ptr<rhi::device>> createdDevice = rhi::device::create(deviceDesc);
            if (!createdDevice.has_value()) {
                failure = "null device: " + createdDevice.get_error().to_string();
                return false;
            }
            device = std::move(createdDevice).value();

            core::result<std::unique_ptr<rhi::shader_library>> opened =
              rhi::shader_library::open(*device, shader_directory());
            if (!opened.has_value()) {
                failure = "shader library: " + opened.get_error().to_string();
                return false;
            }
            shaders = std::move(opened).value();

            renderer::renderer_desc desc = {};
            desc.device = device.get();
            desc.shaders = shaders.get();
            desc.swapchain = nullptr;
            desc.jobs = &jobs;
            desc.enableImGui = false;
            core::result<std::unique_ptr<renderer::renderer>> created = renderer::renderer::create(desc);
            if (!created.has_value()) {
                failure = "renderer: " + created.get_error().to_string();
                return false;
            }
            instance = std::move(created).value();
            return true;
        }
    };

    [[nodiscard]] assets::asset_id bench_asset_id(u64 index) noexcept {
        return assets::asset_id(0xBE4Cu, index + 1);
    }

    /// <summary>
    /// A perspective view from 'eye' towards 'target' into the render target.
    /// </summary>
    [[nodiscard]] renderer::render_view make_view(
      const math::vec3 &eye,
      const math::vec3 &target,
      renderer::render_target_id renderTarget
    ) {
        renderer::render_view view = {};
        view.cameraData.projection = renderer::projection_type::PERSPECTIVE;
        view.cameraData.verticalFovRadians = 1.0472f;
        view.cameraData.nearPlane = 0.1f;
        view.cameraData.farPlane = 500.0f;
        view.viewFromWorld = math::look_at(eye, target, math::vec3(0.0f, 1.0f, 0.0f));
        view.cameraPosition = eye;
        view.target = renderTarget;
        return view;
    }

    /// <summary>
    /// The frustum the renderer culls the view with (renderer.cpp prepare_view: clip_from_view of the
    /// camera at the target's aspect ratio times viewFromWorld).
    /// </summary>
    [[nodiscard]] math::frustum view_frustum(const renderer::render_view &view) {
        const f32 aspect = static_cast<f32>(TARGET_WIDTH) / static_cast<f32>(TARGET_HEIGHT);
        const math::mat4 clipFromView = renderer::clip_from_view(view.cameraData, aspect);
        return renderer::frustum_from_clip(clipFromView * view.viewFromWorld);
    }

    /// <summary>
    /// An orthographic view from the grid's centre straight down -z whose box is wider and taller than the
    /// grid: exactly the instances in front of the camera (the half with z &lt; 0, plus the row straddling
    /// the near plane) pass the frustum, which is the design row's "50 % visible" independent of any field
    /// of view. Nothing is rendered through it: the target is the (absent) swapchain.
    /// </summary>
    [[nodiscard]] renderer::render_view make_half_visible_view() {
        renderer::render_view view =
          make_view(math::vec3(0.0f), math::vec3(0.0f, 0.0f, -1.0f), renderer::render_target_id{});
        view.cameraData.projection = renderer::projection_type::ORTHOGRAPHIC;
        view.cameraData.orthographicHeight = static_cast<f32>(GRID_SIDE) * GRID_SPACING * 1.5f;
        return view;
    }

    /// <summary>
    /// A grid of unit cubes centred on the camera's position: the half with z &gt; 0 lies behind the camera,
    /// which looks down -z. The perspective view of renderer.null_frame_10k sees about a quarter of them
    /// (its pyramid narrows towards the camera); the orthographic view of renderer.extract_10k exactly the
    /// front half.
    /// </summary>
    void fill_grid(
      renderer::render_scene &scene,
      renderer::gpu_mesh_id mesh,
      renderer::gpu_material_id material,
      const math::aabb &localBounds
    ) {
        const std::span<renderer::mesh_instance> instances = scene.allocate_meshes(GRID_SIDE * GRID_SIDE);
        for (u32 index = 0; index < GRID_SIDE * GRID_SIDE; ++index) {
            const f32 x = (static_cast<f32>(index % GRID_SIDE) - static_cast<f32>(GRID_SIDE) * 0.5f) * GRID_SPACING;
            const f32 z = (static_cast<f32>(index / GRID_SIDE) - static_cast<f32>(GRID_SIDE) * 0.5f) * GRID_SPACING;
            const math::vec3 position(x, 0.0f, z);
            renderer::mesh_instance &instance = instances[index];
            instance.mesh = mesh;
            instance.materials[0] = material;
            instance.worldFromLocal = glm::translate(math::mat4(1.0f), position);
            instance.worldBounds.min = position + localBounds.min;
            instance.worldBounds.max = position + localBounds.max;
            instance.entityId = index + 1;
        }
    }
}

CONJURE_BENCHMARK(renderer.extract_10k) {
    headless_renderer fixture = {};
    if (!fixture.create(context.jobs())) {
        context.skip(fixture.failure);
        return;
    }
    renderer::renderer &instance = *fixture.instance;

    const renderer::mesh_asset cube = renderer::mesh_asset::unit_cube();
    const renderer::gpu_mesh_id mesh = instance.upload_mesh(bench_asset_id(1), cube);
    const renderer::material_asset material = {};
    const renderer::gpu_material_id opaque = instance.upload_material(bench_asset_id(2), material);
    const renderer::render_view view = make_half_visible_view();
    const math::frustum frustum = view_frustum(view);
    const renderer::cull_options options = {};
    std::vector<u32> visible = {};
    visible.reserve(GRID_SIDE * GRID_SIDE);
    u64 frameIndex = 0;

    // Extraction through the renderer's pair (begin_extract clears the write slot and keeps its capacity),
    // then the culling render() runs first for every view, on the benchmark job system above 2,048 instances.
    auto extractAndCull = [&]() {
        renderer::render_scene &scene = instance.begin_extract();
        scene.frameIndex = ++frameIndex;
        scene.views.push_back(view);
        fill_grid(scene, mesh, opaque, cube.bounds());
        instance.end_extract();
        renderer::cull_instances(scene.meshes, frustum, options, &context.jobs(), visible);
    };
    extractAndCull();
    const f32 visibleFraction = static_cast<f32>(visible.size()) / static_cast<f32>(GRID_SIDE * GRID_SIDE);
    if (visibleFraction < MIN_VISIBLE_FRACTION || visibleFraction > MAX_VISIBLE_FRACTION) {
        context.skip(std::format("the layout is not about 50 % visible: {} of {} instances pass the frustum",
          visible.size(), GRID_SIDE * GRID_SIDE));
        return;
    }

    context.set_items(GRID_SIDE * GRID_SIDE);
    context.measure(extractAndCull);
}

CONJURE_BENCHMARK(renderer.null_frame_10k) {
    headless_renderer fixture = {};
    if (!fixture.create(context.jobs())) {
        context.skip(fixture.failure);
        return;
    }
    renderer::renderer &instance = *fixture.instance;

    const renderer::mesh_asset cube = renderer::mesh_asset::unit_cube();
    const renderer::gpu_mesh_id mesh = instance.upload_mesh(bench_asset_id(1), cube);
    const renderer::material_asset material = {};
    const renderer::gpu_material_id opaque = instance.upload_material(bench_asset_id(2), material);

    renderer::render_target_desc targetDesc = {};
    targetDesc.size = math::uvec2(TARGET_WIDTH, TARGET_HEIGHT);
    const core::result<renderer::render_target_id> target = instance.create_render_target(targetDesc);
    if (!target.has_value()) {
        context.skip("render target: " + target.get_error().to_string());
        return;
    }

    renderer::render_scene scene = {};
    scene.views.push_back(make_view(CAMERA_EYE, CAMERA_TARGET, target.value()));
    fill_grid(scene, mesh, opaque, cube.bounds());
    renderer::light sun = {};
    sun.type = renderer::light_type::DIRECTIONAL;
    sun.direction = glm::normalize(math::vec3(0.3f, -1.0f, 0.2f));
    sun.intensity = 3.0f;
    sun.castsShadows = true;
    scene.lights.push_back(sun);

    context.set_items(GRID_SIDE * GRID_SIDE);
    context.measure([&]() {
        ++scene.frameIndex;
        CONJURE_UNUSED(instance.render(scene));
    });
}
