#include "integration_engine_support.h"

#include "conjure/testing/test_assets.h"

#include "conjure/core/json/writer.h"
#include "conjure/core/string_id.h"
#include "conjure/ecs/world.h"
#include "conjure/scene/components.h"
#include "conjure/scene/scene_serializer.h"

#include <fstream>
#include <sstream>
#include <system_error>

#ifndef CONJURE_INTEGRATION_RUNTIME_EXECUTABLE
    #define CONJURE_INTEGRATION_RUNTIME_EXECUTABLE ""
#endif
#ifndef CONJURE_INTEGRATION_RUNTIME_BIN_DIR
    #define CONJURE_INTEGRATION_RUNTIME_BIN_DIR ""
#endif
#ifndef CONJURE_INTEGRATION_TEST_SCRIPTS_DIR
    #define CONJURE_INTEGRATION_TEST_SCRIPTS_DIR ""
#endif
#ifndef CONJURE_INTEGRATION_EDITOR_COMMAND_DIR
    #define CONJURE_INTEGRATION_EDITOR_COMMAND_DIR ""
#endif

using namespace conjure;
using namespace conjure::testing;

namespace {

    [[nodiscard]] math::vec3 translation_of(const math::mat4 &matrix) noexcept {
        return math::vec3(matrix[3]);
    }
}

headless_engine_options integration_test::quiet_options() {
    headless_engine_options options = {};
    options.enableRenderer = false;
    options.enablePhysics = false;
    options.enableAudio = false;
    return options;
}

headless_engine_options integration_test::physics_options() {
    headless_engine_options options = quiet_options();
    options.enablePhysics = true;
    return options;
}

headless_engine_options integration_test::renderer_options() {
    headless_engine_options options = quiet_options();
    options.enableRenderer = true;
    return options;
}

std::string integration_test::serialise_scene(const scene::scene &source) {
    core::json::writer out = {};
    CONJURE_REQUIRE_OK(scene::scene_serializer::save_scene(source, out));
    return out.finish();
}

ecs::entity integration_test::create_at(
  scene::scene &target,
  std::string_view name,
  const math::vec3 &position,
  ecs::entity parent
) {
    const core::result<ecs::entity> created = target.create_entity(core::string_id(name), parent);
    CONJURE_REQUIRE_OK(created);
    scene::local_transform transform = {};
    transform.position = position;
    CONJURE_REQUIRE_OK(target.set_local_transform(created.value(), transform));
    return created.value();
}

math::vec3 integration_test::propagated_position(const scene::scene &source, ecs::entity target) {
    const scene::world_transform *transform = source.world().get<scene::world_transform>(target);
    REQUIRE(transform != nullptr);
    return translation_of(transform->matrix);
}

math::vec3 integration_test::rendered_position(const scene::scene &source, ecs::entity target) {
    const scene::render_world_transform *transform = source.world().get<scene::render_world_transform>(target);
    REQUIRE(transform != nullptr);
    return translation_of(transform->matrix);
}

std::string integration_test::read_file(const std::filesystem::path &path) {
    std::ifstream stream(path, std::ios::binary);
    REQUIRE_MESSAGE(stream.is_open(), "cannot open " << path.generic_string());
    std::ostringstream buffer = {};
    buffer << stream.rdbuf();
    return buffer.str();
}

void integration_test::write_file(const std::filesystem::path &path, std::string_view text) {
    std::error_code ignored = {};
    std::filesystem::create_directories(path.parent_path(), ignored);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    REQUIRE_MESSAGE(stream.is_open(), "cannot write " << path.generic_string());
    stream << text;
}

std::string integration_test::project_json(
  std::string_view name,
  std::string_view startupScene,
  std::string_view extraMembers
) {
    std::string text = "{\n  \"schema_version\": 1,\n  \"name\": \"";
    text += name;
    text += "\",\n  \"assets_root\": \"assets\"";
    if (!startupScene.empty()) {
        text += ",\n  \"startup_scene\": \"";
        text += startupScene;
        text += "\"";
    }
    if (!extraMembers.empty()) {
        text += ",\n  ";
        text += extraMembers;
    }
    text += "\n}\n";
    return text;
}

std::string integration_test::author_roundtrip_scene(headless_engine &engine) {
    scene::scene &draft = engine.active_scene();
    draft.set_name("roundtrip");
    const ecs::entity root = create_at(draft, "Root", math::vec3(0.0f, 1.0f, 0.0f));
    const ecs::entity child = create_at(draft, "Child", math::vec3(1.0f, 0.0f, 0.0f), root);
    CONJURE_UNUSED(create_at(draft, "Leaf", math::vec3(0.0f, 0.0f, 2.0f), child));
    CONJURE_UNUSED(create_at(draft, "Sibling", math::vec3(-1.0f, 0.0f, 0.0f), root));
    const ecs::entity other = create_at(draft, "Other", math::vec3(5.0f, 0.0f, 5.0f));
    CONJURE_REQUIRE_OK(draft.set_tag(other, core::string_id("Pickup")));
    CONJURE_REQUIRE_OK(draft.set_enabled(other, false));
    engine.step_frames(1);
    return serialise_scene(draft);
}

std::string integration_test::author_small_scene(headless_engine &engine, std::string_view prefix) {
    scene::scene &draft = engine.active_scene();
    draft.clear();
    draft.set_name(std::string(prefix));
    const std::string rootName = std::string(prefix) + "_Root";
    const std::string childName = std::string(prefix) + "_Child";
    const std::string otherName = std::string(prefix) + "_Other";
    const ecs::entity root = create_at(draft, rootName, math::vec3(1.0f, 2.0f, 3.0f));
    CONJURE_UNUSED(create_at(draft, childName, math::vec3(0.5f, 0.0f, 0.0f), root));
    CONJURE_UNUSED(create_at(draft, otherName, math::vec3(-1.0f, 0.0f, 0.0f)));
    const std::string text = serialise_scene(draft);
    draft.clear();
    return text;
}

std::filesystem::path integration_test::copy_minimal_project(const std::filesystem::path &directory) {
    const std::filesystem::path target = directory / "minimal";
    std::error_code status = {};
    std::filesystem::copy(
      minimal_project_directory(),
      target,
      std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
      status
    );
    REQUIRE_MESSAGE(!status, "copying the minimal project failed: " << status.message());
    REQUIRE(std::filesystem::is_regular_file(target / "project.json"));
    return target;
}

std::filesystem::path integration_test::runtime_executable() {
    return std::filesystem::path(CONJURE_INTEGRATION_RUNTIME_EXECUTABLE);
}

std::filesystem::path integration_test::runtime_bin_directory() {
    return std::filesystem::path(CONJURE_INTEGRATION_RUNTIME_BIN_DIR);
}

std::filesystem::path integration_test::test_scripts_assembly(std::string_view variant) {
    const std::filesystem::path root(CONJURE_INTEGRATION_TEST_SCRIPTS_DIR);
    if (root.empty()) {
        return {};
    }
    return root / std::string(variant) / "Conjure.TestScripts.dll";
}

std::filesystem::path integration_test::editor_command_directory() {
    return std::filesystem::path(CONJURE_INTEGRATION_EDITOR_COMMAND_DIR);
}

std::filesystem::path integration_test::minimal_project_directory() {
    return repo_asset("projects/minimal");
}
