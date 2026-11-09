#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/log.h"
#include "conjure/core/string_id.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/world.h"
#include "conjure/ecs/world_hash.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

namespace {
    /// <summary>
    /// A system that appends its name to a shared log when it runs.
    /// </summary>
    system_desc logging_system(
      std::string_view name,
      std::vector<std::string> &log,
      std::mutex &mutex,
      system_phase phase = system_phase::UPDATE
    ) {
        system_desc desc = {};
        desc.name = core::string_id(name);
        desc.phase = phase;
        desc.run = [name = std::string(name), &log, &mutex](system_context &) {
            const std::scoped_lock lock(mutex);
            log.push_back(name);
        };
        return desc;
    }

    usize index_of(const std::vector<std::string> &log, std::string_view name) {
        for (usize index = 0; index < log.size(); ++index) {
            if (log[index] == name) {
                return index;
            }
        }
        return log.size();
    }

    core::string_id sid(std::string_view text) {
        return core::string_id(text);
    }
}

TEST_SUITE("ecs/scheduler") {

    TEST_CASE("system_id - default is invalid") {
        CHECK_FALSE(system_id{}.is_valid());
        CHECK(system_id{0, 1}.is_valid());
        CHECK(system_id{1, 1} == system_id{1, 1});
        const scheduler_desc desc = {};
        CHECK(desc.parallelSystems);
        CHECK(desc.parallelChunks);
        CHECK(desc.minEntitiesPerJob == 1024);
    }

    TEST_CASE("scheduler - after before ordering") {
        for_each_job_mode([](core::jobs::job_system *jobs) {
            ecs_fixture fixture = {};
            scheduler scheduler(fixture.world, jobs);
            std::vector<std::string> log = {};
            std::mutex mutex = {};

            // Registered out of order; edges force c -> a -> b, d anywhere after a.
            system_desc b = logging_system("b", log, mutex);
            b.after.push_back(sid("a"));
            system_desc a = logging_system("a", log, mutex);
            a.after.push_back(sid("c"));
            system_desc d = logging_system("d", log, mutex);
            d.after.push_back(sid("a"));
            system_desc c = logging_system("c", log, mutex);
            c.before.push_back(sid("a"));
            // All four write the same component so no two share a stage: the order is observable.
            for (system_desc *desc : {&a, &b, &c, &d}) {
                desc->writes.set(fixture.id<position>());
            }
            CONJURE_REQUIRE_OK(scheduler.add_system(std::move(b)));
            CONJURE_REQUIRE_OK(scheduler.add_system(std::move(a)));
            CONJURE_REQUIRE_OK(scheduler.add_system(std::move(d)));
            CONJURE_REQUIRE_OK(scheduler.add_system(std::move(c)));
            CHECK(scheduler.system_count() == 4);

            const frame_info frame = {};
            scheduler.run_phase(system_phase::UPDATE, frame);
            REQUIRE(log.size() == 4);
            CHECK(index_of(log, "c") < index_of(log, "a"));
            CHECK(index_of(log, "a") < index_of(log, "b"));
            CHECK(index_of(log, "a") < index_of(log, "d"));
            // Registration order breaks the tie between b and d.
            CHECK(index_of(log, "b") < index_of(log, "d"));
            CHECK(scheduler.stages(system_phase::UPDATE).size() == 4);
        });
    }

    TEST_CASE("scheduler - cycle is SYSTEM_CYCLE and not added") {
        ecs_fixture fixture = {};
        scheduler scheduler(fixture.world, nullptr);
        std::vector<std::string> log = {};
        std::mutex mutex = {};
        system_desc a = logging_system("a", log, mutex);
        a.after.push_back(sid("b"));
        system_desc b = logging_system("b", log, mutex);
        b.after.push_back(sid("a"));
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(a)));
        CONJURE_CHECK_ERROR(scheduler.add_system(std::move(b)), ecs_error::SYSTEM_CYCLE);
        CHECK(scheduler.system_count() == 1);

        // A self edge is a cycle as well; a cycle through 'before' too.
        system_desc self = logging_system("self", log, mutex);
        self.before.push_back(sid("self"));
        CONJURE_CHECK_ERROR(scheduler.add_system(std::move(self)), ecs_error::SYSTEM_CYCLE);
        system_desc c = logging_system("c", log, mutex);
        c.before.push_back(sid("a"));
        c.after.push_back(sid("a"));
        CONJURE_CHECK_ERROR(scheduler.add_system(std::move(c)), ecs_error::SYSTEM_CYCLE);
        CHECK(scheduler.system_count() == 1);

        const frame_info frame = {};
        scheduler.run_phase(system_phase::UPDATE, frame);
        CHECK(log == std::vector<std::string>{"a"});
        CONJURE_REQUIRE_OK(scheduler.finalise());
    }

    TEST_CASE("scheduler - unknown name ignored") {
        ecs_fixture fixture = {};
        scheduler scheduler(fixture.world, nullptr);
        std::vector<std::string> log = {};
        std::mutex mutex = {};
        system_desc a = logging_system("a", log, mutex);
        a.after.push_back(sid("module.compiled_out"));
        a.before.push_back(sid("also.missing"));
        testing::log_capture logs = {};
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(a)));
        const frame_info frame = {};
        scheduler.run_phase(system_phase::UPDATE, frame);
        CHECK(log == std::vector<std::string>{"a"});
