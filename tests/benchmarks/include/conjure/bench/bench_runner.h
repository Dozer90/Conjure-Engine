#pragma once

#include "conjure/bench/benchmark.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstdio>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// The benchmark runner (docs/design/08_testing.md 3.6, 4.7): loads thresholds.json, runs the scenarios
// that match the name filter, prints one machine-readable line per scenario (and per reported phase) and
// decides the exit code.
//
//   conjure_benchmarks [--class desktop|ci] [--enforce|--no-enforce] [--filter <glob>] [--out <file>]
//                      [--threads N|auto] [--thresholds <file>] [--iterations N] [--warmup N] [--entities N]
//                      [--seed N] [--list] [--help]
//
// --threads follows the engine's jobs_settings: 0 runs every job inline on the calling thread (the mode
// the determinism tests use), auto (the default) is hardware_concurrency - 1 clamped to [1, 16].
//
// Report line (one per scenario, phase=total; one more per reported sub-phase):
//   benchmark name=<name> phase=<phase> status=<status> iterations=<n> items=<n> threads=<n>
//       min_ms=<f> median_ms=<f> p95_ms=<f> p99_ms=<f> max_ms=<f> allocs=<n> [threshold_p50_ms=<f> ...]
//       [reason="<text>"]
// The reason (SKIPPED and FAILED rows) is double-quoted with '"' and '\' escaped by a backslash.
//
// Exit codes: 0 every selected scenario passed (or only threshold verdicts failed and thresholds are not
// enforced); 1 a scenario failed (bench_context::fail, or a return without measure() or skip()) whether
// or not thresholds are enforced, or under --enforce a threshold is breached or a scenario is
// unthresholded; 2 bad command line or no scenario matched; 77 every selected scenario skipped (CTest's
// SKIP_RETURN_CODE).

namespace conjure::bench {

    /// <summary>
    /// Exit code of a run in which every selected scenario skipped (the CTest skip convention).
    /// </summary>
    inline constexpr int BENCH_EXIT_SKIPPED = 77;

    /// <summary>
    /// Exit code of a bad command line or an empty selection.
    /// </summary>
    inline constexpr int BENCH_EXIT_USAGE = 2;

    /// <summary>
    /// Exit code of a scenario failure (always) or of a breached threshold or an unthresholded scenario
    /// under --enforce.
    /// </summary>
    inline constexpr int BENCH_EXIT_FAILED = 1;

    /// <summary>
    /// The thresholds of one scenario for both machine classes (one thresholds.json entry).
    /// </summary>
    struct scenario_thresholds {
        bench_threshold desktop = {};
        bench_threshold ci = {};

        /// <summary>
        /// The threshold of a class.
        /// </summary>
        [[nodiscard]] const bench_threshold &of(machine_class value) const noexcept {
            return value == machine_class::CI ? ci : desktop;
        }
    };

    /// <summary>
    /// Parses the command line described in the header comment over 'defaults'. INVALID_ARGUMENT names
    /// the offending option; --help sets showHelp and parses nothing else.
    /// </summary>
    [[nodiscard]] core::result<bench_options> parse_command_line(
      std::span<const std::string_view> arguments,
      const bench_options &defaults
    );

    /// <summary>
    /// The usage text printed for --help and after a command line error.
    /// </summary>
    [[nodiscard]] std::string_view usage_text() noexcept;

    /// <summary>
    /// True when 'name' matches the glob 'pattern' ('*' any run, '?' one character, case-sensitive).
    /// </summary>
    [[nodiscard]] bool matches_filter(std::string_view name, std::string_view pattern) noexcept;

    /// <summary>
    /// Parses "desktop" / "ci" (case-insensitive). INVALID_ARGUMENT otherwise.
    /// </summary>
    [[nodiscard]] core::result<machine_class> parse_machine_class(std::string_view text);

    class bench_runner {
        private:
            bench_options mOptions = {};
            std::unordered_map<std::string, scenario_thresholds> mThresholds = {};
            std::vector<bench_result> mResults = {};
            std::FILE *mOutput = stdout;

        public:
            explicit bench_runner(const bench_options &options);
            ~bench_runner();

            bench_runner(const bench_runner &) = delete;
            bench_runner &operator=(const bench_runner &) = delete;

        public:
            /// <summary>
            /// Reads a thresholds.json file ({"version": 1, "scenarios": {"<name>": {"desktop": {"p50_ms",
            /// "p95_ms", "p99_ms"}, "ci": {...}, "allocs": n}}}). IO_FAILURE, PARSE_FAILURE, or
            /// INVALID_ARGUMENT naming the malformed member; an entry or class member outside that set (a
            /// misspelled "p50ms" or "dekstop") is malformed too. Replaces the thresholds held so far.
            /// </summary>
            [[nodiscard]] core::result<void> load_thresholds(const std::filesystem::path &path);

