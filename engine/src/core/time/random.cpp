#include "conjure/core/random.h"

#include <atomic>

using namespace conjure::core::random;

/*===========================================================================*/
/*===========================================================================*/
// Process seed and per-thread engines

namespace {
    // Read by every thread creating its engine, written by seed(); relaxed is enough because the
    // seed is a plain value with no data published alongside it.
    std::atomic<conjure::u64> sProcessSeed = DEFAULT_SEED;

    struct thread_engine {
        engine generator = engine(DEFAULT_SEED);
        bool initialised = false;
    };

    thread_engine &this_thread_engine() noexcept {
        thread_local thread_engine sEngine = {};
        return sEngine;
    }
}

engine &conjure::core::random::default_engine() noexcept {
    thread_engine &state = this_thread_engine();
    if (!state.initialised) {
        state.generator.seed(sProcessSeed.load(std::memory_order_relaxed));
        state.initialised = true;
    }
    return state.generator;
}

void conjure::core::random::seed(conjure::u64 value) noexcept {
    sProcessSeed.store(value, std::memory_order_relaxed);

    thread_engine &state = this_thread_engine();
    state.generator.seed(value);
    state.initialised = true;
}

conjure::u64 conjure::core::random::current_seed() noexcept {
    return sProcessSeed.load(std::memory_order_relaxed);
}


/*===========================================================================*/
/*===========================================================================*/
// random_engine

random_engine::random_engine() :
    mEngine(DEFAULT_SEED),
    mSeed(DEFAULT_SEED) {}

random_engine::random_engine(conjure::u64 seedValue) :
    mEngine(seedValue),
    mSeed(seedValue) {}

random_engine::~random_engine() {}

void random_engine::seed(conjure::u64 seedValue) noexcept {
    mEngine.seed(seedValue);
    mSeed = seedValue;
}
