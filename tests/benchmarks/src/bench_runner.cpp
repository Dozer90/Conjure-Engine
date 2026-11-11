#include "conjure/bench/bench_runner.h"

#include "conjure/core/error.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/log.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <format>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

using namespace conjure;
using namespace conjure::bench;

namespace {

    constexpr u32 THRESHOLDS_FILE_VERSION = 1;
    constexpr u32 RESULTS_FILE_VERSION = 1;

    constexpr std::string_view USAGE =
      "usage: conjure_benchmarks [options]\n"
      "  --class desktop|ci        machine class whose thresholds apply (default: desktop)\n"
      "  --enforce | --no-enforce  fail (exit 1) on a breached or missing threshold\n"
      "  --filter <glob>           run the scenarios whose name matches ('*' and '?'; default: *)\n"
      "  --out <file>              write every result as JSON\n"
      "  --threads N|auto          job system workers: 0 runs every job inline, auto is hardware threads - 1\n"
      "                            clamped to 1..16 (default: auto)\n"
      "  --thresholds <file>       thresholds.json to read (default: the compiled-in path)\n"
      "  --iterations N            measured iterations per scenario (default: 200)\n"
      "  --warmup N                untimed iterations before measuring (default: 20)\n"
      "  --entities N              entity count scenarios build (default: 10000)\n"
      "  --seed N                  seed of the pseudo-random layouts (default: 0x5EED)\n"
      "  --list                    print the registered scenario names and exit\n"
      "  --help                    this text\n";

