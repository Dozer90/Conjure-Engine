#pragma once

// Runs a second doctest program for the self-tests that can only observe a behaviour from another process:
// conjure_support_engine_child (CMakeLists.txt here), whose cases fail from a constructor, destructor or
// step_frames, and conjure_support_engine_tests itself in golden update mode. The helpers read back the exit
// code, the failure text, how many cases ran and how many assertions failed.

#include "conjure/testing/doctest.h"

#include "conjure/core/process.h"
#include "conjure/core/result.h"
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
    /// Runs a doctest program with the given arguments. The CONJURE_TEST_* variables a developer may have set
    /// are removed from the program's environment so the observed behaviour never depends on the shell.
    /// </summary>
    [[nodiscard]] inline core::process_result run_support_engine_program(
      std::string_view executable, std::vector<std::string> arguments
    ) {
        core::process_desc desc = {};
        desc.executable = std::string(executable);
        desc.arguments = std::move(arguments);
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
    /// Runs one case of conjure_support_engine_child with -tc=&lt;name&gt; (run_support_engine_program).
    /// </summary>
    [[nodiscard]] inline core::process_result run_support_engine_child(std::string_view caseName) {
        return run_support_engine_program(
          CONJURE_SUPPORT_ENGINE_CHILD_PATH, {std::string("-tc=") + std::string(caseName)}
        );
    }

    /// <summary>
    /// True when the captured output contains the text.
    /// </summary>
    [[nodiscard]] inline bool output_contains(std::string_view output, std::string_view needle) {
        return output.find(needle) != std::string_view::npos;
    }

    /// <summary>
    /// The number of cases doctest selected, from its summary line ("[doctest] test cases:  3 |  3 passed |"),
    /// or nullopt when the output has no such line. A -tc filter that matched nothing would otherwise pass as
    /// an empty run.
    /// </summary>
    [[nodiscard]] inline std::optional<u32> selected_case_count(std::string_view output) {
        constexpr std::string_view CASE_SUMMARY = "[doctest] test cases:";
        const usize summary = output.find(CASE_SUMMARY);
        if (summary == std::string_view::npos) {
            return std::nullopt;
        }
        usize begin = summary + CASE_SUMMARY.size();
        while (begin < output.size() && output[begin] == ' ') {
            ++begin;
        }
        u32 count = 0;
        const std::from_chars_result parsed =
          std::from_chars(output.data() + begin, output.data() + output.size(), count);
        if (parsed.ec != std::errc()) {
            return std::nullopt;
        }
        return count;
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
