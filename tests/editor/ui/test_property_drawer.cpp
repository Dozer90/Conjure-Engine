#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/math.h"
#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/type_info.h"
#include "conjure/editor/ui/imgui_frame_scope.h"
#include "conjure/editor/ui/property_drawer.h"
#include "conjure/renderer/camera.h"
#include "conjure/renderer/components.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>
#include <functional>
#include <limits>
#include <numbers>
#include <string>
#include <string_view>

// property_drawer.h: one widget per field kind, drawn without a renderer backend.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;
using conjure::core::reflect::field_kind;
using conjure::core::reflect::field_unit;

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

    /// <summary>
    /// The text the widgets of one frame of the window show, through ImGui's log capture: a value arrives as
    /// "{ text }" (a drag showing an infinity is "{ inf }"), a label as it is.
    /// </summary>
    [[nodiscard]] std::string shown_text(imgui_frame_scope &scope, const std::function<void()> &draw) {
        std::string text = {};
        // The capture runs on a frame after the window's first one, when its layout is settled.
        draw_window(scope, draw);
        draw_window(scope, [&]() {
            ImGui::LogToBuffer();
            draw();
            text = ImGui::GetCurrentContext()->LogBuffer.c_str();
            ImGui::LogFinish();
        });
        return text;
    }

    /// <summary>
    /// Drags the widget that 'draw' submits with the left mouse button: presses at the centre of its rectangle
    /// (which 'draw' stores in 'centre' after drawing it), moves 'pixels' to the right in two steps, each past
    /// ImGui's drag threshold, and releases.
    /// </summary>
    void drag_by(imgui_frame_scope &scope, const std::function<void()> &draw, const ImVec2 &centre, f32 pixels) {
        draw_window(scope, draw);
        draw_window(scope, draw);
        ImGuiIO &io = ImGui::GetIO();
        io.AddMousePosEvent(centre.x, centre.y);
        draw_window(scope, draw);
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        draw_window(scope, draw);
        io.AddMousePosEvent(centre.x + pixels * 0.5f, centre.y);
        draw_window(scope, draw);
        io.AddMousePosEvent(centre.x + pixels, centre.y);
        draw_window(scope, draw);
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        draw_window(scope, draw);
    }

    /// <summary>
    /// The centre of the last item ImGui submitted.
    /// </summary>
    [[nodiscard]] ImVec2 last_item_centre() {
        const ImVec2 low = ImGui::GetItemRectMin();
        const ImVec2 high = ImGui::GetItemRectMax();
        return ImVec2((low.x + high.x) * 0.5f, (low.y + high.y) * 0.5f);
    }

    [[nodiscard]] core::json::document float_value(f64 value) {
        return core::json::document::from_value(core::json::object({{"v", value}}).root()["v"]);
    }

    [[nodiscard]] usize count_of(std::string_view text, std::string_view part) {
        usize found = 0;
        for (usize at = text.find(part); at != std::string_view::npos; at = text.find(part, at + part.size())) {
            ++found;
        }
        return found;
    }

    [[nodiscard]] core::reflect::field_info float_field(const char *name, core::reflect::field_kind kind) {
        core::reflect::field_info field = {};
        field.name = name;
        field.kind = kind;
        return field;
    }

    [[nodiscard]] core::reflect::field_info angle_field(
      const char *name,
      core::reflect::field_kind kind,
      core::reflect::field_unit unit
    ) {
        core::reflect::field_info field = float_field(name, kind);
        field.attributes.unit = unit;
        return field;
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

    TEST_CASE("regression - code sweep F213 property_drawer_registry cached Euler angles follow a rotation changed "
              "from outside by a hundredth of a degree") {
        property_drawer_registry registry = {};
        imgui_frame_scope scope{};
        property_draw_context context = {};
        context.label = "rotation";
        core::json::document rotation = core::json::array({0.0f, 0.0f, 0.0f, 1.0f});
        const std::function<void()> draw = [&]() {
            (void) registry.draw(core::reflect::field_kind::QUAT, context, rotation);
        };
        // Yaw 100 degrees is cached as typed; its decomposition would show (180, 80, 180).
        type_into(scope, []() { ImGui::SetKeyboardFocusHere(1); }, draw, "100");
        REQUIRE(count_of(shown_text(scope, draw), "100.0 deg") == 1);
        const math::quat typed = math::quat(
          rotation.root()[3].as_f32(),
          rotation.root()[0].as_f32(),
          rotation.root()[1].as_f32(),
          rotation.root()[2].as_f32()
        );
        const auto store = [&rotation](const math::quat &value) {
            rotation = core::json::array({value.x, value.y, value.z, value.w});
        };
        SUBCASE("a change of 0.01 degrees is a new rotation: the angles are decomposed again") {
            // Inside the cosine tolerance the check used before (0.16 degrees), which kept the stale angles.
            store(typed * glm::angleAxis(glm::radians(0.01f), math::vec3(1.0f, 0.0f, 0.0f)));
            CHECK(count_of(shown_text(scope, draw), "100.0 deg") == 0);
        }
        SUBCASE("float rounding of the stored value keeps the angles as typed") {
            store(math::quat(typed.w, typed.x + 2.0e-6f, typed.y - 2.0e-6f, typed.z));
            CHECK(count_of(shown_text(scope, draw), "100.0 deg") == 1);
        }
        SUBCASE("the same rotation as -q keeps the angles as typed") {
            store(-typed);
            CHECK(count_of(shown_text(scope, draw), "100.0 deg") == 1);
        }
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

    TEST_CASE("regression - code sweep F250 property_drawer_registry shows an infinite or NaN float as inf, -inf or "
      "nan") {
        // The reflection JSON of a float: +infinity is "inf", -infinity "-inf" and NaN null (10_core.md C15).
        core::reflect::field_info bounded = float_field("bounded", core::reflect::field_kind::F32);
        bounded.attributes.min = 0.0;
        bounded.attributes.max = 10.0;
        core::reflect::type_info type = {};
        type.name = "test.limits";
        type.fields = {
            float_field("limit", core::reflect::field_kind::F32),
            float_field("range", core::reflect::field_kind::F64),
            float_field("ratio", core::reflect::field_kind::F32),
            bounded,
            float_field("offset", core::reflect::field_kind::VEC3),
        };
        property_drawer_registry registry = {};
        imgui_frame_scope scope{};
        core::json::document object = core::json::object({
          {"limit", "inf"},
          {"range", "-inf"},
          {"ratio", nullptr},
          {"bounded", "inf"},
          {"offset", core::json::array({1.5f, "-inf", nullptr})},
        });
        const std::string before = object.to_string();
        bool changed = false;
        const std::string shown = shown_text(scope, [&]() {
            changed = registry.draw_object(type, object, false) || changed;
        });
        CAPTURE(shown);
        // limit, bounded (a drag instead of its slider) / range, offset.y / ratio, offset.z.
        CHECK(count_of(shown, "{ inf }") == 2);
        CHECK(count_of(shown, "{ -inf }") == 2);
        CHECK(count_of(shown, "{ nan }") == 2);
        CHECK(shown.find("{ 1.500 }") != std::string::npos);
        CHECK(shown.find("{ 0.000 }") == std::string::npos);
        // Showing a value is not an edit.
        CHECK_FALSE(changed);
        CHECK(object.to_string() == before);
    }

    TEST_CASE("regression - code sweep F250 property_drawer_registry an infinite or NaN component survives an edit "
      "of its neighbour") {
        property_drawer_registry registry = {};
        imgui_frame_scope scope{};
        property_draw_context context = {};
        context.label = "offset";
        core::json::document vector = core::json::array({1.0f, "inf", nullptr, "-inf"});
        type_into(
          scope,
          []() { ImGui::SetKeyboardFocusHere(0); },
          [&]() { (void) registry.draw(core::reflect::field_kind::VEC4, context, vector); },
          "7"
        );
        CHECK(vector.to_string() == R"([7.0,"inf",null,"-inf"])");

        // A number typed over an infinity or a NaN replaces it; the rest stays.
        type_into(
          scope,
          []() { ImGui::SetKeyboardFocusHere(1); },
          [&]() { (void) registry.draw(core::reflect::field_kind::VEC4, context, vector); },
          "2"
        );
        CHECK(vector.to_string() == R"([7.0,2.0,null,"-inf"])");
        type_into(
          scope,
          []() { ImGui::SetKeyboardFocusHere(2); },
          [&]() { (void) registry.draw(core::reflect::field_kind::VEC4, context, vector); },
          "3"
        );
        CHECK(vector.to_string() == R"([7.0,2.0,3.0,"-inf"])");

        // The same for the cells of a matrix.
        context.label = "matrix";
        core::json::document matrix = core::json::document::array();
        for (int index = 0; index < 16; ++index) {
            if (index == 5) {
                matrix.push_back("inf");
            } else if (index == 10) {
                matrix.push_back(nullptr);
            } else {
                matrix.push_back(0.0f);
            }
        }
        type_into(
          scope,
          []() { ImGui::SetKeyboardFocusHere(0); },
          [&]() { (void) registry.draw(core::reflect::field_kind::MAT4, context, matrix); },
          "4"
        );
        CHECK(matrix.root()[0].as_f32() == doctest::Approx(4.0f));
        CHECK(matrix.root()[5].as_string() == "inf");
        CHECK(matrix.root()[10].is_null());
        CHECK(matrix.root()[15].as_f32(1.0f) == doctest::Approx(0.0f));
    }

    TEST_CASE("regression - code sweep F250 property_drawer_registry a typed inf or nan is not written") {
        property_drawer_registry registry = {};
        imgui_frame_scope scope{};
        property_draw_context context = {};
        context.label = "offset";
        // No draw may report an edit either: the inspector would send a component.set that changes nothing.
        bool changed = false;
        core::json::document vector = core::json::array({1.0f, 2.0f, "inf"});
        const std::string before = vector.to_string();
        for (const char *typed : {"inf", "-inf", "nan"}) {
            CAPTURE(typed);
            type_into(
              scope,
              []() { ImGui::SetKeyboardFocusHere(0); },
              [&]() { changed = registry.draw(core::reflect::field_kind::VEC3, context, vector) || changed; },
              typed
            );
            CHECK(vector.to_string() == before);
            CHECK_FALSE(changed);
        }
        // A scalar has always refused it; an f64 input box has no clamp to turn the infinity into a number.
        context.label = "range";
        core::json::document range = core::json::document::from_value(core::json::object({{"v", 1.5}}).root()["v"]);
        type_into(
          scope,
          []() { ImGui::SetKeyboardFocusHere(0); },
          [&]() { changed = registry.draw(core::reflect::field_kind::F64, context, range) || changed; },
          "inf"
        );
        CHECK(range.root().as_f64() == doctest::Approx(1.5));
        CHECK_FALSE(changed);
        // A rotation angle typed as inf would make every component NaN.
        context.label = "rotation";
        core::json::document rotation = core::json::array({0.0f, 0.0f, 0.0f, 1.0f});
        type_into(
          scope,
          []() { ImGui::SetKeyboardFocusHere(1); },
          [&]() { changed = registry.draw(core::reflect::field_kind::QUAT, context, rotation) || changed; },
          "inf"
        );
        CHECK(rotation.to_string() == R"([0.0,0.0,0.0,1.0])");
        CHECK_FALSE(changed);
    }

    TEST_CASE("property_drawer_registry - a scalar holding an infinity takes a typed number and leaves its "
      "neighbours alone") {
        core::reflect::type_info type = {};
        type.name = "test.limits";
        type.fields = {
            float_field("limit", core::reflect::field_kind::F32),
            float_field("range", core::reflect::field_kind::F64),
        };
        property_drawer_registry registry = {};
        imgui_frame_scope scope{};
        core::json::document object = core::json::object({{"limit", "inf"}, {"range", "-inf"}});
        const std::function<void()> draw = [&]() {
            (void) registry.draw_object(type, object, false);
        };
        type_into(scope, []() { ImGui::SetKeyboardFocusHere(0); }, draw, "5");
        CHECK(object.root()["limit"].as_f64() == doctest::Approx(5.0));
        CHECK(object.root()["range"].as_string() == "-inf");
        type_into(scope, []() { ImGui::SetKeyboardFocusHere(1); }, draw, "8");
        CHECK(object.root()["range"].as_f64() == doctest::Approx(8.0));
    }

    TEST_CASE("regression - code sweep F250 property_drawer_registry a colour with an infinite channel is edited as "
      "numbers") {
        // The picker turns channels into bytes, which an infinity has no value for.
        const core::reflect::field_info tint = float_field("tint", core::reflect::field_kind::VEC4);
        property_draw_context context = {};
        context.field = &tint;
        context.label = "tint";
        property_drawer_registry registry = {};
        imgui_frame_scope scope{};
        core::json::document colour = core::json::array({0.25f, "inf", 0.0f, 1.0f});
        const std::function<void()> draw = [&]() {
            (void) registry.draw(core::reflect::field_kind::VEC4, context, colour);
        };
        const std::string shown = shown_text(scope, draw);
        CAPTURE(shown);
        CHECK(shown.find("{ inf }") != std::string::npos);
        CHECK(shown.find("{ 0.250 }") != std::string::npos);
        type_into(scope, []() { ImGui::SetKeyboardFocusHere(0); }, draw, "0.5");
        CHECK(colour.to_string() == R"([0.5,"inf",0.0,1.0])");
        // Finite again, it is a colour picker again: its fields carry the channel letters.
        type_into(scope, []() { ImGui::SetKeyboardFocusHere(1); }, draw, "0.75");
        CHECK(colour.to_string() == R"([0.5,0.75,0.0,1.0])");
        CHECK(shown_text(scope, draw).find("R:") != std::string::npos);
    }

    TEST_CASE("regression - code sweep F250 property_drawer_registry a rotation with an infinite or NaN component "
      "shows nan and a typed angle repairs it") {
        // An infinity turns into a NaN by arithmetic (inf * 0 while normalising), which carries a sign on some
        // hosts: the C library would print it as "-nan(ind)" or "-nan".
        struct broken_rotation {
            usize index = 0;
            std::string_view text = {};   // empty for a NaN (null)
        };
        for (const broken_rotation &broken : {
               broken_rotation{1, ""},
               broken_rotation{1, "inf"},
               broken_rotation{0, "-inf"},
               broken_rotation{3, "inf"},
             }) {
            CAPTURE(broken.index);
            CAPTURE(broken.text);
            property_drawer_registry registry = {};
            imgui_frame_scope scope{};
            property_draw_context context = {};
            context.label = "rotation";
            core::json::document rotation = core::json::document::array();
            for (usize index = 0; index < 4; ++index) {
                if (index != broken.index) {
                    rotation.push_back(index == 3 ? 1.0f : 0.0f);
                } else if (!broken.text.empty()) {
                    rotation.push_back(broken.text);
                } else {
                    rotation.push_back(nullptr);
                }
            }
            const std::string before = rotation.to_string();
            bool changed = false;
            const std::function<void()> draw = [&]() {
                changed = registry.draw(core::reflect::field_kind::QUAT, context, rotation) || changed;
            };
            const std::string shown = shown_text(scope, draw);
            CAPTURE(shown);
            CHECK(count_of(shown, "{ nan deg }") == 3);
            CHECK(shown.find("-nan") == std::string::npos);
            CHECK(shown.find("{ 0.0 deg }") == std::string::npos);
            CHECK_FALSE(changed);
            CHECK(rotation.to_string() == before);
            // Yaw typed: the two angles that are still not numbers count as zero.
            type_into(scope, []() { ImGui::SetKeyboardFocusHere(1); }, draw, "90");
            REQUIRE(rotation.root().size() == 4);
            for (usize index = 0; index < 4; ++index) {
                CAPTURE(index);
                REQUIRE(rotation.root()[index].is_number());
            }
            const math::quat result = math::quat(
              rotation.root()[3].as_f32(),
              rotation.root()[0].as_f32(),
              rotation.root()[1].as_f32(),
              rotation.root()[2].as_f32()
            );
            const math::quat expected = math::quat(glm::radians(math::vec3(0.0f, 90.0f, 0.0f)));
            CHECK(std::abs(glm::dot(result, expected)) > 0.9999f);
        }
    }

    TEST_CASE("regression - code sweep F250 property_drawer_registry a float member the object does not hold is "
      "drawn as zero, not as nan") {
        // Null is NaN only where the object holds it; a missing member copies as null too.
        core::reflect::type_info inner = {};
        inner.name = "test.inner";
        inner.fields = {
            float_field("ratio", core::reflect::field_kind::F32),
            float_field("span", core::reflect::field_kind::F64),
        };
        core::reflect::field_info nested = {};
        nested.name = "nested";
        nested.kind = core::reflect::field_kind::STRUCT;
        nested.structType = &inner;
        core::reflect::type_info type = {};
        type.name = "test.partial";
        type.fields = {
            float_field("speed", core::reflect::field_kind::F32),
            float_field("range", core::reflect::field_kind::F64),
            nested,
        };
        property_drawer_registry registry = {};
        // 'nested' missing, then present but not an object: its floats are missing members either way.
        for (core::json::document object : {core::json::document::object(), core::json::object({{"nested", 5}})}) {
            imgui_frame_scope scope{};
            const std::string before = object.to_string();
            CAPTURE(before);
            bool changed = false;
            const std::string shown = shown_text(scope, [&]() {
                changed = registry.draw_object(type, object, false) || changed;
            });
            CAPTURE(shown);
            // speed, nested.ratio / range, nested.span.
            CHECK(count_of(shown, "{ 0.000 }") == 2);
            CHECK(count_of(shown, "{ 0.000000 }") == 2);
            CHECK(shown.find("nan") == std::string::npos);
            CHECK_FALSE(changed);
            CHECK(object.to_string() == before);
        }
        // A member the object holds as null is still a NaN.
        imgui_frame_scope scope{};
        core::json::document held = core::json::object({{"speed", nullptr}, {"range", 2.0}});
        const std::string shown = shown_text(scope, [&]() { (void) registry.draw_object(type, held, false); });
        CAPTURE(shown);
        CHECK(count_of(shown, "{ nan }") == 1);
        CHECK(held.root()["speed"].is_null());
    }

    TEST_CASE("property_drawer_registry - a typed inf is clamped by an f32 drag and ignored by an f32 slider") {
        property_drawer_registry registry = {};
        imgui_frame_scope scope{};
        // No bound: a drag over the whole float range, which clamps what is typed into it.
        property_draw_context context = {};
        context.label = "speed";
        core::json::document speed = core::json::document::from_value(core::json::object({{"v", 1.5}}).root()["v"]);
        type_into(
          scope,
          []() { ImGui::SetKeyboardFocusHere(0); },
          [&]() { (void) registry.draw(core::reflect::field_kind::F32, context, speed); },
          "inf"
        );
        CHECK(speed.root().as_f64() == static_cast<f64>(std::numeric_limits<f32>::max()));
        // Both bounds: a slider, which does not clamp typed text, so the infinity is dropped.
        core::reflect::field_info bounded = float_field("bounded", core::reflect::field_kind::F32);
        bounded.attributes.min = 0.0;
        bounded.attributes.max = 10.0;
        context.field = &bounded;
        context.label = "bounded";
        core::json::document value = core::json::document::from_value(core::json::object({{"v", 2.5}}).root()["v"]);
        bool changed = false;
        type_into(
          scope,
          []() { ImGui::SetKeyboardFocusHere(0); },
          [&]() { changed = registry.draw(core::reflect::field_kind::F32, context, value) || changed; },
          "inf"
        );
        CHECK(value.root().as_f64() == doctest::Approx(2.5));
        CHECK_FALSE(changed);
    }

    TEST_CASE("regression - code sweep F628 property_drawer_registry shows a radian angle in degrees and writes what "
      "is typed back in radians") {
        // The camera's own field description: tagged RADIANS, with its limits (0.01, 3.1) authored in radians.
        const core::reflect::type_info camera = core::reflect::describe_struct<renderer::camera>();
        const core::reflect::field_info *fov = camera.find_field("verticalFovRadians");
        REQUIRE(fov != nullptr);
        core::reflect::type_info type = {};
        type.name = "test.fov";
        type.fields = {*fov};
        property_drawer_registry registry = {};
        imgui_frame_scope scope{};
        core::json::document object = core::json::object({{"verticalFovRadians", std::numbers::pi / 3.0}});
        const std::function<void()> draw = [&]() {
            (void) registry.draw_object(type, object, false);
        };
        const std::string shown = shown_text(scope, draw);
        CAPTURE(shown);
        CHECK(count_of(shown, "{ 60.000 deg }") == 1);
        type_into(scope, []() { ImGui::SetKeyboardFocusHere(0); }, draw, "90");
        CHECK(object.root()["verticalFovRadians"].as_f64() == doctest::Approx(std::numbers::pi / 2.0).epsilon(1.0e-6));
        // The limits apply in radians whatever the unit typed in: 200 degrees is past 3.1 radians, 0 below 0.01.
        type_into(scope, []() { ImGui::SetKeyboardFocusHere(0); }, draw, "200");
        CHECK(object.root()["verticalFovRadians"].as_f64() == doctest::Approx(3.1));
        type_into(scope, []() { ImGui::SetKeyboardFocusHere(0); }, draw, "0");
        CHECK(object.root()["verticalFovRadians"].as_f64() == doctest::Approx(0.01));
    }

    TEST_CASE("property_drawer_registry - a radian field that is only shown keeps its exact value and its infinity") {
        const core::reflect::field_info loose = angle_field("loose", field_kind::F32, field_unit::RADIANS);
        const core::reflect::field_info wide = angle_field("wide", field_kind::F64, field_unit::RADIANS);
        core::reflect::field_info endless = angle_field("endless", field_kind::F32, field_unit::RADIANS);
        endless.attributes.min = 0.0;
        core::reflect::type_info type = {};
        type.name = "test.angles";
        type.fields = {loose, wide, endless};
        property_drawer_registry registry = {};
        imgui_frame_scope scope{};
        core::json::document object = core::json::object({{"loose", 1.0}, {"wide", -1.0}, {"endless", "inf"}});
        const std::string before = object.to_string();
        bool changed = false;
        const std::string shown = shown_text(scope, [&]() {
            changed = registry.draw_object(type, object, false) || changed;
        });
        CAPTURE(shown);
        CHECK(count_of(shown, "{ 57.296 deg }") == 1);
        CHECK(count_of(shown, "{ -57.295780 deg }") == 1);
        CHECK(count_of(shown, "{ inf deg }") == 1);
        CHECK_FALSE(changed);
        CHECK(object.to_string() == before);
    }

    TEST_CASE("property_drawer_registry - a degree field is shown as stored, an f64 or array radian field converts") {
        core::reflect::field_info slope = angle_field("slope", field_kind::F32, field_unit::DEGREES);
        slope.attributes.min = 0.0;
        slope.attributes.max = 90.0;
        const core::reflect::field_info heading = angle_field("heading", field_kind::F64, field_unit::RADIANS);
        core::reflect::field_info limits = angle_field("limits", field_kind::ARRAY, field_unit::RADIANS);
        limits.elementKind = field_kind::F32;
        limits.arrayCount = 2;
        property_drawer_registry registry = {};
        imgui_frame_scope scope{};
        SUBCASE("degrees") {
            property_draw_context context = {};
            context.field = &slope;
            context.label = "slope";
            core::json::document value = float_value(45.0);
            const std::function<void()> draw = [&]() {
                (void) registry.draw(field_kind::F32, context, value);
            };
            const std::string shown = shown_text(scope, draw);
            CAPTURE(shown);
            CHECK(count_of(shown, "{ 45.000 deg }") == 1);
            type_into(scope, []() { ImGui::SetKeyboardFocusHere(0); }, draw, "30");
            CHECK(value.root().as_f64() == doctest::Approx(30.0));
            type_into(scope, []() { ImGui::SetKeyboardFocusHere(0); }, draw, "120");
            CHECK(value.root().as_f64() == doctest::Approx(90.0));
        }
        SUBCASE("an f64 radian field") {
            property_draw_context context = {};
            context.field = &heading;
            context.label = "heading";
            core::json::document value = float_value(1.0);
            const std::function<void()> draw = [&]() {
                (void) registry.draw(field_kind::F64, context, value);
            };
            const std::string shown = shown_text(scope, draw);
            CAPTURE(shown);
            CHECK(count_of(shown, "{ 57.295780 deg }") == 1);
            type_into(scope, []() { ImGui::SetKeyboardFocusHere(0); }, draw, "180");
            CHECK(value.root().as_f64() == doctest::Approx(std::numbers::pi).epsilon(1.0e-9));
        }
        SUBCASE("the elements of a radian array") {
            core::reflect::type_info type = {};
            type.name = "test.limits_array";
            type.fields = {limits};
            core::json::document object = core::json::object({{"limits", core::json::array({0.5f, 1.0f})}});
            const std::function<void()> draw = [&]() {
                ImGui::SetNextItemOpen(true);
                (void) registry.draw_object(type, object, false);
            };
            const std::string shown = shown_text(scope, draw);
            CAPTURE(shown);
            CHECK(count_of(shown, "{ 28.648 deg }") == 1);
            CHECK(count_of(shown, "{ 57.296 deg }") == 1);
            // Focus 0 is the array's tree node, 1 its first element.
            type_into(scope, []() { ImGui::SetKeyboardFocusHere(1); }, draw, "45");
            CHECK(object.root()["limits"][0].as_f64() == doctest::Approx(std::numbers::pi / 4.0).epsilon(1.0e-6));
            CHECK(object.root()["limits"][1].as_f64() == doctest::Approx(1.0));
        }
    }

    TEST_CASE("property_drawer_registry - a drag over an angle moves in degrees and takes its step in stored units") {
        property_drawer_registry registry = {};
        ImVec2 centre = {};
        struct drag_case {
            std::string_view name = {};
            field_unit unit = field_unit::NONE;
            f64 step = 0.0;
            f64 expected = 0.0;
        };
        // 10 pixels: half a degree per pixel without a step, one step per pixel with one, whatever the unit.
        const drag_case cases[] = {
          {"radians without a step", field_unit::RADIANS, 0.0, 5.0 * std::numbers::pi / 180.0},
          {"radians with a step", field_unit::RADIANS, 0.01, 0.1},
          {"degrees without a step", field_unit::DEGREES, 0.0, 5.0},
          {"degrees with a step", field_unit::DEGREES, 0.25, 2.5},
          {"no unit with a step", field_unit::NONE, 0.01, 0.1},
          {"no unit without a step", field_unit::NONE, 0.0, 0.5},
        };
        for (const drag_case &dragged : cases) {
            CAPTURE(dragged.name);
            // A context of its own: a press where the last case pressed would be a double click, which types.
            imgui_frame_scope scope{};
            core::reflect::field_info field = angle_field("turn", field_kind::F32, dragged.unit);
            field.attributes.step = dragged.step;
            property_draw_context context = {};
            context.field = &field;
            context.label = "turn";
            core::json::document value = float_value(0.0);
            const std::function<void()> draw = [&]() {
                (void) registry.draw(field_kind::F32, context, value);
                centre = last_item_centre();
            };
            drag_by(scope, draw, centre, 10.0f);
            // The shown value is rounded to the format's three decimals as it is dragged.
            CHECK(value.root().as_f64() == doctest::Approx(dragged.expected).epsilon(1.0e-3));
        }
    }

    TEST_CASE("property_drawer_registry - a unit on a field that holds no float changes nothing") {
        // validate_layout refuses such a field; a hand-built description reaches the drawers anyway.
        core::reflect::field_info offset = angle_field("offset", field_kind::VEC3, field_unit::RADIANS);
        offset.attributes.step = 0.01;
        const core::reflect::field_info count = angle_field("count", field_kind::I32, field_unit::DEGREES);
        core::reflect::type_info type = {};
        type.name = "test.no_float";
        type.fields = {offset, count};
        property_drawer_registry registry = {};
        imgui_frame_scope scope{};
        core::json::document object = core::json::object({
          {"offset", core::json::array({1.0f, 2.0f, 3.0f})},
          {"count", 7},
        });
        const std::string shown = shown_text(scope, [&]() { (void) registry.draw_object(type, object, false); });
        CAPTURE(shown);
        CHECK(shown.find("deg") == std::string::npos);
        CHECK(shown.find("{ 1.000 }") != std::string::npos);
        CHECK(shown.find("{ 7 }") != std::string::npos);
    }
}
