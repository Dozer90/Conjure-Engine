// ECS scenarios (docs/design/08_testing.md 7.1, 01_ecs.md 10): query iteration over 100,000 entities
// through one integrating system, command-buffer spawn / destroy churn over 10,000 live entities and a
// changed-filter query over 10,000 entities of which 5 % were written.

#include "conjure/bench/benchmark.h"

#include "conjure/core/reflect/reflect.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/ecs/chunk_view.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/query.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/ecs/view.h"
#include "conjure/ecs/world.h"

#include <array>
#include <span>
#include <vector>

using namespace conjure;
using namespace conjure::bench;

namespace bench_ecs {

    struct position {
        f32 x = 0.0f;
        f32 y = 0.0f;
        f32 z = 0.0f;
    };

    struct velocity {
        f32 x = 0.0f;
        f32 y = 0.0f;
        f32 z = 0.0f;
    };

    /// <summary>
    /// The column the changed-filter scenario writes on 5 % of the entities.
    /// </summary>
    struct marker {
        f32 value = 0.0f;
    };
}

CONJURE_REFLECT_STRUCT(bench_ecs::position, "bench.ecs.position", CONJURE_FIELD(x), CONJURE_FIELD(y), CONJURE_FIELD(z));
CONJURE_REFLECT_STRUCT(bench_ecs::velocity, "bench.ecs.velocity", CONJURE_FIELD(x), CONJURE_FIELD(y), CONJURE_FIELD(z));
CONJURE_REFLECT_STRUCT(bench_ecs::marker, "bench.ecs.marker", CONJURE_FIELD(value));

CONJURE_COMPONENT(bench_ecs::position, "bench.ecs.position");
CONJURE_COMPONENT(bench_ecs::velocity, "bench.ecs.velocity");
CONJURE_COMPONENT(bench_ecs::marker, "bench.ecs.marker");

namespace {

    constexpr u32 ITERATE_ENTITY_COUNT = 100000;
    constexpr u32 CHURN_COUNT = 1000;
    constexpr f32 STEP_SECONDS = 1.0f / 60.0f;

    /// <summary>
    /// A registry with the three components and a world over it.
    /// </summary>
    struct ecs_fixture {
        ecs::component_registry registry = {};
        ecs::world world;          // no default: built on the registry

        ecs_fixture() :
            world((register_all(registry), registry)) {}

        static void register_all(ecs::component_registry &target) {
            CONJURE_UNUSED(target.register_component<bench_ecs::position>());
            CONJURE_UNUSED(target.register_component<bench_ecs::velocity>());
            CONJURE_UNUSED(target.register_component<bench_ecs::marker>());
        }

        /// <summary>
        /// Creates 'count' entities of the archetype 'mask' and returns their ids.
        /// </summary>
        [[nodiscard]] std::vector<ecs::entity> create(const ecs::component_mask &mask, u32 count) {
            std::vector<ecs::entity> created(count);
            const core::result<void> made = world.create_entities(mask, created);
            if (!made.has_value()) {
                created.clear();
            }
            return created;
        }

        [[nodiscard]] ecs::component_mask mask_of(std::span<const ecs::component_id> ids) const {
            ecs::component_mask mask = {};
            for (const ecs::component_id id : ids) {
                mask.set(id);
            }
            return mask;
        }
    };

    [[nodiscard]] ecs::scheduler_desc inline_stages() noexcept {
        // Stages run inline so the per-stage job counter of job_system::schedule never shows up in the
        // allocation gate; parallel_for_chunks still fans the system out over the workers.
        ecs::scheduler_desc desc = {};
        desc.parallelSystems = false;
        desc.parallelChunks = true;
        return desc;
    }

    [[nodiscard]] ecs::frame_info frame_at(u64 frameIndex) noexcept {
        ecs::frame_info frame = {};
        frame.deltaTime = STEP_SECONDS;
        frame.fixedDeltaTime = STEP_SECONDS;
        frame.frameIndex = frameIndex;
        return frame;
    }
}

