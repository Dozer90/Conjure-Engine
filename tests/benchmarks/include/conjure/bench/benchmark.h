#pragma once

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/types.h"

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// The in-house benchmark harness (docs/design/08_testing.md D10, 2.4, 3.6, 4.7): fixed warm-up plus
// fixed measured iterations, min / p50 / p95 / p99 of the wall-clock samples and the exact allocation
// count of the worst iteration. A scenario is a free function registered with CONJURE_BENCHMARK; it
// builds its fixture untimed, then hands the body it wants timed to bench_context::measure.
//
//   CONJURE_BENCHMARK(ecs.iterate_100k) {
//       fixture state = build_fixture(context.entity_count());
//       context.set_items(state.count);
//       context.measure([&]() { state.integrate(); });
//   }
//
// Scenario names are "<module>.<scenario>" and double as the CTest entry names (benchmark.<name>).

namespace conjure::bench {

    /// <summary>
    /// Which machine the thresholds are read for: the reference desktop or a shared CI runner (3x).
    /// </summary>
    enum class machine_class : u8 {
        DESKTOP,
        CI
    };

    /// <summary>
    /// The enumerator name in lower case ("desktop", "ci"), the spelling of thresholds.json and --class.
    /// </summary>
    [[nodiscard]] std::string_view to_string(machine_class value) noexcept;

    /// <summary>
    /// Allocation limit meaning "not checked" (thresholds.json without an "allocs" member).
    /// </summary>
    inline constexpr u64 UNLIMITED_ALLOCATIONS = ~u64{0};

    /// <summary>
    /// The pass limits of one scenario for one machine class. A time of 0 is not checked; the allocation
    /// limit is exact and the same in both classes. bench_runner::run treats a threshold that sets no limit
    /// at all (every time 0, allocations unlimited) as no threshold, so the scenario is UNTHRESHOLDED;
    /// summarise() judges whatever threshold it is given.
    /// </summary>
    struct bench_threshold {
        f64 p50Milliseconds = 0.0;
        f64 p95Milliseconds = 0.0;
        f64 p99Milliseconds = 0.0;
        u64 maxAllocationsPerIteration = UNLIMITED_ALLOCATIONS;

        [[nodiscard]] bool operator==(const bench_threshold &) const = default;
    };

    /// <summary>
    /// The jobThreadCount that asks for the automatic worker count (hardware_concurrency - 1 clamped to
    /// [1, 16]); the same sentinel engine::AUTO_WORKER_COUNT uses, so 0 keeps the engine's meaning of
    /// "every job runs inline on the calling thread" (06_engine_loop.md jobs_settings, 08_testing.md D4).
    /// </summary>
    inline constexpr u32 AUTO_JOB_THREAD_COUNT = 0xFFFFFFFFu;

    /// <summary>
    /// Everything the command line can set; the defaults are what CTest runs with.
    /// </summary>
    struct bench_options {
        u32 warmupIterations = 20;
        u32 measuredIterations = 200;
        u32 entityCount = 10000;
        u32 jobThreadCount = AUTO_JOB_THREAD_COUNT; // 0 = inline jobs, AUTO = hardware - 1 clamped to [1, 16]
        u64 seed = 0x5EED;
        machine_class machineClass = machine_class::DESKTOP;
        bool enforceThresholds = false;
        bool listOnly = false;
        bool showHelp = false;
        std::string nameFilter = "*";               // glob over the scenario name ('*' and '?')
        std::filesystem::path thresholdsFile = {};
        std::filesystem::path outputFile = {};
    };

    /// <summary>
    /// The percentiles of one sample set in milliseconds.
    /// </summary>
    struct bench_statistics {
        f64 minMilliseconds = 0.0;
        f64 p50Milliseconds = 0.0;
        f64 p95Milliseconds = 0.0;
        f64 p99Milliseconds = 0.0;
        f64 maxMilliseconds = 0.0;

        [[nodiscard]] bool operator==(const bench_statistics &) const = default;
    };

    /// <summary>
    /// One sub-phase of a scenario (an engine phase of a full frame): reported, never thresholded.
    /// </summary>
    struct bench_phase_result {
        std::string name = {};
        bench_statistics statistics = {};
    };

    /// <summary>
    /// How a scenario ended.
    /// </summary>
    enum class bench_status : u8 {
        PASSED,
        FAILED,             // a thresholded metric is above its limit, the scenario failed, or it never measured
        UNTHRESHOLDED,      // no thresholds entry, or one that sets no limit for the selected machine class
        SKIPPED             // a machine precondition was not met
    };

    /// <summary>
    /// The enumerator name of a status ("PASSED").
    /// </summary>
    [[nodiscard]] std::string_view to_string(bench_status value) noexcept;

