#include "conjure/testing/doctest.h"

#include "ecs_test_fixture.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/log.h"
#include "conjure/core/memory/allocation_counters.h"
#include "conjure/core/reflect/type_id.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/world.h"
#include "conjure/ecs/world_hash.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace conjure;
using namespace conjure::ecs;
using namespace ecs_test;

namespace {
    /// <summary>
    /// A component the fixture does not register: the tests of 01_ecs.md D15 register it after the
    /// systems that name it.
    /// </summary>
    struct late_gear {
        f32 teeth = 0.0f;
    };
}

CONJURE_COMPONENT(late_gear, "test.late_gear");

namespace {
    constexpr core::string_id LATE_GEAR_NAME{"test.late_gear"};

    /// <summary>
    /// A system that declares a write of the component registered under 'name', resolved by name the way
    /// the engine's module consumer resolves a module system's lists.
    /// </summary>
    system_desc writer_by_name(std::string_view systemName, core::string_id name) {
        system_desc desc = {};
        desc.name = core::string_id(systemName);
        desc.phase = system_phase::UPDATE;
        desc.run = [](system_context &) {};
        desc.resolveAccess = [name](const component_registry &registry, component_mask &, component_mask &writes) {
            const component_id id = registry.find(name);
            if (id.is_valid()) {
                writes.set(id);
            }
        };
        return desc;
    }

    /// <summary>
    /// Registers a four-byte dynamic component no system names.
    /// </summary>
    component_id register_unrelated(component_registry &registry, std::string_view name) {
        const std::array<dynamic_field_desc, 1> fields = {
            dynamic_field_desc{core::string_id("value"), core::reflect::type_id_of<f32>(), 0},
        };
        dynamic_component_desc desc = {};
        desc.name = core::string_id(name);
        desc.fields = fields;
        desc.size = sizeof(f32);
        desc.alignment = alignof(f32);
        const core::result<component_id> registered = registry.register_dynamic(desc);
        CONJURE_REQUIRE_OK(registered);
        return registered.value();
    }

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

