#include "conjure/testing/doctest.h"

#include "conjure/testing/test_environment.h"
#include "renderer_test_support.h"

#include "conjure/renderer/camera.h"
#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/renderer.h"

#include "conjure/core/log.h"
#include "conjure/core/math.h"
#include "conjure/rhi/backend.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/shader_library.h"

#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <utility>

#ifndef CONJURE_TEST_SHADER_DIR
    #define CONJURE_TEST_SHADER_DIR ""
#endif
#ifndef CONJURE_TEST_SHADER_SOURCE_DIR
    #define CONJURE_TEST_SHADER_SOURCE_DIR ""
#endif

using namespace conjure;
using namespace conjure::testing;
using namespace conjure::testing::renderer_support;

namespace {

    /// <summary>
    /// Tries the software adapter once per process (the same probe tests/rhi installs).
    /// </summary>
    bool software_gpu_available() noexcept {
        static const bool available = []() noexcept {
            if (!rhi::is_backend_compiled(rhi::graphics_backend::D3D12)) {
                return false;
            }
            const bool consoleWasEnabled = core::log::console_enabled();
            core::log::set_console_enabled(false);
            const core::result<std::unique_ptr<rhi::device>> probe = rhi::device::create(software_device_desc());
            core::log::set_console_enabled(consoleWasEnabled);
            return probe.has_value();
        }();
        return available;
    }

    struct probe_installer {
        probe_installer() {
            set_precondition_probe(precondition::SOFTWARE_GPU_ADAPTER, &software_gpu_available);
        }
    };

    const probe_installer sProbeInstaller = {};

    /// <summary>
    /// Opens the shader index on 'device' and creates the renderer over both, or fails the case.
    /// </summary>
    [[nodiscard]] headless_renderer renderer_on(std::unique_ptr<rhi::device> device, core::jobs::job_system *jobs) {
        headless_renderer fixture = {};
        fixture.device = std::move(device);
        fixture.shaders = open_shaders(*fixture.device);
        core::result<std::unique_ptr<renderer::renderer>> created =
          renderer::renderer::create(headless_desc(*fixture.device, *fixture.shaders, jobs));
        CONJURE_REQUIRE_OK(created);
        fixture.instance = std::move(created).value();
        return fixture;
    }

    /// <summary>
    /// Creates a device from 'desc' or fails the case.
    /// </summary>
    [[nodiscard]] std::unique_ptr<rhi::device> device_from(const rhi::device_desc &desc) {
        core::result<std::unique_ptr<rhi::device>> created = rhi::device::create(desc);
        CONJURE_REQUIRE_OK(created);
        return std::move(created).value();
    }
}

std::filesystem::path conjure::testing::renderer_support::shader_directory() {
    return std::filesystem::path(CONJURE_TEST_SHADER_DIR);
}

std::filesystem::path conjure::testing::renderer_support::shader_source_directory() {
    return std::filesystem::path(CONJURE_TEST_SHADER_SOURCE_DIR);
}

rhi::device_desc conjure::testing::renderer_support::null_device_desc(bool enableDebugLayer) {
    rhi::device_desc desc = {};
    desc.backend = rhi::graphics_backend::NULL_DEVICE;
    desc.enableDebugLayer = enableDebugLayer;
    return desc;
}

rhi::device_desc conjure::testing::renderer_support::software_device_desc() {
    rhi::device_desc desc = {};
    desc.backend = rhi::graphics_backend::D3D12;
    desc.preferSoftwareAdapter = true;
    return desc;
}

std::unique_ptr<rhi::device> conjure::testing::renderer_support::make_device(bool software) {
    return device_from(software ? software_device_desc() : null_device_desc(true));
}

std::unique_ptr<rhi::device> conjure::testing::renderer_support::make_raw_device() {
    return device_from(null_device_desc(false));
}

std::unique_ptr<rhi::shader_library> conjure::testing::renderer_support::open_shaders(rhi::device &device) {
    const std::filesystem::path directory = shader_directory();
    REQUIRE_FALSE(directory.empty());
    core::result<std::unique_ptr<rhi::shader_library>> opened = rhi::shader_library::open(device, directory);
    CONJURE_REQUIRE_OK(opened);
    return std::move(opened).value();
}

