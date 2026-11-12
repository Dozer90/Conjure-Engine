#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/math.h"
#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/type_info.h"
#include "conjure/editor/ui/imgui_frame_scope.h"
#include "conjure/editor/ui/property_drawer.h"

#include <imgui.h>

#include <cmath>
#include <functional>
#include <string>

// property_drawer.h: one widget per field kind, drawn without a renderer backend.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;

namespace {
    /// <summary>
    /// One frame of a window pinned at the origin (400x300) whose contents 'draw' submits.
    /// </summary>
    void draw_window(imgui_frame_scope &scope, const std::function<void()> &draw) {
        scope.begin_frame();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(400.0f, 300.0f), ImGuiCond_Always);
        ImGui::Begin("drawers");
        draw();
        ImGui::End();
        scope.end_frame();
    }

    /// <summary>
    /// Types text into a widget and presses Enter, the way a user does: 'focus' (called inside the window right
    /// before the widgets) moves the keyboard focus, which turns a drag into a text field on the next frame; the
    /// characters arrive on the frame after and Enter commits.
    /// </summary>
    void type_into(
      imgui_frame_scope &scope,
      const std::function<void()> &focus,
      const std::function<void()> &draw,
      const char *text
    ) {
        draw_window(scope, [&]() {
            focus();
            draw();
        });
        draw_window(scope, draw);
        ImGui::GetIO().AddInputCharactersUTF8(text);
        draw_window(scope, draw);
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, true);
        draw_window(scope, draw);
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, false);
        draw_window(scope, draw);
    }

    [[nodiscard]] core::json::document scalar(u64 value) {
        return core::json::document::from_value(core::json::object({{"v", value}}).root()["v"]);
    }

    /// <summary>
    /// The JSON form of a flags enum with no bit set.
    /// </summary>
    [[nodiscard]] core::json::document no_flags() {
        return core::json::document::from_value(core::json::object({{"v", ""}}).root()["v"]);
    }

    [[nodiscard]] core::json::document transform_json(f32 x) {
        return core::json::object({
          {"position", core::json::array({x, 0.0f, 0.0f})},
          {"rotation", core::json::array({0.0f, 0.0f, 0.0f, 1.0f})},
          {"scale", core::json::array({1.0f, 1.0f, 1.0f})},
        });
    }
}

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

    TEST_CASE("property_drawer_registry - a rotation typed past 90 degrees keeps its Euler angles for the next edit") {
        property_drawer_registry registry = {};
        imgui_frame_scope scope{};
        property_draw_context context = {};
        context.label = "rotation";
        core::json::document rotation = core::json::array({0.0f, 0.0f, 0.0f, 1.0f});
        const std::function<void()> draw = [&]() {
            (void) registry.draw(core::reflect::field_kind::QUAT, context, rotation);
        };
        // Yaw to 100 degrees, then set pitch to 0: the second edit must start from (0, 100, 0), not from the
        // decomposition (180, 80, 180) of the stored quaternion.
        type_into(scope, []() { ImGui::SetKeyboardFocusHere(1); }, draw, "100");
        type_into(scope, []() { ImGui::SetKeyboardFocusHere(0); }, draw, "0");
        const math::quat result = math::quat(
          rotation.root()[3].as_f32(),
          rotation.root()[0].as_f32(),
          rotation.root()[1].as_f32(),
          rotation.root()[2].as_f32()
        );
        const math::quat expected = math::quat(glm::radians(math::vec3(0.0f, 100.0f, 0.0f)));
        CHECK(std::abs(glm::dot(result, expected)) > 0.9999f);
    }

    TEST_CASE("property_drawer_registry - typing into an unranged u64 field writes the typed value") {
        property_drawer_registry registry = {};
        imgui_frame_scope scope{};
        property_draw_context context = {};
        context.label = "big";
        core::json::document value = scalar(5);
        type_into(
          scope,
          []() { ImGui::SetKeyboardFocusHere(); },
          [&]() { (void) registry.draw(core::reflect::field_kind::U64, context, value); },
          "12345678901234567890"
        );
        CHECK(value.root().as_u64() == 12345678901234567890ull);
        // A narrower kind clamps a typed value to its own range.
        core::json::document small = scalar(5);
        type_into(
          scope,
          []() { ImGui::SetKeyboardFocusHere(); },
          [&]() { (void) registry.draw(core::reflect::field_kind::I8, context, small); },
          "300"
        );
        CHECK(small.root().as_i64() == 127);
    }

    TEST_CASE("property_drawer_registry - a field with only a minimum draws a clamped drag") {
        core::reflect::field_info count = {};
        count.name = "count";
        count.kind = core::reflect::field_kind::I32;
        count.attributes.min = 0.0;
        core::reflect::field_info speed = {};
        speed.name = "speed";
        speed.kind = core::reflect::field_kind::F32;
        speed.attributes.min = 0.0;
        core::reflect::type_info type = {};
        type.name = "test.bounded";
        type.fields = {count, speed};
        property_drawer_registry registry = {};
        imgui_frame_scope scope{};
        core::json::document object = core::json::object({{"count", 5}, {"speed", 1.0f}});
        const std::function<void()> draw = [&]() {
            (void) registry.draw_object(type, object, false);
        };
        type_into(scope, []() { ImGui::SetKeyboardFocusHere(0); }, draw, "-5");
        CHECK(object.root()["count"].as_i64() == 0);
        type_into(scope, []() { ImGui::SetKeyboardFocusHere(1); }, draw, "-5");
        CHECK(object.root()["speed"].as_f64() == doctest::Approx(0.0));
        // Typing above the open end is kept.
        type_into(scope, []() { ImGui::SetKeyboardFocusHere(0); }, draw, "123456");
        CHECK(object.root()["count"].as_i64() == 123456);
    }

    TEST_CASE("property_drawer_registry - an f32 field bounded beyond the slider range draws a clamped drag") {
        // ImGui's float slider asserts on a bound beyond half the float range, so this field must be a drag.
        core::reflect::field_info wide = {};
        wide.name = "wide";
        wide.kind = core::reflect::field_kind::F32;
        wide.attributes.min = 0.0;
        wide.attributes.max = 3.0e38;
        core::reflect::type_info type = {};
        type.name = "test.wide";
        type.fields = {wide};
        property_drawer_registry registry = {};
        imgui_frame_scope scope{};
        core::json::document object = core::json::object({{"wide", 1.0f}});
        const std::function<void()> draw = [&]() {
            (void) registry.draw_object(type, object, false);
        };
        type_into(scope, []() { ImGui::SetKeyboardFocusHere(0); }, draw, "5");
        CHECK(object.root()["wide"].as_f64() == doctest::Approx(5.0));
        type_into(scope, []() { ImGui::SetKeyboardFocusHere(0); }, draw, "-5");
        CHECK(object.root()["wide"].as_f64() == doctest::Approx(0.0));
    }

    TEST_CASE("property_drawer_registry - editing one of two transform fields leaves the other unchanged") {
        property_drawer_registry registry = {};
        imgui_frame_scope scope{};
        property_draw_context start = {};
        start.label = "start";
        property_draw_context end = {};
        end.label = "end";
        core::json::document startValue = transform_json(1.0f);
        core::json::document endValue = transform_json(1.0f);
        type_into(
          scope,
          []() { ImGui::SetKeyboardFocusHere(); },
          [&]() {
              (void) registry.draw(core::reflect::field_kind::TRANSFORM, start, startValue);
              (void) registry.draw(core::reflect::field_kind::TRANSFORM, end, endValue);
          },
          "7"
        );
        CHECK(startValue.root()["position"][0].as_f32() == doctest::Approx(7.0f));
        CHECK(endValue.root()["position"][0].as_f32() == doctest::Approx(1.0f));
    }

    TEST_CASE("property_drawer_registry - two flag fields of one enum toggle independently") {
        core::reflect::enum_info flags = {};
        flags.name = "test.flags";
        flags.isFlags = true;
        flags.values = {{"NONE", 0}, {"A", 1}, {"B", 2}};
        core::reflect::field_info firstField = {};
        firstField.name = "first";
        firstField.kind = core::reflect::field_kind::ENUM;
        firstField.enumType = &flags;
        core::reflect::field_info secondField = firstField;
        secondField.name = "second";
        property_draw_context first = {};
        first.field = &firstField;
        first.label = "first";
        property_draw_context second = {};
        second.field = &secondField;
        second.label = "second";
        core::json::document firstValue = no_flags();
        core::json::document secondValue = no_flags();
        property_drawer_registry registry = {};
        imgui_frame_scope scope{};
        // The last item "second" submits is its B checkbox.
        ImVec2 secondB = ImVec2(0.0f, 0.0f);
        const std::function<void()> draw = [&]() {
            (void) registry.draw(core::reflect::field_kind::ENUM, first, firstValue);
            (void) registry.draw(core::reflect::field_kind::ENUM, second, secondValue);
            secondB = ImVec2(
              (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
              (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f
            );
        };
        draw_window(scope, draw);
        const math::vec2 target = math::vec2(secondB.x, secondB.y);
        scope.set_mouse(target, false);
        draw_window(scope, draw);
        scope.set_mouse(target, true);
        draw_window(scope, draw);
        scope.set_mouse(target, false);
        draw_window(scope, draw);
        CHECK(flags.parse_value(secondValue.root().as_string()).value_or(-1) == 2);
        CHECK(flags.parse_value(firstValue.root().as_string()).value_or(-1) == 0);
    }
}