    TEST_CASE("scheduler - unknown names are logged once when the system is added") {
        // add_system logs the new system's unknown after/before names; adding other systems, finalise()
        // and the lazy rebuilds of run_phase resolve the same edges without logging them again.
        ecs_fixture fixture = {};
        scheduler scheduler(fixture.world, nullptr);
        std::vector<std::string> log = {};
        std::mutex mutex = {};
        testing::log_capture logs = {};
        system_desc dangling = logging_system("dangling", log, mutex);
        dangling.after.push_back(sid("module.compiled_out"));
        dangling.before.push_back(sid("also.missing"));
        const core::result<system_id> danglingId = scheduler.add_system(std::move(dangling));
        CONJURE_REQUIRE_OK(danglingId);
        CONJURE_REQUIRE_OK(scheduler.add_system(logging_system("second", log, mutex)));
        CONJURE_REQUIRE_OK(scheduler.add_system(logging_system("third", log, mutex)));
        CONJURE_REQUIRE_OK(scheduler.finalise());
        const frame_info frame = {};
        scheduler.run_phase(system_phase::UPDATE, frame);
        CONJURE_REQUIRE_OK(scheduler.set_enabled(danglingId.value(), false));
        scheduler.run_phase(system_phase::UPDATE, frame);
        CHECK(log.size() == 5);
#if !CONJURE_DISTRIBUTION
        u32 unknownLogs = 0;
        for (const std::string &message : logs.messages(core::log_level::DEBUG)) {
            if (message.find("unknown system") != std::string::npos) {
                ++unknownLogs;
            }
        }
        CHECK(unknownLogs == 2);
#endif
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

    TEST_CASE("scheduler - since_version excludes own writes after a version bump inside the run") {
        // A structural call inside an EXCLUSIVE system bumps the world version in the middle of the run;
        // the system's views still stamp its run version, so its own writes never pass its next
        // since_version().
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> ids(100);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position, velocity>(), ids));
        scheduler scheduler(world, nullptr);
        std::vector<u32> changedCounts = {};
        system_desc writer = make_system<position>(sid("bumping.writer"), system_phase::UPDATE,
          [&](system_context &context) {
              // The new entity lands in another archetype, so the visited chunks are the position ones.
              CONJURE_CHECK_OK(context.world().create_entity(fixture.mask<velocity>()));
              u32 changed = 0;
              context.view<position>().changed<position>(context.since_version()).for_each(
                [&](entity, position &value) {
                    value.x += 1.0f;
                    ++changed;
                });
              changedCounts.push_back(changed);
          });
        writer.flags = system_flags::EXCLUSIVE;
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(writer)));

        const frame_info frame = {};
        for (u32 run = 0; run < 3; ++run) {
            scheduler.run_phase(system_phase::UPDATE, frame);
        }
        CHECK(changedCounts == std::vector<u32>{100, 0, 0});
        CHECK(world.entity_count() == 103);
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

    TEST_CASE("regression - code sweep F259 a duplicate system name in one phase is NAME_CONFLICT") {
        // add_system used to accept a second system of the same name, and an after/before edge to that
        // name then bound to whichever was registered first. The name is unique per phase now.
        ecs_fixture fixture = {};
        scheduler scheduler(fixture.world, nullptr);
        std::vector<std::string> log = {};
        std::mutex mutex = {};
        const core::result<system_id> original = scheduler.add_system(logging_system("shared", log, mutex));
        CONJURE_REQUIRE_OK(original);

        // The duplicate is refused and leaves nothing behind: no record, no stage, no run.
        system_desc duplicate = {};
        duplicate.name = sid("shared");
        duplicate.phase = system_phase::UPDATE;
        duplicate.run = [&log, &mutex](system_context &) {
            const std::scoped_lock lock(mutex);
            log.push_back("duplicate");
        };
        const core::result<system_id> refused = scheduler.add_system(std::move(duplicate));
        REQUIRE_FALSE(refused.has_value());
        CONJURE_CHECK_ERROR(refused, ecs_error::NAME_CONFLICT);
        CHECK(refused.get_error().message().find("'shared'") != std::string::npos);
        CHECK(refused.get_error().message().find("UPDATE") != std::string::npos);
        CHECK(scheduler.system_count() == 1);
        CHECK(scheduler.find_system(system_phase::UPDATE, sid("shared")) == original.value());

        // An edge to the name orders against the surviving system: registered second, 'leader' still runs
        // first, in a stage of its own.
        system_desc leader = logging_system("leader", log, mutex);
        leader.before.push_back(sid("shared"));
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(leader)));
        CONJURE_REQUIRE_OK(scheduler.finalise());
        const frame_info frame = {};
        scheduler.run_phase(system_phase::UPDATE, frame);
        CHECK(log == std::vector<std::string>{"leader", "shared"});
        CHECK(scheduler.stages(system_phase::UPDATE).size() == 2);
    }

    TEST_CASE("scheduler - a system name may repeat across phases, after removal and when it is null") {
        ecs_fixture fixture = {};
        scheduler scheduler(fixture.world, nullptr);
        std::vector<std::string> log = {};
        std::mutex mutex = {};
        const core::result<system_id> update = scheduler.add_system(logging_system("shared", log, mutex));
        CONJURE_REQUIRE_OK(update);

        // The same name in another phase is another system.
        const core::result<system_id> lateUpdate =
          scheduler.add_system(logging_system("shared", log, mutex, system_phase::LATE_UPDATE));
        CONJURE_REQUIRE_OK(lateUpdate);
        CHECK(lateUpdate.value() != update.value());
        CHECK(scheduler.system_count() == 2);

        // Unnamed systems are never compared: any number of them shares a phase.
        for (u32 index = 0; index < 3; ++index) {
            CAPTURE(index);
            system_desc unnamed = {};
            unnamed.phase = system_phase::UPDATE;
            unnamed.run = [&log, &mutex](system_context &) {
                const std::scoped_lock lock(mutex);
                log.push_back("unnamed");
            };
            CONJURE_REQUIRE_OK(scheduler.add_system(std::move(unnamed)));
        }
        CHECK(scheduler.system_count() == 5);

        // A disabled system still holds its name; a removed one gives it back.
        CONJURE_REQUIRE_OK(scheduler.set_enabled(update.value(), false));
        CONJURE_CHECK_ERROR(scheduler.add_system(logging_system("shared", log, mutex)), ecs_error::NAME_CONFLICT);
        CONJURE_REQUIRE_OK(scheduler.remove_system(update.value()));
        const core::result<system_id> again = scheduler.add_system(logging_system("shared", log, mutex));
        CONJURE_REQUIRE_OK(again);
        CHECK(again.value() != update.value());
        CHECK(scheduler.system_count() == 5);

        // A system refused for a cycle never took the name either.
        system_desc cyclic = logging_system("cyclic", log, mutex);
        cyclic.after.push_back(sid("cyclic"));
        CONJURE_CHECK_ERROR(scheduler.add_system(std::move(cyclic)), ecs_error::SYSTEM_CYCLE);
        CONJURE_REQUIRE_OK(scheduler.add_system(logging_system("cyclic", log, mutex)));

        const frame_info frame = {};
        scheduler.run_phase(system_phase::UPDATE, frame);
        scheduler.run_phase(system_phase::LATE_UPDATE, frame);
        CHECK(index_of(log, "cyclic") < log.size());
        CHECK(std::count(log.begin(), log.end(), std::string("shared")) == 2);
        CHECK(std::count(log.begin(), log.end(), std::string("unnamed")) == 3);
    }

    TEST_CASE("regression - code sweep F259 a null name in after or before binds to no unnamed system") {
        // An after/before entry holding the null string_id used to bind to the first-registered unnamed
        // system of the phase, the one name add_system does not make unique. It is an unknown name now.
        ecs_fixture fixture = {};
        scheduler scheduler(fixture.world, nullptr);
        for (u32 index = 0; index < 2; ++index) {
            CAPTURE(index);
            system_desc unnamed = {};
            unnamed.phase = system_phase::UPDATE;
            unnamed.run = [](system_context &) {};
            CONJURE_REQUIRE_OK(scheduler.add_system(std::move(unnamed)));
        }

        std::vector<std::string> log = {};
        std::mutex mutex = {};
        testing::log_capture logs = {};
        system_desc named = logging_system("named", log, mutex);
        named.before.push_back(core::string_id());
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(named)));
        CHECK(scheduler.system_count() == 3);

        // No edge and no access conflict: the three systems share one stage. Bound to the first unnamed
        // system, the edge would move that system into a second stage behind 'named'.
        CONJURE_REQUIRE_OK(scheduler.finalise());
        const std::span<const stage_info> stages = scheduler.stages(system_phase::UPDATE);
        REQUIRE(stages.size() == 1);
        CHECK(stages[0].systems.size() == 3);
