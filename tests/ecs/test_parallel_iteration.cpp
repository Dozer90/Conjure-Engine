#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/query.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/view.h"
#include "conjure/ecs/world.h"

#include <atomic>
#include <mutex>
#include <unordered_set>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

namespace {
    /// <summary>
    /// A standalone system context over a world, outside the scheduler.
    /// </summary>
    struct context_harness {
        frame_info frame = {};
        system_desc desc = {};
        system_scratch scratch = {};
        command_buffer commands;        // no default: bound to the world
        system_context context;         // no default: built from the parts above

        context_harness(
          world &target,
          core::jobs::job_system *jobs,
          u32 minEntitiesPerJob,
          bool parallelChunks = true
        ) :
            commands(target),
            context(
              target,
              frame,
              0,
              target.bump_version(),
              commands,
              jobs,
              desc,
              scratch,
              parallelChunks,
              minEntitiesPerJob
            ) {
            desc.name = core::string_id("test.harness");
            desc.writes = ~component_mask{};
            desc.reads = ~component_mask{};
        }
    };

    query position_query(ecs_fixture &fixture) {
        query_desc desc = {};
        desc.with = fixture.mask<position>();
        return fixture.world.create_query(desc);
    }

    f32 integrate_inline(ecs_fixture &fixture, std::span<const entity> ids) {
        f32 sum = 0.0f;
        for (const entity id : ids) {
            const velocity *speed = fixture.world.get<velocity>(id);
            position *place = fixture.world.get<position>(id);
            place->x += speed->x;
            sum += place->x;
        }
        return sum;
    }
}