renderer::renderer_desc conjure::testing::renderer_support::headless_desc(
  rhi::device &device,
  rhi::shader_library &shaders,
  core::jobs::job_system *jobs
) {
    renderer::renderer_desc desc = {};
    desc.device = &device;
    desc.shaders = &shaders;
    desc.swapchain = nullptr;
    desc.jobs = jobs;
    desc.cascadeResolution = 512;
    desc.atlasResolution = 1024;
    desc.enableImGui = false;
    return desc;
}

conjure::testing::renderer_support::headless_renderer::headless_renderer() = default;

conjure::testing::renderer_support::headless_renderer::~headless_renderer() {
    instance.reset();
    shaders.reset();
    device.reset();
}

headless_renderer conjure::testing::renderer_support::make_headless_renderer(
  bool software,
  core::jobs::job_system *jobs
) {
    return renderer_on(make_device(software), jobs);
}

headless_renderer conjure::testing::renderer_support::make_raw_headless_renderer(core::jobs::job_system *jobs) {
    return renderer_on(make_raw_device(), jobs);
}

assets::asset_id conjure::testing::renderer_support::test_asset_id(u64 index) noexcept {
    return assets::asset_id(0x7E57u, index + 1);
}

renderer::render_view conjure::testing::renderer_support::look_at_view(
  const math::vec3 &eye,
  const math::vec3 &target,
  renderer::render_target_id renderTarget,
  f32 verticalFovRadians
) {
    renderer::render_view view = {};
    view.cameraData.projection = renderer::projection_type::PERSPECTIVE;
    view.cameraData.verticalFovRadians = verticalFovRadians;
    view.cameraData.nearPlane = 0.1f;
    view.cameraData.farPlane = 500.0f;
    view.viewFromWorld = math::look_at(eye, target, math::vec3(0.0f, 1.0f, 0.0f));
    view.cameraPosition = eye;
    view.target = renderTarget;
    return view;
}

renderer::mesh_instance conjure::testing::renderer_support::instance_at(
  renderer::gpu_mesh_id mesh,
  renderer::gpu_material_id material,
  const math::vec3 &position,
  const math::aabb &localBounds,
  f32 scale
) {
    renderer::mesh_instance instance = {};
    instance.mesh = mesh;
    instance.materials[0] = material;
    instance.worldFromLocal = glm::scale(glm::translate(math::mat4(1.0f), position), math::vec3(scale));
    instance.worldBounds.min = position + localBounds.min * scale;
    instance.worldBounds.max = position + localBounds.max * scale;
    return instance;
}

math::uvec2 conjure::testing::renderer_support::project_to_pixel(
  const math::mat4 &clipFromWorld,
  const math::vec3 &world,
  math::uvec2 size
) noexcept {
    const math::vec4 clip = clipFromWorld * math::vec4(world, 1.0f);
    const math::uvec2 outside(std::numeric_limits<u32>::max(), std::numeric_limits<u32>::max());
    if (clip.w <= 0.0f) {
        return outside;
    }
    const f32 ndcX = clip.x / clip.w;
    const f32 ndcY = clip.y / clip.w;
    if (ndcX < -1.0f || ndcX > 1.0f || ndcY < -1.0f || ndcY > 1.0f) {
        return outside;
    }
    const f32 px = (ndcX * 0.5f + 0.5f) * static_cast<f32>(size.x);
    const f32 py = (0.5f - ndcY * 0.5f) * static_cast<f32>(size.y);
    const u32 x = static_cast<u32>(std::min(std::max(px, 0.0f), static_cast<f32>(size.x - 1)));
    const u32 y = static_cast<u32>(std::min(std::max(py, 0.0f), static_cast<f32>(size.y - 1)));
    return math::uvec2(x, y);
}

f32 conjure::testing::renderer_support::srgb_to_linear(u8 encoded) noexcept {
    const f32 value = static_cast<f32>(encoded) / 255.0f;
    if (value <= 0.04045f) {
        return value / 12.92f;
    }
    return std::pow((value + 0.055f) / 1.055f, 2.4f);
}

f32 conjure::testing::renderer_support::linear_luminance(core::rgba8 pixel) noexcept {
    return 0.2126f * srgb_to_linear(pixel.r) + 0.7152f * srgb_to_linear(pixel.g) + 0.0722f * srgb_to_linear(pixel.b);
}
