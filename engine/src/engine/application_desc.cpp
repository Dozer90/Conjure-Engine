#include "conjure/engine/application_desc.h"

#include "conjure/engine/engine_error.h"
#include "conjure/engine/log_category.h"

#include "conjure/core/log.h"

#include <utility>

using namespace conjure;
using namespace conjure::engine;

core::result<application_desc> application_desc::from_command_line(
  std::span<const std::string_view> arguments,
  const engine_config &baseConfig
) {
    CONJURE_TRY_ASSIGN(const command_line_options options, parse_command_line(arguments));
    return from_options(options, baseConfig);
}

core::result<application_desc> application_desc::from_options(
  const command_line_options &options,
  const engine_config &baseConfig
) {
    application_desc desc = {};
    desc.unrecognisedArguments = options.unrecognisedArguments;
    desc.showHelp = options.help;
    if (options.help) {
        desc.config = baseConfig;
        return desc;
    }

    // Precedence (D9): defaults < --headless defaults < project.json < the remaining flags. Headless
    // comes first so a project's window size or audio backend does not drag a headless run back to real
    // devices; the project then sets what the engine_config::headless guard lets through (project::apply_to).
    // Scripting is one of the things the guard holds back, so a headless run executes scripts only when
    // --scripting, one of the remaining flags, turns it on again (D12).
    desc.config = baseConfig;
    if (options.headless) {
        const engine_config headless = engine_config::headless_defaults();
        desc.config.headless = true;
        desc.config.window.width = headless.window.width;
        desc.config.window.height = headless.window.height;
        desc.config.rendering.enabled = headless.rendering.enabled;
        desc.config.rendering.backend = headless.rendering.backend;
        desc.config.audio.backend = headless.audio.backend;
        desc.config.assets.hotReload = headless.assets.hotReload;
        desc.config.scripting.enabled = headless.scripting.enabled;
        desc.config.time.forcedFrameDeltaSeconds = headless.time.forcedFrameDeltaSeconds;
        desc.config.jobs.workerCount = headless.jobs.workerCount;
    }

    if (options.projectPath.has_value()) {
        CONJURE_TRY_ASSIGN(conjure::engine::project loaded, conjure::engine::project::load(*options.projectPath));
        desc.config.projectPath = loaded.root_directory();
        loaded.apply_to(desc.config);
        desc.project = std::move(loaded);
    }

    apply_command_line(options, desc.config);
    // AUTO and NULL_DEVICE name no device of their own, so only an explicit d3d12 or vulkan warns.
    const bool realBackend = options.rhi.has_value()
      && (*options.rhi == rhi_backend_choice::D3D12 || *options.rhi == rhi_backend_choice::VULKAN);
    if (options.headless && realBackend && !desc.config.rendering.enabled) {
        CONJURE_LOG_WARN(
          LOG_ENGINE,
          "--rhi {} has no effect with --headless: a headless run renders on the null device only",
          to_string(*options.rhi)
        );
    }
    CONJURE_TRY(desc.config.validate());
    return desc;
}
