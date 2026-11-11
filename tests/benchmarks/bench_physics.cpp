// Physics scenarios (docs/design/08_testing.md 7.1, 14_physics.md 10): one fixed step over 10,000
// dynamic boxes resting awake on a plane, the same over 10,000 sleeping boxes after 600 settle steps, the
// Jolt -> transform sync of 10,000 awake bodies as the engine's FIXED_END phase runs it (the PARALLEL
// physics.pull_transforms over chunks, then physics.contact_events) and one fixed step over 2,000
// settling boxes stacked two high. The step scenarios run the pipeline physics.push_transforms /
// physics.step / physics.pull_transforms / physics.contact_events inline: push, step, pull, collect.

#include "conjure/bench/benchmark.h"

#include "conjure/core/clock.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/math.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/ecs/world.h"
#include "conjure/physics/components.h"
#include "conjure/physics/physics_settings.h"
#include "conjure/physics/physics_world.h"
#include "conjure/physics/register.h"
#include "conjure/scene/components.h"
#include "conjure/scene/register.h"

#include <cmath>
#include <span>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::bench;

namespace {

    constexpr f32 STEP_SECONDS = 1.0f / 60.0f;
    constexpr f32 GRID_SPACING = 2.0f;
    constexpr f32 GROUND_HALF_EXTENT = 400.0f;
    constexpr u32 SETTLE_STEPS = 600;
    constexpr u32 SETTLING_BOX_COUNT = 2000;

    /// <summary>
    /// Registry (scene + physics components), world and an attached physics world.
    /// </summary>
    class physics_fixture {
        public:
            ecs::component_registry registry = {};
            ecs::world world;                   // no default: built on the registry
            physics::physics_world physics;     // no default: built from settings and jobs

        public:
            physics_fixture(core::jobs::job_system &jobs, const physics::physics_settings &settings) :
                world((register_all(registry), registry)),
                physics(settings, jobs) {
                CONJURE_UNUSED(physics.attach(world));
            }

            ~physics_fixture() {
                physics.detach();
            }

            physics_fixture(const physics_fixture &) = delete;
            physics_fixture &operator=(const physics_fixture &) = delete;

        public:
            static void register_all(ecs::component_registry &target) {
                CONJURE_UNUSED(scene::register_components(target));
                CONJURE_UNUSED(physics::register_components(target));
            }

            /// <summary>
            /// A box body at a position (pending until the next push). NULL_ENTITY when the world refused it.
            /// </summary>
            [[nodiscard]] ecs::entity spawn_box(
              const math::vec3 &position,
              physics::motion_type motion,
              const math::vec3 &halfExtents,
              physics::rigid_body_flags flags
            ) {
                const core::result<ecs::entity> created = world.create_entity();
                if (!created.has_value()) {
                    return ecs::NULL_ENTITY;
                }
                scene::local_transform where = {};
                where.position = position;
                physics::rigid_body body = {};
                body.motionType = motion;
                body.flags = flags;
                physics::box_collider collider = {};
                collider.halfExtents = halfExtents;
                const bool placed = world.add<scene::local_transform>(created.value(), where).has_value() &&
                  world.add<physics::rigid_body>(created.value(), body).has_value() &&
                  world.add<physics::box_collider>(created.value(), collider).has_value();
                return placed ? created.value() : ecs::NULL_ENTITY;
            }

            /// <summary>
            /// A static slab whose top face is y = 0, wide enough for every grid below.
            /// </summary>
            [[nodiscard]] ecs::entity spawn_ground() {
                return spawn_box(
                  math::vec3(0.0f, -0.5f, 0.0f),
                  physics::motion_type::STATIC,
                  math::vec3(GROUND_HALF_EXTENT, 0.5f, GROUND_HALF_EXTENT),
                  physics::rigid_body_flags::NONE
                );
            }

            /// <summary>
            /// 'count' unit boxes on a square grid, 'layers' high, starting at 'baseHeight'. Returns how
            /// many were created.
            /// </summary>
            [[nodiscard]] u32 spawn_grid(u32 count, u32 layers, f32 baseHeight, physics::rigid_body_flags flags) {
                const u32 columns = count / layers;
                const u32 side = static_cast<u32>(std::ceil(std::sqrt(static_cast<f32>(columns))));
                const f32 origin = -static_cast<f32>(side - 1) * GRID_SPACING * 0.5f;
                u32 created = 0;
                for (u32 index = 0; index < count; ++index) {
                    const u32 column = index / layers;
                    const u32 layer = index % layers;
                    const f32 x = origin + static_cast<f32>(column % side) * GRID_SPACING;
                    const f32 z = origin + static_cast<f32>(column / side) * GRID_SPACING;
                    const f32 y = baseHeight + static_cast<f32>(layer) * 1.0f;
                    const ecs::entity box = spawn_box(
                      math::vec3(x, y, z),
                      physics::motion_type::DYNAMIC,
                      math::vec3(0.5f),
                      flags
                    );
                    if (box != ecs::NULL_ENTITY) {
                        ++created;
                    }
                }
                return created;
            }

