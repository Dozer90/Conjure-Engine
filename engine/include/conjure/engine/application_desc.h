#pragma once

#include "conjure/engine/command_line.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/project.h"

#include "conjure/core/api.h"
#include "conjure/core/clock.h"
#include "conjure/core/result.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The application descriptor (docs/design/06_engine_loop.md 2.11, D9): the final, validated configuration
// plus the loaded project and an optional clock override. application never re-reads configuration.

namespace conjure::engine {

    struct CONJURE_API application_desc {
        engine_config config = {};
        std::optional<project> project = std::nullopt;
        core::clock *clockOverride = nullptr;                   // tests inject core::manual_clock; null = steady clock
        std::vector<std::string> unrecognisedArguments = {};    // returned to the executable (--exec, --export)
        bool showHelp = false;

        /// <summary>
        /// Performs the whole configuration merge: baseConfig, headless_defaults() when --headless is given
        /// (before the project and the remaining flags), then project.json (when --project names a
        /// directory), then the command-line overrides, then validate(). Fails with BAD_ARGUMENTS, the
        /// project errors or INVALID_CONFIG; a desc with showHelp set is returned without loading anything.
        /// A real --rhi backend together with --headless logs a WARN: a headless run renders on the null
        /// device only, so the flag has no effect. --headless leaves scripting.enabled false whatever the
        /// project says; --scripting, applied with the remaining flags, sets it to true.
        /// </summary>
        [[nodiscard]] static core::result<application_desc> from_command_line(
          std::span<const std::string_view> arguments,
          const engine_config &baseConfig
        );

        /// <summary>
        /// The same merge from already parsed options.
        /// </summary>
        [[nodiscard]] static core::result<application_desc> from_options(
          const command_line_options &options,
          const engine_config &baseConfig
        );
    };
}