    /// <summary>
    /// One report row.
    /// </summary>
    struct bench_result {
        std::string name = {};
        u32 iterations = 0;
        u32 jobThreadCount = 0;
        u64 itemsPerIteration = 0;
        bench_statistics statistics = {};
        u64 allocationsPerIteration = 0;            // maximum over the measured iterations
        bench_threshold threshold = {};
        bool hasThreshold = false;
        bench_status status = bench_status::UNTHRESHOLDED;
        std::string reason = {};                    // why the scenario skipped or failed; empty otherwise
        bool scenarioFailed = false;                // never measured or called fail(): exit 1 even without --enforce
        std::vector<bench_phase_result> phases = {};
    };

    /// <summary>
    /// The percentile a sorted sample set yields for a quantile in [0, 1]: samples[floor(q * (n - 1))]
    /// (3.6). An empty set yields 0.
    /// </summary>
    [[nodiscard]] f64 percentile(std::span<const f64> sortedSamples, f64 quantile) noexcept;

    /// <summary>
    /// min / p50 / p95 / p99 / max of a sample set (any order; a sorted copy is made).
    /// </summary>
    [[nodiscard]] bench_statistics compute_statistics(std::span<const f64> samples);

    /// <summary>
    /// The worker count an options value resolves to: hardwareConcurrency - 1 clamped to [1, 16] for
    /// AUTO_JOB_THREAD_COUNT, otherwise the request itself (0 included: every job runs inline).
    /// </summary>
    [[nodiscard]] u32 resolve_job_thread_count(u32 requested, u32 hardwareConcurrency) noexcept;

    /// <summary>
    /// What a running scenario receives: its options, a job system with the real worker count and the
    /// measure loop. One context per scenario run.
    /// </summary>
    class bench_context {
        private:
            using body_fn = void (*)(void *body);

            bench_options mOptions = {};
            u32 mJobThreadCount = 0;
            u64 mItemsPerIteration = 0;
            u64 mMaxAllocationsPerIteration = 0;
            std::vector<f64> mSamplesMilliseconds = {};
            std::vector<bench_phase_result> mPhases = {};
            std::unique_ptr<core::jobs::job_system> mJobs = nullptr;
            std::string mSkipReason = {};
            std::string mFailureReason = {};
            bool mSkipped = false;
            bool mFailed = false;
            bool mMeasured = false;

        public:
            explicit bench_context(const bench_options &options);

            /// <summary>
            /// Stops the job system when one was started.
            /// </summary>
            ~bench_context();

            bench_context(const bench_context &) = delete;
            bench_context &operator=(const bench_context &) = delete;

        public:
            /// <summary>
            /// Runs the body warmupIterations times untimed, then measuredIterations times with the wall
            /// clock and the allocation counters around every call (3.6). A second call replaces the
            /// previous samples. Nothing runs after skip().
            /// </summary>
            template <typename Body>
            void measure(Body &&body) {
                using body_type = std::remove_reference_t<Body>;
                body_type *target = &body;
                measure_impl(
                  [](void *context) {
                      (*static_cast<body_type *>(context))();
                  },
                  target
                );
            }

            /// <summary>
            /// Records how many items one iteration processes (entities, instances, bodies) for the report.
            /// </summary>
            void set_items(u64 itemsPerIteration) noexcept {
                mItemsPerIteration = itemsPerIteration;
            }

            /// <summary>
            /// Appends a reported sub-phase (one engine phase of a frame, say) with its own samples.
            /// </summary>
            void add_phase(std::string_view name, std::span<const f64> samplesMilliseconds);

            /// <summary>
            /// Marks the scenario as skipped because a machine precondition is unmet (no .NET runtime).
            /// The runner reports SKIPPED and exits 77 when every selected scenario skipped.
            /// </summary>
            void skip(std::string_view reason);

            /// <summary>
            /// Marks the scenario as failed for a reason the thresholds cannot see (a measured frame returned
            /// an error, the fixture grew during the measured loop). The runner reports FAILED with the
            /// reason whatever the figures say and fails the run with or without --enforce; the samples are
            /// still reported. The first reason wins.
            /// </summary>
            void fail(std::string_view reason);

            /// <summary>
            /// The job system every scenario shares, started on first use with job_thread_count() workers
            /// (started with none, every job runs inline on the calling thread).
            /// </summary>
            [[nodiscard]] core::jobs::job_system &jobs();

            /// <summary>
            /// The entity count of the options (10,000 by default).
            /// </summary>
            [[nodiscard]] u32 entity_count() const noexcept {
                return mOptions.entityCount;
            }

            /// <summary>
            /// The worker count the job system runs with (resolved; 0 means inline jobs, never AUTO).
            /// </summary>
            [[nodiscard]] u32 job_thread_count() const noexcept {
                return mJobThreadCount;
            }

