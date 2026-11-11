#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/type_info.h"
#include "conjure/editor/ui/imgui_frame_scope.h"
#include "conjure/editor/ui/property_drawer.h"

#include <imgui.h>

#include <string>

// property_drawer.h: one widget per field kind, drawn without a renderer backend.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;

TEST_SUITE("editor/ui/property_drawer") {

    TEST_CASE("property_drawer_registry - installs a drawer for every kind and a fallback") {
        property_drawer_registry registry = {};
        for (u32 index = 0; index < static_cast<u32>(core::reflect::field_kind::COUNT); ++index) {
            const core::reflect::field_kind kind = static_cast<core::reflect::field_kind>(index);
            if (kind == core::reflect::field_kind::STRUCT || kind == core::reflect::field_kind::ARRAY) {
                continue;
            }
            CHECK_MESSAGE(static_cast<bool>(registry.drawer(kind)), core::reflect::to_string(kind));
        }
        CHECK(static_cast<bool>(registry.drawer(core::reflect::field_kind::COUNT)));
        int calls = 0;
        registry.set(core::reflect::field_kind::BOOL, [&calls](const property_draw_context &, core::json::document &) {
            ++calls;
            return true;
        });
        property_draw_context context = {};
        context.label = "x";
        core::json::document value = core::json::object({{"v", true}});
        CHECK(registry.draw(core::reflect::field_kind::BOOL, context, value));
        CHECK(calls == 1);
        registry.set(core::reflect::field_kind::COUNT, {});
    }

    TEST_CASE("property_drawer_registry - draw_object renders every component type without edits") {
        editor_test::headless_editor editor = {};
        property_drawer_registry registry = {};
        imgui_frame_scope scope{};
        for (const ecs::component_info &info : editor.context().components().types()) {
            if (info.reflection == nullptr) {
                continue;
            }
            core::json::document object = core::json::document::object();
            scope.begin_frame();
            ImGui::Begin("inspector");
            const bool changed = registry.draw_object(*info.reflection, object, false);
            ImGui::End();
            scope.end_frame();
            CHECK_MESSAGE(!changed, info.name.view());
        }
    }

    TEST_CASE("property_drawer_registry - draw_json handles scalars, vectors, arrays and objects") {
        property_drawer_registry registry = {};
        imgui_frame_scope scope{};
        core::json::document number = core::json::document::from_value(core::json::object({{"v", 1.5}}).root()["v"]);
        core::json::document text = core::json::document::from_value(core::json::object({{"v", "words"}}).root()["v"]);
        core::json::document flag = core::json::document::from_value(core::json::object({{"v", true}}).root()["v"]);
        core::json::document vector = core::json::array({1.0f, 2.0f, 3.0f});
        core::json::document list = core::json::array({"a", "b"});
        core::json::document object = core::json::object({{"inner", core::json::object({{"x", 1}})}, {"n", nullptr}});
        scope.begin_frame();
        ImGui::Begin("json");
        CHECK_FALSE(registry.draw_json("number", number, false));
        CHECK_FALSE(registry.draw_json("text", text, true));
        CHECK_FALSE(registry.draw_json("flag", flag, false));
        CHECK_FALSE(registry.draw_json("vector", vector, false));
        CHECK_FALSE(registry.draw_json("list", list, false));
        CHECK_FALSE(registry.draw_json("object", object, false));
        ImGui::End();
        scope.end_frame();
        CHECK(number.root().as_f64() == doctest::Approx(1.5));
        CHECK(object.root()["inner"]["x"].as_i64() == 1);
    }

    TEST_CASE("property_drawer_registry - the transform and enum drawers accept the component JSON forms") {
        editor_test::headless_editor editor = {};
        property_drawer_registry registry = {};
        const ecs::component_id cameraId = editor.context().components().find(
          core::string_id("conjure.renderer.camera")
        );
        REQUIRE(cameraId.is_valid());
        const core::reflect::type_info *camera = editor.context().components().info(cameraId).reflection;
        REQUIRE(camera != nullptr);
        core::json::document value = core::json::object({
          {"cameraData", core::json::object({{"projection", "ORTHOGRAPHIC"}, {"verticalFovRadians", 1.0f}})},
          {"priority", 3},
          {"isMain", true},
        });
        imgui_frame_scope scope{};
        scope.begin_frame();
        ImGui::Begin("camera");
        CHECK_FALSE(registry.draw_object(*camera, value, false));
        ImGui::End();
        scope.end_frame();
        CHECK(value.root()["cameraData"]["projection"].as_string() == "ORTHOGRAPHIC");
        property_draw_context context = {};
        context.label = "transform";
        core::json::document transform = core::json::object({
          {"position", core::json::array({1.0f, 2.0f, 3.0f})},
          {"rotation", core::json::array({0.0f, 0.0f, 0.0f, 1.0f})},
          {"scale", core::json::array({1.0f, 1.0f, 1.0f})},
        });
        scope.begin_frame();
        ImGui::Begin("transform");
        CHECK_FALSE(registry.draw(core::reflect::field_kind::TRANSFORM, context, transform));
        core::json::document quaternion = core::json::array({0.0f, 0.0f, 0.0f, 1.0f});
        CHECK_FALSE(registry.draw(core::reflect::field_kind::QUAT, context, quaternion));
        core::json::document matrix = core::json::document::array();
        for (int index = 0; index < 16; ++index) {
            matrix.push_back(index % 5 == 0 ? 1.0f : 0.0f);
        }
        CHECK_FALSE(registry.draw(core::reflect::field_kind::MAT4, context, matrix));
        ImGui::End();
        scope.end_frame();
        CHECK(transform.root()["position"][2].as_f32() == doctest::Approx(3.0f));
    }
}
