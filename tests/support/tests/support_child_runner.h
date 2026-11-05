#pragma once

// Runs conjure_support_child (tests/support/tests/CMakeLists.txt) for the self-tests that observe
// behaviour only visible from outside the process: exit codes, the start-up banner, a fixture that
// fails its case from a destructor, an assert that aborts.

#include "conjure/testing/doctest.h"

#include "conjure/core/process.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace conjure::testing {

    /// <summary>
    /// Runs the child with the given doctest / --conjure-* arguments and environment overrides. The
    /// CONJURE_TEST_* variables a developer may have set are removed from the child's environment
    /// unless the overrides name them, so the observed behaviour never depends on the parent's shell.
    /// </summary>
    [[nodiscard]] inline core::process_result run_support_child(
      std::vector<std::string> arguments,
      std::vector<std::pair<std::string, std::string>> environmentOverrides = {}
    ) {
        core::process_desc desc = {};
        desc.executable = CONJURE_SUPPORT_CHILD_PATH;
        desc.arguments = std::move(arguments);
        desc.environment = std::move(environmentOverrides);
        desc.timeoutSeconds = 120.0;

        constexpr std::string_view CLEARED_VARIABLES[] = {
            "CONJURE_TEST_ISOLATED",
            "CONJURE_TEST_WINDOW",
            "CONJURE_TEST_CONTINUE_ON_ASSERT",
            "CONJURE_TEST_DOWNLOADED_ASSET_ROOT",
            "CONJURE_TEST_SEED",
            "CONJURE_TEST_THREADS",
        };
        for (const std::string_view variable : CLEARED_VARIABLES) {
            bool overridden = false;
            for (const auto &[name, value] : desc.environment) {
                overridden = overridden || name == variable;
            }
            if (!overridden) {
                desc.environment.emplace_back(std::string(variable), "");   // an empty value removes it
            }
        }

        core::result<core::process_result> finished = core::process::run(desc);
        REQUIRE_MESSAGE(finished.has_value(), describe_error(finished));
        CHECK_FALSE(finished->timedOut);
        return std::move(finished).value();
    }

    /// <summary>
    /// True when the captured output contains the text.
    /// </summary>
    [[nodiscard]] inline bool output_contains(std::string_view output, std::string_view needle) {
        return output.find(needle) != std::string_view::npos;
    }
}