#if !CONJURE_DISTRIBUTION
        CHECK(logs.count(core::log_level::DEBUG) >= 2);
#endif
        CHECK(logs.count(core::log_level::WARN) == 0);
    }

    TEST_CASE("scheduler - two readers share a stage") {
        ecs_fixture fixture = {};
        scheduler scheduler(fixture.world, nullptr);
        std::vector<std::string> log = {};
        std::mutex mutex = {};
        system_desc a = logging_system("a", log, mutex);
        a.reads = fixture.mask<position>();
        system_desc b = logging_system("b", log, mutex);
        b.reads = fixture.mask<position, velocity>();
        system_desc c = logging_system("c", log, mutex);
        c.writes = fixture.mask<health>();
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(a)));
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(b)));
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(c)));

        const std::span<const stage_info> stages = scheduler.stages(system_phase::UPDATE);
        REQUIRE(stages.size() == 1);
        CHECK(stages[0].systems.size() == 3);
        CHECK_FALSE(stages[0].exclusive);
    }

    TEST_CASE("scheduler - reader and writer split stages") {
        ecs_fixture fixture = {};
        scheduler scheduler(fixture.world, nullptr);
        std::vector<std::string> log = {};
        std::mutex mutex = {};
        system_desc reader = logging_system("reader", log, mutex);
        reader.reads = fixture.mask<position>();
        system_desc writer = logging_system("writer", log, mutex);
        writer.writes = fixture.mask<position>();
        system_desc otherWriter = logging_system("other", log, mutex);
        otherWriter.writes = fixture.mask<velocity>();
        system_desc secondWriter = logging_system("second", log, mutex);
        secondWriter.writes = fixture.mask<position>();
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(reader)));
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(writer)));
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(otherWriter)));
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(secondWriter)));

        const std::span<const stage_info> stages = scheduler.stages(system_phase::UPDATE);
        REQUIRE(stages.size() == 3);
        CHECK(stages[0].systems.size() == 1);     // reader
        CHECK(stages[1].systems.size() == 2);     // writer + other (disjoint writes)
        CHECK(stages[2].systems.size() == 1);     // second writer of position
        const system_desc *first = scheduler.system(stages[0].systems[0]);
        REQUIRE(first != nullptr);
        CHECK(first->name == sid("reader"));
    }

    TEST_CASE("scheduler - exclusive system alone with unlocked world") {
        for_each_job_mode([](core::jobs::job_system *jobs) {
            ecs_fixture fixture = {};
            world &world = fixture.world;
            scheduler scheduler(world, jobs);
            std::vector<std::string> log = {};
            std::mutex mutex = {};
            system_desc a = logging_system("a", log, mutex);
            a.reads = fixture.mask<position>();
            system_desc exclusive = {};
            exclusive.name = sid("exclusive");
            exclusive.flags = system_flags::EXCLUSIVE;
            exclusive.run = [&](system_context &context) {
                CHECK_FALSE(context.world().is_locked());
                CHECK(std::this_thread::get_id() == context.world().owner_thread());
                // Direct structural calls are allowed here.
                const core::result<entity> created = context.world().create_entity();
                CHECK(created.has_value());
                const std::scoped_lock lock(mutex);
                log.push_back("exclusive");
            };
            system_desc b = logging_system("b", log, mutex);
            b.reads = fixture.mask<position>();
            b.run = [&](system_context &context) {
                CHECK(context.world().is_locked());
                const std::scoped_lock lock(mutex);
                log.push_back("b");
            };
            CONJURE_REQUIRE_OK(scheduler.add_system(std::move(a)));
            CONJURE_REQUIRE_OK(scheduler.add_system(std::move(exclusive)));
            CONJURE_REQUIRE_OK(scheduler.add_system(std::move(b)));

            const std::span<const stage_info> stages = scheduler.stages(system_phase::UPDATE);
            REQUIRE(stages.size() == 3);
            CHECK(stages[1].exclusive);
            CHECK(stages[1].systems.size() == 1);
            CHECK_FALSE(stages[0].exclusive);
            CHECK_FALSE(stages[2].exclusive);

            const frame_info frame = {};
            scheduler.run_phase(system_phase::UPDATE, frame);
            CHECK(log == std::vector<std::string>{"a", "exclusive", "b"});
            CHECK(world.entity_count() == 1);
        });
    }

    TEST_CASE("scheduler - main thread flag runs on caller thread") {
        for_each_job_mode([](core::jobs::job_system *jobs) {
            ecs_fixture fixture = {};
            scheduler scheduler(fixture.world, jobs);
            const std::thread::id caller = std::this_thread::get_id();
            std::atomic<u32> mainThreadRuns = 0;
            std::atomic<u32> parallelRuns = 0;
            for (u32 index = 0; index < 4; ++index) {
                system_desc desc = {};
                desc.name = sid("main." + std::to_string(index));
                desc.flags = system_flags::MAIN_THREAD;
                desc.run = [&, caller](system_context &) {
                    CHECK(std::this_thread::get_id() == caller);
                    ++mainThreadRuns;
                };
                CONJURE_REQUIRE_OK(scheduler.add_system(std::move(desc)));
                system_desc parallel = {};
                parallel.name = sid("parallel." + std::to_string(index));
                parallel.run = [&](system_context &) {
                    ++parallelRuns;
                };
                CONJURE_REQUIRE_OK(scheduler.add_system(std::move(parallel)));
            }
            // No access conflicts: everything is one stage.
            CHECK(scheduler.stages(system_phase::UPDATE).size() == 1);
            const frame_info frame = {};
            scheduler.run_phase(system_phase::UPDATE, frame);
            CHECK(mainThreadRuns.load() == 4);
            CHECK(parallelRuns.load() == 4);
        });
    }

    TEST_CASE("scheduler - disabled system skipped") {
        ecs_fixture fixture = {};
        scheduler scheduler(fixture.world, nullptr);
        std::vector<std::string> log = {};
        std::mutex mutex = {};
        const core::result<system_id> a = scheduler.add_system(logging_system("a", log, mutex));
        system_desc b = logging_system("b", log, mutex);
        b.flags = system_flags::DISABLED;
        const core::result<system_id> bId = scheduler.add_system(std::move(b));
        CONJURE_REQUIRE_OK(a);
        CONJURE_REQUIRE_OK(bId);
        const frame_info frame = {};
        scheduler.run_phase(system_phase::UPDATE, frame);
        CHECK(log == std::vector<std::string>{"a"});

        CONJURE_REQUIRE_OK(scheduler.set_enabled(bId.value(), true));
        CONJURE_REQUIRE_OK(scheduler.set_enabled(a.value(), false));
        scheduler.run_phase(system_phase::UPDATE, frame);
        CHECK(log == std::vector<std::string>{"a", "b"});
        CHECK(scheduler.stages(system_phase::UPDATE).size() == 1);
        CHECK(scheduler.stages(system_phase::UPDATE)[0].systems.size() == 1);

        CONJURE_REQUIRE_OK(scheduler.remove_system(a.value()));
        CONJURE_CHECK_ERROR(scheduler.remove_system(a.value()), ecs_error::SYSTEM_NOT_FOUND);
        CONJURE_CHECK_ERROR(scheduler.set_enabled(a.value(), true), ecs_error::SYSTEM_NOT_FOUND);
        CONJURE_CHECK_ERROR(scheduler.set_enabled(system_id{}, true), ecs_error::SYSTEM_NOT_FOUND);
        CHECK(scheduler.system(a.value()) == nullptr);
        CHECK(scheduler.system_count() == 1);
        scheduler.run_phase(system_phase::UPDATE, frame);
        CHECK(log == std::vector<std::string>{"a", "b", "b"});

        // A removed slot is reused with a fresh generation; the stale id stays dead.
        const core::result<system_id> c = scheduler.add_system(logging_system("c", log, mutex));
        CONJURE_REQUIRE_OK(c);
        CHECK(c.value().index == a.value().index);
        CHECK(c.value().generation != a.value().generation);
        CHECK(scheduler.system(a.value()) == nullptr);
        CHECK(scheduler.system(c.value()) != nullptr);
    }

    TEST_CASE("scheduler - commands visible from next stage") {
        for_each_job_mode([](core::jobs::job_system *jobs) {
            ecs_fixture fixture = {};
            world &world = fixture.world;
            scheduler scheduler(world, jobs);
            std::atomic<u32> seenBySecond = 0;
            std::atomic<u32> seenByFirst = 0;
            system_desc spawner = make_system<position>(sid("spawner"), system_phase::UPDATE,
              [&](system_context &context) {
                seenByFirst = context.world().entity_count();
                for (u32 index = 0; index < 5; ++index) {
                    const entity created = context.commands().create_entity();
                    context.commands().add<position>(created);
                }
                CHECK(context.world().entity_count() == seenByFirst.load());
            });
            system_desc counter = make_system<position>(sid("counter"), system_phase::UPDATE,
              [&](system_context &context) {
                seenBySecond = context.view<position>().entity_count();
            });
            counter.after.push_back(sid("spawner"));
            CONJURE_REQUIRE_OK(scheduler.add_system(std::move(spawner)));
            CONJURE_REQUIRE_OK(scheduler.add_system(std::move(counter)));
            CHECK(scheduler.stages(system_phase::UPDATE).size() == 2);

            const frame_info frame = {};
            scheduler.run_phase(system_phase::UPDATE, frame);
            CHECK(seenByFirst.load() == 0);
            CHECK(seenBySecond.load() == 5);
            CHECK(world.entity_count() == 5);
            scheduler.run_phase(system_phase::UPDATE, frame);
            CHECK(seenByFirst.load() == 5);
            CHECK(seenBySecond.load() == 10);
        });
    }

    TEST_CASE("scheduler - serial and parallel modes give same result [parallel]") {
        const auto run_simulation = [](core::jobs::job_system *jobs, scheduler_desc options) {
            ecs_fixture fixture = {};
            world &world = fixture.world;
            std::vector<entity> ids(6000);
            CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position, velocity>(), ids));
            for (u32 index = 0; index < ids.size(); ++index) {
                CONJURE_REQUIRE_OK(world.set(ids[index], velocity{static_cast<f32>(index % 13), 1.0f, 0.0f}));
            }
            scheduler scheduler(world, jobs, options);
            CONJURE_REQUIRE_OK(scheduler.add_system(make_system<const velocity, position>(
              sid("integrate"),
              system_phase::UPDATE,
              [](system_context &context) {
                  view<const velocity, position> moving = context.view<const velocity, position>();
                  context.parallel_for_chunks(moving.underlying(), [&](chunk_view &chunk, command_buffer &commands) {
                      const std::span<const velocity> velocities = chunk.get<const velocity>();
                      std::span<position> positions = chunk.get<position>();
                      for (u32 row = 0; row < chunk.count(); ++row) {
                          positions[row].x += velocities[row].x * context.delta_time();
                          positions[row].y += velocities[row].y * context.delta_time();
                          if (positions[row].x > 20.0f) {
                              commands.add<frozen_tag>(chunk.entities()[row]);
                          }
                      }
                  });
              }
            )));
            CONJURE_REQUIRE_OK(scheduler.add_system(make_system<const position, health>(
              sid("damage"),
              system_phase::UPDATE,
              [](system_context &context) {
                  context.view<const position, maybe<health>>().for_each(
                    [&](entity id, const position &place, health *hitPoints) {
                        if (hitPoints == nullptr && place.y > 3.0f) {
                            context.commands().add<health>(id, health{50, 100});
                        } else if (hitPoints != nullptr) {
                            hitPoints->value -= 1;
                        }
                    }
                  );
              }
            )));
            frame_info frame = {};
            frame.deltaTime = 0.5f;
            for (u32 step = 0; step < 12; ++step) {
                scheduler.run_phase(system_phase::UPDATE, frame);
            }
            CHECK(world.instance_count(fixture.id<frozen_tag>()) > 0);
            CHECK(world.instance_count(fixture.id<health>()) == 6000);
            return world_hash(world);
        };

        scheduler_desc serial = {};
        serial.parallelSystems = false;
        serial.parallelChunks = false;
        const u64 reference = run_simulation(nullptr, serial);
        CHECK(run_simulation(nullptr, scheduler_desc{}) == reference);

        core::jobs::job_system jobs = {};
        core::jobs::job_system_desc desc = {};
        desc.workerCount = parallel_worker_count();
        REQUIRE(jobs.start(desc).has_value());
        scheduler_desc parallel = {};
        parallel.minEntitiesPerJob = 256;
        CHECK(run_simulation(&jobs, parallel) == reference);
        scheduler_desc chunksOnly = {};
        chunksOnly.parallelSystems = false;
        chunksOnly.minEntitiesPerJob = 64;
        CHECK(run_simulation(&jobs, chunksOnly) == reference);
        jobs.stop();
    }

    TEST_CASE("scheduler - since_version excludes own writes") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> ids(100);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position, velocity>(), ids));
        scheduler scheduler(world, nullptr);
        std::vector<u64> sinceVersions = {};
        std::vector<u32> changedCounts = {};
        CONJURE_REQUIRE_OK(scheduler.add_system(make_system<position>(sid("writer"), system_phase::UPDATE,
          [&](system_context &context) {
              sinceVersions.push_back(context.since_version());
              u32 changed = 0;
              context.view<position>().changed<position>(context.since_version()).for_each(
                [&](entity, position &value) {
                    value.x += 1.0f;
                    ++changed;
                });
              changedCounts.push_back(changed);
          })));
        CONJURE_REQUIRE_OK(scheduler.add_system(make_system<velocity>(sid("other"), system_phase::UPDATE,
          [&](system_context &context) {
              context.view<velocity>().for_each([](entity, velocity &value) {
                  value.x += 1.0f;
              });
          })));

        const frame_info frame = {};
        scheduler.run_phase(system_phase::UPDATE, frame);
        scheduler.run_phase(system_phase::UPDATE, frame);
        scheduler.run_phase(system_phase::UPDATE, frame);
        REQUIRE(sinceVersions.size() == 3);
        CHECK(sinceVersions[0] == 0);
        CHECK(sinceVersions[1] > sinceVersions[0]);
        CHECK(sinceVersions[2] > sinceVersions[1]);
        REQUIRE(changedCounts.size() == 3);
        CHECK(changedCounts[0] == 100);
        CHECK(changedCounts[1] == 0);
        CHECK(changedCounts[2] == 0);
        // The other system's writes to velocity never count as position changes.
        CHECK(world.get<velocity>(ids[0])->x == 3.0f);
    }

    TEST_CASE("scheduler - undeclared write asserts") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        CONJURE_REQUIRE_OK(world.create_entity(fixture.mask<position, velocity>()));
        scheduler scheduler(world, nullptr);
        u32 asserted = 0;
        CONJURE_REQUIRE_OK(scheduler.add_system(make_system<const position>(sid("sneaky"), system_phase::UPDATE,
          [&](system_context &context) {
              testing::expect_assert guard = {};
              context.view<const position>().for_each_chunk([&](chunk_view &chunk) {
                  static_cast<void>(chunk.get<position>());
              });
              static_cast<void>(context.view<velocity>());
              asserted = guard.count();
          })));
        const frame_info frame = {};
        scheduler.run_phase(system_phase::UPDATE, frame);
        // The chunk view handed out by a system view carries the context, so the undeclared write
        // through chunk.get<position>() asserts as well as the writing view<velocity>().
        CHECK(asserted == 2 * ASSERTS_PER_VIOLATION);
    }

    TEST_CASE("scheduler - timings recorded") {
        ecs_fixture fixture = {};
        scheduler scheduler(fixture.world, nullptr);
        std::vector<std::string> log = {};
        std::mutex mutex = {};
        system_desc a = logging_system("a", log, mutex);
        a.writes = fixture.mask<position>();
        system_desc b = logging_system("b", log, mutex);
        b.writes = fixture.mask<position>();
        system_desc c = logging_system("c", log, mutex, system_phase::LATE_UPDATE);
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(a)));
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(b)));
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(c)));

        CHECK(scheduler.last_timings(system_phase::UPDATE).empty());
        CONJURE_REQUIRE_OK(scheduler.finalise());
        REQUIRE(scheduler.last_timings(system_phase::UPDATE).size() == 2);
        CHECK(scheduler.last_timings(system_phase::UPDATE)[0].name == sid("a"));
        CHECK(scheduler.last_timings(system_phase::UPDATE)[1].name == sid("b"));
        CHECK(scheduler.last_timings(system_phase::UPDATE)[0].microseconds == 0);
        CHECK(scheduler.last_timings(system_phase::LATE_UPDATE).size() == 1);
        CHECK(scheduler.last_timings(system_phase::FRAME_END).empty());
        CHECK(scheduler.last_timings(system_phase::COUNT).empty());

        const frame_info frame = {};
        scheduler.run_phase(system_phase::UPDATE, frame);
        CHECK(log == std::vector<std::string>{"a", "b"});
        const std::span<const system_timing> timings = scheduler.last_timings(system_phase::UPDATE);
        REQUIRE(timings.size() == 2);
        CHECK(timings[0].name == sid("a"));
        CHECK(timings[1].name == sid("b"));
        CHECK(scheduler.last_timings(system_phase::LATE_UPDATE)[0].name == sid("c"));
    }

    TEST_CASE("scheduler - run_phase_filtered selects by flags") {
        ecs_fixture fixture = {};
        scheduler scheduler(fixture.world, nullptr);
        std::vector<std::string> log = {};
        std::mutex mutex = {};
        system_desc always = logging_system("always", log, mutex);
        always.flags = system_flags::RUNS_WHILE_PAUSED;
        system_desc exclusiveAlways = logging_system("exclusive", log, mutex);
        exclusiveAlways.flags = system_flags::RUNS_WHILE_PAUSED | system_flags::EXCLUSIVE;
        system_desc gameplay = logging_system("gameplay", log, mutex);
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(always)));
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(exclusiveAlways)));
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(gameplay)));

        const frame_info frame = {};
        scheduler.run_phase_filtered(system_phase::UPDATE, frame, system_flags::RUNS_WHILE_PAUSED);
        CHECK(log == std::vector<std::string>{"always", "exclusive"});
        log.clear();
        scheduler.run_phase(system_phase::UPDATE, frame);
        CHECK(log == std::vector<std::string>{"always", "exclusive", "gameplay"});
        log.clear();
        scheduler.run_phase_filtered(system_phase::UPDATE, frame, system_flags::MAIN_THREAD);
        CHECK(log.empty());
        scheduler.run_phase(system_phase::FRAME_END, frame);
        CHECK(log.empty());
    }

    TEST_CASE("scheduler - invalid descriptions and phases are refused") {
        ecs_fixture fixture = {};
        scheduler scheduler(fixture.world, nullptr);
        system_desc noRun = {};
        noRun.name = sid("no.run");
        CHECK_FALSE(scheduler.add_system(std::move(noRun)).has_value());
        system_desc badPhase = {};
        badPhase.name = sid("bad.phase");
        badPhase.phase = system_phase::COUNT;
        badPhase.run = [](system_context &) {};
        CHECK_FALSE(scheduler.add_system(std::move(badPhase)).has_value());
        CHECK(scheduler.system_count() == 0);
        CHECK(scheduler.stages(system_phase::COUNT).empty());

        testing::expect_assert guard = {};
        const frame_info frame = {};
        scheduler.run_phase(system_phase::COUNT, frame);
        CHECK(guard.count() == 1 * ASSERTS_PER_VIOLATION);
        const iteration_scope scope = fixture.world.lock_iteration();
        scheduler.run_phase(system_phase::UPDATE, frame);
        CHECK(guard.count() == 2 * ASSERTS_PER_VIOLATION);
    }

    TEST_CASE("scheduler - run_phase while a system holds the world locked flushes afterwards") {
        ecs_fixture fixture = {};
        world &world = fixture.world;
        scheduler scheduler(world, nullptr);
        CONJURE_REQUIRE_OK(scheduler.add_system(make_system<position>(sid("locked.writer"), system_phase::UPDATE,
          [&](system_context &context) {
              CHECK(context.world().is_locked());
              const entity created = context.commands().create_entity();
              context.commands().add<position>(created, position{1.0f, 2.0f, 3.0f});
              CHECK_FALSE(context.world().is_alive(created));
          })));
        const frame_info frame = {};
        scheduler.run_phase(system_phase::UPDATE, frame);
        CHECK(world.entity_count() == 1);
        CHECK_FALSE(world.is_locked());
        CHECK(world.view<const position>().entity_count() == 1);
    }
}
