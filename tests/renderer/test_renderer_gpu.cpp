#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "renderer_test_support.h"

#include "conjure/renderer/camera.h"
#include "conjure/renderer/environment_asset.h"
#include "conjure/renderer/material_asset.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/render_target.h"
#include "conjure/renderer/renderer.h"
#include "conjure/renderer/texture_asset.h"

#include "conjure/assets/asset_ref.h"
#include "conjure/core/image.h"
#include "conjure/core/log.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"
#include "conjure/rhi/device.h"
#include "conjure/rhi/rhi_error.h"
#include "conjure/rhi/shader_library.h"
#include "conjure/rhi/swapchain.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>
#include <vector>

// [gpu] cases (docs/design/02_renderer.md 10): the frame on the WARP D3D12 device with analytic
// expectations read back from an off-screen target. Skipped (exit 77) where no software adapter
// exists. Most cases disable post-processing, or set a manual exposure without bloom or a tonemap curve, so
// the read-back is the linear shading (times that exposure) encoded as sRGB; the ambient-occlusion debug view
// reads back the AO term the same way.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;
using namespace conjure::testing::renderer_support;

namespace {

    constexpr math::uvec2 TARGET_SIZE = math::uvec2(128, 128);

    [[nodiscard]] math::mat4 clip_from_world_of(const render_view &view) {
        const f32 aspect = static_cast<f32>(TARGET_SIZE.x) / static_cast<f32>(TARGET_SIZE.y);
        return clip_from_view(view.cameraData, aspect) * view.viewFromWorld;
    }

    [[nodiscard]] material_asset white_rough() {
        material_asset material = {};
        material.desc().baseColourFactor = math::vec4(1.0f);
        material.desc().metallicFactor = 0.0f;
        material.desc().roughnessFactor = 1.0f;
        return material;
    }

    /// <summary>
    /// A 40 x 40 metre floor in the XZ plane at y = 0 facing +Y.
    /// </summary>
    [[nodiscard]] mesh_asset floor_mesh() {
        mesh_geometry geometry = {};
        geometry.positions = {
          math::vec3(-20.0f, 0.0f, 20.0f),
          math::vec3(20.0f, 0.0f, 20.0f),
          math::vec3(20.0f, 0.0f, -20.0f),
          math::vec3(-20.0f, 0.0f, -20.0f),
        };
        geometry.normals.assign(4, math::vec3(0.0f, 1.0f, 0.0f));
        geometry.uv0 = {math::vec2(0.0f, 1.0f), math::vec2(1.0f, 1.0f), math::vec2(1.0f, 0.0f), math::vec2(0.0f, 0.0f)};
        geometry.indices = {0, 1, 2, 0, 2, 3};
        core::result<mesh_asset> mesh = mesh_asset::from_geometry(geometry, "floor");
        CONJURE_REQUIRE_OK(mesh);
        return std::move(mesh).value();
    }

    [[nodiscard]] core::rgba8 pixel_at(
      const core::image &image,
      const math::mat4 &clipFromWorld,
      const math::vec3 &world
    ) {
        const math::uvec2 pixel = project_to_pixel(clipFromWorld, world, TARGET_SIZE);
        REQUIRE(pixel.x != std::numeric_limits<u32>::max());
        return image.pixel(pixel.x, pixel.y);
    }

    /// <summary>
    /// Mean linear luminance of the projected disc of a unit sphere at the origin seen along -Z, one
    /// and a half pixels in from its rim.
    /// </summary>
    [[nodiscard]] f32 sphere_disc_luminance(const core::image &image, const math::mat4 &clipFromWorld) {
        const math::uvec2 centrePixel = project_to_pixel(clipFromWorld, math::vec3(0.0f, 0.0f, 1.0f), TARGET_SIZE);
        const math::uvec2 edgePixel = project_to_pixel(clipFromWorld, math::vec3(1.0f, 0.0f, 0.0f), TARGET_SIZE);
        const f32 radius = static_cast<f32>(edgePixel.x - centrePixel.x) - 1.5f;
        REQUIRE(radius > 10.0f);
        f32 sum = 0.0f;
        u32 count = 0;
        for (u32 y = 0; y < TARGET_SIZE.y; ++y) {
            for (u32 x = 0; x < TARGET_SIZE.x; ++x) {
                const f32 dx = static_cast<f32>(x) - static_cast<f32>(centrePixel.x);
                const f32 dy = static_cast<f32>(y) - static_cast<f32>(centrePixel.y);
                if (dx * dx + dy * dy <= radius * radius) {
                    sum += linear_luminance(image.pixel(x, y));
                    ++count;
                }
            }
        }
        REQUIRE(count > 100);
        return sum / static_cast<f32>(count);
    }

    /// <summary>
    /// Mean linear luminance of the floor (y = 0) over a world-space rectangle in XZ, sampled every 5 cm.
    /// </summary>
    [[nodiscard]] f32 floor_luminance(
      const core::image &image,
      const math::mat4 &clipFromWorld,
      const math::vec2 &minimum,
      const math::vec2 &maximum
    ) {
        const math::uvec2 size(image.width(), image.height());
        f32 sum = 0.0f;
        u32 count = 0;
        for (f32 z = minimum.y; z <= maximum.y; z += 0.05f) {
            for (f32 x = minimum.x; x <= maximum.x; x += 0.05f) {
                const math::uvec2 pixel = project_to_pixel(clipFromWorld, math::vec3(x, 0.0f, z), size);
                REQUIRE(pixel.x != std::numeric_limits<u32>::max());
                sum += linear_luminance(image.pixel(pixel.x, pixel.y));
                ++count;
            }
        }
        REQUIRE(count > 0);
        return sum / static_cast<f32>(count);
    }

    /// <summary>
    /// The unit cube stretched into the world-space box from 'minimum' to 'maximum'.
    /// </summary>
    [[nodiscard]] mesh_instance box_between(
      gpu_mesh_id cube,
      gpu_material_id material,
      const math::vec3 &minimum,
      const math::vec3 &maximum
    ) {
        mesh_instance box = instance_at(cube, material, math::vec3(0.0f), mesh_asset::unit_cube().bounds());
        const math::mat4 centred = glm::translate(math::mat4(1.0f), 0.5f * (minimum + maximum));
        box.worldFromLocal = glm::scale(centred, maximum - minimum);
        box.worldBounds.min = minimum;
        box.worldBounds.max = maximum;
        return box;
    }

    /// <summary>
    /// A 1 x 2 x 1 m pillar of the unit cube standing on the floor with its base centred on 'base'.
    /// </summary>
    [[nodiscard]] mesh_instance pillar_at(gpu_mesh_id cube, gpu_material_id material, const math::vec3 &base) {
        return box_between(cube, material, base + math::vec3(-0.5f, 0.0f, -0.5f), base + math::vec3(0.5f, 2.0f, 0.5f));
    }

    /// <summary>
    /// A view straight down at the origin from 'height' metres with -Z up on screen: screen x is world x and
    /// screen y is world z, with square pixels on a target of any aspect.
    /// </summary>
    [[nodiscard]] render_view top_down_view(f32 height, render_target_id target, f32 verticalFovRadians) {
        const math::vec3 eye(0.0f, height, 0.0f);
        render_view view = look_at_view(eye, math::vec3(0.0f), target, verticalFovRadians);
        view.viewFromWorld = math::look_at(eye, math::vec3(0.0f), math::vec3(0.0f, 0.0f, -1.0f));
        return view;
    }

