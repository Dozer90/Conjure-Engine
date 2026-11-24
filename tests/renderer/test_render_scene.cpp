#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"
#include "conjure/testing/assert_capture.h"

#include "conjure/renderer/render_scene.h"
#include "conjure/renderer/render_settings.h"

#include "conjure/core/enum_flags.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <array>
#include <span>

// render_scene.h (docs/design/02_renderer.md 2.3, R4, R8): the extraction contract and the double
// buffered pair with its begin / end protocol.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

TEST_SUITE("renderer/render_scene") {

    TEST_CASE("mesh_instance - defaults are an identity transform on layer 1 casting and receiving shadows") {
        const mesh_instance instance = {};
        CHECK_FALSE(instance.mesh.is_valid());
        for (const gpu_material_id &material : instance.materials) {
            CHECK_FALSE(material.is_valid());
        }
        CHECK(instance.worldFromLocal == math::mat4(1.0f));
        CHECK_FALSE(instance.worldBounds.is_valid());
        CHECK(instance.entityId == 0);
        CHECK(instance.layerMask == 1);
        CHECK(instance.castsShadows);
        CHECK(instance.receivesShadows);
    }

    TEST_CASE("render_view - defaults target the swapchain with every layer, no flags and no settings of its own") {
        const render_view view = {};
        CHECK(view.cameraData == camera());
        CHECK(view.viewFromWorld == math::mat4(1.0f));
        CONJURE_CHECK_APPROX(view.cameraPosition, math::vec3(0.0f));
        CHECK(view.viewport.is_empty());
        CHECK_FALSE(view.target.is_valid());
        CHECK(view.flags == view_flags::NONE);
        CHECK(view.layerMask == 0xFFFFFFFFu);
        CHECK(view.settingsOverride == view_settings_override_flags::NONE);
        CHECK(view.debugMode == view_debug_mode::NONE);
        CHECK(view.postProcess == post_process_settings());
        CHECK(view.ssao == ssao_settings());
        CHECK(static_cast<u8>(view_debug_mode::AMBIENT_OCCLUSION) == 1);
        const view_flags combined = view_flags::DRAW_DEBUG | view_flags::SKIP_SHADOWS;
        CHECK(has_flag(combined, view_flags::DRAW_DEBUG));
        CHECK(has_flag(combined, view_flags::SKIP_SHADOWS));
        CHECK_FALSE(has_flag(combined, view_flags::NO_POST_PROCESS));
    }

    TEST_CASE("effective_view_settings - each group the view names replaces the scene's, the others stay") {
        post_process_settings scenePostProcess = {};
        scenePostProcess.tonemap = tonemap_operator::REINHARD_EXTENDED;
        scenePostProcess.bloomIntensity = 0.2f;
        ssao_settings sceneSsao = {};
        sceneSsao.radius = 3.0f;

        render_view view = {};
        view.postProcess.tonemap = tonemap_operator::NONE;
        view.postProcess.exposure = exposure_mode::MANUAL;
        view.ssao.enabled = false;

        const std::array<view_settings_override_flags, 4> masks = {
          view_settings_override_flags::NONE,
          view_settings_override_flags::POST_PROCESS,
          view_settings_override_flags::SSAO,
          view_settings_override_flags::POST_PROCESS | view_settings_override_flags::SSAO,
        };
        for (const view_settings_override_flags mask : masks) {
            CAPTURE(static_cast<u32>(mask));
            view.settingsOverride = mask;
            const view_render_settings settings = effective_view_settings(scenePostProcess, sceneSsao, view);
            const bool ownPostProcess = has_flag(mask, view_settings_override_flags::POST_PROCESS);
            const bool ownSsao = has_flag(mask, view_settings_override_flags::SSAO);
            CHECK(settings.postProcess == (ownPostProcess ? view.postProcess : scenePostProcess));
            CHECK(settings.ssao == (ownSsao ? view.ssao : sceneSsao));
        }
    }

    TEST_CASE("effective_view_settings - the configured values come back unsanitised and without the view flags") {
        // Turning post-processing off and the debug view are applied by the renderer, not here, and nothing
        // is clamped: the result is what was configured.
        render_view view = {};
        view.flags = view_flags::NO_POST_PROCESS;
        view.debugMode = view_debug_mode::AMBIENT_OCCLUSION;
        view.settingsOverride = view_settings_override_flags::POST_PROCESS;
        view.postProcess.bloomIntensity = 50.0f;
        const view_render_settings settings = effective_view_settings(post_process_settings(), ssao_settings(), view);
        CHECK(settings.postProcess.bloomIntensity == 50.0f);
        CHECK(settings.ssao == ssao_settings());
        view_render_settings other = settings;
        CHECK(other == settings);
        other.ssao.radius = 2.0f;
        CHECK_FALSE(other == settings);
    }

    TEST_CASE("render_scene - allocate_meshes resizes once and hands out the whole span") {
        render_scene scene = {};
        const std::span<mesh_instance> span = scene.allocate_meshes(5);
        CHECK(span.size() == 5);
        CHECK(scene.meshes.size() == 5);
        CHECK(span.data() == scene.meshes.data());
        for (const mesh_instance &instance : span) {
            CHECK(instance.layerMask == 1);
        }
        span[2].entityId = 42;
        CHECK(scene.meshes[2].entityId == 42);
        // Reallocating replaces the array; the elements are default initialised again.
        const std::span<mesh_instance> again = scene.allocate_meshes(3);
        CHECK(again.size() == 3);
        CHECK(scene.meshes[2].entityId == 0);
        CHECK(scene.allocate_meshes(0).empty());
    }

    TEST_CASE("render_scene - clear empties every array, resets the settings and keeps capacity") {
        render_scene scene = {};
        scene.frameIndex = 10;
        scene.deltaSeconds = 0.016f;
        scene.views.resize(2);
        static_cast<void>(scene.allocate_meshes(100));
        scene.lights.resize(3);
        scene.sprites.resize(4);
        scene.environment.intensity = 2.0f;
        scene.postProcess.tonemap = tonemap_operator::AGX;
        scene.shadows.enabled = false;
        scene.ssao.radius = 5.0f;
        scene.debugDraw.line(math::vec3(0.0f), math::vec3(1.0f), math::vec4(1.0f));
        const usize meshCapacity = scene.meshes.capacity();

        scene.clear();
        CHECK(scene.frameIndex == 0);
        CHECK(scene.deltaSeconds == 0.0f);
        CHECK(scene.views.empty());
        CHECK(scene.meshes.empty());
        CHECK(scene.lights.empty());
        CHECK(scene.sprites.empty());
        CHECK(scene.environment == environment_settings());
        CHECK(scene.postProcess == post_process_settings());
        CHECK(scene.shadows == shadow_settings());
        CHECK(scene.ssao == ssao_settings());
        CHECK(scene.debugDraw.empty());
        CHECK(scene.meshes.capacity() == meshCapacity);
    }

    TEST_CASE("render_scene_pair - begin_extract clears the write slot and end_extract swaps") {
        render_scene_pair pair = {};
        CHECK_FALSE(pair.is_extracting());
        CHECK(pair.write_index() == 0);
        const render_scene *firstRead = &pair.read();

        render_scene &write = pair.begin_extract();
        CHECK(pair.is_extracting());
        CHECK(&write == &pair.write());
        CHECK(&write != firstRead);
        write.frameIndex = 1;
        write.lights.resize(2);
        pair.end_extract();
        CHECK_FALSE(pair.is_extracting());
        CHECK(pair.write_index() == 1);
        CHECK(&pair.read() == &write);
        CHECK(pair.read().frameIndex == 1);
        CHECK(pair.read().lights.size() == 2);

        // The next extraction writes the other slot and clears it; the read slot is untouched.
        render_scene &second = pair.begin_extract();
        CHECK(&second == firstRead);
        CHECK(second.frameIndex == 0);
        CHECK(second.lights.empty());
        CHECK(pair.read().frameIndex == 1);
        second.frameIndex = 2;
        pair.end_extract();
        CHECK(pair.write_index() == 0);
        CHECK(pair.read().frameIndex == 2);

        // The slot written in frame 1 comes back cleared in frame 3.
        render_scene &third = pair.begin_extract();
        CHECK(&third == &write);
        CHECK(third.frameIndex == 0);
        CHECK(third.lights.empty());
        pair.end_extract();
    }

    TEST_CASE("render_scene_pair - mismatched begin and end calls assert") {
        render_scene_pair pair = {};
        CONJURE_CHECK_ASSERTS(pair.end_extract());
        CHECK_FALSE(pair.is_extracting());
        CHECK(pair.write_index() == 0);
        static_cast<void>(pair.begin_extract());
        CONJURE_CHECK_ASSERTS(static_cast<void>(pair.begin_extract()));
        CHECK(pair.is_extracting());
        pair.end_extract();
        CHECK_FALSE(pair.is_extracting());
        CHECK(pair.write_index() == 1);
    }
}