TEST_SUITE("ecs/parallel_iteration") {

    TEST_CASE("parallel_for_chunks - sum of written column equals inline result [parallel]") {
        for_each_job_mode([](core::jobs::job_system *jobs) {
            ecs_fixture fixture = {};
            world &world = fixture.world;
            std::vector<entity> ids(10000);
            CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position, velocity>(), ids));
            for (u32 index = 0; index < ids.size(); ++index) {
                CONJURE_REQUIRE_OK(world.set(ids[index], velocity{static_cast<f32>(index % 7), 0.0f, 0.0f}));
            }

            // Reference: a sequential pass over a copy of the same starting state.
            std::vector<f32> expected(ids.size());
            for (u32 index = 0; index < ids.size(); ++index) {
                expected[index] = static_cast<f32>(index % 7);
            }

            context_harness harness(world, jobs, 256);
            query positions = position_query(fixture);
            std::atomic<u32> rows = 0;
            harness.context.parallel_for_chunks(positions, [&](chunk_view &chunk, command_buffer &) {
                const std::span<const velocity> velocities = chunk.get<const velocity>();
                std::span<position> result = chunk.get<position>();
                for (u32 row = 0; row < chunk.count(); ++row) {
                    result[row].x += velocities[row].x;
                }
                rows.fetch_add(chunk.count(), std::memory_order_relaxed);
            });
            CHECK(rows.load() == 10000);
            CHECK(harness.context.last_grain_count() > 1);
            for (u32 index = 0; index < ids.size(); ++index) {
                CHECK(world.get<position>(ids[index])->x == expected[index]);
            }
            CHECK(harness.commands.empty());
            CHECK_FALSE(world.is_locked());
        });
    }

    TEST_CASE("parallel_for_chunks - each chunk visited once [parallel]") {
        for_each_job_mode([](core::jobs::job_system *jobs) {
            ecs_fixture fixture = {};
            world &world = fixture.world;
            std::vector<entity> a(3000);
            std::vector<entity> b(2000);
            CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), a));
            CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position, health>(), b));

            context_harness harness(world, jobs, 100);
            query positions = position_query(fixture);
            std::mutex mutex = {};
            std::vector<u32> visits(5000, 0);
            u32 chunks = 0;
            harness.context.parallel_for_chunks(positions, [&](chunk_view &chunk, command_buffer &) {
                const std::scoped_lock lock(mutex);
                ++chunks;
                for (const entity id : chunk.entities()) {
                    ++visits[id.index];
                }
            });
            for (u32 index = 0; index < visits.size(); ++index) {
                CAPTURE(index);
                CHECK(visits[index] == 1);
            }
            CHECK(chunks == positions.chunk_count());
        });
    }

    TEST_CASE("parallel_for_chunks - grain buffers apply in grain order [parallel]") {
        for_each_job_mode([](core::jobs::job_system *jobs) {
            ecs_fixture fixture = {};
            world &world = fixture.world;
            std::vector<entity> ids(4000);
            CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), ids));

            // Every grain records one marker entity carrying the grain's first row index; after the
            // flush the markers must appear in ascending grain order whatever worker ran which grain.
            context_harness harness(world, jobs, 50);
            query positions = position_query(fixture);
            harness.context.parallel_for_chunks(positions, [&](chunk_view &chunk, command_buffer &commands) {
                const entity marker = commands.create_entity();
                commands.add<health>(marker, health{static_cast<i32>(chunk.entities()[0].index), 0});
            });
            const u32 grains = harness.context.last_grain_count();
            CHECK(grains > 1);
            CHECK(harness.commands.command_count() == 2 * positions.chunk_count());

            // Markers are created in command order, so their archetype rows follow it.
            CHECK(world.flush(harness.commands).applied == 2 * positions.chunk_count());
            std::vector<i32> order = {};
            world.view<const health>().for_each([&](entity, const health &marker) {
                order.push_back(marker.value);
            });
            REQUIRE(order.size() == positions.chunk_count());
            for (usize index = 1; index < order.size(); ++index) {
                CHECK(order[index - 1] < order[index]);
            }
        });
    }

    TEST_CASE("parallel_for_chunks - create_entity from jobs yields unique ids [parallel]") {
        for_each_job_mode([](core::jobs::job_system *jobs) {
            ecs_fixture fixture = {};
            world &world = fixture.world;
            std::vector<entity> ids(5000);
            CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), ids));

            context_harness harness(world, jobs, 64);
            query positions = position_query(fixture);
            std::mutex mutex = {};
            std::vector<entity> spawned = {};
            harness.context.parallel_for_chunks(positions, [&](chunk_view &chunk, command_buffer &commands) {
                std::vector<entity> local = {};
                for (u32 row = 0; row < chunk.count(); ++row) {
                    const entity child = commands.create_entity();
                    commands.add<link>(child, link{chunk.entities()[row], NULL_ENTITY});
                    local.push_back(child);
                }
                const std::scoped_lock lock(mutex);
                spawned.insert(spawned.end(), local.begin(), local.end());
            });
            REQUIRE(spawned.size() == 5000);
            std::unordered_set<u64> unique = {};
            for (const entity id : spawned) {
                CHECK(unique.insert(id.to_u64()).second);
                CHECK_FALSE(world.is_alive(id));
            }
            CHECK(world.flush(harness.commands).applied == 10000);
            CHECK(world.entity_count() == 10000);
            for (const entity id : spawned) {
                CHECK(world.is_alive(id));
                CHECK(world.has<link>(id));
            }
        });
    }

    TEST_CASE("parallel_for_chunks - grain size follows minEntitiesPerJob [parallel]") {
        for_each_job_mode([](core::jobs::job_system *jobs) {
            ecs_fixture fixture = {};
            world &world = fixture.world;
            std::vector<entity> ids(4000);
            CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), ids));
            query positions = position_query(fixture);
            const u32 chunks = positions.chunk_count();
            const u32 capacity = world.archetypes()[1]->layout().capacity;
            REQUIRE(chunks >= 4);

            const auto grains_for = [&](u32 minEntitiesPerJob, bool parallelChunks) {
                context_harness harness(world, jobs, minEntitiesPerJob, parallelChunks);
                harness.context.parallel_for_chunks(positions, [](chunk_view &, command_buffer &) {});
                return harness.context.last_grain_count();
            };

            // One chunk per grain when a chunk already holds the minimum.
            CHECK(grains_for(1, true) == chunks);
            CHECK(grains_for(capacity, true) == chunks);
            // Two chunks per grain.
            CHECK(grains_for(capacity * 2, true) == (chunks + 1) / 2);
            // Everything in one grain.
            CHECK(grains_for(capacity * chunks, true) == 1);
            CHECK(grains_for(1u << 30, true) == 1);
            // Disabling chunk parallelism keeps the grain count: only the dispatch changes.
            CHECK(grains_for(1, false) == chunks);
        });
    }

    TEST_CASE("parallel_for_chunks - empty query runs nothing") {
        ecs_fixture fixture = {};
        context_harness harness(fixture.world, nullptr, 1024);
        query positions = position_query(fixture);
        u32 calls = 0;
        harness.context.parallel_for_chunks(positions, [&](chunk_view &, command_buffer &) {
            ++calls;
        });
        CHECK(calls == 0);
        CHECK(harness.context.last_grain_count() == 0);
    }

    TEST_CASE("view - parallel_for_each_chunk forwards to the context [parallel]") {
        for_each_job_mode([](core::jobs::job_system *jobs) {
            ecs_fixture fixture = {};
            world &world = fixture.world;
            std::vector<entity> ids(1000);
            CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position, velocity>(), ids));
            for (const entity id : ids) {
                CONJURE_REQUIRE_OK(world.set(id, velocity{2.0f, 0.0f, 0.0f}));
            }
            context_harness harness(world, jobs, 128);
            view<position, const velocity> moving = world.view<position, const velocity>();
            moving.parallel_for_each_chunk(harness.context, [](chunk_view &chunk, command_buffer &) {
                const std::span<const velocity> velocities = chunk.get<const velocity>();
                std::span<position> positions = chunk.get<position>();
                for (u32 row = 0; row < chunk.count(); ++row) {
                    positions[row].x += velocities[row].x;
                }
            });
            for (const entity id : ids) {
                CHECK(world.get<position>(id)->x == 2.0f);
            }
            CHECK(integrate_inline(fixture, ids) == 4.0f * 1000.0f);
        });
    }
}
