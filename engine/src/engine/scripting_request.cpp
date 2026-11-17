#include "engine/scripting_request.h"

namespace conjure::engine {

    bool names_game_scripts(const scripting_settings &settings) noexcept {
        return settings.enabled && !settings.gameAssembly.empty();
    }

    bool asks_for_missing_scripting_module(const engine_config &config) noexcept {
        if (!config.scripting.enabled) {
            return false;
        }
        const bool headlessOptIn = config.headless && !config.editor;
        return headlessOptIn || names_game_scripts(config.scripting);
    }
}
