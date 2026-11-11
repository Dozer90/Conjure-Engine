#include "conjure/testing/doctest.h"

#include "conjure/core/types.h"
#include "conjure/engine/frame_stats.h"

#include <cmath>

// frame_stats.h (docs/design/06_engine_loop.md 2.7): the 240-entry ring of frame samples.

using namespace conjure;
using namespace conjure::engine;

namespace {

    [[nodiscard]] frame_sample sample_of(f32 milliseconds, bool dropped = false) {
        frame_sample sample = {};
        sample.frameMilliseconds = milliseconds;
        sample.droppedFixedTime = dropped;
        return sample;
    }
}

TEST_SUITE("engine/frame_stats") {

    TEST_CASE("frame_stats - starts empty") {
        const frame_stats stats = {};
        CHECK(stats.sample_count() == 0);
        CHECK(stats.history().empty());
        CHECK(stats.total_committed() == 0);
        CHECK(stats.latest() == frame_sample{});
        CHECK(stats.average_frame_milliseconds(10) == 0.0f);
    }

    TEST_CASE("frame_stats - the ring wraps at 240") {
        frame_stats stats = {};
        for (u32 frame = 0; frame < 300; ++frame) {
            stats.commit(sample_of(static_cast<f32>(frame)));
        }
        CHECK(stats.sample_count() == frame_stats::HISTORY_SIZE);
        CHECK(stats.history().size() == frame_stats::HISTORY_SIZE);
        CHECK(stats.total_committed() == 300);
        CHECK(stats.latest().frameMilliseconds == 299.0f);
        CHECK(stats.sample_at(1).frameMilliseconds == 298.0f);
        CHECK(stats.sample_at(frame_stats::HISTORY_SIZE - 1).frameMilliseconds == 60.0f);
        CHECK(stats.sample_at(frame_stats::HISTORY_SIZE) == frame_sample{});
    }

    TEST_CASE("frame_stats - latest is the last committed sample") {
        frame_stats stats = {};
        stats.commit(sample_of(1.0f));
        stats.commit(sample_of(2.0f));
        stats.commit(sample_of(3.0f));
        CHECK(stats.latest().frameMilliseconds == 3.0f);
        CHECK(stats.sample_at(2).frameMilliseconds == 1.0f);
        CHECK(stats.sample_at(3) == frame_sample{});
    }

    TEST_CASE("frame_stats - the average covers the requested recent frames") {
        frame_stats stats = {};
        for (u32 frame = 1; frame <= 10; ++frame) {
            stats.commit(sample_of(static_cast<f32>(frame)));
        }
        CHECK(std::abs(stats.average_frame_milliseconds(4) - 8.5f) < 1.0e-5f);
        CHECK(std::abs(stats.average_frame_milliseconds(10) - 5.5f) < 1.0e-5f);
        CHECK(std::abs(stats.average_frame_milliseconds(1000) - 5.5f) < 1.0e-5f);
        CHECK(stats.average_frame_milliseconds(0) == 0.0f);
    }

    TEST_CASE("frame_stats - dropped fixed time is counted and clear resets everything") {
        frame_stats stats = {};
        stats.commit(sample_of(1.0f, true));
        stats.commit(sample_of(1.0f, false));
        stats.commit(sample_of(1.0f, true));
        CHECK(stats.total_dropped_fixed_steps() == 2);
        stats.clear();
        CHECK(stats.sample_count() == 0);
        CHECK(stats.total_committed() == 0);
        CHECK(stats.total_dropped_fixed_steps() == 0);
        CHECK(stats.latest() == frame_sample{});
    }
}