    [[nodiscard]] std::optional<u64> parse_u64(std::string_view text) {
        int base = 10;
        if (text.starts_with("0x") || text.starts_with("0X")) {
            text.remove_prefix(2);
            base = 16;
        }
        if (text.empty()) {
            return std::nullopt;
        }
        u64 value = 0;
        const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value, base);
        if (parsed.ec != std::errc() || parsed.ptr != text.data() + text.size()) {
            return std::nullopt;
        }
        return value;
    }

    [[nodiscard]] core::error option_error(std::string_view option, std::string_view detail) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "{}: {}", option, detail);
    }

    /// <summary>
    /// Reads one non-negative number of a threshold object; absent means 0 (not checked).
    /// </summary>
    [[nodiscard]] core::result<f64> read_milliseconds(
      const core::json::value &object,
      std::string_view key,
      std::string_view scenario
    ) {
        const std::optional<core::json::value> member = object.get(key);
        if (!member.has_value()) {
            return 0.0;
        }
        if (!member->is_number() || member->as_f64(-1.0) < 0.0) {
            return core::make_error(
              core::error_code::INVALID_ARGUMENT,
              "thresholds: scenario '{}' member '{}' must be a non-negative number",
              scenario,
              key
            );
        }
        return member->as_f64();
    }

    [[nodiscard]] core::result<bench_threshold> read_class_threshold(
      const core::json::value &entry,
      std::string_view className,
      std::string_view scenario
    ) {
        bench_threshold threshold = {};
        const std::optional<core::json::value> object = entry.get(className);
        if (!object.has_value()) {
            return threshold;
        }
        if (!object->is_object()) {
            return core::make_error(
              core::error_code::INVALID_ARGUMENT,
              "thresholds: scenario '{}' member '{}' must be an object",
              scenario,
              className
            );
        }
        CONJURE_TRY_ASSIGN(threshold.p50Milliseconds, read_milliseconds(*object, "p50_ms", scenario));
        CONJURE_TRY_ASSIGN(threshold.p95Milliseconds, read_milliseconds(*object, "p95_ms", scenario));
        CONJURE_TRY_ASSIGN(threshold.p99Milliseconds, read_milliseconds(*object, "p99_ms", scenario));
        return threshold;
    }

    [[nodiscard]] core::result<scenario_thresholds> read_scenario(
      const core::json::value &entry,
      std::string_view scenario
    ) {
        if (!entry.is_object()) {
            return core::make_error(
              core::error_code::INVALID_ARGUMENT,
              "thresholds: scenario '{}' must be an object",
              scenario
            );
        }
        scenario_thresholds thresholds = {};
        CONJURE_TRY_ASSIGN(thresholds.desktop, read_class_threshold(entry, "desktop", scenario));
        CONJURE_TRY_ASSIGN(thresholds.ci, read_class_threshold(entry, "ci", scenario));

        u64 allocations = UNLIMITED_ALLOCATIONS;
        if (const std::optional<core::json::value> allocs = entry.get("allocs"); allocs.has_value()) {
            if (!allocs->is_integer() || allocs->as_i64(-1) < 0) {
                return core::make_error(
                  core::error_code::INVALID_ARGUMENT,
                  "thresholds: scenario '{}' member 'allocs' must be a non-negative integer",
                  scenario
                );
            }
            allocations = allocs->as_u64();
        }
        thresholds.desktop.maxAllocationsPerIteration = allocations;
        thresholds.ci.maxAllocationsPerIteration = allocations;
        return thresholds;
    }

    void write_statistics(core::json::writer &out, const bench_statistics &statistics) {
        out.key("min_ms");
        out.value(statistics.minMilliseconds);
        out.key("p50_ms");
        out.value(statistics.p50Milliseconds);
        out.key("p95_ms");
        out.value(statistics.p95Milliseconds);
        out.key("p99_ms");
        out.value(statistics.p99Milliseconds);
        out.key("max_ms");
        out.value(statistics.maxMilliseconds);
    }

    [[nodiscard]] std::string statistics_fields(const bench_statistics &statistics) {
        return std::format(
          "min_ms={:.4f} median_ms={:.4f} p95_ms={:.4f} p99_ms={:.4f} max_ms={:.4f}",
          statistics.minMilliseconds,
          statistics.p50Milliseconds,
          statistics.p95Milliseconds,
          statistics.p99Milliseconds,
          statistics.maxMilliseconds
        );
    }

    /// <summary>
    /// The reason text as a double-quoted field whose quotes and backslashes are escaped, so a Windows
    /// path or a quoted error message keeps the report line parseable.
    /// </summary>
    [[nodiscard]] std::string quoted_reason(std::string_view reason) {
        std::string text = "\"";
        text.reserve(reason.size() + 2);
        for (const char character : reason) {
            if (character == '"' || character == '\\') {
                text += '\\';
            }
            text += character;
        }
        text += '"';
        return text;
    }

    [[nodiscard]] std::string threshold_fields(const bench_threshold &threshold) {
        std::string text = std::format(" threshold_p50_ms={:.4f}", threshold.p50Milliseconds);
        if (threshold.p95Milliseconds > 0.0) {
            text += std::format(" threshold_p95_ms={:.4f}", threshold.p95Milliseconds);
        }
        if (threshold.p99Milliseconds > 0.0) {
            text += std::format(" threshold_p99_ms={:.4f}", threshold.p99Milliseconds);
        }
        if (threshold.maxAllocationsPerIteration != UNLIMITED_ALLOCATIONS) {
            text += std::format(" threshold_allocs={}", threshold.maxAllocationsPerIteration);
        }
        return text;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Free functions

core::result<machine_class> conjure::bench::parse_machine_class(std::string_view text) {
    std::string lowered(text);
    for (char &character : lowered) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    if (lowered == "desktop") {
        return machine_class::DESKTOP;
    }
    if (lowered == "ci") {
        return machine_class::CI;
    }
    return core::make_error(core::error_code::INVALID_ARGUMENT, "unknown machine class '{}' (desktop, ci)", text);
}

bool conjure::bench::matches_filter(std::string_view name, std::string_view pattern) noexcept {
    // Iterative glob: remember the last '*' so a mismatch after it retries with one more character
    // consumed by the star.
    usize nameIndex = 0;
    usize patternIndex = 0;
    usize starPattern = std::string_view::npos;
    usize starName = 0;
    while (nameIndex < name.size()) {
        const bool literalMatch =
          patternIndex < pattern.size() && (pattern[patternIndex] == '?' || pattern[patternIndex] == name[nameIndex]);
        if (literalMatch) {
            ++nameIndex;
            ++patternIndex;
            continue;
        }
        if (patternIndex < pattern.size() && pattern[patternIndex] == '*') {
            starPattern = patternIndex;
            starName = nameIndex;
            ++patternIndex;
            continue;
        }
        if (starPattern != std::string_view::npos) {
            patternIndex = starPattern + 1;
            ++starName;
            nameIndex = starName;
            continue;
        }
        return false;
    }
    while (patternIndex < pattern.size() && pattern[patternIndex] == '*') {
        ++patternIndex;
    }
    return patternIndex == pattern.size();
}

std::string_view conjure::bench::usage_text() noexcept {
    return USAGE;
}

core::result<bench_options> conjure::bench::parse_command_line(
  std::span<const std::string_view> arguments,
  const bench_options &defaults
) {
    bench_options options = defaults;
    for (usize index = 0; index < arguments.size(); ++index) {
        const std::string_view argument = arguments[index];
        if (argument == "--help" || argument == "-h") {
            options.showHelp = true;
            return options;
        }
        if (argument == "--list") {
            options.listOnly = true;
            continue;
        }
        if (argument == "--enforce") {
            options.enforceThresholds = true;
            continue;
        }
        if (argument == "--no-enforce") {
            options.enforceThresholds = false;
            continue;
        }

        // Every remaining option takes a value.
        if (index + 1 >= arguments.size()) {
            return option_error(argument, "missing value");
        }
        const std::string_view value = arguments[++index];
        if (argument == "--class") {
            CONJURE_TRY_ASSIGN(options.machineClass, parse_machine_class(value));
        } else if (argument == "--filter") {
            if (value.empty()) {
                return option_error(argument, "the filter must not be empty");
            }
            options.nameFilter = std::string(value);
        } else if (argument == "--out") {
            options.outputFile = std::filesystem::path(value);
        } else if (argument == "--thresholds") {
            options.thresholdsFile = std::filesystem::path(value);
        } else if (argument == "--threads") {
            if (value == "auto") {
                options.jobThreadCount = AUTO_JOB_THREAD_COUNT;
            } else {
                const std::optional<u64> parsed = parse_u64(value);
                if (!parsed.has_value() || *parsed > core::jobs::MAX_WORKER_COUNT) {
                    return option_error(argument, "expected a worker count between 0 and 256, or auto");
                }
                options.jobThreadCount = static_cast<u32>(*parsed);
            }
        } else if (argument == "--iterations") {
            const std::optional<u64> parsed = parse_u64(value);
            if (!parsed.has_value() || *parsed == 0 || *parsed > 1'000'000) {
                return option_error(argument, "expected an iteration count between 1 and 1000000");
            }
            options.measuredIterations = static_cast<u32>(*parsed);
        } else if (argument == "--warmup") {
            const std::optional<u64> parsed = parse_u64(value);
            if (!parsed.has_value() || *parsed > 1'000'000) {
                return option_error(argument, "expected a warm-up count between 0 and 1000000");
            }
            options.warmupIterations = static_cast<u32>(*parsed);
        } else if (argument == "--entities") {
            const std::optional<u64> parsed = parse_u64(value);
            if (!parsed.has_value() || *parsed == 0 || *parsed > 10'000'000) {
                return option_error(argument, "expected an entity count between 1 and 10000000");
            }
            options.entityCount = static_cast<u32>(*parsed);
        } else if (argument == "--seed") {
            const std::optional<u64> parsed = parse_u64(value);
            if (!parsed.has_value()) {
                return option_error(argument, "expected an integer seed");
            }
            options.seed = *parsed;
        } else {
            return option_error(argument, "unknown option");
        }
    }
    return options;
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

bench_runner::bench_runner(const bench_options &options) :
    mOptions(options) {}

bench_runner::~bench_runner() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<void> bench_runner::load_thresholds(const std::filesystem::path &path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        return core::make_error(core::error_code::IO_FAILURE, "thresholds file '{}' cannot be read", path.string());
    }
    std::ostringstream text = {};
    text << stream.rdbuf();
    return load_thresholds_text(text.str());
}

core::result<void> bench_runner::load_thresholds_text(std::string_view text) {
    CONJURE_TRY_ASSIGN(const core::json::document parsed, core::json::document::parse(text));
    const core::json::value root = parsed.root();
    if (!root.is_object()) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "thresholds: the root must be an object");
    }
    const u64 version = root["version"].as_u64(0);
    if (version != THRESHOLDS_FILE_VERSION) {
        return core::make_error(
          core::error_code::INVALID_ARGUMENT,
          "thresholds: unsupported version {} (expected {})",
          version,
          THRESHOLDS_FILE_VERSION
        );
    }
    const core::json::value scenarios = root["scenarios"];
    if (!scenarios.is_object()) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "thresholds: 'scenarios' must be an object");
    }

    std::unordered_map<std::string, scenario_thresholds> loaded = {};
    for (const core::json::member member : scenarios.members()) {
        CONJURE_TRY_ASSIGN(const scenario_thresholds thresholds, read_scenario(member.item, member.key));
        loaded.emplace(std::string(member.key), thresholds);
    }
    mThresholds = std::move(loaded);
    return core::result<void>::success();
}

