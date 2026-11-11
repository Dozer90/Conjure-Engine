// Entry point of conjure_benchmarks (docs/design/08_testing.md 3.6, 4.7).
//
//   1. the command line (bench_runner.h) over the compiled-in defaults: thresholds.json next to the
//      sources, thresholds enforced in Development and Distribution, reported only in Debug
//   2. the main thread is designated (scripting and the engine frame loop assert on it), the console
//      log is quietened to WARN so stdout carries the report lines, the seed is applied
//   3. every registered scenario matching --filter runs; the exit code is the runner's

#include "conjure/bench/bench_runner.h"
#include "conjure/bench/benchmark.h"

#include "conjure/core/jobs/main_thread.h"
#include "conjure/core/log.h"
#include "conjure/core/random.h"
#include "conjure/core/types.h"

#include <cstdio>
#include <string_view>
#include <vector>

#ifndef CONJURE_BENCH_THRESHOLDS_FILE
    #define CONJURE_BENCH_THRESHOLDS_FILE ""
#endif
#ifndef CONJURE_BENCH_ENFORCE_DEFAULT
    #define CONJURE_BENCH_ENFORCE_DEFAULT 1
#endif

using namespace conjure;
using namespace conjure::bench;

namespace {

    [[nodiscard]] bench_options default_options() {
        bench_options options = {};
        options.enforceThresholds = CONJURE_BENCH_ENFORCE_DEFAULT != 0;
        options.thresholdsFile = std::filesystem::path(CONJURE_BENCH_THRESHOLDS_FILE);
        return options;
    }

    void list_scenarios() {
        for (const bench_descriptor &descriptor : bench_registry::instance().descriptors()) {
            std::fprintf(stdout, "%.*s\n", static_cast<int>(descriptor.name.size()), descriptor.name.data());
        }
        std::fflush(stdout);
    }
}

int main(int argc, char **argv) {
    std::vector<std::string_view> arguments = {};
    arguments.reserve(static_cast<usize>(argc));
    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }

    const core::result<bench_options> parsed = parse_command_line(arguments, default_options());
    if (!parsed.has_value()) {
        const std::string message = parsed.get_error().to_string();
        std::fprintf(stderr, "conjure_benchmarks: %s\n%.*s", message.c_str(),
          static_cast<int>(usage_text().size()), usage_text().data());
        return BENCH_EXIT_USAGE;
    }
    const bench_options &options = parsed.value();
    if (options.showHelp) {
        std::fprintf(stdout, "%.*s", static_cast<int>(usage_text().size()), usage_text().data());
        return 0;
    }
    if (options.listOnly) {
        list_scenarios();
        return 0;
    }

    core::jobs::set_main_thread();
    core::log::set_global_level(core::log_level::WARN);
    core::random::seed(options.seed);

    bench_runner runner(options);
    if (!options.thresholdsFile.empty()) {
        const core::result<void> loaded = runner.load_thresholds(options.thresholdsFile);
        if (!loaded.has_value()) {
            const std::string message = loaded.get_error().to_string();
            std::fprintf(stderr, "conjure_benchmarks: %s\n", message.c_str());
            return BENCH_EXIT_USAGE;
        }
    }

    std::fprintf(
      stdout,
      "benchmark run class=%.*s enforce=%d iterations=%u warmup=%u entities=%u seed=%llu\n",
      static_cast<int>(to_string(options.machineClass).size()),
      to_string(options.machineClass).data(),
      options.enforceThresholds ? 1 : 0,
      options.measuredIterations,
      options.warmupIterations,
      options.entityCount,
      static_cast<unsigned long long>(options.seed)
    );
    std::fflush(stdout);

    const int exitCode = runner.run_all(bench_registry::instance().descriptors());
    if (!options.outputFile.empty()) {
        const core::result<void> written = runner.write_json(options.outputFile);
        if (!written.has_value()) {
            const std::string message = written.get_error().to_string();
            std::fprintf(stderr, "conjure_benchmarks: %s\n", message.c_str());
            core::log::flush();
            return exitCode == 0 ? BENCH_EXIT_USAGE : exitCode;
        }
    }
    core::log::flush();
    return exitCode;
}
