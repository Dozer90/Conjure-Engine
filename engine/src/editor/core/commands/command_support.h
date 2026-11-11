#pragma once

#include "conjure/editor/command_registry.h"
#include "conjure/editor/editor_command.h"
#include "conjure/editor/editor_context.h"
#include "conjure/editor/editor_error.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/math.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/entity.h"
#include "conjure/scene/scene.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Shared helpers of the command implementations (engine/src/editor/core/commands/*.cpp): argument readers
// with INVALID_ARGS messages naming the path, math <-> JSON conversions, entity resolution that fills the
// invocation's error details, subtree capture and restore for inverses, and the registration entry points
// of every command group.

namespace conjure::editor::support {

    /*-----------------------------------------------------------------------*/
    // Errors

    [[nodiscard]] core::error invalid_args(std::string message);
    [[nodiscard]] core::error not_available(std::string message);
    [[nodiscard]] core::error io_error(std::string message);
    [[nodiscard]] core::error internal(std::string message);

    /// <summary>
    /// Wraps a foreign error: classify_error for the code, the message kept.
    /// </summary>
    [[nodiscard]] core::error wrap(const core::error &failure);

    /*-----------------------------------------------------------------------*/
    // Arguments

    [[nodiscard]] core::result<std::string_view> require_string(const core::json::value &args, std::string_view key);
    [[nodiscard]] std::string_view string_or(
      const core::json::value &args,
      std::string_view key,
      std::string_view fallback
    );
    [[nodiscard]] bool bool_or(const core::json::value &args, std::string_view key, bool fallback);
    [[nodiscard]] core::result<u32> u32_or(const core::json::value &args, std::string_view key, u32 fallback);
    [[nodiscard]] core::result<f32> f32_or(const core::json::value &args, std::string_view key, f32 fallback);

    /// <summary>
    /// [x, y, z] of finite numbers.
    /// </summary>
    [[nodiscard]] core::result<math::vec3> read_vec3(const core::json::value &value, std::string_view name);

    /// <summary>
    /// [x, y, z, w] as a quaternion or [pitch, yaw, roll] in degrees.
    /// </summary>
    [[nodiscard]] core::result<math::quat> read_rotation(const core::json::value &value, std::string_view name);

    /// <summary>
    /// {position?, rotation?, scale?} over a base transform.
    /// </summary>
    [[nodiscard]] core::result<math::transform> read_transform(
      const core::json::value &value,
      std::string_view name,
      const math::transform &base
    );

    [[nodiscard]] core::json::document vec3_json(const math::vec3 &value);
    [[nodiscard]] core::json::document quat_json(const math::quat &value);
    [[nodiscard]] core::json::document transform_json(const math::transform &value);
    [[nodiscard]] core::json::document aabb_json(const math::aabb &value);
    [[nodiscard]] core::json::document string_array(std::span<const std::string> values);
    [[nodiscard]] core::json::document guid_array(std::span<const core::uuid> values);

    /*-----------------------------------------------------------------------*/
    // Entities

    /// <summary>
    /// entity_ref::resolve over the invocation's scene, filling errorDetails with the candidates.
    /// </summary>
    [[nodiscard]] core::result<ecs::entity> resolve_entity(
      command_invocation &invocation,
      const core::json::value &reference
    );

    /// <summary>
    /// The guid of a live entity as a string (generated when missing).
    /// </summary>
    [[nodiscard]] core::result<std::string> guid_string(scene::scene &scene, ecs::entity entity);

    /// <summary>
    /// The guid of a possibly dead or null entity: "" when it has none.
    /// </summary>
    [[nodiscard]] std::string guid_or_empty(const scene::scene &scene, ecs::entity entity);

    /// <summary>
    /// {guid, name, path} of an entity.
    /// </summary>
    [[nodiscard]] core::json::document entity_summary(scene::scene &scene, ecs::entity entity);

    /// <summary>
    /// The subtree in the prefab file format (guids kept), the payload of entity.restore.
    /// </summary>
    [[nodiscard]] core::result<core::json::document> capture_subtree(const scene::scene &scene, ecs::entity root);

    /// <summary>
    /// Instantiates a capture_subtree() payload with its guids under 'parent' at 'index'.
    /// </summary>
    [[nodiscard]] core::result<ecs::entity> restore_subtree(
      scene::scene &scene,
      const core::json::value &archive,
      ecs::entity parent,
      u32 index
    );

