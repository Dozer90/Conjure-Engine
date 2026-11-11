#pragma once

// Shared helpers of the engine-level integration scenarios (docs/design/08_testing.md 6.1): quiet
// headless_engine options, scene authoring and canonical serialisation, native file helpers and the
// build-time paths of the fixtures (the runtime executable, the managed test scripts, the editor command
// files). Everything here is main-thread only and allocation-agnostic; the scenarios that gate
// allocations measure their own frames.

#include "conjure/testing/doctest.h"
#include "conjure/testing/headless_engine.h"

#include "conjure/core/math/glm_types.h"
#include "conjure/core/types.h"
#include "conjure/ecs/entity.h"
#include "conjure/scene/scene.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace integration_test {

    /// <summary>
    /// headless_engine options with the renderer, physics and audio off: the fastest fixture for scenarios
    /// about scenes, hierarchy and modules.
    /// </summary>
    [[nodiscard]] conjure::testing::headless_engine_options quiet_options();

    /// <summary>
    /// quiet_options() with physics enabled.
    /// </summary>
    [[nodiscard]] conjure::testing::headless_engine_options physics_options();

    /// <summary>
    /// quiet_options() with the renderer on the null device (needs the shader index conjure_shaders wrote).
    /// </summary>
    [[nodiscard]] conjure::testing::headless_engine_options renderer_options();

    /// <summary>
    /// The scene serialised with scene_serializer::save_scene: the canonical text every round trip compares.
    /// </summary>
    [[nodiscard]] std::string serialise_scene(const conjure::scene::scene &source);

    /// <summary>
    /// Creates a named root (or child of 'parent') carrying a transform at 'position'. Fails the case on error.
    /// </summary>
    [[nodiscard]] conjure::ecs::entity create_at(
      conjure::scene::scene &target,
      std::string_view name,
      const conjure::math::vec3 &position,
      conjure::ecs::entity parent = conjure::ecs::NULL_ENTITY
    );

    /// <summary>
    /// The translation of an entity's world_transform component (what propagation wrote, not a fresh walk).
    /// </summary>
    [[nodiscard]] conjure::math::vec3 propagated_position(
      const conjure::scene::scene &source,
      conjure::ecs::entity target
    );

    /// <summary>
    /// The translation of an entity's render_world_transform component (what extraction reads).
    /// </summary>
    [[nodiscard]] conjure::math::vec3 rendered_position(
      const conjure::scene::scene &source,
      conjure::ecs::entity target
    );

    /// <summary>
    /// Reads a whole file as bytes-as-text (binary mode, no newline translation). Fails the case when missing.
    /// </summary>
    [[nodiscard]] std::string read_file(const std::filesystem::path &path);

    /// <summary>
    /// Writes a text file in binary mode, creating its parent directories. Fails the case on error.
    /// </summary>
    void write_file(const std::filesystem::path &path, std::string_view text);

    /// <summary>
    /// The project.json text of a minimal development project: name, assets root and optional startup scene
    /// plus extra members (already formatted JSON members, comma-free).
    /// </summary>
    [[nodiscard]] std::string project_json(
      std::string_view name,
      std::string_view startupScene = {},
      std::string_view extraMembers = {}
    );

    /// <summary>
    /// The scene files the scenarios author: the chain, the roundtrip fixture and the determinism scene.
    /// </summary>
    [[nodiscard]] std::string author_roundtrip_scene(conjure::testing::headless_engine &engine);
    [[nodiscard]] std::string author_small_scene(conjure::testing::headless_engine &engine, std::string_view prefix);

    /// <summary>
    /// Copies tests/assets/projects/minimal into 'directory'/minimal and returns that path: the asset database
    /// writes .meta sidecars and its cache next to a project's sources, which must never land in the repository
    /// (and two test processes must never share one cache).
    /// </summary>
    [[nodiscard]] std::filesystem::path copy_minimal_project(const std::filesystem::path &directory);

    /// <summary>
    /// Build-time paths; empty when the build did not produce the artefact.
    /// </summary>
    [[nodiscard]] std::filesystem::path runtime_executable();
    [[nodiscard]] std::filesystem::path runtime_bin_directory();
    [[nodiscard]] std::filesystem::path test_scripts_assembly(std::string_view variant);
    [[nodiscard]] std::filesystem::path editor_command_directory();
    [[nodiscard]] std::filesystem::path minimal_project_directory();
}