    /// <summary>
    /// Renders the scene twice (the first frame flushes the uploads) and reads the target back.
    /// </summary>
    [[nodiscard]] core::image render_and_read(
      renderer_type &instance,
      const render_scene &scene,
      render_target_id target
    ) {
        CONJURE_REQUIRE_OK(instance.render(scene));
        CONJURE_REQUIRE_OK(instance.render(scene));
        core::result<core::image> readBack = instance.read_back(target);
        CONJURE_REQUIRE_OK(readBack);
        return std::move(readBack).value();
    }
}

TEST_SUITE("renderer/gpu") {

    TEST_CASE("gpu - a lit sphere shades brightest at the centre, dark at the rim and black outside") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer(true);
        const render_target_id target = make_target(*fixture, TARGET_SIZE);
        const mesh_asset sphereMesh = mesh_asset::uv_sphere(1.0f, 32, 64);
        const gpu_mesh_id sphere = fixture->upload_mesh(test_asset_id(1), sphereMesh);
        const gpu_material_id material = fixture->upload_material(test_asset_id(2), white_rough());

        render_scene scene = {};
        render_view view = look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), target, 0.6f);
        view.flags = view_flags::NO_POST_PROCESS;
        scene.views.push_back(view);
        scene.meshes.push_back(instance_at(sphere, material, math::vec3(0.0f), sphereMesh.bounds()));
        light sun = {};
        sun.type = light_type::DIRECTIONAL;
        sun.direction = math::vec3(0.0f, 0.0f, -1.0f);   // from the camera toward the sphere
        sun.intensity = math::HALF_PI;                    // Lambert: albedo / pi * E = 0.5 at the centre
        sun.castsShadows = false;
        scene.lights.push_back(sun);
        scene.shadows.enabled = false;
        scene.ssao.enabled = false;

        CONJURE_REQUIRE_OK(fixture->render(scene));
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().viewsRendered == 1);
        CHECK(fixture->stats().instancesDrawn >= 1);   // the prepass and the forward pass both reference it
        core::result<core::image> readBack = fixture->read_back(target);
        CONJURE_REQUIRE_OK(readBack);
        const core::image &image = readBack.value();
        REQUIRE(image.width() == TARGET_SIZE.x);
        REQUIRE(image.height() == TARGET_SIZE.y);

        const math::mat4 clipFromWorld = clip_from_world_of(view);
        const core::rgba8 centre = pixel_at(image, clipFromWorld, math::vec3(0.0f, 0.0f, 1.0f));
        const core::rgba8 mid = pixel_at(image, clipFromWorld, glm::normalize(math::vec3(0.7f, 0.0f, 0.7f)));
        const core::rgba8 rim = pixel_at(image, clipFromWorld, glm::normalize(math::vec3(0.97f, 0.0f, 0.24f)));
        const core::rgba8 corner = image.pixel(2, 2);
        CHECK(corner.r == 0);
        CHECK(corner.g == 0);
        CHECK(corner.b == 0);
        CHECK(centre.a == 255);
        const f32 centreLinear = linear_luminance(centre);
        const f32 midLinear = linear_luminance(mid);
        const f32 rimLinear = linear_luminance(rim);
        CHECK(centreLinear > midLinear);
        CHECK(midLinear > rimLinear);
        // Lambert with the fallback ambient (radiance 0.25 -> 0.25 * albedo): 0.5 + 0.25 at the centre.
        CHECK(centreLinear == doctest::Approx(0.75f).epsilon(0.25));
        // White material: the shading is grey.
        CHECK(std::abs(static_cast<i32>(centre.r) - static_cast<i32>(centre.g)) <= 2);
        CHECK(std::abs(static_cast<i32>(centre.g) - static_cast<i32>(centre.b)) <= 2);

        // The mean linear luminance of the projected disc follows the analytic cosine average
        // (2/3 of the peak sun term) plus the ambient: 2/3 * 0.5 + 0.25 = 0.583.
        const math::uvec2 centrePixel = project_to_pixel(clipFromWorld, math::vec3(0.0f, 0.0f, 1.0f), TARGET_SIZE);
        const math::uvec2 edgePixel = project_to_pixel(clipFromWorld, math::vec3(1.0f, 0.0f, 0.0f), TARGET_SIZE);
        const f32 radius = static_cast<f32>(edgePixel.x - centrePixel.x);
        REQUIRE(radius > 10.0f);
        f32 sum = 0.0f;
        u32 count = 0;
        for (u32 y = 0; y < TARGET_SIZE.y; ++y) {
            for (u32 x = 0; x < TARGET_SIZE.x; ++x) {
                const f32 dx = static_cast<f32>(x) - static_cast<f32>(centrePixel.x);
                const f32 dy = static_cast<f32>(y) - static_cast<f32>(centrePixel.y);
                if (dx * dx + dy * dy <= (radius - 1.5f) * (radius - 1.5f)) {
                    sum += linear_luminance(image.pixel(x, y));
                    ++count;
                }
            }
        }
        REQUIRE(count > 100);
        CHECK(sum / static_cast<f32>(count) == doctest::Approx(0.583f).epsilon(0.3));
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("gpu - a floor pixel under a cube is darker than one in the open under a shadowing sun") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer(true);
        const render_target_id target = make_target(*fixture, TARGET_SIZE);
        const mesh_asset floorMesh = floor_mesh();
        const mesh_asset cubeMesh = mesh_asset::unit_cube();
        const gpu_mesh_id floor = fixture->upload_mesh(test_asset_id(1), floorMesh);
        const gpu_mesh_id cube = fixture->upload_mesh(test_asset_id(2), cubeMesh);
        const gpu_material_id material = fixture->upload_material(test_asset_id(3), white_rough());

        render_scene scene = {};
        render_view view = look_at_view(math::vec3(0.0f, 6.0f, 6.0f), math::vec3(0.0f), target, 0.8f);
        view.flags = view_flags::NO_POST_PROCESS;
        scene.views.push_back(view);
        scene.meshes.push_back(instance_at(floor, material, math::vec3(0.0f), floorMesh.bounds()));
        mesh_instance box = instance_at(cube, material, math::vec3(0.0f, 1.5f, 0.0f), cubeMesh.bounds());
        scene.meshes.push_back(box);
        light sun = {};
        sun.type = light_type::DIRECTIONAL;
        sun.direction = math::vec3(0.0f, -1.0f, 0.0f);
        sun.intensity = math::PI;
        sun.castsShadows = true;
        sun.filter = shadow_filter::HARD;
        scene.lights.push_back(sun);
        scene.ssao.enabled = false;
        scene.shadows.distance = 50.0f;

        const auto renderAndRead = [&]() -> core::image {
            CONJURE_REQUIRE_OK(fixture->render(scene));
            CONJURE_REQUIRE_OK(fixture->render(scene));
            core::result<core::image> readBack = fixture->read_back(target);
            CONJURE_REQUIRE_OK(readBack);
            return std::move(readBack).value();
        };
        const math::mat4 clipFromWorld = clip_from_world_of(view);
        const core::image shadowed = renderAndRead();
        CHECK(fixture->stats().shadowCascadesRendered == MAX_SHADOW_CASCADES);
        const core::rgba8 underCube = pixel_at(shadowed, clipFromWorld, math::vec3(0.0f, 0.0f, 0.0f));
        const core::rgba8 open = pixel_at(shadowed, clipFromWorld, math::vec3(3.0f, 0.0f, 0.0f));
        const f32 underLinear = linear_luminance(underCube);
        const f32 openLinear = linear_luminance(open);
        CHECK(openLinear > 0.5f);                       // sun from above on a white floor plus ambient
        CHECK(underLinear < openLinear * 0.6f);         // only the ambient remains in the shadow
        CHECK(underLinear > 0.05f);                     // ambient keeps it from being black

        // Without shadows both floor pixels receive the sun and the difference vanishes.
        scene.shadows.enabled = false;
        const core::image unshadowed = renderAndRead();
        CHECK(fixture->stats().shadowCascadesRendered == 0);
        const f32 underUnshadowed = linear_luminance(pixel_at(unshadowed, clipFromWorld, math::vec3(0.0f)));
        const f32 openUnshadowed = linear_luminance(pixel_at(unshadowed, clipFromWorld, math::vec3(3.0f, 0.0f, 0.0f)));
        CHECK(underUnshadowed == doctest::Approx(openUnshadowed).epsilon(0.1));
        CHECK(underUnshadowed > underLinear * 1.5f);

        // PCF and PCSS produce a shadow as well.
        scene.shadows.enabled = true;
        for (const shadow_filter filter : {shadow_filter::PCF, shadow_filter::PCSS}) {
            scene.lights[0].filter = filter;
            const core::image filtered = renderAndRead();
            const f32 filteredUnder = linear_luminance(pixel_at(filtered, clipFromWorld, math::vec3(0.0f)));
            const f32 filteredOpen = linear_luminance(pixel_at(filtered, clipFromWorld, math::vec3(3.0f, 0.0f, 0.0f)));
            CAPTURE(static_cast<int>(filter));
            CHECK(filteredUnder < filteredOpen * 0.6f);
        }
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("regression - a tilted receiver facing the sun is not self-shadowed under the reverse-Z bias") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer(true);
        const render_target_id target = make_target(*fixture, TARGET_SIZE);
        const mesh_asset sphereMesh = mesh_asset::uv_sphere(1.0f, 32, 64);
        const gpu_mesh_id sphere = fixture->upload_mesh(test_asset_id(1), sphereMesh);
        const gpu_material_id material = fixture->upload_material(test_asset_id(2), white_rough());

        render_scene scene = {};
        render_view view = look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), target, 0.6f);
        view.flags = view_flags::NO_POST_PROCESS;
        scene.views.push_back(view);
        scene.meshes.push_back(instance_at(sphere, material, math::vec3(0.0f), sphereMesh.bounds()));
        light sun = {};
        sun.type = light_type::DIRECTIONAL;
        sun.direction = glm::normalize(math::vec3(0.0f, -1.0f, -1.0f));   // from above and behind the camera
        sun.intensity = math::PI;
        sun.castsShadows = true;
        sun.filter = shadow_filter::HARD;
        scene.lights.push_back(sun);
        scene.ssao.enabled = false;
        scene.shadows.distance = 50.0f;

        const auto renderAndRead = [&]() -> core::image {
            CONJURE_REQUIRE_OK(fixture->render(scene));
            CONJURE_REQUIRE_OK(fixture->render(scene));
            core::result<core::image> readBack = fixture->read_back(target);
            CONJURE_REQUIRE_OK(readBack);
            return std::move(readBack).value();
        };
        const math::mat4 clipFromWorld = clip_from_world_of(view);
        // Receivers on the lit hemisphere at 40 and 58 degrees to the sun: a bias of the wrong sign
        // pushes the stored sphere toward the light past the normal offset and darkens them (acne).
        const std::array<math::vec3, 2> receivers = {
          glm::normalize(math::vec3(0.5f, 0.3f, 0.8f)),
          glm::normalize(math::vec3(0.8f, 0.2f, 0.55f)),
        };
        const core::image shadowed = renderAndRead();
        CHECK(fixture->stats().shadowCascadesRendered == MAX_SHADOW_CASCADES);
        scene.shadows.enabled = false;
        const core::image unshadowed = renderAndRead();
        for (const math::vec3 &receiver : receivers) {
            CAPTURE(receiver.x);
            const f32 lit = linear_luminance(pixel_at(unshadowed, clipFromWorld, receiver));
            const f32 tested = linear_luminance(pixel_at(shadowed, clipFromWorld, receiver));
            CHECK(lit > 0.3f);
            CHECK(tested == doctest::Approx(lit).epsilon(0.1));
        }
        // The hemisphere facing away from the sun is in its own shadow either way (ambient only).
        const math::vec3 away = glm::normalize(math::vec3(0.0f, -0.8f, 0.6f));
        const f32 back = linear_luminance(pixel_at(shadowed, clipFromWorld, away));
        CHECK(back < 0.4f);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("regression - PCSS on the cascades filters differently from PCF and keeps the shadow") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer(true);
        const render_target_id target = make_target(*fixture, TARGET_SIZE);
        const mesh_asset floorMesh = floor_mesh();
        const mesh_asset cubeMesh = mesh_asset::unit_cube();
        const gpu_mesh_id floor = fixture->upload_mesh(test_asset_id(1), floorMesh);
        const gpu_mesh_id cube = fixture->upload_mesh(test_asset_id(2), cubeMesh);
        const gpu_material_id material = fixture->upload_material(test_asset_id(3), white_rough());

        render_scene scene = {};
        render_view view = look_at_view(math::vec3(0.0f, 6.0f, 6.0f), math::vec3(0.0f), target, 0.8f);
        view.flags = view_flags::NO_POST_PROCESS;
        scene.views.push_back(view);
        scene.meshes.push_back(instance_at(floor, material, math::vec3(0.0f), floorMesh.bounds()));
        scene.meshes.push_back(instance_at(cube, material, math::vec3(0.0f, 1.5f, 0.0f), cubeMesh.bounds()));
        light sun = {};
        sun.type = light_type::DIRECTIONAL;
        sun.direction = math::vec3(0.0f, -1.0f, 0.0f);
        sun.intensity = math::PI;
        sun.castsShadows = true;
        sun.sourceRadius = 0.3f;   // a wide sun: the penumbra under the cube spans several texels
        scene.lights.push_back(sun);
        scene.ssao.enabled = false;
        scene.shadows.distance = 50.0f;

        const auto renderAndRead = [&](shadow_filter filter) -> core::image {
            scene.lights[0].filter = filter;
            CONJURE_REQUIRE_OK(fixture->render(scene));
            CONJURE_REQUIRE_OK(fixture->render(scene));
            core::result<core::image> readBack = fixture->read_back(target);
            CONJURE_REQUIRE_OK(readBack);
            return std::move(readBack).value();
        };
        const math::mat4 clipFromWorld = clip_from_world_of(view);
        const core::image pcf = renderAndRead(shadow_filter::PCF);
        const core::image pcss = renderAndRead(shadow_filter::PCSS);
        const core::image pcfAgain = renderAndRead(shadow_filter::PCF);
        const auto differing = [](const core::image &a, const core::image &b) {
            u32 count = 0;
            for (u32 y = 0; y < TARGET_SIZE.y; ++y) {
                for (u32 x = 0; x < TARGET_SIZE.x; ++x) {
                    const core::rgba8 left = a.pixel(x, y);
                    const core::rgba8 right = b.pixel(x, y);
                    if (std::abs(static_cast<i32>(left.r) - static_cast<i32>(right.r)) > 2) {
                        ++count;
                    }
                }
            }
            return count;
        };
        CHECK(differing(pcf, pcfAgain) == 0);   // the frame is deterministic
        CHECK(differing(pcf, pcss) > 0);         // PCSS is its own filter, not PCF under another name
        const f32 underCube = linear_luminance(pixel_at(pcss, clipFromWorld, math::vec3(0.0f)));
        const f32 open = linear_luminance(pixel_at(pcss, clipFromWorld, math::vec3(3.0f, 0.0f, 0.0f)));
        CHECK(open > 0.5f);
        CHECK(underCube < open * 0.6f);
        CHECK(underCube > 0.05f);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("gpu - the post-processed frame of an emissive quad is non-black and the bloom chain runs") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer(true);
        const render_target_id target = make_target(*fixture, TARGET_SIZE);
        const mesh_asset quadMesh = mesh_asset::quad(1.0f);
        const gpu_mesh_id quad = fixture->upload_mesh(test_asset_id(1), quadMesh);
        material_asset glowing = {};
        glowing.desc().unlit = true;
        glowing.desc().baseColourFactor = math::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        glowing.desc().emissiveFactor = math::vec3(1.0f, 0.5f, 0.0f);
        glowing.desc().emissiveStrength = 4.0f;
        const gpu_material_id material = fixture->upload_material(test_asset_id(2), glowing);
        render_scene scene = {};
        const render_view view = look_at_view(math::vec3(0.0f, 0.0f, 3.0f), math::vec3(0.0f), target, 0.9f);
        scene.views.push_back(view);
        scene.meshes.push_back(instance_at(quad, material, math::vec3(0.0f), quadMesh.bounds()));
        scene.postProcess.exposure = exposure_mode::MANUAL;
        scene.postProcess.manualEv100 = 0.0f;
        scene.ssao.enabled = false;
        scene.shadows.enabled = false;
        const std::array<tonemap_operator, 3> operators = {
          tonemap_operator::ACES_FITTED, tonemap_operator::REINHARD_EXTENDED, tonemap_operator::AGX
        };
        for (const tonemap_operator operatorValue : operators) {
            scene.postProcess.tonemap = operatorValue;
            CONJURE_REQUIRE_OK(fixture->render(scene));
            CHECK(fixture->stats().executed("bloom_down_0"));
            CHECK(fixture->stats().executed("bloom_up_0"));
            CHECK(fixture->stats().executed("tonemap"));
            CHECK_FALSE(fixture->stats().executed("exposure_histogram"));
            core::result<core::image> readBack = fixture->read_back(target);
            CONJURE_REQUIRE_OK(readBack);
            const core::rgba8 centre = pixel_at(readBack.value(), clip_from_world_of(view), math::vec3(0.0f));
            CAPTURE(static_cast<int>(operatorValue));
            CHECK(centre.r > 100);
            CHECK(centre.r >= centre.g);
            CHECK(centre.g > centre.b);
            // Bloom spreads light past the quad's edge.
            const core::rgba8 beside = pixel_at(
              readBack.value(),
              clip_from_world_of(view),
              math::vec3(1.15f, 0.0f, 0.0f)
            );
            CHECK(beside.r > 0);
        }
        // Automatic exposure runs the histogram and converges to a non-black frame too.
        scene.postProcess.exposure = exposure_mode::AUTOMATIC;
        scene.deltaSeconds = 1.0f;
        for (u32 frame = 0; frame < 8; ++frame) {
            CONJURE_REQUIRE_OK(fixture->render(scene));
        }
        CHECK(fixture->stats().executed("exposure_histogram"));
        CHECK(fixture->stats().executed("exposure_adapt"));
        core::result<core::image> adapted = fixture->read_back(target);
        CONJURE_REQUIRE_OK(adapted);
        CHECK(pixel_at(adapted.value(), clip_from_world_of(view), math::vec3(0.0f)).r > 60);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("regression - under the uniform fallback sky a smooth grey metal sphere reflects more than a rough one") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer(true);
        const render_target_id target = make_target(*fixture, TARGET_SIZE);
        const mesh_asset sphereMesh = mesh_asset::uv_sphere(1.0f, 32, 64);
        const gpu_mesh_id sphere = fixture->upload_mesh(test_asset_id(1), sphereMesh);
        const auto greyMetal = [](f32 roughness) {
            material_asset material = {};
            material.desc().baseColourFactor = math::vec4(0.5f, 0.5f, 0.5f, 1.0f);
            material.desc().metallicFactor = 1.0f;
            material.desc().roughnessFactor = roughness;
            return material;
        };
        const gpu_material_id smooth = fixture->upload_material(test_asset_id(2), greyMetal(0.05f));
        const gpu_material_id rough = fixture->upload_material(test_asset_id(3), greyMetal(1.0f));

        // No lights, no shadows, no SSAO: a metal is lit by the split-sum specular of the constant
        // fallback sky alone, so only the BRDF lookup table tells the two spheres apart.
        render_scene scene = {};
        render_view view = look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), target, 0.6f);
        view.flags = view_flags::NO_POST_PROCESS;
        scene.views.push_back(view);
        scene.shadows.enabled = false;
        scene.ssao.enabled = false;
        const math::mat4 clipFromWorld = clip_from_world_of(view);
        const auto discLuminance = [&](gpu_material_id material) {
            scene.meshes.clear();
            scene.meshes.push_back(instance_at(sphere, material, math::vec3(0.0f), sphereMesh.bounds()));
            return sphere_disc_luminance(render_and_read(*fixture, scene, target), clipFromWorld);
        };
        const f32 smoothLuminance = discLuminance(smooth);
        const f32 roughLuminance = discLuminance(rough);
        CAPTURE(smoothLuminance);
        CAPTURE(roughLuminance);
        // About 0.5 and 0.3 of the sky radiance; the table baked upside down swapped them.
        CHECK(smoothLuminance > roughLuminance * 1.1f);
        CHECK(smoothLuminance == doctest::Approx(0.5f * FALLBACK_ENVIRONMENT_RADIANCE).epsilon(0.2));
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("regression - GTAO darkens the floor beside a wall equally far along x and y on a 2:1 target") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        // The horizon search measured its step in units of the target's width along both axes, so on a 2:1
        // target it searched half as far vertically. The thickness heuristic, at most 1, ends the occlusion of
        // a wall about half a radius away, so a reach that only the full search has is shown through the
        // projected radius's clamp (CONJURE_GTAO_MAX_RADIUS_PIXELS, 128 half-resolution pixels) instead: with
        // a 10 m radius the search reaches 128 pixels, 0.5 m of floor in this view, while the falloff ends at
        // 20 m. Walls 0.28 to 0.38 m from the floor patch at the centre lie beyond the 0.25 m the vertical
        // search reached. One pair of walls stands across x and one across z, each read against the bare
        // floor through the ambient-occlusion debug view. Measured on WARP: 0.505 along both; with the step
        // measured in the width, 0.547 along x and 0 along z.
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer(true);
        constexpr math::uvec2 WIDE = math::uvec2(1024, 512);
        const render_target_id target = make_target(*fixture, WIDE);
        const mesh_asset floorMesh = floor_mesh();
        const mesh_asset cubeMesh = mesh_asset::unit_cube();
        const gpu_mesh_id floor = fixture->upload_mesh(test_asset_id(1), floorMesh);
        const gpu_mesh_id cube = fixture->upload_mesh(test_asset_id(2), cubeMesh);
        const gpu_material_id material = fixture->upload_material(test_asset_id(3), white_rough());

        // From 6 m the view covers 0.5 m of floor above and below the centre: nearly orthographic, so the bare
        // floor is unoccluded and the walls' faces hardly show.
        render_scene scene = {};
        render_view view = top_down_view(6.0f, target, 2.0f * std::atan(0.5f / 6.0f));
        view.debugMode = view_debug_mode::AMBIENT_OCCLUSION;
        scene.views.push_back(view);
        scene.meshes.push_back(instance_at(floor, material, math::vec3(0.0f), floorMesh.bounds()));
        scene.shadows.enabled = false;
        scene.ssao.radius = MAX_SSAO_RADIUS;
        scene.ssao.thickness = 1.0f;
        scene.ssao.intensity = 3.0f;
        const core::image bare = render_and_read(*fixture, scene, target);

        // 0.4 m high walls, 0.3 m thick and 1 m long, whose inner faces stand 0.33 m from the centre.
        constexpr f32 NEAR_FACE = 0.33f;
        constexpr f32 FAR_FACE = NEAR_FACE + 0.3f;
        constexpr f32 HEIGHT = 0.4f;
        constexpr f32 HALF_LENGTH = 0.5f;
        const math::vec3 acrossX(FAR_FACE - NEAR_FACE, HEIGHT, 2.0f * HALF_LENGTH);
        const math::vec3 acrossZ(2.0f * HALF_LENGTH, HEIGHT, FAR_FACE - NEAR_FACE);
        const math::vec3 positiveX(NEAR_FACE, 0.0f, -HALF_LENGTH);
        const math::vec3 negativeX(-FAR_FACE, 0.0f, -HALF_LENGTH);
        const math::vec3 positiveZ(-HALF_LENGTH, 0.0f, NEAR_FACE);
        const math::vec3 negativeZ(-HALF_LENGTH, 0.0f, -FAR_FACE);
        scene.meshes.push_back(box_between(cube, material, positiveX, positiveX + acrossX));
        scene.meshes.push_back(box_between(cube, material, negativeX, negativeX + acrossX));
        const core::image wallsAcrossX = render_and_read(*fixture, scene, target);
        CHECK(fixture->stats().executed("ssao_gtao"));
        scene.meshes.resize(1);
        scene.meshes.push_back(box_between(cube, material, positiveZ, positiveZ + acrossZ));
        scene.meshes.push_back(box_between(cube, material, negativeZ, negativeZ + acrossZ));
        const core::image wallsAcrossZ = render_and_read(*fixture, scene, target);

        const f32 aspect = static_cast<f32>(WIDE.x) / static_cast<f32>(WIDE.y);
        const math::mat4 clipFromWorld = clip_from_view(view.cameraData, aspect) * view.viewFromWorld;
        // The read-back is the AO term as grey; the darkening is the bare floor's term minus the term with the
        // walls, over the floor within 5 cm of the centre (the bound past 0.05 keeps the last row of samples).
        const math::vec2 patchMinimum(-0.05f);
        const math::vec2 patchMaximum(0.051f);
        const f32 bareFloor = floor_luminance(bare, clipFromWorld, patchMinimum, patchMaximum);
        const f32 alongX = bareFloor - floor_luminance(wallsAcrossX, clipFromWorld, patchMinimum, patchMaximum);
        const f32 alongZ = bareFloor - floor_luminance(wallsAcrossZ, clipFromWorld, patchMinimum, patchMaximum);
        CAPTURE(bareFloor);
        CAPTURE(alongX);
        CAPTURE(alongZ);
        CHECK(bareFloor > 0.95f);
        CHECK(alongX > 0.2f);
        CHECK(alongZ > alongX * 0.8f);
        CHECK(alongZ < alongX / 0.8f);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("regression - a normal-mapped quad under non-uniform scale bends its normal along the world tangent") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer(true);
        const render_target_id target = make_target(*fixture, TARGET_SIZE);

        // A quad facing +Z with its UVs turned 45 degrees: U grows along (1, 1, 0), which makes the
        // generated tangent (1, 1, 0) / sqrt(2).
        mesh_geometry geometry = {};
        geometry.positions = {
          math::vec3(-1.0f, -1.0f, 0.0f), math::vec3(1.0f, -1.0f, 0.0f),
          math::vec3(1.0f, 1.0f, 0.0f),   math::vec3(-1.0f, 1.0f, 0.0f),
        };
        geometry.normals.assign(4, math::vec3(0.0f, 0.0f, 1.0f));
        geometry.uv0 = {math::vec2(0.0f, 0.5f), math::vec2(0.5f, 1.0f), math::vec2(1.0f, 0.5f), math::vec2(0.5f, 0.0f)};
        geometry.indices = {0, 1, 2, 0, 2, 3};
        core::result<mesh_asset> quadMesh = mesh_asset::from_geometry(geometry, "turned_quad");
        CONJURE_REQUIRE_OK(quadMesh);
        const gpu_mesh_id quad = fixture->upload_mesh(test_asset_id(1), quadMesh.value());

        // A constant normal map tilted 30 degrees toward +tangent: (sin 30, 0, cos 30) encoded.
        const texture_asset normalMap = texture_asset::solid_colour(
          core::rgba8{191, 128, 238, 255},
          texture_colour_space::LINEAR,
          4,
          4
        );
        static_cast<void>(fixture->upload_texture(test_asset_id(2), normalMap));
        material_asset bumped = white_rough();
        bumped.desc().normalScale = 1.0f;
        bumped.desc().textures[static_cast<usize>(material_texture::NORMAL)] =
          assets::asset_ref<texture_asset>(test_asset_id(2));
        const gpu_material_id material = fixture->upload_material(test_asset_id(3), bumped);

        render_scene scene = {};
        render_view view = look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), target, 0.6f);
        view.flags = view_flags::NO_POST_PROCESS;
        scene.views.push_back(view);
        // Scale (4, 1, 1): the world tangent is M * t, along (4, 1, 0); the normal matrix (the inverse
        // transpose) turns it to (1, 4, 0) instead.
        mesh_instance stretched = instance_at(quad, material, math::vec3(0.0f), quadMesh.value().bounds());
        stretched.worldFromLocal = glm::scale(math::mat4(1.0f), math::vec3(4.0f, 1.0f, 1.0f));
        stretched.worldBounds.min = math::vec3(-4.0f, -1.0f, 0.0f);
        stretched.worldBounds.max = math::vec3(4.0f, 1.0f, 0.0f);
        scene.meshes.push_back(stretched);
        scene.shadows.enabled = false;
        scene.ssao.enabled = false;

        const math::vec3 surfaceNormal(0.0f, 0.0f, 1.0f);
        const f32 sinTilt = 0.5f;
        const f32 cosTilt = std::sqrt(3.0f) * 0.5f;
        const math::vec3 expected = glm::normalize(
          glm::normalize(math::vec3(4.0f, 1.0f, 0.0f)) * sinTilt + surfaceNormal * cosTilt
        );
        const math::vec3 mistaken = glm::normalize(
          glm::normalize(math::vec3(1.0f, 4.0f, 0.0f)) * sinTilt + surfaceNormal * cosTilt
        );
        const auto centreUnderLightAlong = [&](const math::vec3 &normal) {
            light sun = {};
            sun.type = light_type::DIRECTIONAL;
            sun.direction = -normal;
            sun.intensity = math::HALF_PI;              // Lambert: albedo / pi * E = 0.5 at N.L = 1
            sun.castsShadows = false;
            scene.lights.clear();
            scene.lights.push_back(sun);
            const core::image image = render_and_read(*fixture, scene, target);
            return linear_luminance(pixel_at(image, clip_from_world_of(view), math::vec3(0.0f)));
        };
        const f32 aligned = centreUnderLightAlong(expected);
        const f32 misaligned = centreUnderLightAlong(mistaken);
        CAPTURE(aligned);
        CAPTURE(misaligned);
        // N.L is 1 for the light along the true perturbed normal and 0.87 for the other one: 0.5 and
        // 0.43 of direct light on top of the 0.25 ambient.
        CHECK(aligned > misaligned + 0.03f);
        CHECK(aligned == doctest::Approx(0.75f).epsilon(0.15));
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("regression - code sweep F104 [gpu] a submitted view shows the scene in its own render target") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        // What the editor's viewport does: the scene's own camera draws to the swapchain while a second view,
        // submitted for the frame, draws the same scene through another camera into an off-screen target.
        log_capture logs = {};
        std::unique_ptr<rhi::device> device = make_device(true);
        std::unique_ptr<rhi::shader_library> shaders = open_shaders(*device);
        rhi::swapchain_desc swapchainDesc = {};
        swapchainDesc.width = 160;
        swapchainDesc.height = 96;
        core::result<std::unique_ptr<rhi::swapchain>> swapchain = rhi::swapchain::create(
          *device,
          rhi::native_window(),
          swapchainDesc
        );
        CONJURE_REQUIRE_OK(swapchain);
        renderer_desc desc = headless_desc(*device, *shaders);
        desc.swapchain = swapchain.value().get();
        core::result<std::unique_ptr<renderer_type>> created = renderer_type::create(desc);
        CONJURE_REQUIRE_OK(created);
        renderer_type &instance = *created.value();
        const render_target_id target = make_target(instance, TARGET_SIZE);

        const mesh_asset sphereMesh = mesh_asset::uv_sphere(1.0f, 32, 64);
        const gpu_mesh_id sphere = instance.upload_mesh(test_asset_id(1), sphereMesh);
        const gpu_material_id material = instance.upload_material(test_asset_id(2), white_rough());
        light sun = {};
        sun.type = light_type::DIRECTIONAL;
        sun.direction = math::vec3(0.0f, 0.0f, -1.0f);
        sun.intensity = math::HALF_PI;
        sun.castsShadows = false;

        // The off-screen camera looks at the sphere from +z; the swapchain camera from the side.
        render_view offscreen = look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), target, 0.6f);
        offscreen.flags = view_flags::NO_POST_PROCESS;
        // The first frame flushes the uploads; the second one draws the mesh.
        for (u32 frame = 0; frame < 2; ++frame) {
            render_scene &scene = instance.begin_extract();
            scene.views.push_back(look_at_view(math::vec3(5.0f, 0.0f, 0.0f), math::vec3(0.0f), render_target_id()));
            scene.meshes.push_back(instance_at(sphere, material, math::vec3(0.0f), sphereMesh.bounds()));
            scene.lights.push_back(sun);
            scene.shadows.enabled = false;
            scene.ssao.enabled = false;
            instance.submit_view(offscreen);
            instance.end_extract();
            CONJURE_REQUIRE_OK(instance.render_frame());
            CHECK(instance.stats().viewsRendered == 2);
        }

        core::result<core::image> readBack = instance.read_back(target);
        CONJURE_REQUIRE_OK(readBack);
        const core::image &image = readBack.value();
        const math::mat4 clipFromWorld = clip_from_world_of(offscreen);
        // The lit sphere at the projected centre, nothing in the corner.
        const core::rgba8 centre = pixel_at(image, clipFromWorld, math::vec3(0.0f, 0.0f, 1.0f));
        CHECK(linear_luminance(centre) == doctest::Approx(0.75f).epsilon(0.25));
        CHECK(centre.a == 255);
        const core::rgba8 corner = image.pixel(2, 2);
        CHECK(corner.r == 0);
        CHECK(corner.g == 0);
        CHECK(corner.b == 0);
        CHECK(logs.count(core::log_level::ERR) == 0);

        created.value().reset();
        swapchain.value().reset();
    }

    TEST_CASE("regression - code sweep F384 [gpu] a target's depth holds the scene depth its first view drew") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        // The target's depth texture used to be allocated and never drawn into: the views drew into a
        // transient depth buffer and the read-back had nothing to read. It now holds the reverse-Z depth of
        // the first view of the last frame, cleared to the far plane (0) where nothing was drawn.
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer(true);
        const render_target_id target = make_target(*fixture, TARGET_SIZE);
        const mesh_asset sphereMesh = mesh_asset::uv_sphere(1.0f, 32, 64);
        const gpu_mesh_id sphere = fixture->upload_mesh(test_asset_id(1), sphereMesh);
        const gpu_material_id material = fixture->upload_material(test_asset_id(2), white_rough());
        render_view facing = look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), target, 0.6f);
        facing.flags = view_flags::NO_POST_PROCESS;
        render_view away = look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f, 0.0f, 10.0f), target, 0.6f);
        away.flags = view_flags::NO_POST_PROCESS;
        render_scene scene = {};
        scene.meshes.push_back(instance_at(sphere, material, math::vec3(0.0f), sphereMesh.bounds()));
        scene.shadows.enabled = false;
        scene.ssao.enabled = false;

        // The sphere's nearest point projects to the centre pixel at the depth the projection gives it.
        const math::mat4 clipFromWorld = clip_from_world_of(facing);
        const math::vec4 nearest = clipFromWorld * math::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        const f32 expectedDepth = nearest.z / nearest.w;
        REQUIRE(expectedDepth > 0.0f);
        const math::uvec2 centre = project_to_pixel(clipFromWorld, math::vec3(0.0f, 0.0f, 1.0f), TARGET_SIZE);
        REQUIRE(centre.x != std::numeric_limits<u32>::max());
        const auto depthAt = [&fixture, target](math::uvec2 pixel) {
            core::result<depth_read_back> depth = fixture->read_back_depth(target);
            CONJURE_REQUIRE_OK(depth);
            REQUIRE(depth.value().size == TARGET_SIZE);
            return depth.value().depth[static_cast<usize>(pixel.y) * TARGET_SIZE.x + pixel.x];
        };

        // The first frame flushes the uploads; the second draws the sphere into the target's depth.
        scene.views = {facing};
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(depthAt(centre) == doctest::Approx(expectedDepth).epsilon(0.01));
        CHECK(depthAt(math::uvec2(2, 2)) == 0.0f);

        // Two views into the target: the first owns the depth, the second draws with a transient buffer.
        scene.views = {away, facing};
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().viewsRendered == 2);
        CHECK(depthAt(centre) == 0.0f);
        scene.views = {facing, away};
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(depthAt(centre) == doctest::Approx(expectedDepth).epsilon(0.01));

        // A frame without a view into the target leaves its depth as the last such frame left it.
        const render_target_id other = make_target(*fixture, TARGET_SIZE);
        scene.views = {look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), other, 0.6f)};
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(fixture->stats().viewsRendered == 1);
        CHECK(depthAt(centre) == doctest::Approx(expectedDepth).epsilon(0.01));

        // A frame with nothing to draw leaves the far plane, not the previous frame's sphere.
        scene.views = {facing};
        scene.meshes.clear();
        CONJURE_REQUIRE_OK(fixture->render(scene));
        CHECK(depthAt(centre) == 0.0f);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("regression - code sweep F044 [gpu] rendering, resizing the swapchain and rendering again succeeds") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        // The debug layer is on and its messages reach the log: a back buffer released while the queue still
        // uses it, or a ResizeBuffers refused over a live framebuffer, fails the case.
        log_capture logs = {};
        rhi::device_desc deviceDesc = software_device_desc();
        deviceDesc.enableDebugLayer = true;
        core::result<std::unique_ptr<rhi::device>> device = rhi::device::create(deviceDesc);
        CONJURE_REQUIRE_OK(device);
        std::unique_ptr<rhi::shader_library> shaders = open_shaders(*device.value());
        rhi::swapchain_desc swapchainDesc = {};
        swapchainDesc.width = 160;
        swapchainDesc.height = 96;
        core::result<std::unique_ptr<rhi::swapchain>> swapchain = rhi::swapchain::create(
          *device.value(),
          rhi::native_window(),
          swapchainDesc
        );
        CONJURE_REQUIRE_OK(swapchain);
        renderer_desc desc = headless_desc(*device.value(), *shaders);
        desc.swapchain = swapchain.value().get();
        core::result<std::unique_ptr<renderer_type>> created = renderer_type::create(desc);
        CONJURE_REQUIRE_OK(created);
        renderer_type &instance = *created.value();

        const mesh_asset sphereMesh = mesh_asset::uv_sphere(1.0f, 16, 32);
        const gpu_mesh_id sphere = instance.upload_mesh(test_asset_id(1), sphereMesh);
        const gpu_material_id material = instance.upload_material(test_asset_id(2), white_rough());
        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), render_target_id(), 0.6f));
        scene.meshes.push_back(instance_at(sphere, material, math::vec3(0.0f), sphereMesh.bounds()));
        light sun = {};
        sun.type = light_type::DIRECTIONAL;
        sun.direction = math::vec3(0.0f, 0.0f, -1.0f);
        sun.intensity = math::HALF_PI;
        scene.lights.push_back(sun);

        // More frames than back buffers: every buffer has a cached framebuffer and a present behind it.
        for (u32 frame = 0; frame < 5; ++frame) {
            CONJURE_REQUIRE_OK(instance.render(scene));
        }
        const std::array<math::uvec2, 3> sizes = {math::uvec2(96, 64), math::uvec2(320, 200), math::uvec2(1, 1)};
        for (const math::uvec2 size : sizes) {
            CAPTURE(size.x);
            // Straight after a present: the resize itself must wait for it.
            CONJURE_REQUIRE_OK(instance.resize_swapchain(size));
            CHECK(swapchain.value()->desc().width == size.x);
            CHECK(swapchain.value()->desc().height == size.y);
            for (u32 frame = 0; frame < 4; ++frame) {
                CONJURE_REQUIRE_OK(instance.render(scene));
                CHECK(instance.stats().viewsRendered == 1);
            }
        }
        CHECK_FALSE(device.value()->is_lost());

        // Teardown in the engine's order, with the last present still queued.
        created.value().reset();
        swapchain.value().reset();
        shaders.reset();
        device.value().reset();
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("regression - code sweep F612 [gpu] a swapchain refusing the acquire is logged once until one succeeds") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        // The headless case injects the refusal with null_fault::ACQUIRE_OUT_OF_DATE; this one takes the
        // path of a real swapchain. On D3D12 a resize that goes around the renderer is refused by
        // ResizeBuffers (the renderer still holds framebuffers over the back buffers) and leaves the
        // swapchain without back buffers, so every acquire answers OUT_OF_DATE until
        // renderer::resize_swapchain repairs it.
        // Each such frame used to log a WARN.
        log_capture logs = {};
        std::unique_ptr<rhi::device> device = make_device(true);
        std::unique_ptr<rhi::shader_library> shaders = open_shaders(*device);
        rhi::swapchain_desc swapchainDesc = {};
        swapchainDesc.width = 160;
        swapchainDesc.height = 96;
        core::result<std::unique_ptr<rhi::swapchain>> swapchain = rhi::swapchain::create(
          *device,
          rhi::native_window(),
          swapchainDesc
        );
        CONJURE_REQUIRE_OK(swapchain);
        renderer_desc desc = headless_desc(*device, *shaders);
        desc.swapchain = swapchain.value().get();
        core::result<std::unique_ptr<renderer_type>> created = renderer_type::create(desc);
        CONJURE_REQUIRE_OK(created);
        renderer_type &instance = *created.value();
        render_scene scene = {};
        scene.views.push_back(look_at_view(math::vec3(0.0f, 0.0f, 5.0f), math::vec3(0.0f), render_target_id()));
        const char *const warningText = " skipped: ";

        for (u32 round = 1; round <= 2; ++round) {
            CAPTURE(round);
            // One frame per back buffer, so a framebuffer of the renderer references each of them.
            for (u32 frame = 0; frame < swapchainDesc.bufferCount; ++frame) {
                CAPTURE(frame);
                CONJURE_REQUIRE_OK(instance.render(scene));
                CHECK(instance.stats().viewsRendered == 1);
            }
            CHECK(warning_count(logs, warningText) == round - 1);

            const core::result<void> refused = swapchain.value()->resize(96 + round, 64);
            CONJURE_CHECK_ERROR(refused, rhi::rhi_error::SWAPCHAIN_CREATION_FAILED);
            REQUIRE(swapchain.value()->buffer_count() == 0);

            // Every frame is skipped and counted; the first of the round is the one that is logged.
            for (u32 frame = 0; frame < 4; ++frame) {
                CAPTURE(frame);
                CONJURE_REQUIRE_OK(instance.render(scene));
                CHECK(instance.stats().viewsRendered == 0);
                CHECK(instance.stats().viewsSkipped == 1);
                CHECK(warning_count(logs, warningText) == round);
            }

            // The acquire that succeeds after the repair re-arms the warning for the next round.
            CONJURE_REQUIRE_OK(instance.resize_swapchain(math::uvec2(96 + round, 64)));
            CONJURE_REQUIRE_OK(instance.render(scene));
            CHECK(instance.stats().viewsRendered == 1);
            CHECK(instance.stats().viewsSkipped == 0);
            CHECK(warning_count(logs, warningText) == round);
        }
        CHECK(logs.contains(core::log_level::WARN, "logged once until an acquire succeeds"));
        CHECK_FALSE(logs.contains(core::log_level::WARN, "present failed"));
        CHECK_FALSE(device->is_lost());

        // Teardown in the engine's order, with the last present still queued.
        created.value().reset();
        swapchain.value().reset();
        shaders.reset();
        device.reset();
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("gpu - a camera's manual EV100 override one stop lower renders its view twice as bright") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer(true);
        const render_target_id sceneTarget = make_target(*fixture, TARGET_SIZE);
        const render_target_id overrideTarget = make_target(*fixture, TARGET_SIZE);
        const mesh_asset floorMesh = floor_mesh();
        const gpu_mesh_id floor = fixture->upload_mesh(test_asset_id(1), floorMesh);
        const gpu_material_id material = fixture->upload_material(test_asset_id(2), white_rough());

        // Manual exposure 0.5 (EV100 1 - log2(1.2)) without bloom or a tonemap curve: the read-back is half
        // the linear shading, about 0.2 on the white floor (0.5 / pi of sun plus 0.25 of ambient).
        render_scene scene = {};
        scene.meshes.push_back(instance_at(floor, material, math::vec3(0.0f), floorMesh.bounds()));
        light sun = {};
        sun.type = light_type::DIRECTIONAL;
        sun.direction = math::vec3(0.0f, -1.0f, 0.0f);
        sun.intensity = 0.5f;
        sun.castsShadows = false;
        scene.lights.push_back(sun);
        scene.shadows.enabled = false;
        scene.ssao.enabled = false;
        scene.postProcess.exposure = exposure_mode::MANUAL;
        scene.postProcess.manualEv100 = 1.0f - std::log2(1.2f);
        scene.postProcess.bloomEnabled = false;
        scene.postProcess.tonemap = tonemap_operator::NONE;
        const render_view sceneView = look_at_view(math::vec3(0.0f, 6.0f, 6.0f), math::vec3(0.0f), sceneTarget, 0.8f);
        render_view overrideView = sceneView;
        overrideView.target = overrideTarget;
        overrideView.settingsOverride = view_settings_override_flags::POST_PROCESS;
        overrideView.postProcess = scene.postProcess;
        overrideView.postProcess.manualEv100 -= 1.0f;
        scene.views.push_back(sceneView);
        scene.views.push_back(overrideView);

        const core::image sceneImage = render_and_read(*fixture, scene, sceneTarget);
        core::result<core::image> overrideImage = fixture->read_back(overrideTarget);
        CONJURE_REQUIRE_OK(overrideImage);
        CHECK(fixture->stats().viewsRendered == 2);
        CHECK(fixture->stats().settingsClamped == 0);
        const math::mat4 clipFromWorld = clip_from_world_of(sceneView);
        const math::vec2 minimum(-1.0f, -1.0f);
        const math::vec2 maximum(1.0f, 1.0f);
        const f32 sceneLuminance = floor_luminance(sceneImage, clipFromWorld, minimum, maximum);
        const f32 overrideLuminance = floor_luminance(overrideImage.value(), clipFromWorld, minimum, maximum);
        CAPTURE(sceneLuminance);
        CAPTURE(overrideLuminance);
        CHECK(sceneLuminance > 0.1f);
        CHECK(overrideLuminance < 0.9f);   // below the clip, so the ratio is the exposure's
        CHECK(overrideLuminance / sceneLuminance == doctest::Approx(2.0f).epsilon(0.15));
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("gpu - the ambient-occlusion debug view is dark beside a pillar, white on open floor and empty pixels") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer(true);
        const render_target_id target = make_target(*fixture, TARGET_SIZE);
        const mesh_asset floorMesh = floor_mesh();
        const mesh_asset cubeMesh = mesh_asset::unit_cube();
        const gpu_mesh_id floor = fixture->upload_mesh(test_asset_id(1), floorMesh);
        const gpu_mesh_id cube = fixture->upload_mesh(test_asset_id(2), cubeMesh);
        const gpu_material_id material = fixture->upload_material(test_asset_id(3), white_rough());

        // From 6 m the view covers about 3.3 m to each side of the centre and the floor, scaled to 5 x 5 m,
        // 2.5 m: the corners of the image are empty. The pillar stands left of the centre with its face toward
        // the centre visible, so the open floor is at the centre, where the view looks straight at it.
        render_scene scene = {};
        render_view view = top_down_view(6.0f, target, 1.0f);
        view.debugMode = view_debug_mode::AMBIENT_OCCLUSION;
        scene.views.push_back(view);
        mesh_instance smallFloor = instance_at(floor, material, math::vec3(0.0f), floorMesh.bounds());
        smallFloor.worldFromLocal = glm::scale(math::mat4(1.0f), math::vec3(0.125f, 1.0f, 0.125f));
        smallFloor.worldBounds.min = math::vec3(-2.5f, 0.0f, -2.5f);
        smallFloor.worldBounds.max = math::vec3(2.5f, 0.0f, 2.5f);
        scene.meshes.push_back(smallFloor);
        scene.meshes.push_back(pillar_at(cube, material, math::vec3(-1.5f, 0.0f, 0.0f)));
        light sun = {};
        sun.type = light_type::DIRECTIONAL;
        sun.direction = math::vec3(0.0f, -1.0f, 0.0f);
        sun.intensity = math::PI;
        sun.castsShadows = true;
        scene.lights.push_back(sun);
        // The debug view shows the term although SSAO is off for the scene, and draws no shadows.
        scene.ssao.enabled = false;
        scene.ssao.radius = 1.0f;
        scene.ssao.thickness = 1.0f;
        scene.ssao.intensity = MAX_SSAO_INTENSITY;

        const core::image image = render_and_read(*fixture, scene, target);
        CHECK(fixture->stats().executed("ssao_gtao"));
        CHECK(fixture->stats().shadowCascadesRendered == 0);
        const math::mat4 clipFromWorld = clip_from_world_of(view);

        // The floor read-back is grey, so its linear luminance is the decoded AO term: the floor within 3 cm of
        // the pillar's face (x = -1) against the floor around the centre.
        const auto ambientOcclusion = [&](const math::vec2 &minimum, const math::vec2 &maximum) {
            return floor_luminance(image, clipFromWorld, minimum, maximum);
        };
        const f32 besidePillar = ambientOcclusion(math::vec2(-0.99f, -0.3f), math::vec2(-0.97f, 0.3f));
        const f32 openFloor = ambientOcclusion(math::vec2(-0.3f, -0.3f), math::vec2(0.3f, 0.3f));
        CAPTURE(besidePillar);
        CAPTURE(openFloor);
        CHECK(besidePillar < 0.8f);
        CHECK(openFloor > 0.95f);
        const core::rgba8 open = pixel_at(image, clipFromWorld, math::vec3(0.0f));
        CHECK(open.r == open.g);
        CHECK(open.g == open.b);
        const core::rgba8 empty = image.pixel(1, 1);
        CHECK(empty.r == 255);
        CHECK(empty.g == 255);
        CHECK(empty.b == 255);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("gpu - a camera's tonemap NONE override clips the emissive quad where ACES compresses it") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer(true);
        const render_target_id sceneTarget = make_target(*fixture, TARGET_SIZE);
        const render_target_id overrideTarget = make_target(*fixture, TARGET_SIZE);
        const mesh_asset quadMesh = mesh_asset::quad(1.0f);
        const gpu_mesh_id quad = fixture->upload_mesh(test_asset_id(1), quadMesh);
        material_asset glowing = {};
        glowing.desc().unlit = true;
        glowing.desc().baseColourFactor = math::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        glowing.desc().emissiveFactor = math::vec3(1.0f, 0.5f, 0.0f);
        glowing.desc().emissiveStrength = 4.0f;
        const gpu_material_id material = fixture->upload_material(test_asset_id(2), glowing);

        // Exposure 1 / 1.2 makes the quad (3.3, 1.7, 0): NONE clips red and green alike, ACES keeps them
        // apart below white.
        render_scene scene = {};
        scene.meshes.push_back(instance_at(quad, material, math::vec3(0.0f), quadMesh.bounds()));
        scene.postProcess.exposure = exposure_mode::MANUAL;
        scene.postProcess.manualEv100 = 0.0f;
        scene.postProcess.bloomEnabled = false;
        scene.postProcess.tonemap = tonemap_operator::ACES_FITTED;
        scene.ssao.enabled = false;
        scene.shadows.enabled = false;
        const render_view sceneView = look_at_view(math::vec3(0.0f, 0.0f, 3.0f), math::vec3(0.0f), sceneTarget, 0.9f);
        render_view overrideView = sceneView;
        overrideView.target = overrideTarget;
        overrideView.settingsOverride = view_settings_override_flags::POST_PROCESS;
        overrideView.postProcess = scene.postProcess;
        overrideView.postProcess.tonemap = tonemap_operator::NONE;
        scene.views.push_back(sceneView);
        scene.views.push_back(overrideView);

        const core::image sceneImage = render_and_read(*fixture, scene, sceneTarget);
        core::result<core::image> overrideImage = fixture->read_back(overrideTarget);
        CONJURE_REQUIRE_OK(overrideImage);
        CHECK(fixture->stats().viewsRendered == 2);
        const math::mat4 clipFromWorld = clip_from_world_of(sceneView);
        const core::rgba8 compressed = pixel_at(sceneImage, clipFromWorld, math::vec3(0.0f));
        const core::rgba8 clipped = pixel_at(overrideImage.value(), clipFromWorld, math::vec3(0.0f));
        CAPTURE(static_cast<int>(compressed.r));
        CAPTURE(static_cast<int>(compressed.g));
        CHECK(clipped.r == 255);
        CHECK(clipped.g == 255);
        CHECK(clipped.b == 0);
        CHECK(compressed.r > compressed.g);
        CHECK(compressed.g < 250);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }
}