            /// <summary>
            /// One fixed step: push, step, pull, collect.
            /// </summary>
            void run_step() {
                physics.push_transforms(STEP_SECONDS);
                physics.step(STEP_SECONDS);
                physics.pull_transforms();
                physics.collect_contact_events();
            }

            void run_steps(u32 count) {
                for (u32 index = 0; index < count; ++index) {
                    run_step();
                }
            }
    };

    /// <summary>
    /// The engine's view of the physics systems: a scheduler over the fixture's world with the four systems
    /// of physics::register_systems, stages inline (so job_system::schedule's counter stays out of the
    /// allocation gate) and parallel chunks, which is how physics.pull_transforms fans out over the workers
    /// in engine::application (14_physics.md 3.7 PARALLEL). Every run of FIXED_END also collects the
    /// scheduler's per-system timings so the pull and the contact collection are reported apart.
    /// </summary>
    class physics_schedule {
        public:
            ecs::scheduler scheduler;                   // no default: bound to the world
            ecs::frame_info frame = {};
            std::vector<f64> pullMilliseconds = {};
            std::vector<f64> contactsMilliseconds = {};
            std::string failure = {};

        public:
            physics_schedule(physics_fixture &fixture, core::jobs::job_system &jobs) :
                scheduler(fixture.world, &jobs, make_scheduler_desc()) {
                const core::result<void> registered = physics::register_systems(scheduler, fixture.physics);
                if (!registered.has_value()) {
                    failure = "register_systems: " + registered.get_error().to_string();
                    return;
                }
                const core::result<void> finalised = scheduler.finalise();
                if (!finalised.has_value()) {
                    failure = "scheduler: " + finalised.get_error().to_string();
                    return;
                }
                frame.deltaTime = STEP_SECONDS;
                frame.fixedDeltaTime = STEP_SECONDS;
                frame.inFixedStep = true;
            }

            ~physics_schedule() {}

            physics_schedule(const physics_schedule &) = delete;
            physics_schedule &operator=(const physics_schedule &) = delete;

        public:
            [[nodiscard]] bool ready() const noexcept {
                return failure.empty();
            }

            /// <summary>
            /// Reserves the timing vectors so the measured runs never grow them.
            /// </summary>
            void reserve(u32 runs) {
                pullMilliseconds.reserve(pullMilliseconds.size() + runs);
                contactsMilliseconds.reserve(contactsMilliseconds.size() + runs);
            }

            /// <summary>
            /// One FIXED_END: physics.pull_transforms (PARALLEL) then physics.contact_events (MAIN_THREAD).
            /// </summary>
            void run_fixed_end() {
                ++frame.frameIndex;
                scheduler.run_phase(ecs::system_phase::FIXED_END, frame);
                for (const ecs::system_timing &timing : scheduler.last_timings(ecs::system_phase::FIXED_END)) {
                    const f64 milliseconds = static_cast<f64>(timing.microseconds) / 1000.0;
                    if (timing.name == physics::SYSTEM_PULL_TRANSFORMS) {
                        pullMilliseconds.push_back(milliseconds);
                    } else if (timing.name == physics::SYSTEM_CONTACT_EVENTS) {
                        contactsMilliseconds.push_back(milliseconds);
                    }
                }
            }

            /// <summary>
            /// The last 'count' entries of a timing vector.
            /// </summary>
            [[nodiscard]] static std::span<const f64> tail(const std::vector<f64> &samples, u32 count) noexcept {
                const usize keep = samples.size() < count ? samples.size() : count;
                return std::span<const f64>(samples).subspan(samples.size() - keep, keep);
            }

        private:
            [[nodiscard]] static ecs::scheduler_desc make_scheduler_desc() noexcept {
                ecs::scheduler_desc desc = {};
                desc.parallelSystems = false;
                desc.parallelChunks = true;
                return desc;
            }
    };