            /// <summary>
            /// The seed scenarios derive their pseudo-random layouts from.
            /// </summary>
            [[nodiscard]] u64 seed() const noexcept {
                return mOptions.seed;
            }

            [[nodiscard]] const bench_options &options() const noexcept {
                return mOptions;
            }

            /// <summary>
            /// The measured samples in iteration order (empty before measure()).
            /// </summary>
            [[nodiscard]] std::span<const f64> samples_milliseconds() const noexcept {
                return mSamplesMilliseconds;
            }

            /// <summary>
            /// The largest allocation count any measured iteration made.
            /// </summary>
            [[nodiscard]] u64 max_allocations_per_iteration() const noexcept {
                return mMaxAllocationsPerIteration;
            }

            [[nodiscard]] u64 items_per_iteration() const noexcept {
                return mItemsPerIteration;
            }

            [[nodiscard]] std::span<const bench_phase_result> phases() const noexcept {
                return mPhases;
            }

            [[nodiscard]] bool is_skipped() const noexcept {
                return mSkipped;
            }

            [[nodiscard]] std::string_view skip_reason() const noexcept {
                return mSkipReason;
            }

            [[nodiscard]] bool is_failed() const noexcept {
                return mFailed;
            }

            [[nodiscard]] std::string_view failure_reason() const noexcept {
                return mFailureReason;
            }

            /// <summary>
            /// True once measure() ran to completion. A scenario that returns neither measured nor skipped
            /// is reported FAILED by the runner and fails the run whether or not thresholds are enforced
            /// (08_testing.md D12: silent passes are worse than failures).
            /// </summary>
            [[nodiscard]] bool is_measured() const noexcept {
                return mMeasured;
            }

        private:
            void measure_impl(body_fn invoke, void *body);
    };

    /// <summary>
    /// The signature of a scenario.
    /// </summary>
    using bench_function = void (*)(bench_context &context);

    /// <summary>
    /// One registered scenario.
    /// </summary>
    struct bench_descriptor {
        std::string_view name = {};
        bench_function function = nullptr;
    };

    /// <summary>
    /// The process-wide list of scenarios, filled by CONJURE_BENCHMARK at static initialisation and kept
    /// sorted by name.
    /// </summary>
    class bench_registry {
        private:
            std::vector<bench_descriptor> mDescriptors = {};

        public:
            bench_registry();
            ~bench_registry();

            bench_registry(const bench_registry &) = delete;
            bench_registry &operator=(const bench_registry &) = delete;

        public:
            /// <summary>
            /// The registry CONJURE_BENCHMARK adds to.
            /// </summary>
            [[nodiscard]] static bench_registry &instance();

            /// <summary>
            /// Adds a scenario. A duplicate name or a null function asserts and is ignored.
            /// </summary>
            void add(const bench_descriptor &descriptor);

            /// <summary>
            /// Every scenario sorted by name.
            /// </summary>
            [[nodiscard]] std::span<const bench_descriptor> descriptors() const noexcept {
                return mDescriptors;
            }

            /// <summary>
            /// The scenario of this exact name, or nullptr.
            /// </summary>
            [[nodiscard]] const bench_descriptor *find(std::string_view name) const noexcept;

            /// <summary>
            /// Forgets every scenario (tests of the registry itself).
            /// </summary>
            void clear() noexcept;
    };

    namespace detail {
        /// <summary>
        /// Registers a scenario from a static initialiser.
        /// </summary>
        struct bench_registrar {
            bench_registrar(std::string_view name, bench_function function);
        };
    }
}

#define CONJURE_BENCH_CONCAT_INNER(left, right) left##right
#define CONJURE_BENCH_CONCAT(left, right) CONJURE_BENCH_CONCAT_INNER(left, right)
#define CONJURE_BENCH_FUNCTION(line) CONJURE_BENCH_CONCAT(conjure_bench_function_, line)
#define CONJURE_BENCH_REGISTRAR(line) CONJURE_BENCH_CONCAT(conjureBenchRegistrar_, line)

// Defines the scenario function conjure_bench_function_<line> and registers it under 'name' (unquoted,
// dots allowed: CONJURE_BENCHMARK(ecs.iterate_100k)). Used at namespace scope; the body follows the macro.
#define CONJURE_BENCHMARK(name)                                                                             \
    static void CONJURE_BENCH_FUNCTION(__LINE__)(::conjure::bench::bench_context &context);                 \
    static const ::conjure::bench::detail::bench_registrar CONJURE_BENCH_REGISTRAR(__LINE__)(               \
      #name,                                                                                                \
      &CONJURE_BENCH_FUNCTION(__LINE__)                                                                     \
    );                                                                                                      \
    static void CONJURE_BENCH_FUNCTION(__LINE__)(::conjure::bench::bench_context &context)