            /// <summary>
            /// load_thresholds from text (tests and embedded defaults), with the same validation: unknown
            /// members of an entry or of a class object are INVALID_ARGUMENT. A class whose times are all 0
            /// or absent, in an entry without an "allocs" gate, sets no limit, so run() reports the scenario
            /// UNTHRESHOLDED for that class (an "allocs" gate alone is a limit for both classes).
            /// </summary>
            [[nodiscard]] core::result<void> load_thresholds_text(std::string_view text);

            /// <summary>
            /// The thresholds of a scenario, or nullptr when the file has no entry.
            /// </summary>
            [[nodiscard]] const scenario_thresholds *thresholds_of(std::string_view name) const noexcept;

            /// <summary>
            /// Number of scenarios the thresholds file describes.
            /// </summary>
            [[nodiscard]] usize threshold_count() const noexcept {
                return mThresholds.size();
            }

            /// <summary>
            /// Runs one scenario in a fresh bench_context, summarises it against the loaded thresholds,
            /// prints its report lines and keeps the result. A scenario that skipped is SKIPPED; one that
            /// returned without measuring, or called fail(), is FAILED with the reason and scenarioFailed
            /// set, whatever its figures. An entry that sets no limit for the selected machine class counts
            /// as no entry: the row is UNTHRESHOLDED.
            /// </summary>
            bench_result run(const bench_descriptor &descriptor);

            /// <summary>
            /// Runs every descriptor whose name matches the filter and returns the exit code described
            /// in the header comment. Afterwards prints "benchmark: thresholds entry '&lt;name&gt;' matches
            /// no registered scenario" for every loaded entry that names none of 'descriptors' (all of
            /// them, not only the selected ones), sorted by name; the warning leaves the exit code alone.
            /// </summary>
            [[nodiscard]] int run_all(std::span<const bench_descriptor> descriptors);

            /// <summary>
            /// The exit code the results collected so far yield (what run_all returns): 2 without results,
            /// 77 when every one skipped, 1 when a scenario failed (returned without measure() or skip(),
            /// or called fail()) in every configuration, --no-enforce included, 1 under --enforce when a
            /// threshold is breached or a scenario is unthresholded, 0 otherwise.
            /// </summary>
            [[nodiscard]] int exit_code() const noexcept;

            /// <summary>
            /// Writes every result as JSON ({"version": 1, "class": "desktop", "enforce": b, "iterations":
            /// n, "warmup": n, "results": [{"name", "status", "iterations", "threads", "items", the
            /// statistics, "allocs", "threshold"?, "reason"?, "phases"}]}) to a file, creating parent
            /// directories. IO_FAILURE from the file system.
            /// </summary>
            [[nodiscard]] core::result<void> write_json(const std::filesystem::path &path) const;

            /// <summary>
            /// The JSON text write_json writes.
            /// </summary>
            [[nodiscard]] std::string to_json() const;

            /// <summary>
            /// The results of every run so far, in run order.
            /// </summary>
            [[nodiscard]] std::span<const bench_result> results() const noexcept {
                return mResults;
            }

            [[nodiscard]] const bench_options &options() const noexcept {
                return mOptions;
            }

            /// <summary>
            /// Where the report lines go (stdout by default; tests redirect).
            /// </summary>
            void set_output(std::FILE *output) noexcept {
                mOutput = output == nullptr ? stdout : output;
            }

            /// <summary>
            /// Builds a result row from samples and an allocation count: the percentiles of 3.6, then the
            /// verdict. Without a threshold the row is UNTHRESHOLDED; otherwise PASSED when every set
            /// metric is at or below its limit and the allocations are within the limit, FAILED otherwise.
            /// </summary>
            [[nodiscard]] static bench_result summarise(
              std::string_view name,
              std::span<const f64> samplesMilliseconds,
              u64 maxAllocations,
              const bench_threshold *threshold
            );

            /// <summary>
            /// Formats the report line of a result (phase "total") or of one of its phases.
            /// </summary>
            [[nodiscard]] static std::string format_line(const bench_result &result);
            [[nodiscard]] static std::string format_phase_line(
              const bench_result &result,
              const bench_phase_result &phase
            );

        private:
            void print(const bench_result &result) const;
            void warn_unmatched_thresholds(std::span<const bench_descriptor> descriptors) const;
    };
}
