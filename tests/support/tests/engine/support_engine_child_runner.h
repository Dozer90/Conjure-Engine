#pragma once

// Runs conjure_support_engine_child (CMakeLists.txt here) for the self-tests that observe a fixture failing
// its case from a constructor or destructor: exit code, the failure text and how many assertions failed.

#include "conjure/testing/doctest.h"

#include "conjure/core/process.h"
#include "conjure/core/types.h"

#include <cctype>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace conjure::testing {

    /// <summary>
    /// Runs one case of the child with -tc=&lt;name&gt;. The CONJURE_TEST_* variables a developer may have
    /// set are removed from the child's environment so the observed behaviour never depends on the shell.
    /// </summary>
    [[nodiscard]] inline core::process_result run_support_engine_child(std::string_view caseName) {
        core::process_desc desc = {};
        desc.executable = CONJURE_SUPPORT_ENGINE_CHILD_PATH;
        desc.arguments = {std::string("-tc=") + std::string(caseName)};
        desc.timeoutSeconds = 120.0;

        constexpr std::string_view CLEARED_VARIABLES[] = {
            "CONJURE_TEST_ISOLATED",
            "CONJURE_TEST_WINDOW",
            "CONJURE_TEST_CONTINUE_ON_ASSERT",
            "CONJURE_TEST_DOWNLOADED_ASSET_ROOT",
            "CONJURE_TEST_SEED",
            "CONJURE_TEST_THREADS",
            "CONJURE_TEST_UPDATE_GOLDEN",
        };
        for (const std::string_view variable : CLEARED_VARIABLES) {
            desc.environment.emplace_back(std::string(variable), "");   // an empty value removes it
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

    /// <summary>
    /// The failed-assertion count of doctest's summary line ("[doctest] assertions: 3 | 2 passed | 1 failed |"),
    /// or nullopt when the output has no such line (the child crashed before reporting).
    /// </summary>
    [[nodiscard]] inline std::optional<u32> failed_assertion_count(std::string_view output) {
        const usize summary = output.find("[doctest] assertions:");
        if (summary == std::string_view::npos) {
            return std::nullopt;
        }
        const usize failedWord = output.find(" failed", summary);
        if (failedWord == std::string_view::npos) {
            return std::nullopt;
        }
        usize begin = failedWord;
        while (begin > summary && std::isdigit(static_cast<unsigned char>(output[begin - 1])) != 0) {
            --begin;
        }
        u32 count = 0;
        const std::from_chars_result parsed =
          std::from_chars(output.data() + begin, output.data() + failedWord, count);
        if (parsed.ec != std::errc() || parsed.ptr != output.data() + failedWord) {
            return std::nullopt;
        }
        return count;
    }
}