    /// <summary>
    /// Keeps the entities none of whose ancestors are also in the list (outermost subtrees), in the order
    /// given.
    /// </summary>
    [[nodiscard]] std::vector<ecs::entity> outermost(const scene::scene &scene, std::span<const ecs::entity> entities);

    /// <summary>
    /// The position of an entity among its siblings: the index in its parent's child list, or among the scene
    /// roots for a root. APPEND_CHILD when the entity is not in the tree.
    /// </summary>
    [[nodiscard]] u32 tree_position(const scene::scene &scene, ecs::entity entity) noexcept;

    /// <summary>
    /// An entity.restore request for the subtree, the inverse of destroying it.
    /// </summary>
    [[nodiscard]] core::result<command_inverse> restore_inverse(scene::scene &scene, ecs::entity root);

    /// <summary>
    /// An editor.batch request of the given children.
    /// </summary>
    [[nodiscard]] core::json::document batch_args(std::span<const command_inverse> children, std::string_view label);

    /// <summary>
    /// Applies an entity.restore inverse made by restore_inverse() (rollback inside a handler, before any
    /// response leaves).
    /// </summary>
    [[nodiscard]] core::result<ecs::entity> apply_restore_inverse(scene::scene &scene, const command_inverse &inverse);

    /*-----------------------------------------------------------------------*/
    // Encodings and tool output

    /// <summary>
    /// Standard base64 with padding (the payload of asset.restore).
    /// </summary>
    [[nodiscard]] std::string base64_encode(std::span<const std::byte> bytes);

    /// <summary>
    /// Decodes standard base64; padding and whitespace are skipped, any other foreign character is INVALID_ARGS.
    /// </summary>
    [[nodiscard]] core::result<std::vector<std::byte>> base64_decode(std::string_view text);

    /// <summary>
    /// One diagnostic line of a build tool.
    /// </summary>
    struct tool_diagnostic {
        std::string file = "";
        std::string code = "";                              // "C2065", "LNK1104", "" for GCC/Clang and CMake
        std::string message = "";
        u32 line = 0;
        u32 column = 0;
        bool isError = false;
    };

    /// <summary>
    /// Recognises the MSVC ("file(12,3): error C2065: ..."), GCC/Clang ("file:12:3: error: ...") and CMake
    /// ("CMake Error at ...") diagnostic formats, including "fatal error"; anything else (a path or a message
    /// that happens to contain the word) is nullopt.
    /// </summary>
    [[nodiscard]] std::optional<tool_diagnostic> parse_tool_diagnostic(std::string_view line);

    /// <summary>
    /// True for a non-empty argument of letters, digits, '_', '-' and '.' that does not start with '-' (so it
    /// can never be read as an option by the child process).
    /// </summary>
    [[nodiscard]] bool is_safe_tool_argument(std::string_view text) noexcept;

    /*-----------------------------------------------------------------------*/
    // Registration

    [[nodiscard]] command_descriptor make_command(
      std::string name,
      std::string summary,
      std::string argsSchema,
      std::string resultSchema,
      command_flags flags,
      command_handler handler
    );

    /// <summary>
    /// Registers a descriptor; a failure is a programmer error (asserted and logged).
    /// </summary>
    void add_command(command_registry &registry, command_descriptor descriptor);

    void register_session_commands(command_registry &registry);
    void register_editor_commands(command_registry &registry);
    void register_history_commands(command_registry &registry);
    void register_console_commands(command_registry &registry);
    void register_project_commands(command_registry &registry);
    void register_scene_commands(command_registry &registry);
    void register_entity_commands(command_registry &registry);
    void register_component_commands(command_registry &registry);
    void register_asset_commands(command_registry &registry);
    void register_prefab_commands(command_registry &registry);
    void register_script_commands(command_registry &registry);
    void register_play_commands(command_registry &registry);
    void register_input_commands(command_registry &registry);
    void register_view_commands(command_registry &registry);
    void register_physics_commands(command_registry &registry);
    void register_selection_commands(command_registry &registry);
    void register_reflection_commands(command_registry &registry);
    void register_module_commands(command_registry &registry);
    void register_profiler_commands(command_registry &registry);

    /*-----------------------------------------------------------------------*/
    // Schema fragments shared by several commands

    inline constexpr std::string_view ENTITY_REF_SCHEMA = R"({"type":"string","description":"guid or hierarchy path"})";
    inline constexpr std::string_view EMPTY_SCHEMA = "{\"type\":\"object\"}";
    inline constexpr std::string_view ANY_OBJECT_SCHEMA = "{\"type\":\"object\",\"additionalProperties\":true}";
}