const scenario_thresholds *bench_runner::thresholds_of(std::string_view name) const noexcept {
    const auto found = mThresholds.find(std::string(name));
    return found == mThresholds.end() ? nullptr : &found->second;
}

bench_result bench_runner::run(const bench_descriptor &descriptor) {
    bench_context context(mOptions);
    if (descriptor.function != nullptr) {
        descriptor.function(context);
    }

    const scenario_thresholds *thresholds = thresholds_of(descriptor.name);
    const bench_threshold *threshold = thresholds == nullptr ? nullptr : &thresholds->of(mOptions.machineClass);
    bench_result result = {};
    if (context.is_skipped()) {
        result.name = std::string(descriptor.name);
        result.status = bench_status::SKIPPED;
        result.reason = std::string(context.skip_reason());
        if (threshold != nullptr) {
            result.threshold = *threshold;
            result.hasThreshold = true;
        }
    } else {
        result = summarise(
          descriptor.name,
          context.samples_milliseconds(),
          context.max_allocations_per_iteration(),
          threshold
        );
        result.phases.assign(context.phases().begin(), context.phases().end());
        // A scenario that returned without measuring would otherwise pass every threshold over zero
        // samples; a scenario that failed for a reason the figures cannot show must not pass either.
        if (!context.is_measured()) {
            result.status = bench_status::FAILED;
            result.reason = "the scenario returned without measure() or skip()";
        } else if (context.is_failed()) {
            result.status = bench_status::FAILED;
            result.reason = std::string(context.failure_reason());
        }
    }
    result.iterations = static_cast<u32>(context.samples_milliseconds().size());
    result.jobThreadCount = context.job_thread_count();
    result.itemsPerIteration = context.items_per_iteration();
    print(result);
    mResults.push_back(result);
    return result;
}