CONJURE_BENCHMARK(ecs.iterate_100k) {
    ecs_fixture fixture = {};
    const ecs::component_id positionId = fixture.registry.id_of<bench_ecs::position>();
    const ecs::component_id velocityId = fixture.registry.id_of<bench_ecs::velocity>();
    const std::vector<ecs::entity> entities =
      fixture.create(fixture.mask_of(std::array{positionId, velocityId}), ITERATE_ENTITY_COUNT);
    if (entities.size() != ITERATE_ENTITY_COUNT) {
        context.skip("the world refused 100,000 entities");
        return;
    }
    u32 index = 0;
    fixture.world.view<bench_ecs::velocity>().for_each([&index](ecs::entity, bench_ecs::velocity &value) {
        value.x = static_cast<f32>(index % 7) * 0.1f;
        value.y = static_cast<f32>(index % 11) * 0.1f;
        value.z = static_cast<f32>(index % 13) * 0.1f;
        ++index;
    });

    ecs::scheduler scheduler(fixture.world, &context.jobs(), inline_stages());
    ecs::system_desc integrate = ecs::make_system<const bench_ecs::velocity, bench_ecs::position>(
      core::string_id("bench.integrate"),
      ecs::system_phase::UPDATE,
      [](ecs::system_context &system) {
          const f32 dt = system.delta_time();
          system.view<const bench_ecs::velocity, bench_ecs::position>().parallel_for_each_chunk(
            system,
            [dt](ecs::chunk_view &chunk, ecs::command_buffer &) {
                const std::span<const bench_ecs::velocity> velocities = chunk.get<const bench_ecs::velocity>();
                const std::span<bench_ecs::position> positions = chunk.get<bench_ecs::position>();
                for (u32 row = 0; row < chunk.count(); ++row) {
                    positions[row].x += velocities[row].x * dt;
                    positions[row].y += velocities[row].y * dt;
                    positions[row].z += velocities[row].z * dt;
                }
            }
          );
      }
    );
    CONJURE_UNUSED(scheduler.add_system(std::move(integrate)));
    CONJURE_UNUSED(scheduler.finalise());

    context.set_items(ITERATE_ENTITY_COUNT);
    u64 frameIndex = 0;
    context.measure([&]() {
        scheduler.run_phase(ecs::system_phase::UPDATE, frame_at(++frameIndex));
    });
}

CONJURE_BENCHMARK(ecs.spawn_destroy_10k) {
    ecs_fixture fixture = {};
    const ecs::component_id positionId = fixture.registry.id_of<bench_ecs::position>();
    const ecs::component_id velocityId = fixture.registry.id_of<bench_ecs::velocity>();
    const u32 liveCount = context.entity_count();
    std::vector<ecs::entity> alive = fixture.create(fixture.mask_of(std::array{positionId, velocityId}), liveCount);
    if (alive.size() != liveCount) {
        context.skip("the world refused the live entities");
        return;
    }

    // The oldest CHURN_COUNT entities die and CHURN_COUNT fresh ones take their slots in the ring.
    ecs::command_buffer commands(fixture.world);
    usize oldest = 0;
    context.set_items(2 * CHURN_COUNT);
    context.measure([&]() {
        for (u32 index = 0; index < CHURN_COUNT; ++index) {
            const usize slot = (oldest + index) % alive.size();
            commands.destroy_entity(alive[slot]);
            const ecs::entity spawned = commands.create_entity();
            commands.add<bench_ecs::position>(spawned, bench_ecs::position{1.0f, 2.0f, 3.0f});
            commands.add<bench_ecs::velocity>(spawned, bench_ecs::velocity{0.1f, 0.0f, 0.0f});
            alive[slot] = spawned;
        }
        oldest = (oldest + CHURN_COUNT) % alive.size();
        CONJURE_UNUSED(fixture.world.flush(commands));
    });
}

CONJURE_BENCHMARK(ecs.query_changed_10k) {
    ecs_fixture fixture = {};
    const ecs::component_id positionId = fixture.registry.id_of<bench_ecs::position>();
    const ecs::component_id markerId = fixture.registry.id_of<bench_ecs::marker>();
    const u32 entityCount = context.entity_count();
    const std::vector<ecs::entity> entities =
      fixture.create(fixture.mask_of(std::array{positionId, markerId}), entityCount);
    if (entities.size() != entityCount) {
        context.skip("the world refused the entities");
        return;
    }
    const u32 modifiedCount = entityCount / 20;    // 5 %, contiguous in chunk order

    ecs::query_desc desc = {};
    desc.with.set(positionId);
    desc.with.set(markerId);
    desc.writes.set(positionId);
    desc.changed.set(markerId);
    desc.sinceVersion = fixture.world.version();
    CONJURE_UNUSED(fixture.world.bump_version());
    ecs::query changedQuery(fixture.world, desc);
    changedQuery.refresh();

    context.set_items(entityCount);
    context.measure([&]() {
        // The writes of this iteration land above the version the query remembers from the last one.
        for (u32 index = 0; index < modifiedCount; ++index) {
            bench_ecs::marker *value = fixture.world.get<bench_ecs::marker>(entities[index]);
            value->value += 1.0f;
        }
        changedQuery.for_each_chunk([](ecs::chunk_view &chunk) {
            const std::span<const bench_ecs::marker> markers = chunk.get<const bench_ecs::marker>();
            const std::span<bench_ecs::position> positions = chunk.get<bench_ecs::position>();
            for (u32 row = 0; row < chunk.count(); ++row) {
                positions[row].y = markers[row].value;
            }
        });
        // Writes stamp the current version, so the next iteration remembers this one and bumps past it.
        desc.sinceVersion = fixture.world.version();
        CONJURE_UNUSED(fixture.world.bump_version());
        changedQuery.reset(desc);
    });
}
