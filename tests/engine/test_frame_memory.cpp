#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/types.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/frame_memory.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

// frame_memory.h (docs/design/06_engine_loop.md 2.6, D6): the per-frame arena facade: alignment, parallel
// allocation without overlap, reset, overflow fallback and statistics.

using namespace conjure;
using namespace conjure::engine;
using namespace conjure::testing;

namespace {

    [[nodiscard]] memory_settings small_settings(usize arenaBytes, usize blockBytes) {
        memory_settings settings = {};
        settings.frameArenaBytes = arenaBytes;
        settings.frameThreadBlockBytes = blockBytes;
        return settings;
    }

    [[nodiscard]] bool overlaps(std::span<const std::byte> a, std::span<const std::byte> b) noexcept {
        const std::byte *aEnd = a.data() + a.size();
        const std::byte *bEnd = b.data() + b.size();
        return a.data() < bEnd && b.data() < aEnd;
    }
}

TEST_SUITE("engine/frame_memory") {

    TEST_CASE("regression - review finding 18: reset while a scheduler phase is in flight asserts and is refused") {
        frame_memory memory(small_settings(64 * KIB, 8 * KIB));
        const u64 epoch = memory.epoch();
        CHECK_FALSE(memory.is_phase_in_flight());
        memory.set_phase_in_flight(true);
        CHECK(memory.is_phase_in_flight());
        CONJURE_CHECK_ASSERTS(memory.reset());
        CHECK(memory.epoch() == epoch);
        CHECK(memory.reset_count() == 0);
        memory.set_phase_in_flight(false);
        memory.reset();
        CHECK(memory.epoch() != epoch);
        CHECK(memory.reset_count() == 1);
    }

    TEST_CASE("frame_memory - alignment is honoured from 1 to 256") {
        frame_memory memory(small_settings(64 * KIB, 8 * KIB));
        for (usize alignment = 1; alignment <= 256; alignment *= 2) {
            void *block = memory.allocate(24, alignment);
            REQUIRE(block != nullptr);
            CHECK(reinterpret_cast<std::uintptr_t>(block) % alignment == 0);
        }
        CHECK(memory.used_bytes() > 0);
    }

    TEST_CASE("frame_memory - eight jobs allocate without overlap") {
        core::jobs::job_system jobs = {};
        core::jobs::job_system_desc desc = {};
        desc.workerCount = 8;
        CONJURE_REQUIRE_OK(jobs.start(desc));

        frame_memory memory(small_settings(4 * MIB, 64 * KIB));
        constexpr u32 JOB_COUNT = 8;
        constexpr u32 PER_JOB = 200;
        std::array<std::vector<std::span<std::byte>>, JOB_COUNT> results = {};
        std::vector<core::jobs::job_handle> handles = {};
        for (u32 job = 0; job < JOB_COUNT; ++job) {
            std::vector<std::span<std::byte>> *target = &results[job];
            handles.push_back(jobs.schedule([&memory, target, job]() {
                for (u32 index = 0; index < PER_JOB; ++index) {
                    const usize size = 16 + (index * 7 + job) % 240;
                    std::span<std::byte> block = memory.allocate_span<std::byte>(size);
                    REQUIRE(block.size() == size);
                    std::memset(block.data(), static_cast<int>(job + 1), size);
                    target->push_back(block);
                }
            }));
        }
        for (const core::jobs::job_handle &handle : handles) {
            jobs.wait(handle);
        }
        jobs.stop();

        std::vector<std::span<std::byte>> all = {};
        for (u32 job = 0; job < JOB_COUNT; ++job) {
            for (const std::span<std::byte> &block : results[job]) {
                CHECK(std::all_of(block.begin(), block.end(), [job](std::byte value) {
                    return value == static_cast<std::byte>(job + 1);
                }));
                all.push_back(block);
            }
        }
        std::sort(all.begin(), all.end(), [](const std::span<std::byte> &a, const std::span<std::byte> &b) {
            return a.data() < b.data();
        });
        for (usize index = 1; index < all.size(); ++index) {
            CHECK_FALSE(overlaps(all[index - 1], all[index]));
        }
        CHECK(memory.stats().overflowCount == 0);
    }

    TEST_CASE("frame_memory - reset reclaims everything and bumps the epoch") {
        frame_memory memory(small_settings(64 * KIB, 8 * KIB));
        const u64 epoch = memory.epoch();
        CHECK(memory.allocate(1000) != nullptr);
        CHECK(memory.used_bytes() >= 1000);
        memory.reset();
        CHECK(memory.used_bytes() == 0);
        CHECK(memory.epoch() != epoch);
        CHECK(memory.reset_count() == 1);
        memory.reset();
        CHECK(memory.reset_count() == 2);
    }

    TEST_CASE("frame_memory - overflow falls back to the heap and counts, warning once per interval") {
        log_capture logs = {};
        frame_memory memory(small_settings(4 * KIB, 1 * KIB));
        std::vector<void *> blocks = {};
        for (u32 index = 0; index < 64; ++index) {
            void *block = memory.allocate(512, 16);
            REQUIRE(block != nullptr);
            std::memset(block, 0xAB, 512);
            blocks.push_back(block);
        }
        const frame_memory_stats before = memory.stats();
        CHECK(before.overflowCount > 0);
        CHECK(before.capacityBytes == 4 * KIB);

        memory.reset();
        CHECK(logs.count(core::log_level::WARN) == 1);
        for (u32 frame = 0; frame < frame_memory::OVERFLOW_WARNING_INTERVAL - 1; ++frame) {
            CHECK(memory.allocate(8 * KIB) != nullptr);
            memory.reset();
        }
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK(memory.allocate(8 * KIB) != nullptr);
        memory.reset();
        CHECK(logs.count(core::log_level::WARN) == 2);
    }

    TEST_CASE("frame_memory - stats report the high-water mark") {
        frame_memory memory(small_settings(64 * KIB, 8 * KIB));
        CHECK(memory.allocate(3000, 16) != nullptr);
        CHECK(memory.allocate(5000, 16) != nullptr);
        const frame_memory_stats stats = memory.stats();
        CHECK(stats.highWaterBytes >= 8000);
        CHECK(stats.highWaterBytes <= stats.capacityBytes);
        CHECK(stats.overflowCount == 0);
        memory.reset();
        CHECK(memory.stats().highWaterBytes >= 8000);
    }

    TEST_CASE("frame_memory - construct places a trivially destructible object") {
        frame_memory memory(small_settings(64 * KIB, 8 * KIB));
        struct point {
            f32 x = 0.0f;
            f32 y = 0.0f;
        };
        point *value = memory.construct<point>(1.0f, 2.0f);
        REQUIRE(value != nullptr);
        CHECK(value->x == 1.0f);
        CHECK(value->y == 2.0f);
        CHECK(&memory.allocator() != nullptr);
    }
}