#if !CONJURE_DISTRIBUTION
        u32 unknownLogs = 0;
        for (const std::string &message : logs.messages(core::log_level::DEBUG)) {
            if (message.find("unknown system") != std::string::npos) {
                ++unknownLogs;
            }
        }
        CHECK(unknownLogs == 1);
#endif
    }

    TEST_CASE("regression - code sweep F259 an unnamed system that names the null name is not a cycle") {
        // The null entry of the only unnamed system of a phase used to bind to that system itself, a self
        // edge add_system refused with SYSTEM_CYCLE.
        ecs_fixture fixture = {};
        scheduler scheduler(fixture.world, nullptr);
        u32 runs = 0;
        system_desc unnamed = {};
        unnamed.phase = system_phase::UPDATE;
        unnamed.after.push_back(core::string_id());
        unnamed.run = [&runs](system_context &) {
            ++runs;
        };
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(unnamed)));
        CHECK(scheduler.system_count() == 1);

        system_desc second = {};
        second.phase = system_phase::UPDATE;
        second.before.push_back(core::string_id());
        second.run = [&runs](system_context &) {
            ++runs;
        };
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(second)));
        CONJURE_REQUIRE_OK(scheduler.finalise());
        CHECK(scheduler.stages(system_phase::UPDATE).size() == 1);
        const frame_info frame = {};
        scheduler.run_phase(system_phase::UPDATE, frame);
        CHECK(runs == 2);
    }

    TEST_CASE("scheduler - find_system returns the live system of a phase by name") {
        ecs_fixture fixture = {};
        scheduler scheduler(fixture.world, nullptr);
        std::vector<std::string> log = {};
        std::mutex mutex = {};
        CHECK_FALSE(scheduler.find_system(system_phase::UPDATE, sid("mover")).is_valid());

        const core::result<system_id> mover = scheduler.add_system(logging_system("mover", log, mutex));
        const core::result<system_id> late =
          scheduler.add_system(logging_system("late", log, mutex, system_phase::LATE_UPDATE));
        CONJURE_REQUIRE_OK(mover);
        CONJURE_REQUIRE_OK(late);
        CHECK(scheduler.find_system(system_phase::UPDATE, sid("mover")) == mover.value());
        CHECK(scheduler.find_system(system_phase::LATE_UPDATE, sid("late")) == late.value());
        const system_desc *found = scheduler.system(scheduler.find_system(system_phase::UPDATE, sid("mover")));
        REQUIRE(found != nullptr);
        CHECK(found->name == sid("mover"));

        // The lookup is per phase, and neither the null name nor an out-of-range phase finds anything.
        CHECK_FALSE(scheduler.find_system(system_phase::LATE_UPDATE, sid("mover")).is_valid());
        CHECK_FALSE(scheduler.find_system(system_phase::UPDATE, sid("late")).is_valid());
        CHECK_FALSE(scheduler.find_system(system_phase::UPDATE, sid("missing")).is_valid());
        CHECK_FALSE(scheduler.find_system(system_phase::COUNT, sid("mover")).is_valid());
        system_desc unnamed = {};
        unnamed.phase = system_phase::UPDATE;
        unnamed.run = [](system_context &) {};
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(unnamed)));
        CHECK_FALSE(scheduler.find_system(system_phase::UPDATE, core::string_id()).is_valid());

        // A disabled system is still found; a removed one is not, and a later system of the name is.
        CONJURE_REQUIRE_OK(scheduler.set_enabled(mover.value(), false));
        CHECK(scheduler.find_system(system_phase::UPDATE, sid("mover")) == mover.value());
        CONJURE_REQUIRE_OK(scheduler.remove_system(mover.value()));
        CHECK_FALSE(scheduler.find_system(system_phase::UPDATE, sid("mover")).is_valid());
        const core::result<system_id> replacement = scheduler.add_system(logging_system("mover", log, mutex));
        CONJURE_REQUIRE_OK(replacement);
        CHECK(scheduler.find_system(system_phase::UPDATE, sid("mover")) == replacement.value());
        CHECK(replacement.value() != mover.value());
    }

    TEST_CASE("regression - code sweep F077 a system gains the access of a component registered after it") {
        // A system resolved its access once, when it was added, so a component it named that was registered
        // later (an optional module loading afterwards) never reached its masks: it kept sharing a stage
        // with a writer of that component. The scheduler resolves again once the registry has grown.
        ecs_fixture fixture = {};
        scheduler scheduler(fixture.world, nullptr);
        REQUIRE_FALSE(fixture.id<late_gear>().is_valid());

        bool readerSawAccess = false;
        const core::result<system_id> reader = scheduler.add_system(make_system<const late_gear>(
          sid("gear.reader"),
          system_phase::UPDATE,
          [&readerSawAccess](system_context &context) {
              readerSawAccess = !context.desc().reads.none();
          }
        ));
        const core::result<system_id> writer = scheduler.add_system(writer_by_name("gear.writer", LATE_GEAR_NAME));
        const core::result<system_id> bystander = scheduler.add_system(
          make_system<position>(sid("bystander"), system_phase::UPDATE, [](system_context &) {})
        );
        CONJURE_REQUIRE_OK(reader);
        CONJURE_REQUIRE_OK(writer);
        CONJURE_REQUIRE_OK(bystander);
        CONJURE_REQUIRE_OK(scheduler.finalise());

        // Nothing is known of the component yet: both systems have an empty access set and all three
        // share one stage.
        CHECK(scheduler.system(reader.value())->reads.none());
        CHECK(scheduler.system(writer.value())->writes.none());
        CHECK(scheduler.stages(system_phase::UPDATE).size() == 1);
        const frame_info frame = {};
        scheduler.run_phase(system_phase::UPDATE, frame);
        CHECK_FALSE(readerSawAccess);

        const core::result<component_id> gear =
          fixture.registry.register_component<late_gear>(component_flags::TRANSIENT);
        CONJURE_REQUIRE_OK(gear);
        component_mask gearMask = {};
        gearMask.set(gear.value());

        SUBCASE("run_phase resolves again before it runs the phase") {
            scheduler.run_phase(system_phase::UPDATE, frame);
            CHECK(readerSawAccess);
        }
        SUBCASE("finalise resolves again") {
            CONJURE_REQUIRE_OK(scheduler.finalise());
        }
        SUBCASE("stages resolves again") {
            CHECK(scheduler.stages(system_phase::UPDATE).size() == 2);
        }

        CHECK(scheduler.system(reader.value())->reads == gearMask);
        CHECK(scheduler.system(reader.value())->writes.none());
        CHECK(scheduler.system(writer.value())->writes == gearMask);
        CHECK(scheduler.system(bystander.value())->writes == fixture.mask<position>());
        // The reader and the writer no longer share a stage; the bystander, registered last, joins the
        // stage that is open when its turn comes.
        const std::span<const stage_info> stages = scheduler.stages(system_phase::UPDATE);
        REQUIRE(stages.size() == 2);
        CHECK(stages[0].systems == std::vector<system_id>{reader.value()});
        CHECK(stages[1].systems == std::vector<system_id>{writer.value(), bystander.value()});
    }

    TEST_CASE("scheduler - resolveAccess runs again only when the registry has grown") {
        ecs_fixture fixture = {};
        scheduler scheduler(fixture.world, nullptr);
        u32 enabledCalls = 0;
        u32 disabledCalls = 0;
        u32 removedCalls = 0;
        const auto counting_system = [&fixture](std::string_view name, u32 &calls) {
            system_desc desc = {};
            desc.name = sid(name);
            desc.phase = system_phase::LATE_UPDATE;
            desc.reads = fixture.mask<health>();
            desc.run = [](system_context &) {};
            desc.resolveAccess = [&calls](
              const component_registry &registry,
              component_mask &,
              component_mask &writes
            ) {
                ++calls;
                writes.set(registry.id_of<position>());
            };
            return desc;
        };
        const core::result<system_id> enabled = scheduler.add_system(counting_system("enabled", enabledCalls));
        const core::result<system_id> disabled = scheduler.add_system(counting_system("disabled", disabledCalls));
        const core::result<system_id> removed = scheduler.add_system(counting_system("removed", removedCalls));
        CONJURE_REQUIRE_OK(enabled);
        CONJURE_REQUIRE_OK(disabled);
        CONJURE_REQUIRE_OK(removed);
        CONJURE_REQUIRE_OK(scheduler.set_enabled(disabled.value(), false));
        CONJURE_REQUIRE_OK(scheduler.remove_system(removed.value()));

        // add_system resolved each once; with an unchanged registry nothing resolves again.
        const frame_info frame = {};
        CONJURE_REQUIRE_OK(scheduler.finalise());
        scheduler.run_phase(system_phase::LATE_UPDATE, frame);
        CHECK(scheduler.stages(system_phase::LATE_UPDATE).size() == 1);
        CHECK(enabledCalls == 1);
        CHECK(disabledCalls == 1);
        CHECK(removedCalls == 1);

        // One pass per growth, whichever entry point notices it first and however often it is called
        // afterwards. A disabled system is resolved too (it can be enabled again); a removed one is not.
        CHECK(register_unrelated(fixture.registry, "test.unrelated.a").is_valid());
        scheduler.run_phase(system_phase::LATE_UPDATE, frame);
        scheduler.run_phase(system_phase::UPDATE, frame);
        CONJURE_REQUIRE_OK(scheduler.finalise());
        CHECK(enabledCalls == 2);
        CHECK(disabledCalls == 2);
        CHECK(removedCalls == 1);
        CHECK(register_unrelated(fixture.registry, "test.unrelated.b").is_valid());
        CHECK(register_unrelated(fixture.registry, "test.unrelated.c").is_valid());
        CHECK(scheduler.stages(system_phase::UPDATE).empty());
        CHECK(enabledCalls == 3);
        CHECK(disabledCalls == 3);
        CHECK(removedCalls == 1);

        // Hand-declared bits stay, the hook's bits are merged in, and repeating the hook changes nothing.
        const system_desc *desc = scheduler.system(enabled.value());
        REQUIRE(desc != nullptr);
        CHECK(desc->reads == fixture.mask<health>());
        CHECK(desc->writes == fixture.mask<position>());

        // A system added after the growth is resolved by add_system alone.
        u32 lateCalls = 0;
        CONJURE_REQUIRE_OK(scheduler.add_system(counting_system("late", lateCalls)));
        CONJURE_REQUIRE_OK(scheduler.finalise());
        CHECK(lateCalls == 1);
        CHECK(enabledCalls == 3);

        // While the world is locked for iteration a stage may be running, so the pass waits for the next
        // call that finds it unlocked.
        CHECK(register_unrelated(fixture.registry, "test.unrelated.d").is_valid());
        {
            const iteration_scope scope = fixture.world.lock_iteration();
            CONJURE_REQUIRE_OK(scheduler.finalise());
            CHECK(scheduler.stages(system_phase::LATE_UPDATE).size() == 2);
            CHECK(enabledCalls == 3);
            CHECK(lateCalls == 1);
        }
        CHECK(scheduler.stages(system_phase::LATE_UPDATE).size() == 2);
        CHECK(enabledCalls == 4);
        CHECK(disabledCalls == 4);
        CHECK(lateCalls == 2);
    }

    TEST_CASE("scheduler - a registry growth that changes no access set rebuilds and logs nothing") {
        ecs_fixture fixture = {};
        scheduler scheduler(fixture.world, nullptr);
        CONJURE_REQUIRE_OK(scheduler.add_system(
          make_system<const late_gear, position>(sid("gear.reader"), system_phase::UPDATE, [](system_context &) {})
        ));
        CONJURE_REQUIRE_OK(scheduler.add_system(writer_by_name("gear.writer", LATE_GEAR_NAME)));
        system_desc fixedMasks = {};
        fixedMasks.name = sid("fixed.masks");
        fixedMasks.phase = system_phase::LATE_UPDATE;
        fixedMasks.reads = fixture.mask<velocity>();
        fixedMasks.run = [](system_context &) {};
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(fixedMasks)));
        const frame_info frame = {};
        scheduler.run_phase(system_phase::UPDATE, frame);
        scheduler.run_phase(system_phase::UPDATE, frame);
        scheduler.run_phase(system_phase::LATE_UPDATE, frame);
        scheduler.run_phase(system_phase::LATE_UPDATE, frame);

        const testing::log_capture logs = {};
        // An unrelated component: the hooks run, find nothing new, and the warmed-up phases stay as built.
        // The growth is noticed by the run of a phase without systems, so the two allocation gates cover
        // the phases alone (a rebuild allocates), not what a hook does to look a type up.
        CHECK(register_unrelated(fixture.registry, "test.unrelated.quiet").is_valid());
        scheduler.run_phase(system_phase::FRAME_END, frame);
        CONJURE_CHECK_NO_ALLOCATIONS(scheduler.run_phase(system_phase::UPDATE, frame));
        CONJURE_CHECK_NO_ALLOCATIONS(scheduler.run_phase(system_phase::LATE_UPDATE, frame));
        CHECK(logs.count(core::log_level::DEBUG) == 0);
        CHECK(scheduler.stages(system_phase::UPDATE).size() == 1);

        // The named component: both systems of UPDATE gain access (one DEBUG line each), the phase is
        // rebuilt into two stages, and the phase none of whose systems changed is not touched.
        CONJURE_REQUIRE_OK(fixture.registry.register_component<late_gear>(component_flags::TRANSIENT));
        scheduler.run_phase(system_phase::FRAME_END, frame);
        CONJURE_CHECK_NO_ALLOCATIONS(scheduler.run_phase(system_phase::LATE_UPDATE, frame));
        CHECK(scheduler.stages(system_phase::UPDATE).size() == 2);
