#include "conjure/testing/doctest.h"
#include "physics_test_fixture.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/assets/assets_error.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/physics/collision_mesh_asset.h"
#include "conjure/physics/physics_error.h"
#include "conjure/physics/register.h"
#include "conjure/scene/components.h"

#include <span>

using namespace conjure;
using namespace physics_test;

namespace {

    [[nodiscard]] const ecs::system_desc *find_system(ecs::scheduler &scheduler, core::string_id name) {
        for (u32 phase = 0; phase < ecs::SYSTEM_PHASE_COUNT; ++phase) {
            for (const ecs::stage_info &stage : scheduler.stages(static_cast<ecs::system_phase>(phase))) {
                for (const ecs::system_id id : stage.systems) {
                    const ecs::system_desc *desc = scheduler.system(id);
                    if (desc != nullptr && desc->name == name) {
                        return desc;
                    }
                }
            }
        }
        return nullptr;
    }
}

TEST_SUITE("physics/register") {

    TEST_CASE("register_systems - adds four systems with the designed phases flags and labels") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        ecs::scheduler scheduler(fixture.world, nullptr);
        CONJURE_REQUIRE_OK(physics::register_systems(scheduler, fixture.physics));
        CHECK(scheduler.system_count() == 4);

        const ecs::system_desc *push = find_system(scheduler, physics::SYSTEM_PUSH_TRANSFORMS);
        REQUIRE(push != nullptr);
        CHECK(push->phase == ecs::system_phase::PHYSICS_STEP);
        CHECK(has_flag(push->flags, ecs::system_flags::MAIN_THREAD));
        CHECK_FALSE(has_flag(push->flags, ecs::system_flags::RUNS_WHILE_PAUSED));
        REQUIRE(push->before.size() == 1);
        CHECK(push->before[0] == physics::SYSTEM_STEP);
        CHECK(push->reads.test(fixture.registry.id_of<physics::rigid_body>()));
        CHECK(push->reads.test(fixture.registry.id_of<physics::mesh_collider>()));
        CHECK(push->reads.test(fixture.registry.id_of<scene::local_transform>()));
        CHECK(push->reads.test(fixture.registry.id_of<scene::parent>()));
        CHECK(push->writes.test(fixture.registry.id_of<scene::interpolated_transform>()));

        const ecs::system_desc *step = find_system(scheduler, physics::SYSTEM_STEP);
        REQUIRE(step != nullptr);
        CHECK(step->phase == ecs::system_phase::PHYSICS_STEP);
        CHECK(has_flag(step->flags, ecs::system_flags::MAIN_THREAD));
        CHECK(step->reads.none());
        CHECK(step->writes.none());

        const ecs::system_desc *pull = find_system(scheduler, physics::SYSTEM_PULL_TRANSFORMS);
        REQUIRE(pull != nullptr);
        CHECK(pull->phase == ecs::system_phase::FIXED_END);
        CHECK(pull->flags == ecs::system_flags::NONE);
        CHECK(pull->writes.test(fixture.registry.id_of<scene::local_transform>()));
        CHECK(pull->writes.test(fixture.registry.id_of<physics::rigid_body>()));
        CHECK(pull->writes.test(fixture.registry.id_of<physics::character_controller>()));
        CHECK(pull->reads.test(fixture.registry.id_of<scene::parent>()));
        REQUIRE(pull->before.size() == 1);
        CHECK(pull->before[0] == physics::SYSTEM_CONTACT_EVENTS);

        const ecs::system_desc *contacts = find_system(scheduler, physics::SYSTEM_CONTACT_EVENTS);
        REQUIRE(contacts != nullptr);
        CHECK(contacts->phase == ecs::system_phase::FIXED_END);
        CHECK(has_flag(contacts->flags, ecs::system_flags::MAIN_THREAD));

        // Stage order: push before step, pull before contacts.
        const std::span<const ecs::stage_info> stepStages = scheduler.stages(ecs::system_phase::PHYSICS_STEP);
        REQUIRE(stepStages.size() == 2);
        CHECK(scheduler.system(stepStages[0].systems[0])->name == physics::SYSTEM_PUSH_TRANSFORMS);
        CHECK(scheduler.system(stepStages[1].systems[0])->name == physics::SYSTEM_STEP);
        const std::span<const ecs::stage_info> endStages = scheduler.stages(ecs::system_phase::FIXED_END);
        REQUIRE(endStages.size() == 2);
        CHECK(scheduler.system(endStages[0].systems[0])->name == physics::SYSTEM_PULL_TRANSFORMS);
        CHECK(scheduler.system(endStages[1].systems[0])->name == physics::SYSTEM_CONTACT_EVENTS);
    }

    TEST_CASE("register_systems - before attach is NOT_ATTACHED") {
        ecs::component_registry registry = {};
        register_physics_test_components(registry);
        ecs::world world(registry);
        core::jobs::job_system jobs = {};
        physics::physics_world physics(test_settings(), jobs);
        ecs::scheduler scheduler(world, nullptr);
        CONJURE_CHECK_ERROR(physics::register_systems(scheduler, physics), physics::physics_error::NOT_ATTACHED);
        CHECK(scheduler.system_count() == 0);
    }

    TEST_CASE("register_systems - finalise succeeds together with the scene systems") {
        for_each_job_mode([](core::jobs::job_system &jobs) {
            physics_fixture fixture(jobs);
            ecs::scheduler scheduler(fixture.world, jobs.worker_count() == 0 ? nullptr : &jobs);
            CONJURE_REQUIRE_OK(physics::register_systems(scheduler, fixture.physics));
            // The scene systems need a scene_manager; their presence is simulated by a system writing the
            // world transforms in TRANSFORM, which must not conflict with the physics stages.
            ecs::system_desc transforms = {};
            transforms.name = core::string_id("scene.propagate_transforms");
            transforms.phase = ecs::system_phase::TRANSFORM;
            transforms.flags = ecs::system_flags::RUNS_WHILE_PAUSED;
            transforms.resolveAccess = [](
              const ecs::component_registry &registry,
              ecs::component_mask &reads,
              ecs::component_mask &writes
            ) {
                reads.set(registry.id_of<scene::local_transform>());
                reads.set(registry.id_of<scene::interpolated_transform>());
                writes.set(registry.id_of<scene::world_transform>());
            };
            transforms.run = [](ecs::system_context &) {};
            CONJURE_REQUIRE_OK(scheduler.add_system(std::move(transforms)));
            CONJURE_REQUIRE_OK(scheduler.finalise());
            CHECK(scheduler.stages(ecs::system_phase::PHYSICS_STEP).size() == 2);
            CHECK(scheduler.stages(ecs::system_phase::FIXED_END).size() == 2);
            CHECK(scheduler.stages(ecs::system_phase::TRANSFORM).size() == 1);
            CHECK(scheduler.stages(ecs::system_phase::UPDATE).empty());
        });
    }

    TEST_CASE("register_systems - running PHYSICS_STEP and FIXED_END through run_phase moves a falling body") {
        for_each_job_mode([](core::jobs::job_system &jobs) {
            physics_fixture fixture(jobs);
            ecs::scheduler_desc desc = {};
            desc.parallelSystems = false;
            ecs::scheduler scheduler(fixture.world, jobs.worker_count() == 0 ? nullptr : &jobs, desc);
            CONJURE_REQUIRE_OK(physics::register_systems(scheduler, fixture.physics));
            CONJURE_REQUIRE_OK(scheduler.finalise());

            const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 10.0f, 0.0f));
            ecs::frame_info frame = {};
            frame.deltaTime = STEP;
            frame.fixedDeltaTime = STEP;
            frame.inFixedStep = true;
            for (u32 step = 0; step < 30; ++step) {
                scheduler.run_phase(ecs::system_phase::PHYSICS_STEP, frame);
                scheduler.run_phase(ecs::system_phase::FIXED_END, frame);
            }
            CHECK(fixture.physics.has_body(box));
            // The first PHYSICS_STEP creates the body and steps once; 30 steps of fall were simulated.
            const f32 expected = 10.0f - free_fall_drop(30);
            CHECK(fixture.transform_position(box).y == doctest::Approx(expected).epsilon(0.02));
            const scene::interpolated_transform *interpolated = fixture.world.get<scene::interpolated_transform>(box);
            REQUIRE(interpolated != nullptr);
            CHECK(interpolated->current.position.y < interpolated->previous.position.y);
        });
    }

    TEST_CASE("register_asset_types - registers the collision mesh type and importer once") {
        assets::asset_type_registry types = {};
        assets::importer_registry importers = {};
        CONJURE_REQUIRE_OK(physics::register_asset_types(types, importers));
        const assets::asset_type_desc *type = types.find(physics::COLLISION_MESH_ASSET_TYPE);
        REQUIRE(type != nullptr);
        CHECK(type->id == assets::asset_type_traits<physics::collision_mesh_asset>::id());
        CHECK(type->version == 1);
        CHECK(type->loader != nullptr);
        CHECK(type->loader->type() == type->id);
        CHECK(importers.importers().size() == 1);
        const assets::asset_importer *importer = importers.find(physics::COLLISION_MESH_IMPORTER_ID);
        REQUIRE(importer != nullptr);
        CHECK(importers.find_for_extension(".gltf") == importer);
        CHECK(importers.find_for_extension(".GLB") == importer);
        CHECK(importers.find_for_extension("obj") == importer);
        CHECK(importers.find_for_extension(".fbx") == nullptr);
        CONJURE_CHECK_ERROR(physics::register_asset_types(types, importers), assets::asset_error_code::TYPE_DUPLICATE);
    }
}
