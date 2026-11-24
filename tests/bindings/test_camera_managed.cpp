#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "bindings_test_support.h"

#include "conjure/core/math.h"
#include "conjure/core/string_id.h"
#include "conjure/ecs/entity.h"
#include "conjure/engine/application.h"
#include "conjure/engine/game_screen.h"
#include "conjure/platform/null_backend.h"
#include "conjure/platform/window.h"
#include "conjure/renderer/components.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"
#include "conjure/scripting/script_runtime.h"
#include "engine/subsystems/renderer_components.h"

#include <cmath>
#include <format>
#include <string>
#include <string_view>

// The C# Camera, Screen and picking facades (wave 3 GAP-CSHARP 2.2, 8 L3) over the real CLR inside a headless
// application with the 1280 x 720 null window: Grid-Sweeper-style ScreenToWorld2D, the screen and the UI scale of
// the null window and monitor, the mouse in pixels, a camera turned and asked for a ray in the same OnUpdate, the
// zero screen of a minimised window, and Entity.IsActiveInHierarchy. The probes are in
// Conjure.TestScripts/CameraProbes.cs.

using namespace conjure;
using namespace conjure::testing;
using namespace bindings_test;

namespace {

    constexpr std::string_view PICKING_PROBE = "Conjure.TestScripts.CameraPickingProbe";
    constexpr std::string_view TURN_PROBE = "Conjure.TestScripts.CameraTurnProbe";
    constexpr std::string_view SCREEN_PROBE = "Conjure.TestScripts.ScreenProbe";
    constexpr std::string_view HIERARCHY_ACTIVITY_PROBE = "Conjure.TestScripts.HierarchyActivityProbe";

    /// <summary>
    /// The null window of a headless application.
    /// </summary>
    [[nodiscard]] platform::null_backend &null_window(managed_app &fixture) {
        platform::window *window = fixture.app().context().window;
        REQUIRE(window != nullptr);
        platform::null_backend *backend = window->as_null();
        REQUIRE(backend != nullptr);
        return *backend;
    }

    /// <summary>
    /// A value in thousandths, rounded, as the probes report it.
    /// </summary>
    [[nodiscard]] i64 thousandths(f32 value) noexcept {
        return static_cast<i64>(std::lround(value * 1000.0f));
    }
}