    /// <summary>
    /// Times 'count' calls of the serial pull_transforms() (no system_context: the single-threaded
    /// fallback) for the informational phase of physics.sync_10k.
    /// </summary>
    [[nodiscard]] std::vector<f64> time_serial_pull(physics_fixture &fixture, u32 count) {
        std::vector<f64> samples = {};
        samples.reserve(count);
        for (u32 index = 0; index < count; ++index) {
            const u64 start = core::ticks_now();
            fixture.physics.pull_transforms();
            const u64 end = core::ticks_now();
            samples.push_back(core::ticks_to_seconds(end - start) * 1000.0);
        }
        return samples;
    }

    [[nodiscard]] physics::physics_settings bench_settings(bool allowSleeping) {
        physics::physics_settings settings = {};
        settings.allowSleeping = allowSleeping;
        settings.timeBeforeSleepSeconds = 0.3f;
        return settings;
    }

    /// <summary>
    /// 10,000 awake boxes resting on the plane: the state after they fell onto it, with sleeping off.
    /// </summary>
    [[nodiscard]] bool build_active_field(physics_fixture &fixture, u32 count) {
        if (fixture.spawn_ground() == ecs::NULL_ENTITY) {
            return false;
        }
        const u32 created = fixture.spawn_grid(count, 1, 0.75f, physics::rigid_body_flags::NONE);
        if (created != count) {
            return false;
        }
        // The quarter-metre drop lands within the first few steps; a short settle makes every measured
        // step a resting-contact step.
        fixture.run_steps(30);
        return fixture.physics.body_count() == count + 1;
    }
}

CONJURE_BENCHMARK(physics.step_10k_active) {
    physics_fixture fixture(context.jobs(), bench_settings(false));
    if (!build_active_field(fixture, context.entity_count())) {
        context.skip("the physics world refused the bodies");
        return;
    }
    context.set_items(context.entity_count());
    context.measure([&]() {
        fixture.run_step();
    });
}

CONJURE_BENCHMARK(physics.step_10k_sleeping) {
    physics_fixture fixture(context.jobs(), bench_settings(true));
    if (fixture.spawn_ground() == ecs::NULL_ENTITY) {
        context.skip("the physics world refused the ground");
        return;
    }
    const physics::rigid_body_flags asleep =
      physics::rigid_body_flags::ALLOW_SLEEPING | physics::rigid_body_flags::START_ASLEEP;
    if (fixture.spawn_grid(context.entity_count(), 1, 0.5f, asleep) != context.entity_count()) {
        context.skip("the physics world refused the bodies");
        return;
    }
    fixture.run_steps(SETTLE_STEPS);
    if (fixture.physics.active_body_count() != 0) {
        context.skip("the field did not fall asleep within the settle steps");
        return;
    }
    context.set_items(context.entity_count());
    context.measure([&]() {
        fixture.run_step();
    });
}

CONJURE_BENCHMARK(physics.sync_10k) {
    physics_fixture fixture(context.jobs(), bench_settings(false));
    if (!build_active_field(fixture, context.entity_count())) {
        context.skip("the physics world refused the bodies");
        return;
    }
    // The design row is the sync the engine frame runs: physics.pull_transforms is PARALLEL over chunks
    // when driven through a scheduler with a job system, while pull_transforms() alone takes the serial
    // fallback. The serial figure is kept as an informational phase.
    physics_schedule schedule(fixture, context.jobs());
    if (!schedule.ready()) {
        context.skip(schedule.failure);
        return;
    }
    const u32 measured = context.options().measuredIterations;
    schedule.reserve(context.options().warmupIterations + measured);
    context.set_items(context.entity_count());
    context.measure([&]() {
        schedule.run_fixed_end();
    });
    context.add_phase("physics.pull_transforms", physics_schedule::tail(schedule.pullMilliseconds, measured));
    context.add_phase("physics.contact_events", physics_schedule::tail(schedule.contactsMilliseconds, measured));
    context.add_phase("SERIAL_PULL", time_serial_pull(fixture, measured));
}

CONJURE_BENCHMARK(physics.step_2k_settling) {
    physics_fixture fixture(context.jobs(), bench_settings(false));
    if (fixture.spawn_ground() == ecs::NULL_ENTITY) {
        context.skip("the physics world refused the ground");
        return;
    }
    // 1,000 columns of two boxes dropped from a quarter metre: box-on-box and box-on-ground contacts.
    if (fixture.spawn_grid(SETTLING_BOX_COUNT, 2, 0.75f, physics::rigid_body_flags::NONE) != SETTLING_BOX_COUNT) {
        context.skip("the physics world refused the bodies");
        return;
    }
    fixture.run_steps(30);
    context.set_items(SETTLING_BOX_COUNT);
    context.measure([&]() {
        fixture.run_step();
    });
}
