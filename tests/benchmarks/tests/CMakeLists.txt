# Self-tests of the benchmark harness (docs/design/08_testing.md 11 "test_benchmark.cpp"): one test file per
# public header of include/conjure/bench. Registered like every unit test module (label unit;benchmarks), so
# they run in the default presets while the benchmarks themselves do not.

conjure_add_test_module(benchmarks
    SOURCES
        test_bench_runner.cpp
        test_benchmark.cpp
    DEPS
        conjure::bench_harness
    LABELS
        benchmarks
)