TEST_SUITE("bindings/camera_managed") {

    TEST_CASE(
      "Camera - ScreenToWorld2D picks the grid cell under a pixel and every refusal maps to its exception [dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_app fixture = {};
        const scripting::script_entry_id probe = fixture.attach(fixture.create("grid_camera"), PICKING_PROBE);
        CHECK(fixture.field_string(probe, "Errors").empty());
        // Before any entity carries a camera, Main is the null entity: no camera, and picking through it is a dead
        // entity's failure.
        CHECK(fixture.field_i64(probe, "NoMainExists") == 0);
        CHECK(fixture.field_string(probe, "NoMainRay") == "EntityNotAliveException");
        CHECK(fixture.field_i64(probe, "IsMain") == 1);
        CHECK(fixture.field_i64(probe, "Exists") == 1);
        CHECK(fixture.field_i64(probe, "PlainExists") == 0);
        // Pixel (964, 180) of the 1280 x 720 screen through a 10 m high orthographic camera is (4.5, 2.5): the
        // centre of cell (4, 2), and back to the same pixel at 10 m.
        CHECK(fixture.field_string(probe, "Cell") == "4500,2500");
        CHECK(fixture.field_string(probe, "CellIndex") == "4,2");
        CHECK(fixture.field_string(probe, "BackToScreen") == "964000,180000,10000");
        CHECK(fixture.field_i64(probe, "BackVisible") == 1);
        CHECK(fixture.field_string(probe, "CentreRay") == "0,0,9900 0,0,-1000");
        CHECK(fixture.field_i64(probe, "CentreRayDone") == 1);
        CHECK(fixture.field_i64(probe, "ParallelPlane") == 0);
        CHECK(fixture.field_i64(probe, "PlaneBehind") == 0);
        CHECK(fixture.field_string(probe, "BehindTheCamera2D") == "InvalidOperationException");

        // The refusals of the same run: the message tells the missing camera from the missing transform.
        CHECK(fixture.field_string(probe, "DeadCamera") == "EntityNotAliveException");
        CHECK(fixture.field_string(probe, "NoCameraComponent") ==
          "InvalidOperationException: camera_world_to_screen: camera");
        CHECK(fixture.field_string(probe, "NoTransform") ==
          "InvalidOperationException: camera_screen_to_world: transform");
        CHECK(fixture.field_string(probe, "NonFinite") == "ArgumentException");
        CHECK(fixture.field_string(probe, "BadCameraData") == "ArgumentException");
        CHECK(fixture.field_string(probe, "ZeroDirectionRay") == "ArgumentException");
        CHECK(fixture.field_string(probe, "NonFiniteRayOrigin") == "ArgumentException");
        CHECK(fixture.field_string(probe, "NonFiniteRayDirection") == "ArgumentException");
        CHECK(fixture.field_string(probe, "ExtremeRayDirections") == "1000,0,0 707,707,0");
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "Camera - a camera turned in OnUpdate gives the turned ray in the same OnUpdate [dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_app fixture = {};
        const scripting::script_entry_id probe = fixture.attach(fixture.create("turning_camera"), TURN_PROBE);
        CHECK(fixture.field_string(probe, "Errors").empty());
        // 90 degrees about +y: the forward axis -z becomes -x before any transform system or extraction ran.
        CHECK(fixture.field_string(probe, "Forward") == "-1000,0,0");
        CHECK(fixture.field_string(probe, "AheadPixel") == "640000,360000,5000");
        CHECK(fixture.field_i64(probe, "AheadVisible") == 1);
        CHECK(fixture.field_i64(probe, "BehindVisible") == 0);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "Screen - Size, UiScale and the mouse in pixels follow the null window and monitor [dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_app fixture = {};
        f32 scale = 1.0f;
        SUBCASE("content scale 1") {
            scale = 1.0f;
        }
        SUBCASE("content scale 1.5 (144 DPI)") {
            scale = 1.5f;
        }
        CAPTURE(scale);
        // The null backend changes the window's and its monitor's scale together.
        null_window(fixture).simulate_content_scale(math::vec2(scale));
        const scripting::script_entry_id probe = fixture.attach(fixture.create("screen"), SCREEN_PROBE, 3);
        CHECK(fixture.field_string(probe, "Errors").empty());

        const engine::game_screen screen = engine::game_screen_of(fixture.app().context().window);
        const i64 width = static_cast<i64>(screen.pixels.x);
        const i64 height = static_cast<i64>(screen.pixels.y);
        CHECK(width == std::lround(1280.0f * scale));
        CHECK(height == std::lround(720.0f * scale));
        CHECK(fixture.field_i64(probe, "Width") == width);
        CHECK(fixture.field_i64(probe, "Height") == height);
        const f32 aspectRatio = static_cast<f32>(width) / static_cast<f32>(height);
        const std::string size = std::to_string(width * 1000) + "," + std::to_string(height * 1000);
        CHECK(fixture.field_string(probe, "Size") == size);
        CHECK(fixture.field_i64(probe, "AspectRatio") == thousandths(aspectRatio));
        CHECK(fixture.field_string(probe, "ContentScale") ==
          std::to_string(thousandths(scale)) + "," + std::to_string(thousandths(scale)));
        CHECK(fixture.field_i64(probe, "UiScale") == thousandths(scale));
        CHECK(fixture.field_i64(probe, "UiScale") == thousandths(screen.uiScale));
        CHECK(fixture.field_i64(probe, "Focused") == (screen.focused ? 1 : 0));
        CHECK(fixture.field_i64(probe, "Minimised") == 0);
        CHECK(fixture.field_i64(probe, "Fullscreen") == 0);
        CHECK(fixture.field_string(probe, "LogicalToPixels") ==
          std::to_string(thousandths(10.0f * scale)) + "," + std::to_string(thousandths(20.0f * scale)));
        // Injected at (100, 50) logical in the first update: MouseScreenPosition is logical x content scale.
        CHECK(fixture.field_string(probe, "Mouse") == "100000,50000");
        CHECK(fixture.field_string(probe, "MouseScreen") ==
          std::to_string(thousandths(100.0f * scale)) + "," + std::to_string(thousandths(50.0f * scale)));
        CHECK(fixture.field_i64(probe, "RayDone") == -1);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "Screen - a minimised window is a zero screen and picking reports it [dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_app fixture = {};
        const ecs::entity camera = fixture.create("screen_camera");
        CONJURE_REQUIRE_OK(fixture.app().context().world->add<renderer::camera_component>(camera));
        const scripting::script_entry_id probe = fixture.attach(camera, SCREEN_PROBE);
        CHECK(fixture.field_string(probe, "Errors").empty());
        CHECK(fixture.field_i64(probe, "Width") == 1280);
        CHECK(fixture.field_i64(probe, "Minimised") == 0);
        CHECK(fixture.field_i64(probe, "RayDone") == 1);
        CHECK(fixture.field_string(probe, "RayToWorld") == "none");
        CHECK(fixture.field_i64(probe, "ProjectDone") == 1);
        CHECK(fixture.field_string(probe, "Projected") == "640000,360000,5000");

        null_window(fixture).simulate_minimise(true);
        fixture.step(1);
        CHECK(fixture.field_string(probe, "Errors").empty());
        CHECK(fixture.field_i64(probe, "Width") == 0);
        CHECK(fixture.field_i64(probe, "Height") == 0);
        CHECK(fixture.field_i64(probe, "AspectRatio") == 0);
        CHECK(fixture.field_i64(probe, "Minimised") == 1);
        CHECK(fixture.field_i64(probe, "RayDone") == 0);
        CHECK(fixture.field_string(probe, "RayToWorld") == "InvalidOperationException");
        CHECK(fixture.field_i64(probe, "ProjectDone") == 0);
        CHECK(fixture.field_string(probe, "Projected") == "0,0,5000");
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "Entity - IsActiveInHierarchy follows the entity's own and its ancestors' state [dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_app fixture = {};
        scene::scene &active = fixture.app().context().scenes->active();
        const ecs::entity parent = fixture.create("activity_parent");
        const core::result<ecs::entity> child = active.create_entity(core::string_id("activity_child"), parent);
        CONJURE_REQUIRE_OK(child);
        const std::string entities =
          std::format(R"({{"ParentEntity":{},"ChildEntity":{}}})", parent.to_u64(), child.value().to_u64());
        const scripting::script_entry_id probe =
          fixture.attach(fixture.create("observer"), HIERARCHY_ACTIVITY_PROBE, 2, entities);
        CHECK(fixture.field_string(probe, "Errors").empty());
        CHECK(fixture.field_i64(probe, "Parent") == 1);
        CHECK(fixture.field_i64(probe, "Child") == 1);
        CHECK(fixture.field_i64(probe, "Self") == 1);
        CHECK(fixture.field_i64(probe, "Dead") == 0);

        SUBCASE("a disabled parent makes its child inactive") {
            CONJURE_REQUIRE_OK(active.set_enabled(parent, false));
            fixture.step(1);
            CHECK(fixture.field_i64(probe, "Parent") == 0);
            CHECK(fixture.field_i64(probe, "Child") == 0);
        }
        SUBCASE("a disabled child leaves its parent active") {
            CONJURE_REQUIRE_OK(active.set_enabled(child.value(), false));
            fixture.step(1);
            CHECK(fixture.field_i64(probe, "Parent") == 1);
            CHECK(fixture.field_i64(probe, "Child") == 0);
        }
        CHECK(fixture.field_i64(probe, "Self") == 1);
        CHECK(fixture.field_string(probe, "Errors").empty());
        CHECK(logs.count(core::log_level::ERR) == 0);
    }
}
