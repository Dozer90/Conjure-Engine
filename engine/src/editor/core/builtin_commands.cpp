#include "conjure/editor/builtin_commands.h"

#include "editor/core/commands/command_support.h"

namespace conjure::editor {

    void register_builtin_commands(command_registry &registry) {
        support::register_session_commands(registry);
        support::register_editor_commands(registry);
        support::register_history_commands(registry);
        support::register_console_commands(registry);
        support::register_project_commands(registry);
        support::register_scene_commands(registry);
        support::register_entity_commands(registry);
        support::register_component_commands(registry);
        support::register_asset_commands(registry);
        support::register_prefab_commands(registry);
        support::register_script_commands(registry);
        support::register_play_commands(registry);
        support::register_input_commands(registry);
        support::register_view_commands(registry);
        support::register_physics_commands(registry);
        support::register_selection_commands(registry);
        support::register_reflection_commands(registry);
        support::register_module_commands(registry);
        support::register_profiler_commands(registry);
    }

    usize builtin_command_count() noexcept {
        command_registry scratch = {};
        register_builtin_commands(scratch);
        return scratch.size();
    }
}