int bench_runner::run_all(std::span<const bench_descriptor> descriptors) {
    u32 selected = 0;
    for (const bench_descriptor &descriptor : descriptors) {
        if (!matches_filter(descriptor.name, mOptions.nameFilter)) {
            continue;
        }
        ++selected;
        CONJURE_UNUSED(run(descriptor));
    }
    if (selected == 0) {
        std::fprintf(mOutput, "benchmark: no scenario matches '%s'\n", mOptions.nameFilter.c_str());
        std::fflush(mOutput);
        return BENCH_EXIT_USAGE;
    }
    return exit_code();
}

int bench_runner::exit_code() const noexcept {
    if (mResults.empty()) {
        return BENCH_EXIT_USAGE;
    }
    bool allSkipped = true;
    bool anyFailed = false;
    for (const bench_result &result : mResults) {
        if (result.status != bench_status::SKIPPED) {
            allSkipped = false;
        }
        if (result.status == bench_status::FAILED || result.status == bench_status::UNTHRESHOLDED) {
            anyFailed = true;
        }
    }
    if (allSkipped) {
        return BENCH_EXIT_SKIPPED;
    }
    return mOptions.enforceThresholds && anyFailed ? BENCH_EXIT_FAILED : 0;
}

core::result<void> bench_runner::write_json(const std::filesystem::path &path) const {
    std::error_code ignored = {};
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ignored);
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        return core::make_error(core::error_code::IO_FAILURE, "results file '{}' cannot be written", path.string());
    }
    stream << to_json();
    if (!stream.good()) {
        return core::make_error(core::error_code::IO_FAILURE, "results file '{}' was not fully written", path.string());
    }
    return core::result<void>::success();
}