#if !CONJURE_DISTRIBUTION
        u32 gained = 0;
        for (const std::string &message : logs.messages(core::log_level::DEBUG)) {
            if (message.find("gained access") != std::string::npos) {
                ++gained;
                CHECK(message.find("UPDATE") != std::string::npos);
            }
        }
        CHECK(gained == 2);
        CHECK(logs.contains(core::log_level::DEBUG, "'gear.reader'"));
        CHECK(logs.contains(core::log_level::DEBUG, "'gear.writer'"));
#endif
        CHECK(logs.count(core::log_level::WARN) == 0);
    }

    TEST_CASE("scheduler - an exclusive system may register a component and run another phase") {
        // The nested run_phase notices the growth while the outer phase is walking its stages; the outer
        // phase is only marked for a rebuild, which happens before its next run.
        ecs_fixture fixture = {};
        scheduler scheduler(fixture.world, nullptr);
        std::vector<std::string> log = {};
        std::mutex mutex = {};
        system_desc registrar = logging_system("registrar", log, mutex);
        registrar.flags = system_flags::EXCLUSIVE;
        registrar.run = [&](system_context &context) {
            if (!fixture.id<late_gear>().is_valid()) {
                CONJURE_REQUIRE_OK(fixture.registry.register_component<late_gear>(component_flags::TRANSIENT));
            }
            scheduler.run_phase(system_phase::FRAME_END, context.frame());
            const std::scoped_lock lock(mutex);
            log.push_back("registrar");
        };
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(registrar)));
        system_desc reader = make_system<const late_gear>(sid("reader"), system_phase::UPDATE, [&](system_context &) {
            const std::scoped_lock lock(mutex);
            log.push_back("reader");
        });
        const core::result<system_id> readerId = scheduler.add_system(std::move(reader));
        CONJURE_REQUIRE_OK(readerId);
        system_desc writer = writer_by_name("writer", LATE_GEAR_NAME);
        writer.run = [&](system_context &) {
            const std::scoped_lock lock(mutex);
            log.push_back("writer");
        };
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(writer)));
        CONJURE_REQUIRE_OK(scheduler.add_system(logging_system("inner", log, mutex, system_phase::FRAME_END)));
        // The exclusive system alone, then the reader and the writer together while neither has access.
        CHECK(scheduler.stages(system_phase::UPDATE).size() == 2);

        const frame_info frame = {};
        scheduler.run_phase(system_phase::UPDATE, frame);
        CHECK(log == std::vector<std::string>{"inner", "registrar", "reader", "writer"});
        CHECK_FALSE(scheduler.system(readerId.value())->reads.none());
        CHECK(scheduler.stages(system_phase::UPDATE).size() == 3);
        log.clear();
        scheduler.run_phase(system_phase::UPDATE, frame);
        CHECK(log == std::vector<std::string>{"inner", "registrar", "reader", "writer"});
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

    TEST_CASE("regression - review 3 nested run_phase from an exclusive system keeps the outer frame [parallel]") {
        // An EXCLUSIVE system runs on the owner thread with the world unlocked, so it may run another
        // phase. The job closures of the outer phase reach their frame through the scheduler, which the
        // nested call must restore: the stages after the exclusive one still see the outer frame.
        for_each_job_mode([](core::jobs::job_system *jobs) {
            ecs_fixture fixture = {};
            scheduler scheduler(fixture.world, jobs);
            frame_info outer = {};
            outer.deltaTime = 0.5f;
            outer.frameIndex = 7;
            frame_info inner = {};
            inner.deltaTime = 0.125f;
            inner.frameIndex = 99;
            std::atomic<u64> innerIndex = 0;
            std::atomic<u64> laterJobIndex = 0;
            std::atomic<f32> laterJobDelta = 0.0f;
            std::atomic<u64> laterMainIndex = 0;

            system_desc innerSystem = {};
            innerSystem.name = sid("inner");
            innerSystem.phase = system_phase::FRAME_END;
            innerSystem.run = [&](system_context &context) {
                innerIndex = context.frame().frameIndex;
            };
            system_desc exclusive = {};
            exclusive.name = sid("exclusive");
            exclusive.flags = system_flags::EXCLUSIVE;
            exclusive.run = [&](system_context &context) {
                CHECK(context.frame().frameIndex == 7);
                scheduler.run_phase(system_phase::FRAME_END, inner);
            };
            system_desc laterJob = {};
            laterJob.name = sid("later.job");
            laterJob.run = [&](system_context &context) {
                laterJobIndex = context.frame().frameIndex;
                laterJobDelta = context.delta_time();
            };
            system_desc laterMain = {};
            laterMain.name = sid("later.main");
            laterMain.flags = system_flags::MAIN_THREAD;
            laterMain.run = [&](system_context &context) {
                laterMainIndex = context.frame().frameIndex;
            };
            CONJURE_REQUIRE_OK(scheduler.add_system(std::move(innerSystem)));
            CONJURE_REQUIRE_OK(scheduler.add_system(std::move(exclusive)));
            CONJURE_REQUIRE_OK(scheduler.add_system(std::move(laterJob)));
            CONJURE_REQUIRE_OK(scheduler.add_system(std::move(laterMain)));
            const std::span<const stage_info> stages = scheduler.stages(system_phase::UPDATE);
            REQUIRE(stages.size() == 2);
            CHECK(stages[0].exclusive);
            CHECK(stages[1].systems.size() == 2);

            for (u32 run = 0; run < 2; ++run) {
                CAPTURE(run);
                scheduler.run_phase(system_phase::UPDATE, outer);
                CHECK(innerIndex.load() == 99);
                CHECK(laterJobIndex.load() == 7);
                CHECK(laterJobDelta.load() == 0.5f);
                CHECK(laterMainIndex.load() == 7);
            }
            // A phase run on its own afterwards is unaffected by the nesting.
            innerIndex = 0;
            scheduler.run_phase(system_phase::FRAME_END, outer);
            CHECK(innerIndex.load() == 7);
        });
    }

    TEST_CASE("scheduler - a warmed-up phase allocates nothing of its own [parallel]") {
        // The ECS-level allocation gate (13_scene.md 10 gates a whole TRANSFORM phase through the scene,
        // 08_testing.md 7.1 the engine frame): the per-stage scratch lives in the phase state, the
        // [this, record] job closure fits the std::function small buffer and the stage jobs complete on
        // a stack-held job_fence, so after two runs a phase allocates nothing in either mode, with
        // parallelSystems on and a four-worker pool included.
        const auto check_phase = [](core::jobs::job_system *jobs) {
            ecs_fixture fixture = {};
            scheduler_desc options = {};
            options.parallelSystems = true;
            scheduler scheduler(fixture.world, jobs, options);
            std::atomic<u32> runs = 0;
            // Three stages: two readers with a main-thread reader, then two writers of the same type.
            const auto counting_system = [&](std::string_view name, system_flags flags, bool writer) {
                system_desc desc = {};
                desc.name = sid(name);
                desc.flags = flags;
                if (writer) {
                    desc.writes.set(fixture.id<position>());
                } else {
                    desc.reads.set(fixture.id<position>());
                }
                desc.run = [&runs](system_context &context) {
                    if (context.delta_time() >= 0.0f) {
                        runs.fetch_add(1, std::memory_order_relaxed);
                    }
                };
                return desc;
            };
            CONJURE_REQUIRE_OK(scheduler.add_system(counting_system("reader.a", system_flags::NONE, false)));
            CONJURE_REQUIRE_OK(scheduler.add_system(counting_system("reader.b", system_flags::NONE, false)));
            CONJURE_REQUIRE_OK(scheduler.add_system(counting_system("reader.main", system_flags::MAIN_THREAD, false)));
            CONJURE_REQUIRE_OK(scheduler.add_system(counting_system("writer.a", system_flags::NONE, true)));
            CONJURE_REQUIRE_OK(scheduler.add_system(counting_system("writer.b", system_flags::NONE, true)));
            const usize stageCount = scheduler.stages(system_phase::UPDATE).size();
            REQUIRE(stageCount == 3);

            const frame_info frame = {};
            scheduler.run_phase(system_phase::UPDATE, frame);
            scheduler.run_phase(system_phase::UPDATE, frame);
            CHECK(runs.load() == 10);
            for (u32 repetition = 0; repetition < 5; ++repetition) {
                CAPTURE(repetition);
                CONJURE_CHECK_NO_ALLOCATIONS(scheduler.run_phase(system_phase::UPDATE, frame));
            }
            CHECK(runs.load() == 35);
        };

        SUBCASE("inline") {
            check_phase(nullptr);
        }
        SUBCASE("four workers") {
            core::jobs::job_system jobs = {};
            core::jobs::job_system_desc desc = {};
            desc.workerCount = 4;
            REQUIRE(jobs.start(desc).has_value());
            check_phase(&jobs);
            jobs.stop();
        }
    }

    TEST_CASE("scheduler - the documented view idiom allocates nothing after the first run [parallel]") {
        // system.h documents context.view<...>().for_each inside the system body; the view's query keeps
        // its matches inline (QUERY_INLINE_MATCHES), so the idiom allocates nothing per run. The second
        // system fans out over chunks on the workers, the shape of the engine's PARALLEL systems.
        for_each_job_mode([](core::jobs::job_system *jobs) {
            ecs_fixture fixture = {};
            world &world = fixture.world;
            std::vector<entity> moving(3000);
            std::vector<entity> still(500);
            CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position, velocity>(), moving));
            CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position, velocity, player_tag>(), still));
            scheduler_desc options = {};
            options.minEntitiesPerJob = 256;
            scheduler scheduler(world, jobs, options);
            std::atomic<u32> rows = 0;
            CONJURE_REQUIRE_OK(scheduler.add_system(make_system<const velocity, position>(
              sid("integrate"),
              system_phase::UPDATE,
              [](system_context &context) {
                  context.view<const velocity, position>().for_each([&](entity, const velocity &v, position &p) {
                      p.x += v.x * context.delta_time();
                  });
              }
            )));
            CONJURE_REQUIRE_OK(scheduler.add_system(make_system<const position>(
              sid("count"),
              system_phase::UPDATE,
              [&rows](system_context &context) {
                  context.view<const position>().parallel_for_each_chunk(
                    context,
                    [&rows](chunk_view &chunk, command_buffer &) {
                        rows.fetch_add(chunk.count(), std::memory_order_relaxed);
                    }
                  );
              }
            )));
            CHECK(scheduler.stages(system_phase::UPDATE).size() == 2);

            frame_info frame = {};
            frame.deltaTime = 0.5f;
            scheduler.run_phase(system_phase::UPDATE, frame);
            scheduler.run_phase(system_phase::UPDATE, frame);
            CHECK(rows.load() == 7000);
            CONJURE_CHECK_NO_ALLOCATIONS(scheduler.run_phase(system_phase::UPDATE, frame));
            CHECK(rows.load() == 10500);
        });
    }

    TEST_CASE("scheduler - remove_system returns the removed system's buffer room") {
        // A system's command buffer keeps the arena room its largest recording grew to (vector-style);
        // the record waits on the free list after remove_system, so the room goes back to the heap there
        // instead of outliving the system.
        ecs_fixture fixture = {};
        world &world = fixture.world;
        std::vector<entity> targets(1000);
        CONJURE_REQUIRE_OK(world.create_entities(fixture.mask<position>(), targets));
        scheduler scheduler(world, nullptr);

        // A removed placeholder leaves the free list with capacity, so the removal measured below pushes
        // without allocating and the only change in live allocations is what the removal frees.
        system_desc placeholderDesc = {};
        placeholderDesc.name = sid("placeholder");
        placeholderDesc.phase = system_phase::UPDATE;
        placeholderDesc.run = [](system_context &) {};
        const core::result<system_id> placeholder = scheduler.add_system(std::move(placeholderDesc));
        CONJURE_REQUIRE_OK(placeholder);
        CONJURE_REQUIRE_OK(scheduler.remove_system(placeholder.value()));

        system_desc recorder = {};
        recorder.name = sid("recorder");
        recorder.phase = system_phase::UPDATE;
        recorder.run = [&targets](system_context &context) {
            for (const entity id : targets) {
                context.commands().set<position>(id, position{1.0f, 2.0f, 3.0f});
                context.commands().add<velocity>(id, velocity{0.5f, 0.0f, 0.0f});
                context.commands().add<player_tag>(id);
                context.commands().remove<player_tag>(id);
            }
        };
        const core::result<system_id> added = scheduler.add_system(std::move(recorder));
        CONJURE_REQUIRE_OK(added);
        const frame_info frame = {};
        // The first run spills the 16 KiB block many times; its flush leaves the buffer with one block
        // of the peak size, which the second run fills without allocating.
        scheduler.run_phase(system_phase::UPDATE, frame);
        scheduler.run_phase(system_phase::UPDATE, frame);
        CHECK(world.has<velocity>(targets[999]));

        const u64 liveBefore = core::memory::snapshot().live_count();
        CONJURE_REQUIRE_OK(scheduler.remove_system(added.value()));
        const u64 liveAfter = core::memory::snapshot().live_count();
        CHECK(liveAfter < liveBefore);
        CHECK(scheduler.system(added.value()) == nullptr);
    }
}
