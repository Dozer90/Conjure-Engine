#include "conjure/testing/doctest.h"

#include "conjure/testing/temp_directory.h"

#include "conjure/bench/bench_runner.h"
#include "conjure/bench/benchmark.h"
#include "conjure/core/error.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/types.h"

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// The runner (docs/design/08_testing.md 3.6, 4.7, 11): the command line, the name filter, thresholds.json,
// summarise's verdicts, the exit codes of a run and the JSON report.

using namespace conjure;
using namespace conjure::bench;
using namespace conjure::testing;

namespace {

    constexpr std::string_view THRESHOLDS_TEXT =
      "{\"version\": 1, \"scenarios\": {"
      "  \"ecs.fast\": {\"desktop\": {\"p50_ms\": 1.0, \"p99_ms\": 2.0}, \"ci\": {\"p50_ms\": 3.0}, \"allocs\": 0},"
      "  \"scene.loose\": {\"desktop\": {\"p50_ms\": 100.0}}"
      "}}";

    [[nodiscard]] bench_options quick_options() {
        bench_options options = {};
        options.warmupIterations = 1;
        options.measuredIterations = 5;
        options.jobThreadCount = 1;
        options.enforceThresholds = true;
        return options;
    }

    [[nodiscard]] core::result<bench_options> parse(std::initializer_list<std::string_view> arguments) {
        const std::vector<std::string_view> list(arguments);
        return parse_command_line(list, bench_options{});
    }

    void fast_scenario(bench_context &context) {
        context.set_items(3);
        context.measure([]() {});
    }

    void allocating_scenario(bench_context &context) {
        context.measure([]() {
            std::vector<u32> grown = {};
            grown.push_back(1);
        });
    }

    void skipping_scenario(bench_context &context) {
        context.skip("precondition unmet");
    }

    /// <summary>
    /// The forgotten-skip shape: an early return before measure() without a skip.
    /// </summary>
    void forgetful_scenario(bench_context &context) {
        context.set_items(3);
    }

    /// <summary>
    /// Measures within every threshold, then reports a failure the figures cannot show.
    /// </summary>
    void failing_scenario(bench_context &context) {
        context.measure([]() {});
        context.fail("frame 3 returned an error: path \"C:\\scratch\"");
        context.fail("second reason is ignored");
    }

    void phased_scenario(bench_context &context) {
        context.measure([]() {});
        const std::array<f64, 3> samples = {0.5, 0.25, 0.75};
        context.add_phase("UPDATE", samples);
    }

    /// <summary>
    /// A runner whose report lines land in a temporary file instead of stdout.
    /// </summary>
    class quiet_runner {
        private:
            temp_directory mDirectory = {};
            std::FILE *mFile = nullptr;

        public:
            bench_runner runner;    // no default: built from options

        public:
            explicit quiet_runner(const bench_options &options) :
                runner(options) {
                const std::string path = mDirectory.file("report.txt").string();
                mFile = std::fopen(path.c_str(), "wb");
                REQUIRE(mFile != nullptr);
                runner.set_output(mFile);
            }

            ~quiet_runner() {
                if (mFile != nullptr) {
                    std::fclose(mFile);
                }
            }

            quiet_runner(const quiet_runner &) = delete;
            quiet_runner &operator=(const quiet_runner &) = delete;

        public:
            /// <summary>
            /// Everything printed so far.
            /// </summary>
            [[nodiscard]] std::string output() {
                std::fflush(mFile);
                std::ifstream stream(mDirectory.file("report.txt"), std::ios::binary);
                std::ostringstream text = {};
                text << stream.rdbuf();
                return text.str();
            }

            [[nodiscard]] const temp_directory &directory() const noexcept {
                return mDirectory;
            }
    };
}

