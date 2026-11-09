#include "conjure/testing/doctest.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/entity.h"
#include "ecs/entity_allocator.h"

#include <algorithm>
#include <array>
#include <functional>
#include <mutex>
#include <unordered_set>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;

TEST_SUITE("ecs/entity_allocator") {

    TEST_CASE("entity_allocator - reserve returns unique ids") {
        entity_allocator allocator = {};
        std::array<entity, 100> ids = {};
        CONJURE_REQUIRE_OK(allocator.reserve(ids));

        std::unordered_set<u64> seen = {};
        for (const entity id : ids) {
            CHECK(id.generation == 1);
            CHECK(seen.insert(id.to_u64()).second);
        }
        CHECK(allocator.peak_index() == 100);
        CHECK(allocator.free_count() == 0);
    }

    TEST_CASE("entity_allocator - release bumps generation and skips zero") {
        entity_allocator allocator = {};
        std::array<entity, 1> first = {};
        CONJURE_REQUIRE_OK(allocator.reserve(first));
        allocator.release(first[0]);
        CHECK(allocator.free_count() == 1);

        std::array<entity, 1> second = {};
        CONJURE_REQUIRE_OK(allocator.reserve(second));
        CHECK(second[0].index == first[0].index);
        CHECK(second[0].generation == first[0].generation + 1);
        CHECK(second[0].generation != 0);
        CHECK(allocator.peak_index() == 1);
    }

    TEST_CASE("entity_allocator - free list is LIFO") {
        entity_allocator allocator = {};
        std::array<entity, 3> ids = {};
        CONJURE_REQUIRE_OK(allocator.reserve(ids));
        allocator.release(ids[0]);
        allocator.release(ids[2]);

        std::array<entity, 1> next = {};
        CONJURE_REQUIRE_OK(allocator.reserve(next));
        CHECK(next[0].index == ids[2].index);
    }

    TEST_CASE("entity_allocator - generation wrap retires index") {
        entity_allocator allocator = {};
        std::array<entity, 2> ids = {};
        CONJURE_REQUIRE_OK(allocator.reserve(ids));

        // Pretend index 0 has reached the last issuable generation.
        allocator.release(entity{ids[0].index, ARCHIVE_LOCAL_GENERATION - 1});
        CHECK(allocator.retired_count() == 1);
        CHECK(allocator.free_count() == 0);

        std::array<entity, 1> next = {};
        CONJURE_REQUIRE_OK(allocator.reserve(next));
        CHECK(next[0].index != ids[0].index);
        CHECK(next[0].generation == 1);

        // A generation one below the retirement line is still reusable.
        allocator.release(entity{ids[1].index, ARCHIVE_LOCAL_GENERATION - 2});
        CHECK(allocator.free_count() == 1);
        std::array<entity, 1> last = {};
        CONJURE_REQUIRE_OK(allocator.reserve(last));
        CHECK(last[0] == entity{ids[1].index, ARCHIVE_LOCAL_GENERATION - 1});
    }

    TEST_CASE("entity_allocator - return_unused keeps generation") {
        entity_allocator allocator = {};
        std::array<entity, 4> ids = {};
        CONJURE_REQUIRE_OK(allocator.reserve(ids));
        allocator.release(ids[1]);
        std::array<entity, 1> reused = {};
        CONJURE_REQUIRE_OK(allocator.reserve(reused));
        CHECK(reused[0].generation == 2);

        allocator.return_unused(reused);
        std::array<entity, 1> again = {};
        CONJURE_REQUIRE_OK(allocator.reserve(again));
        CHECK(again[0] == reused[0]);
    }

    TEST_CASE("entity_allocator - reserve of zero ids takes nothing") {
        entity_allocator allocator = {};
        CONJURE_REQUIRE_OK(allocator.reserve(std::span<entity>()));
        CHECK(allocator.peak_index() == 0);
        CHECK(allocator.free_count() == 0);
        CHECK(allocator.free_slots().empty());

        std::array<entity, 3> ids = {};
        CONJURE_REQUIRE_OK(allocator.reserve(ids));
        CONJURE_REQUIRE_OK(allocator.reserve(std::span<entity>()));
        CHECK(allocator.peak_index() == 3);
        allocator.release(ids[0]);
        CHECK(allocator.free_count() == 1);
        CHECK(allocator.free_slots().size() == 1);
        CHECK(allocator.free_slots()[0] == entity{ids[0].index, 2});
    }

    TEST_CASE("entity_allocator - concurrent reserve from 8 threads is unique [parallel]") {
        entity_allocator allocator = {};
        std::array<entity, 16> warm = {};
        CONJURE_REQUIRE_OK(allocator.reserve(warm));
        allocator.release(warm[3]);
        allocator.release(warm[9]);

        core::jobs::job_system jobs = {};
        core::jobs::job_system_desc desc = {};
        desc.workerCount = 8;
        REQUIRE(jobs.start(desc).has_value());

        constexpr usize PER_THREAD = 500;
        std::array<std::vector<entity>, 8> results = {};
        std::vector<std::function<void()>> tasks = {};
        for (usize thread = 0; thread < 8; ++thread) {
            results[thread].resize(PER_THREAD);
            tasks.push_back([&allocator, &results, thread]() {
                // Reserve in blocks of 64 like a command buffer does.
                for (usize begin = 0; begin < PER_THREAD; begin += 64) {
                    const usize count = std::min<usize>(64, PER_THREAD - begin);
                    const core::result<void> reserved =
                      allocator.reserve(std::span<entity>(results[thread].data() + begin, count));
                    CHECK(reserved.has_value());
                }
            });
        }
        const core::jobs::job_handle handle = jobs.schedule(std::span<const std::function<void()>>(tasks));
        jobs.wait(handle);
        jobs.stop();

        std::unordered_set<u64> seen = {};
        for (const std::vector<entity> &result : results) {
            for (const entity id : result) {
                CHECK(seen.insert(id.to_u64()).second);
            }
        }
        CHECK(seen.size() == 8 * PER_THREAD);
        CHECK(allocator.free_count() == 0);
    }
}
