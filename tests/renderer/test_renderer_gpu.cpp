#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "renderer_test_support.h"

#include "conjure/renderer/camera.h"
#include "conjure/renderer/material_asset.h"
#include "conjure/renderer/mesh_asset.h"
#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/renderer.h"

#include "conjure/core/image.h"
#include "conjure/core/log.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <vector>

// [gpu] cases (docs/design/02_renderer.md 10): the frame on the WARP D3D12 device with analytic
// expectations read back from an off-screen target. Skipped (exit 77) where no software adapter
// exists. Post-processing is disabled so the read-back is the linear shading encoded as sRGB.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;
using namespace conjure::testing::renderer_support;

namespace {

    constexpr math::uvec2 TARGET_SIZE = math::uvec2(128, 128);

    [[nodiscard]] render_target_id make_target(renderer_type &instance) {
        render_target_desc desc = {};
        desc.size = TARGET_SIZE;
        desc.colourFormat = rhi::texture_format::RGBA8_SRGB;
        core::result<render_target_id> created = instance.create_render_target(desc);
        CONJURE_REQUIRE_OK(created);
        return created.value();
    }

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
}

TEST_SUITE("renderer/gpu") {

    TEST_CASE("gpu - a lit sphere shades brightest at the centre, dark at the rim and black outside") {
        CONJURE_TEST_REQUIRE(precondition::SOFTWARE_GPU_ADAPTER);
        log_capture logs = {};
        headless_renderer fixture = make_headless_renderer(true);
        const render_target_id target = make_target(*fixture);
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
        const render_target_id target = make_target(*fixture);
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
        const render_target_id target = make_target(*fixture);
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
        const render_target_id target = make_target(*fixture);
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
        const render_target_id target = make_target(*fixture);
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
}