TEST_SUITE("benchmarks/bench_runner") {

    TEST_CASE("parse_command_line - every option sets its field and the defaults survive") {
        const core::result<bench_options> parsed = parse({
          "--class", "ci", "--enforce", "--filter", "ecs.*", "--out", "results.json", "--threads", "4",
          "--thresholds", "limits.json", "--iterations", "50", "--warmup", "5", "--entities", "2000", "--seed", "0x10"
        });
        CONJURE_REQUIRE_OK(parsed);
        const bench_options &options = parsed.value();
        CHECK(options.machineClass == machine_class::CI);
        CHECK(options.enforceThresholds);
        CHECK(options.nameFilter == "ecs.*");
        CHECK(options.outputFile == std::filesystem::path("results.json"));
        CHECK(options.jobThreadCount == 4);
        CHECK(options.thresholdsFile == std::filesystem::path("limits.json"));
        CHECK(options.measuredIterations == 50);
        CHECK(options.warmupIterations == 5);
        CHECK(options.entityCount == 2000);
        CHECK(options.seed == 16);
        CHECK_FALSE(options.listOnly);
        CHECK_FALSE(options.showHelp);

        const core::result<bench_options> untouched = parse({});
        CONJURE_REQUIRE_OK(untouched);
        CHECK(untouched.value().nameFilter == "*");
        CHECK(untouched.value().measuredIterations == 200);
        CHECK(untouched.value().warmupIterations == 20);
        CHECK(untouched.value().entityCount == 10000);
        CHECK(untouched.value().machineClass == machine_class::DESKTOP);
        CHECK(untouched.value().jobThreadCount == AUTO_JOB_THREAD_COUNT);
    }

    TEST_CASE("parse_command_line - --threads 0 is inline jobs and auto is the automatic count, as --jobs") {
        const core::result<bench_options> inlineJobs = parse({"--threads", "0"});
        CONJURE_REQUIRE_OK(inlineJobs);
        CHECK(inlineJobs.value().jobThreadCount == 0);
        bench_context context(inlineJobs.value());
        CHECK(context.job_thread_count() == 0);

        bench_options fixed = {};
        fixed.jobThreadCount = 4;
        const std::vector<std::string_view> automatic = {"--threads", "auto"};
        const core::result<bench_options> parsed = parse_command_line(automatic, fixed);
        CONJURE_REQUIRE_OK(parsed);
        CHECK(parsed.value().jobThreadCount == AUTO_JOB_THREAD_COUNT);
        CHECK(bench_context(parsed.value()).job_thread_count() >= 1);
        CONJURE_CHECK_ERROR(parse({"--threads", "AUTO"}), core::error_code::INVALID_ARGUMENT);
    }

    TEST_CASE("parse_command_line - no-enforce, list and help are flags and help stops parsing") {
        bench_options enforcing = {};
        enforcing.enforceThresholds = true;
        const std::vector<std::string_view> relaxed = {"--no-enforce", "--list"};
        const core::result<bench_options> parsed = parse_command_line(relaxed, enforcing);
        CONJURE_REQUIRE_OK(parsed);
        CHECK_FALSE(parsed.value().enforceThresholds);
        CHECK(parsed.value().listOnly);

        const core::result<bench_options> help = parse({"--help", "--bogus"});
        CONJURE_REQUIRE_OK(help);
        CHECK(help.value().showHelp);
        CHECK_FALSE(usage_text().empty());
    }

    TEST_CASE("parse_command_line - bad values and unknown options are INVALID_ARGUMENT naming the option") {
        CONJURE_CHECK_ERROR(parse({"--bogus"}), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(parse({"--filter"}), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(parse({"--filter", ""}), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(parse({"--class", "laptop"}), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(parse({"--threads", "x"}), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(parse({"--threads", "257"}), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(parse({"--iterations", "0"}), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(parse({"--warmup", "-1"}), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(parse({"--entities", "0"}), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(parse({"--seed", "abc"}), core::error_code::INVALID_ARGUMENT);
        const core::result<bench_options> failed = parse({"--iterations", "many"});
        REQUIRE_FALSE(failed.has_value());
        CHECK(failed.get_error().message().find("--iterations") != std::string::npos);
    }

    TEST_CASE("parse_machine_class - accepts both names case-insensitively") {
        CHECK(parse_machine_class("desktop").value() == machine_class::DESKTOP);
        CHECK(parse_machine_class("CI").value() == machine_class::CI);
        CHECK(parse_machine_class("Desktop").value() == machine_class::DESKTOP);
        CONJURE_CHECK_ERROR(parse_machine_class("server"), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(parse_machine_class(""), core::error_code::INVALID_ARGUMENT);
    }

    TEST_CASE("matches_filter - star and question mark globs, exact otherwise") {
        CHECK(matches_filter("ecs.iterate_100k", "*"));
        CHECK(matches_filter("ecs.iterate_100k", "ecs.*"));
        CHECK(matches_filter("ecs.iterate_100k", "*100k"));
        CHECK(matches_filter("ecs.iterate_100k", "ecs.iterate_100k"));
        CHECK(matches_filter("ecs.iterate_100k", "ecs.iterate_1?0k"));
        CHECK(matches_filter("ecs.iterate_100k", "*.*_*"));
        CHECK(matches_filter("", "*"));
        CHECK(matches_filter("", ""));
        CHECK_FALSE(matches_filter("ecs.iterate_100k", "scene.*"));
        CHECK_FALSE(matches_filter("ecs.iterate_100k", "ecs.iterate"));
        CHECK_FALSE(matches_filter("ecs.iterate_100k", "ecs.iterate_100k?"));
        CHECK_FALSE(matches_filter("ecs.iterate_100k", "ECS.*"));
        CHECK_FALSE(matches_filter("abc", ""));
    }

    TEST_CASE("bench_runner - load_thresholds_text reads both classes and the shared allocation limit") {
        bench_runner runner(quick_options());
        CONJURE_REQUIRE_OK(runner.load_thresholds_text(THRESHOLDS_TEXT));
        CHECK(runner.threshold_count() == 2);
        const scenario_thresholds *fast = runner.thresholds_of("ecs.fast");
        REQUIRE(fast != nullptr);
        CHECK(fast->desktop.p50Milliseconds == 1.0);
        CHECK(fast->desktop.p95Milliseconds == 0.0);
        CHECK(fast->desktop.p99Milliseconds == 2.0);
        CHECK(fast->desktop.maxAllocationsPerIteration == 0);
        CHECK(fast->ci.p50Milliseconds == 3.0);
        CHECK(fast->ci.maxAllocationsPerIteration == 0);
        CHECK(&fast->of(machine_class::CI) == &fast->ci);
        CHECK(&fast->of(machine_class::DESKTOP) == &fast->desktop);
        const scenario_thresholds *loose = runner.thresholds_of("scene.loose");
        REQUIRE(loose != nullptr);
        CHECK(loose->desktop.maxAllocationsPerIteration == UNLIMITED_ALLOCATIONS);
        CHECK(loose->ci.p50Milliseconds == 0.0);     // no ci object: the CI class is unthresholded
        CHECK(runner.thresholds_of("missing") == nullptr);
    }

    TEST_CASE("bench_runner - malformed thresholds are rejected and leave the previous set in place") {
        bench_runner runner(quick_options());
        CONJURE_REQUIRE_OK(runner.load_thresholds_text(THRESHOLDS_TEXT));
        CONJURE_CHECK_ERROR(runner.load_thresholds_text("not json"), core::error_code::PARSE_FAILURE);
        CONJURE_CHECK_ERROR(runner.load_thresholds_text("[]"), core::error_code::INVALID_ARGUMENT);
        CONJURE_CHECK_ERROR(
          runner.load_thresholds_text("{\"version\": 2, \"scenarios\": {}}"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          runner.load_thresholds_text("{\"version\": 1, \"scenarios\": []}"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          runner.load_thresholds_text("{\"version\": 1, \"scenarios\": {\"a.b\": 5}}"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          runner.load_thresholds_text("{\"version\": 1, \"scenarios\": {\"a.b\": {\"desktop\": 5}}}"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          runner.load_thresholds_text("{\"version\": 1, \"scenarios\": {\"a.b\": {\"desktop\": {\"p50_ms\": -1}}}}"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          runner.load_thresholds_text("{\"version\": 1, \"scenarios\": {\"a.b\": {\"allocs\": 1.5}}}"),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          runner.load_thresholds_text("{\"version\": 1, \"scenarios\": {\"a.b\": {\"allocs\": -3}}}"),
          core::error_code::INVALID_ARGUMENT
        );
        CHECK(runner.threshold_count() == 2);
        CHECK(runner.thresholds_of("ecs.fast") != nullptr);

        // A missing file is an IO failure.
        temp_directory directory = {};
        CONJURE_CHECK_ERROR(runner.load_thresholds(directory.file("absent.json")), core::error_code::IO_FAILURE);
        CONJURE_REQUIRE_OK(directory.write_text("limits.json", THRESHOLDS_TEXT));
        CONJURE_CHECK_OK(runner.load_thresholds(directory.file("limits.json")));
        CHECK(runner.threshold_count() == 2);
    }

    TEST_CASE("bench_runner - summarise computes the percentiles and judges every set metric") {
        const std::vector<f64> samples = {0.5, 0.9, 0.7, 0.6, 0.8};   // sorted: 0.5 0.6 0.7 0.8 0.9
        bench_threshold threshold = {};
        threshold.p50Milliseconds = 0.7;
        threshold.p99Milliseconds = 0.8;
        threshold.maxAllocationsPerIteration = 0;

        const bench_result passed = bench_runner::summarise("ecs.fast", samples, 0, &threshold);
        CHECK(passed.name == "ecs.fast");
        CHECK(passed.iterations == 5);
        CHECK(passed.statistics.minMilliseconds == 0.5);
        CHECK(passed.statistics.p50Milliseconds == 0.7);
        CHECK(passed.statistics.p95Milliseconds == 0.8);
        CHECK(passed.statistics.p99Milliseconds == 0.8);
        CHECK(passed.statistics.maxMilliseconds == 0.9);
        CHECK(passed.hasThreshold);
        CHECK(passed.threshold == threshold);
        CHECK(passed.status == bench_status::PASSED);

        SUBCASE("a median above the limit fails") {
            threshold.p50Milliseconds = 0.69;
            CHECK(bench_runner::summarise("ecs.fast", samples, 0, &threshold).status == bench_status::FAILED);
        }
        SUBCASE("a p95 above the limit fails") {
            threshold.p95Milliseconds = 0.79;
            CHECK(bench_runner::summarise("ecs.fast", samples, 0, &threshold).status == bench_status::FAILED);
        }
        SUBCASE("a p99 above the limit fails") {
            threshold.p99Milliseconds = 0.79;
            CHECK(bench_runner::summarise("ecs.fast", samples, 0, &threshold).status == bench_status::FAILED);
        }
        SUBCASE("one allocation too many fails and an unlimited gate never does") {
            CHECK(bench_runner::summarise("ecs.fast", samples, 1, &threshold).status == bench_status::FAILED);
            threshold.maxAllocationsPerIteration = UNLIMITED_ALLOCATIONS;
            CHECK(bench_runner::summarise("ecs.fast", samples, 1000, &threshold).status == bench_status::PASSED);
        }
        SUBCASE("a zero time limit is not checked") {
            threshold.p50Milliseconds = 0.0;
            threshold.p99Milliseconds = 0.0;
            CHECK(bench_runner::summarise("ecs.fast", samples, 0, &threshold).status == bench_status::PASSED);
        }
        SUBCASE("without a threshold the row is UNTHRESHOLDED") {
            const bench_result loose = bench_runner::summarise("ecs.fast", samples, 5, nullptr);
            CHECK(loose.status == bench_status::UNTHRESHOLDED);
            CHECK_FALSE(loose.hasThreshold);
            CHECK(loose.allocationsPerIteration == 5);
        }
    }

    TEST_CASE("bench_runner - run reports status items threads and phases and prints the lines") {
        quiet_runner quiet(quick_options());
        CONJURE_REQUIRE_OK(quiet.runner.load_thresholds_text(THRESHOLDS_TEXT));
        const bench_result fast = quiet.runner.run(bench_descriptor{"ecs.fast", &fast_scenario});
        CHECK(fast.status == bench_status::PASSED);
        CHECK(fast.iterations == 5);
        CHECK(fast.itemsPerIteration == 3);
        CHECK(fast.jobThreadCount == 1);
        CHECK(fast.allocationsPerIteration == 0);

        const bench_result phased = quiet.runner.run(bench_descriptor{"scene.loose", &phased_scenario});
        CHECK(phased.status == bench_status::PASSED);
        REQUIRE(phased.phases.size() == 1);
        CHECK(phased.phases[0].name == "UPDATE");
        CHECK(phased.phases[0].statistics.p50Milliseconds == 0.5);

        const bench_result skipped = quiet.runner.run(bench_descriptor{"ecs.fast", &skipping_scenario});
        CHECK(skipped.status == bench_status::SKIPPED);
        CHECK(skipped.reason == "precondition unmet");
        CHECK(skipped.iterations == 0);
        CHECK(skipped.hasThreshold);

        const bench_result unknown = quiet.runner.run(bench_descriptor{"other.scenario", &fast_scenario});
        CHECK(unknown.status == bench_status::UNTHRESHOLDED);
        CHECK(quiet.runner.results().size() == 4);

        const std::string output = quiet.output();
        const std::string_view fastPrefix =
          "benchmark name=ecs.fast phase=total status=PASSED iterations=5 items=3 threads=1 min_ms=";
        CHECK(output.find(fastPrefix) != std::string::npos);
        CHECK(output.find("median_ms=") != std::string::npos);
        CHECK(output.find("p99_ms=") != std::string::npos);
        const std::string_view fastThresholds =
          "allocs=0 threshold_p50_ms=1.0000 threshold_p99_ms=2.0000 threshold_allocs=0";
        CHECK(output.find(fastThresholds) != std::string::npos);
        CHECK(output.find("benchmark name=scene.loose phase=UPDATE status=PASSED") != std::string::npos);
        CHECK(output.find("status=SKIPPED") != std::string::npos);
        CHECK(output.find("reason=\"precondition unmet\"") != std::string::npos);
        CHECK(output.find("status=UNTHRESHOLDED") != std::string::npos);
    }

    TEST_CASE("bench_runner - a scenario that neither measured nor skipped is FAILED and fails the run") {
        quiet_runner quiet(quick_options());
        CONJURE_REQUIRE_OK(quiet.runner.load_thresholds_text(THRESHOLDS_TEXT));
        const bench_result forgotten = quiet.runner.run(bench_descriptor{"ecs.fast", &forgetful_scenario});
        CHECK(forgotten.status == bench_status::FAILED);
        CHECK(forgotten.iterations == 0);
        CHECK(forgotten.itemsPerIteration == 3);
        CHECK(forgotten.hasThreshold);
        CHECK(forgotten.reason == "the scenario returned without measure() or skip()");
        CHECK(quiet.runner.exit_code() == BENCH_EXIT_FAILED);
        CHECK(quiet.output().find("status=FAILED") != std::string::npos);
        CHECK(quiet.output().find("reason=\"the scenario returned without measure() or skip()\"") != std::string::npos);

        // A null function is the same shape: nothing ran, so nothing may pass.
        const bench_result absent = quiet.runner.run(bench_descriptor{"scene.loose", nullptr});
        CHECK(absent.status == bench_status::FAILED);
        CHECK(absent.iterations == 0);

        // Unthresholded scenarios are no exception: a FAILED row, not an UNTHRESHOLDED one.
        const bench_result unknown = quiet.runner.run(bench_descriptor{"other.scenario", &forgetful_scenario});
        CHECK(unknown.status == bench_status::FAILED);
        CHECK_FALSE(unknown.hasThreshold);
    }

    TEST_CASE("bench_runner - fail marks a measured scenario FAILED with its first reason whatever the figures") {
        quiet_runner quiet(quick_options());
        CONJURE_REQUIRE_OK(quiet.runner.load_thresholds_text(THRESHOLDS_TEXT));
        const bench_result failed = quiet.runner.run(bench_descriptor{"ecs.fast", &failing_scenario});
        CHECK(failed.status == bench_status::FAILED);
        CHECK(failed.iterations == 5);
        CHECK(failed.hasThreshold);
        CHECK(failed.allocationsPerIteration == 0);
        CHECK(failed.reason == "frame 3 returned an error: path \"C:\\scratch\"");
        CHECK(quiet.runner.exit_code() == BENCH_EXIT_FAILED);
        // The printed reason is escaped so the line stays parseable.
        const std::string_view escaped = "reason=\"frame 3 returned an error: path \\\"C:\\\\scratch\\\"\"";
        CHECK(quiet.output().find(escaped) != std::string::npos);
    }

    TEST_CASE("bench_runner - an allocating body fails an exact zero gate and passes without enforcement") {
        quiet_runner quiet(quick_options());
        CONJURE_REQUIRE_OK(quiet.runner.load_thresholds_text(THRESHOLDS_TEXT));
        const bench_result failed = quiet.runner.run(bench_descriptor{"ecs.fast", &allocating_scenario});
        CHECK(failed.status == bench_status::FAILED);
        CHECK(failed.allocationsPerIteration >= 1);
        CHECK(quiet.runner.exit_code() == BENCH_EXIT_FAILED);

        bench_options relaxed = quick_options();
        relaxed.enforceThresholds = false;
        quiet_runner lenient(relaxed);
        CONJURE_REQUIRE_OK(lenient.runner.load_thresholds_text(THRESHOLDS_TEXT));
        const bench_result breached = lenient.runner.run(bench_descriptor{"ecs.fast", &allocating_scenario});
        CHECK(breached.status == bench_status::FAILED);
        CHECK_FALSE(breached.scenarioFailed);       // a threshold verdict, which only enforcement turns into exit 1
        CHECK(lenient.runner.exit_code() == 0);
    }

    TEST_CASE("bench_runner - a scenario failure fails the run with enforcement off") {
        bench_options relaxed = quick_options();
        relaxed.enforceThresholds = false;

        // The forgotten-skip shape.
        quiet_runner forgotten(relaxed);
        CONJURE_REQUIRE_OK(forgotten.runner.load_thresholds_text(THRESHOLDS_TEXT));
        const bench_result unmeasured = forgotten.runner.run(bench_descriptor{"ecs.fast", &forgetful_scenario});
        CHECK(unmeasured.status == bench_status::FAILED);
        CHECK(unmeasured.scenarioFailed);
        CHECK(forgotten.runner.exit_code() == BENCH_EXIT_FAILED);

        // A measured scenario that called fail() with figures inside every limit.
        quiet_runner failing(relaxed);
        CONJURE_REQUIRE_OK(failing.runner.load_thresholds_text(THRESHOLDS_TEXT));
        const bench_result failed = failing.runner.run(bench_descriptor{"ecs.fast", &failing_scenario});
        CHECK(failed.status == bench_status::FAILED);
        CHECK(failed.scenarioFailed);
        CHECK(failing.runner.exit_code() == BENCH_EXIT_FAILED);

        // run_all: a passing and a skipped scenario around the failure do not hide it, and neither does an
        // unthresholded scenario, which without enforcement alone would exit 0.
        const std::array<bench_descriptor, 4> descriptors = {
          bench_descriptor{"ecs.fast", &fast_scenario},
          bench_descriptor{"other.unlisted", &fast_scenario},
          bench_descriptor{"scene.loose", &failing_scenario},
          bench_descriptor{"scripting.skipped", &skipping_scenario},
        };
        quiet_runner mixed(relaxed);
        CONJURE_REQUIRE_OK(mixed.runner.load_thresholds_text(THRESHOLDS_TEXT));
        CHECK(mixed.runner.run_all(descriptors) == BENCH_EXIT_FAILED);

        bench_options unthresholdedOnly = relaxed;
        unthresholdedOnly.nameFilter = "other.*";
        quiet_runner unthresholded(unthresholdedOnly);
        CONJURE_REQUIRE_OK(unthresholded.runner.load_thresholds_text(THRESHOLDS_TEXT));
        CHECK(unthresholded.runner.run_all(descriptors) == 0);
    }

    TEST_CASE("bench_runner - a selected class without any limit is UNTHRESHOLDED and fails under enforcement") {
        // scene.loose has no ci object.
        bench_options ciOptions = quick_options();
        ciOptions.machineClass = machine_class::CI;
        quiet_runner quiet(ciOptions);
        CONJURE_REQUIRE_OK(quiet.runner.load_thresholds_text(THRESHOLDS_TEXT));
        const bench_result loose = quiet.runner.run(bench_descriptor{"scene.loose", &fast_scenario});
        CHECK(loose.status == bench_status::UNTHRESHOLDED);
        CHECK_FALSE(loose.hasThreshold);
        CHECK_FALSE(loose.scenarioFailed);
        CHECK(quiet.runner.exit_code() == BENCH_EXIT_FAILED);
        CHECK(quiet.output().find("benchmark name=scene.loose phase=total status=UNTHRESHOLDED") != std::string::npos);

        // Every time 0 without an allocation gate limits nothing either; an allocation gate alone does.
        constexpr std::string_view LIMITLESS_TEXT =
          "{\"version\": 1, \"scenarios\": {"
          "  \"a.zero\": {\"desktop\": {\"p50_ms\": 0, \"p95_ms\": 0, \"p99_ms\": 0}, \"ci\": {}},"
          "  \"a.gated\": {\"allocs\": 0}"
          "}}";
        quiet_runner desktop(quick_options());
        CONJURE_REQUIRE_OK(desktop.runner.load_thresholds_text(LIMITLESS_TEXT));
        const bench_result zero = desktop.runner.run(bench_descriptor{"a.zero", &fast_scenario});
        CHECK(zero.status == bench_status::UNTHRESHOLDED);
        CHECK_FALSE(zero.hasThreshold);
        const bench_result gated = desktop.runner.run(bench_descriptor{"a.gated", &fast_scenario});
        CHECK(gated.status == bench_status::PASSED);
        REQUIRE(gated.hasThreshold);
        CHECK(gated.threshold.maxAllocationsPerIteration == 0);
        CHECK(desktop.runner.exit_code() == BENCH_EXIT_FAILED);

        // Without enforcement an unthresholded row is reported, not failed.
        bench_options relaxed = ciOptions;
        relaxed.enforceThresholds = false;
        quiet_runner lenient(relaxed);
        CONJURE_REQUIRE_OK(lenient.runner.load_thresholds_text(THRESHOLDS_TEXT));
        const bench_result reported = lenient.runner.run(bench_descriptor{"scene.loose", &fast_scenario});
        CHECK(reported.status == bench_status::UNTHRESHOLDED);
        CHECK(lenient.runner.exit_code() == 0);
    }

    TEST_CASE("bench_runner - unknown members of a thresholds entry are rejected") {
        bench_runner runner(quick_options());
        CONJURE_REQUIRE_OK(runner.load_thresholds_text(THRESHOLDS_TEXT));

        // A misspelled limit inside a class object.
        const core::result<void> limit =
          runner.load_thresholds_text("{\"version\": 1, \"scenarios\": {\"a.b\": {\"desktop\": {\"p50ms\": 1.0}}}}");
        REQUIRE_FALSE(limit.has_value());
        CHECK(limit.get_error().is(core::error_code::INVALID_ARGUMENT));
        CHECK(limit.get_error().message().find("'p50ms'") != std::string::npos);
        CHECK(limit.get_error().message().find("'desktop'") != std::string::npos);

        // A misspelled class name, and a misspelled allocation gate.
        const core::result<void> machine =
          runner.load_thresholds_text("{\"version\": 1, \"scenarios\": {\"a.b\": {\"dekstop\": {\"p50_ms\": 1.0}}}}");
        REQUIRE_FALSE(machine.has_value());
        CHECK(machine.get_error().is(core::error_code::INVALID_ARGUMENT));
        CHECK(machine.get_error().message().find("'dekstop'") != std::string::npos);
        CONJURE_CHECK_ERROR(
          runner.load_thresholds_text(
            "{\"version\": 1, \"scenarios\": {\"a.b\": {\"desktop\": {\"p50_ms\": 1.0}, \"alloc\": 0}}}"
          ),
          core::error_code::INVALID_ARGUMENT
        );

        // The rejected files left the previous set in place.
        CHECK(runner.threshold_count() == 2);
        CHECK(runner.thresholds_of("a.b") == nullptr);
    }

    TEST_CASE("bench_runner - run_all names thresholds entries that match no scenario") {
        constexpr std::string_view STALE_TEXT =
          "{\"version\": 1, \"scenarios\": {"
          "  \"zeta.renamed\": {\"desktop\": {\"p50_ms\": 1.0}},"
          "  \"ecs.fast\": {\"desktop\": {\"p50_ms\": 1.0}},"
          "  \"audio.removed\": {\"desktop\": {\"p50_ms\": 1.0}},"
          "  \"scene.loose\": {\"desktop\": {\"p50_ms\": 100.0}}"
          "}}";
        const std::array<bench_descriptor, 2> descriptors = {
          bench_descriptor{"ecs.fast", &fast_scenario},
          bench_descriptor{"scene.loose", &fast_scenario},
        };

        // scene.loose is registered but not selected: its entry is not stale.
        bench_options options = quick_options();
        options.nameFilter = "ecs.*";
        quiet_runner quiet(options);
        CONJURE_REQUIRE_OK(quiet.runner.load_thresholds_text(STALE_TEXT));
        CHECK(quiet.runner.run_all(descriptors) == 0);

        const std::string output = quiet.output();
        const usize audio = output.find("benchmark: thresholds entry 'audio.removed' matches no registered scenario\n");
        const usize zeta = output.find("benchmark: thresholds entry 'zeta.renamed' matches no registered scenario\n");
        REQUIRE(audio != std::string::npos);
        REQUIRE(zeta != std::string::npos);
        CHECK(audio < zeta);
        CHECK(output.find("entry 'ecs.fast'") == std::string::npos);
        CHECK(output.find("entry 'scene.loose'") == std::string::npos);

        // Every entry matched: no warning at all.
        quiet_runner clean(options);
        CONJURE_REQUIRE_OK(clean.runner.load_thresholds_text(THRESHOLDS_TEXT));
        CHECK(clean.runner.run_all(descriptors) == 0);
        CHECK(clean.output().find("matches no registered scenario") == std::string::npos);
    }

    TEST_CASE("bench_runner - run_all filters by name and yields the documented exit codes") {
        const std::array<bench_descriptor, 3> descriptors = {
          bench_descriptor{"ecs.fast", &fast_scenario},
          bench_descriptor{"scene.loose", &fast_scenario},
          bench_descriptor{"scripting.skipped", &skipping_scenario},
        };

        SUBCASE("every thresholded scenario passing is 0") {
            bench_options options = quick_options();
            options.nameFilter = "ecs.*";
            quiet_runner quiet(options);
            CONJURE_REQUIRE_OK(quiet.runner.load_thresholds_text(THRESHOLDS_TEXT));
            CHECK(quiet.runner.run_all(descriptors) == 0);
            CHECK(quiet.runner.results().size() == 1);
        }
        SUBCASE("an unthresholded scenario fails under enforcement") {
            bench_options options = quick_options();
            options.nameFilter = "*";
            quiet_runner quiet(options);
            CONJURE_REQUIRE_OK(quiet.runner.load_thresholds_text("{\"version\": 1, \"scenarios\": {}}"));
            CHECK(quiet.runner.run_all(descriptors) == BENCH_EXIT_FAILED);
            CHECK(quiet.runner.results().size() == 3);
        }
        SUBCASE("a skipped scenario among passing ones does not change the verdict") {
            quiet_runner quiet(quick_options());
            CONJURE_REQUIRE_OK(quiet.runner.load_thresholds_text(THRESHOLDS_TEXT));
            CHECK(quiet.runner.run_all(descriptors) == 0);
        }
        SUBCASE("every selected scenario skipped is 77") {
            bench_options options = quick_options();
            options.nameFilter = "scripting.*";
            quiet_runner quiet(options);
            CHECK(quiet.runner.run_all(descriptors) == BENCH_EXIT_SKIPPED);
        }
        SUBCASE("no match is 2 and says so") {
            bench_options options = quick_options();
            options.nameFilter = "audio.*";
            quiet_runner quiet(options);
            CHECK(quiet.runner.run_all(descriptors) == BENCH_EXIT_USAGE);
            CHECK(quiet.output().find("no scenario matches 'audio.*'") != std::string::npos);
            CHECK(quiet.runner.exit_code() == BENCH_EXIT_USAGE);
        }
    }

    TEST_CASE("bench_runner - write_json round trips the results through the JSON document") {
        bench_options options = quick_options();
        options.machineClass = machine_class::CI;
        quiet_runner quiet(options);
        CONJURE_REQUIRE_OK(quiet.runner.load_thresholds_text(THRESHOLDS_TEXT));
        CONJURE_UNUSED(quiet.runner.run(bench_descriptor{"ecs.fast", &fast_scenario}));
        CONJURE_UNUSED(quiet.runner.run(bench_descriptor{"scene.loose", &phased_scenario}));
        CONJURE_UNUSED(quiet.runner.run(bench_descriptor{"scripting.skipped", &skipping_scenario}));

        const std::filesystem::path path = quiet.directory().file("out/results.json");
        CONJURE_REQUIRE_OK(quiet.runner.write_json(path));
        const core::result<core::json::document> parsed = core::json::document::parse_file(path);
        CONJURE_REQUIRE_OK(parsed);
        const core::json::value root = parsed.value().root();
        CHECK(root["version"].as_u64() == 1);
        CHECK(root["class"].as_string() == "ci");
        CHECK(root["enforce"].as_bool() == true);
        CHECK(root["iterations"].as_u64() == 5);
        const core::json::value results = root["results"];
        REQUIRE(results.is_array());
        REQUIRE(results.size() == 3);
        CHECK(results[0]["name"].as_string() == "ecs.fast");
        CHECK(results[0]["status"].as_string() == "PASSED");
        CHECK(results[0]["items"].as_u64() == 3);
        CHECK(results[0]["threads"].as_u64() == 1);
        CHECK(results[0]["allocs"].as_u64() == 0);
        CHECK(results[0]["threshold"]["p50_ms"].as_f64() == 3.0);    // the ci limit
        CHECK(results[0]["threshold"]["allocs"].as_u64() == 0);
        CHECK(results[0]["p50_ms"].is_number());
        CHECK(results[1]["phases"].size() == 1);
        CHECK(results[1]["phases"][0]["name"].as_string() == "UPDATE");
        CHECK(results[1]["phases"][0]["p50_ms"].as_f64() == 0.5);
        // scene.loose has no ci object: the CI row is unthresholded and carries no threshold object.
        CHECK(results[1]["status"].as_string() == "UNTHRESHOLDED");
        CHECK_FALSE(results[1].contains("threshold"));
        CHECK(results[2]["status"].as_string() == "SKIPPED");
        CHECK(results[2]["reason"].as_string() == "precondition unmet");
        CHECK_FALSE(results[0].contains("reason"));
        CHECK(core::json::document::parse(quiet.runner.to_json()).has_value());

        // A threshold without an allocation gate leaves "allocs" out of its object (scene.loose, desktop).
        quiet_runner desktop(quick_options());
        CONJURE_REQUIRE_OK(desktop.runner.load_thresholds_text(THRESHOLDS_TEXT));
        CONJURE_UNUSED(desktop.runner.run(bench_descriptor{"scene.loose", &phased_scenario}));
        const std::string desktopText = desktop.runner.to_json();
        const core::result<core::json::document> desktopParsed = core::json::document::parse(desktopText);
        CONJURE_REQUIRE_OK(desktopParsed);
        const core::json::value loose = desktopParsed.value().root()["results"][0];
        CHECK(loose["status"].as_string() == "PASSED");
        REQUIRE(loose.contains("threshold"));
        CHECK(loose["threshold"]["p50_ms"].as_f64() == 100.0);
        CHECK_FALSE(loose["threshold"].contains("allocs"));

        // An unwritable path is an IO failure.
        CONJURE_CHECK_ERROR(quiet.runner.write_json(quiet.directory().path()), core::error_code::IO_FAILURE);
    }

    TEST_CASE("bench_runner - format_line and format_phase_line carry the machine-readable fields") {
        bench_result result = {};
        result.name = "ecs.fast";
        result.iterations = 200;
        result.itemsPerIteration = 100000;
        result.jobThreadCount = 8;
        result.statistics.minMilliseconds = 0.1;
        result.statistics.p50Milliseconds = 0.2;
        result.statistics.p95Milliseconds = 0.3;
        result.statistics.p99Milliseconds = 0.4;
        result.statistics.maxMilliseconds = 0.5;
        result.allocationsPerIteration = 2;
        result.status = bench_status::FAILED;
        const std::string line = bench_runner::format_line(result);
        CHECK(line == "benchmark name=ecs.fast phase=total status=FAILED iterations=200 items=100000 threads=8 "
          "min_ms=0.1000 median_ms=0.2000 p95_ms=0.3000 p99_ms=0.4000 max_ms=0.5000 allocs=2");

        result.hasThreshold = true;
        result.threshold.p50Milliseconds = 0.15;
        result.threshold.p95Milliseconds = 0.25;
        const std::string thresholded = bench_runner::format_line(result);
        CHECK(thresholded.find(" threshold_p50_ms=0.1500 threshold_p95_ms=0.2500") != std::string::npos);
        CHECK(thresholded.find("threshold_p99_ms") == std::string::npos);
        CHECK(thresholded.find("threshold_allocs") == std::string::npos);

        bench_phase_result phase = {};
        phase.name = "TRANSFORM";
        phase.statistics.p50Milliseconds = 0.05;
        const std::string phaseLine = bench_runner::format_phase_line(result, phase);
        const std::string_view phasePrefix =
          "benchmark name=ecs.fast phase=TRANSFORM status=FAILED iterations=200 items=100000 threads=8 ";
        CHECK(phaseLine.starts_with(phasePrefix));
        CHECK(phaseLine.find("median_ms=0.0500") != std::string::npos);
        CHECK(phaseLine.find("allocs=") == std::string::npos);
    }

    TEST_CASE("bench_runner - format_line quotes the reason and escapes quotes and backslashes") {
        bench_result result = {};
        result.name = "scripting.update_10k";
        result.status = bench_status::SKIPPED;
        result.reason = "plain";
        CHECK(bench_runner::format_line(result).ends_with(" allocs=0 reason=\"plain\""));

        result.reason = "Conjure.TestScripts.dll (v1) is missing from C:\\build\\managed\\\"v1\"";
        const std::string line = bench_runner::format_line(result);
        const std::string_view expected =
          " reason=\"Conjure.TestScripts.dll (v1) is missing from C:\\\\build\\\\managed\\\\\\\"v1\\\"\"";
        CHECK(line.ends_with(expected));
        // Every unescaped '"' in the line belongs to the two delimiters of the reason field.
        usize bareQuotes = 0;
        for (usize index = 0; index < line.size(); ++index) {
            if (line[index] == '"' && (index == 0 || line[index - 1] != '\\')) {
                ++bareQuotes;
            }
        }
        CHECK(bareQuotes == 2);

        result.reason.clear();
        CHECK(bench_runner::format_line(result).find("reason=") == std::string::npos);
    }
}