std::string bench_runner::to_json() const {
    core::json::writer out = {};
    out.begin_object();
    out.key("version");
    out.value(RESULTS_FILE_VERSION);
    out.key("class");
    out.value(to_string(mOptions.machineClass));
    out.key("enforce");
    out.value(mOptions.enforceThresholds);
    out.key("iterations");
    out.value(mOptions.measuredIterations);
    out.key("warmup");
    out.value(mOptions.warmupIterations);
    out.key("results");
    out.begin_array();
    for (const bench_result &result : mResults) {
        out.begin_object();
        out.key("name");
        out.value(result.name);
        out.key("status");
        out.value(to_string(result.status));
        out.key("iterations");
        out.value(result.iterations);
        out.key("threads");
        out.value(result.jobThreadCount);
        out.key("items");
        out.value(result.itemsPerIteration);
        write_statistics(out, result.statistics);
        out.key("allocs");
        out.value(result.allocationsPerIteration);
        if (result.hasThreshold) {
            out.key("threshold");
            out.begin_object();
            out.key("p50_ms");
            out.value(result.threshold.p50Milliseconds);
            out.key("p95_ms");
            out.value(result.threshold.p95Milliseconds);
            out.key("p99_ms");
            out.value(result.threshold.p99Milliseconds);
            if (result.threshold.maxAllocationsPerIteration != UNLIMITED_ALLOCATIONS) {
                out.key("allocs");
                out.value(result.threshold.maxAllocationsPerIteration);
            }
            out.end_object();
        }
        if (!result.reason.empty()) {
            out.key("reason");
            out.value(result.reason);
        }
        out.key("phases");
        out.begin_array();
        for (const bench_phase_result &phase : result.phases) {
            out.begin_object();
            out.key("name");
            out.value(phase.name);
            write_statistics(out, phase.statistics);
            out.end_object();
        }
        out.end_array();
        out.end_object();
    }
    out.end_array();
    out.end_object();
    return out.finish();
}

bench_result bench_runner::summarise(
  std::string_view name,
  std::span<const f64> samplesMilliseconds,
  u64 maxAllocations,
  const bench_threshold *threshold
) {
    bench_result result = {};
    result.name = std::string(name);
    result.iterations = static_cast<u32>(samplesMilliseconds.size());
    result.statistics = compute_statistics(samplesMilliseconds);
    result.allocationsPerIteration = maxAllocations;
    if (threshold == nullptr) {
        result.status = bench_status::UNTHRESHOLDED;
        return result;
    }

    result.threshold = *threshold;
    result.hasThreshold = true;
    bool passed = true;
    if (threshold->p50Milliseconds > 0.0 && result.statistics.p50Milliseconds > threshold->p50Milliseconds) {
        passed = false;
    }
    if (threshold->p95Milliseconds > 0.0 && result.statistics.p95Milliseconds > threshold->p95Milliseconds) {
        passed = false;
    }
    if (threshold->p99Milliseconds > 0.0 && result.statistics.p99Milliseconds > threshold->p99Milliseconds) {
        passed = false;
    }
    if (threshold->maxAllocationsPerIteration != UNLIMITED_ALLOCATIONS &&
      maxAllocations > threshold->maxAllocationsPerIteration) {
        passed = false;
    }
    result.status = passed ? bench_status::PASSED : bench_status::FAILED;
    return result;
}

std::string bench_runner::format_line(const bench_result &result) {
    std::string line = std::format(
      "benchmark name={} phase=total status={} iterations={} items={} threads={} {} allocs={}",
      result.name,
      to_string(result.status),
      result.iterations,
      result.itemsPerIteration,
      result.jobThreadCount,
      statistics_fields(result.statistics),
      result.allocationsPerIteration
    );
    if (result.hasThreshold) {
        line += threshold_fields(result.threshold);
    }
    if (!result.reason.empty()) {
        line += " reason=" + quoted_reason(result.reason);
    }
    return line;
}

std::string bench_runner::format_phase_line(const bench_result &result, const bench_phase_result &phase) {
    return std::format(
      "benchmark name={} phase={} status={} iterations={} items={} threads={} {}",
      result.name,
      phase.name,
      to_string(result.status),
      result.iterations,
      result.itemsPerIteration,
      result.jobThreadCount,
      statistics_fields(phase.statistics)
    );
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

void bench_runner::print(const bench_result &result) const {
    const std::string line = format_line(result);
    std::fprintf(mOutput, "%s\n", line.c_str());
    for (const bench_phase_result &phase : result.phases) {
        const std::string phaseLine = format_phase_line(result, phase);
        std::fprintf(mOutput, "%s\n", phaseLine.c_str());
    }
    std::fflush(mOutput);
}
