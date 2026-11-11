#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/reflect/type_info.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/reflection_path.h"
#include "conjure/ecs/component_registry.h"

#include <string>

// reflection_path.h (docs/design/04_editor.md 3.5): "component.field.sub[2]" over the component JSON.

using namespace conjure;
using namespace conjure::editor;

namespace {
    const core::reflect::type_info *type_of(editor_test::headless_editor &editor, std::string_view componentName) {
        const ecs::component_id id = editor.context().components().find(core::string_id(componentName));
        REQUIRE(id.is_valid());
        return editor.context().components().info(id).reflection;
    }
}

TEST_SUITE("editor/reflection_path") {

    TEST_CASE("reflection_path::parse - valid paths split into a component and field segments") {
        const core::result<reflection_path> path = reflection_path::parse("transform.position.x");
        CONJURE_REQUIRE_OK(path);
        CHECK(path.value().component() == "transform");
        REQUIRE(path.value().fields().size() == 2);
        CHECK(path.value().fields()[0].name == "position");
        CHECK(path.value().fields()[1].name == "x");
        CHECK(path.value().to_string() == "transform.position.x");
        const core::result<reflection_path> indexed = reflection_path::parse("script.scripts[2].fields.speed");
        CONJURE_REQUIRE_OK(indexed);
        CHECK(indexed.value().fields()[0].name == "scripts");
        CHECK(indexed.value().fields()[0].index == 2);
        CHECK(indexed.value().to_string() == "script.scripts[2].fields.speed");
        const core::result<reflection_path> bare = reflection_path::parse("sprite");
        CONJURE_REQUIRE_OK(bare);
        CHECK(bare.value().fields().empty());
    }

    TEST_CASE("reflection_path::parse - rejects empty text, empty segments, bad indices and trailing dots") {
        CONJURE_CHECK_ERROR(reflection_path::parse(""), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(reflection_path::parse("a..b"), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(reflection_path::parse("a.b["), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(reflection_path::parse("a.b[x]"), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(reflection_path::parse("a.b[]"), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(reflection_path::parse("a.b."), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(reflection_path::parse("a.b c"), protocol_error_code::INVALID_ARGS);
        // An index on the component segment parses and is rejected by resolve().
        CONJURE_REQUIRE_OK(reflection_path::parse("a[1]"));
        CONJURE_CHECK_ERROR(reflection_path::parse("a[1]").value().resolve(nullptr), protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("reflection_path::resolve - nested struct fields and vector members produce JSON pointers") {
        editor_test::headless_editor editor = {};
        const core::reflect::type_info *camera = type_of(editor, "conjure.renderer.camera");
        REQUIRE(camera != nullptr);
        const core::result<field_ref> nested = reflection_path::parse(
          "camera.cameraData.verticalFovRadians"
        ).value().resolve(camera);
        CONJURE_REQUIRE_OK(nested);
        CHECK(nested.value().pointer == "/cameraData/verticalFovRadians");
        CHECK(nested.value().kind == core::reflect::field_kind::F32);
        REQUIRE(nested.value().field != nullptr);
        CHECK(nested.value().field->name == "verticalFovRadians");
        const core::reflect::type_info *sprite = type_of(editor, "conjure.renderer.sprite");
        REQUIRE(sprite != nullptr);
        const core::result<field_ref> member = reflection_path::parse("sprite.size.y").value().resolve(sprite);
        CONJURE_REQUIRE_OK(member);
        CHECK(member.value().pointer == "/size/1");
        CHECK(member.value().kind == core::reflect::field_kind::F32);
        const core::result<field_ref> whole = reflection_path::parse("sprite").value().resolve(sprite);
        CONJURE_REQUIRE_OK(whole);
        CHECK(whole.value().is_whole_component());
        CHECK(whole.value().kind == core::reflect::field_kind::STRUCT);
        const core::reflect::type_info *transform = type_of(editor, "conjure.scene.local_transform");
        REQUIRE(transform != nullptr);
        const core::result<field_ref> position = reflection_path::parse(
          "transform.position.z"
        ).value().resolve(transform);
        CONJURE_REQUIRE_OK(position);
        CHECK(position.value().pointer == "/position/2");
    }

    TEST_CASE("reflection_path::resolve - a missing field names the segment and bad indices fail") {
        editor_test::headless_editor editor = {};
        const core::reflect::type_info *sprite = type_of(editor, "conjure.renderer.sprite");
        const core::result<field_ref> missing = reflection_path::parse("sprite.nothing").value().resolve(sprite);
        CONJURE_CHECK_ERROR(missing, protocol_error_code::PATH_NOT_FOUND);
        CHECK(missing.get_error().message().find("nothing") != std::string::npos);
        CONJURE_CHECK_ERROR(
          reflection_path::parse("sprite.size.w").value().resolve(sprite),
          protocol_error_code::PATH_NOT_FOUND
        );
        CONJURE_CHECK_ERROR(
          reflection_path::parse("sprite.size[7]").value().resolve(sprite),
          protocol_error_code::PATH_NOT_FOUND
        );
        CONJURE_CHECK_ERROR(
          reflection_path::parse("sprite.layer.x").value().resolve(sprite),
          protocol_error_code::PATH_NOT_FOUND
        );
    }

    TEST_CASE("reflection_path::resolve - an unreflected component maps names and indices straight onto JSON") {
        const core::result<field_ref> reference = reflection_path::parse(
          "script.scripts[0].fields.speed"
        ).value().resolve(nullptr);
        CONJURE_REQUIRE_OK(reference);
        CHECK(reference.value().pointer == "/scripts/0/fields/speed");
        CHECK(reference.value().kind == core::reflect::field_kind::COUNT);
        CHECK(reference.value().type == nullptr);
    }

    TEST_CASE("read_at / write_at - round trip values and enums by name or number") {
        editor_test::headless_editor editor = {};
        const core::reflect::type_info *camera = type_of(editor, "conjure.renderer.camera");
        core::json::document component = core::json::object({
          {"cameraData", core::json::object({{"projection", "PERSPECTIVE"}, {"verticalFovRadians", 1.0f}})},
          {"priority", 2},
        });
        const field_ref fov = reflection_path::parse(
          "camera.cameraData.verticalFovRadians"
        ).value().resolve(camera).value();
        const core::result<core::json::document> read = read_at(component.root(), fov);
        CONJURE_REQUIRE_OK(read);
        CHECK(read.value().root().as_f32() == doctest::Approx(1.0f));
        CONJURE_REQUIRE_OK(write_at(component, fov, core::json::object({{"v", 0.5f}}).root()["v"]));
        CHECK(component.root()["cameraData"]["verticalFovRadians"].as_f32() == doctest::Approx(0.5f));

        const field_ref projection = reflection_path::parse(
          "camera.cameraData.projection"
        ).value().resolve(camera).value();
        CHECK(projection.kind == core::reflect::field_kind::ENUM);
        CONJURE_REQUIRE_OK(write_at(component, projection, core::json::object({{"v", "ORTHOGRAPHIC"}}).root()["v"]));
        CHECK(component.root()["cameraData"]["projection"].as_string() == "ORTHOGRAPHIC");
        CONJURE_REQUIRE_OK(write_at(component, projection, core::json::object({{"v", 0}}).root()["v"]));
        const core::result<core::json::document> byNumber = read_at(component.root(), projection);
        CONJURE_REQUIRE_OK(byNumber);
        CHECK((byNumber.value().root().as_string("") == "PERSPECTIVE" || byNumber.value().root().as_i64(-1) == 0));

        const field_ref whole = reflection_path::parse("camera").value().resolve(camera).value();
        const core::result<core::json::document> all = read_at(component.root(), whole);
        CONJURE_REQUIRE_OK(all);
        CHECK(all.value().root()["priority"].as_i64() == 2);
        CONJURE_CHECK_ERROR(
          write_at(component, whole, core::json::object({{"v", 5}}).root()["v"]),
          protocol_error_code::INVALID_ARGS
        );
        const field_ref missing = reflection_path::parse("script.scripts[3].x").value().resolve(nullptr).value();
        CONJURE_CHECK_ERROR(read_at(component.root(), missing), protocol_error_code::PATH_NOT_FOUND);
    }

    TEST_CASE("reflection_path::merge_component - folds a qualified type name into the component segment") {
        reflection_path qualified = reflection_path::parse("conjure.renderer.sprite.size.x").value();
        REQUIRE(qualified.segments().size() == 5);
        CHECK(qualified.merge_component(3));
        CHECK(qualified.component() == "conjure.renderer.sprite");
        REQUIRE(qualified.fields().size() == 2);
        CHECK(qualified.fields()[0].name == "size");
        CHECK(qualified.to_string() == "conjure.renderer.sprite.size.x");

        reflection_path whole = reflection_path::parse("conjure.scripting.script_component").value();
        CHECK(whole.merge_component(3));
        CHECK(whole.fields().empty());
        CHECK(whole.component() == "conjure.scripting.script_component");

        reflection_path untouched = reflection_path::parse("a.b[1].c").value();
        CHECK_FALSE(untouched.merge_component(1));
        CHECK_FALSE(untouched.merge_component(4));
        CHECK_FALSE(untouched.merge_component(2));
        CHECK(untouched.segments().size() == 3);
        CHECK(untouched.component() == "a");
    }

    TEST_CASE("reflection_path::to_string - round trips through parse") {
        for (const std::string_view text : {"a.b", "a.b[3].c", "transform", "x.y[0]"}) {
            const core::result<reflection_path> path = reflection_path::parse(text);
            CONJURE_REQUIRE_OK(path);
            CHECK(path.value().to_string() == text);
            CHECK(reflection_path::parse(path.value().to_string()).value().to_string() == text);
        }
    }
}
