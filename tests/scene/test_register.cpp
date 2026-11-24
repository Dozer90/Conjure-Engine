#include "conjure/testing/doctest.h"
#include "scene_test_fixture.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/assets/assets_error.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/scene/prefab.h"
#include "conjure/scene/register.h"
#include "conjure/scene/scene_serializer.h"

#include <string>
#include <vector>

using namespace conjure;
using namespace scene_test;

namespace {

    /// <summary>
    /// The description of the system registered under 'name', or nullptr.
    /// </summary>
    [[nodiscard]] const ecs::system_desc *find_system(ecs::scheduler &scheduler, std::string_view name) {
        for (u32 phase = 0; phase < ecs::SYSTEM_PHASE_COUNT; ++phase) {
            for (const ecs::stage_info &stage : scheduler.stages(static_cast<ecs::system_phase>(phase))) {
                for (const ecs::system_id id : stage.systems) {
                    const ecs::system_desc *desc = scheduler.system(id);
                    if (desc != nullptr && desc->name == core::string_id(name)) {
                        return desc;
                    }
                }
            }
        }
        return nullptr;
    }
}

TEST_SUITE("scene/register") {

    TEST_CASE("register_systems - adds four systems in expected phases and flags") {
        scene_fixture fixture = {};
        ecs::scheduler &scheduler = fixture.scheduler;
        CHECK(scheduler.system_count() == 4);

        const ecs::system_desc *loads = find_system(scheduler, scene::SYSTEM_APPLY_PENDING_LOADS);
        REQUIRE(loads != nullptr);
        CHECK(loads->phase == ecs::system_phase::FRAME_BEGIN);
        CHECK(has_flag(loads->flags, ecs::system_flags::EXCLUSIVE));
        CHECK(has_flag(loads->flags, ecs::system_flags::RUNS_WHILE_PAUSED));

        const ecs::system_desc *hierarchy = find_system(scheduler, scene::SYSTEM_APPLY_HIERARCHY_CHANGES);
        REQUIRE(hierarchy != nullptr);
        CHECK(hierarchy->phase == ecs::system_phase::TRANSFORM);
        CHECK(has_flag(hierarchy->flags, ecs::system_flags::EXCLUSIVE));
        CHECK(has_flag(hierarchy->flags, ecs::system_flags::RUNS_WHILE_PAUSED));
        REQUIRE(hierarchy->before.size() == 1);
        CHECK(hierarchy->before[0] == core::string_id(scene::SYSTEM_PROPAGATE_TRANSFORMS));

        const ecs::system_desc *propagate = find_system(scheduler, scene::SYSTEM_PROPAGATE_TRANSFORMS);
        REQUIRE(propagate != nullptr);
        CHECK(propagate->phase == ecs::system_phase::TRANSFORM);
        CHECK_FALSE(has_flag(propagate->flags, ecs::system_flags::EXCLUSIVE));
        CHECK(has_flag(propagate->flags, ecs::system_flags::RUNS_WHILE_PAUSED));
        CHECK(propagate->writes.test(fixture.active().ids().worldTransform));

        const ecs::system_desc *destroy = find_system(scheduler, scene::SYSTEM_DESTROY_PENDING);
        REQUIRE(destroy != nullptr);
        CHECK(destroy->phase == ecs::system_phase::FRAME_END);
        CHECK(has_flag(destroy->flags, ecs::system_flags::EXCLUSIVE));
        CHECK(has_flag(destroy->flags, ecs::system_flags::RUNS_WHILE_PAUSED));
        REQUIRE(destroy->after.size() == 1);
        CHECK(destroy->after[0] == core::string_id("scripting.frame_end"));

        // The TRANSFORM phase runs the repair stage before the propagation stage.
        const std::span<const ecs::stage_info> transformStages = scheduler.stages(ecs::system_phase::TRANSFORM);
        REQUIRE(transformStages.size() == 2);
        CHECK(transformStages[0].exclusive);
        const core::string_id repairName(scene::SYSTEM_APPLY_HIERARCHY_CHANGES);
        const core::string_id propagateName(scene::SYSTEM_PROPAGATE_TRANSFORMS);
        CHECK(scheduler.system(transformStages[0].systems[0])->name == repairName);
        CHECK_FALSE(transformStages[1].exclusive);
        CHECK(scheduler.system(transformStages[1].systems[0])->name == propagateName);

        // Paused frames still run every scene system.
        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child", root);
        CONJURE_REQUIRE_OK(fixture.active().set_local_transform(root, at(1.0f, 0.0f, 0.0f)));
        fixture.active().destroy(child);
        scheduler.run_phase_filtered(ecs::system_phase::TRANSFORM, fixture.frame, ecs::system_flags::RUNS_WHILE_PAUSED);
        scheduler.run_phase_filtered(ecs::system_phase::FRAME_END, fixture.frame, ecs::system_flags::RUNS_WHILE_PAUSED);
        CHECK(translation_of(fixture.world_matrix(root)) == math::vec3(1.0f, 0.0f, 0.0f));
        CHECK_FALSE(fixture.active().contains(child));
    }

    TEST_CASE("register_systems - scheduler finalise succeeds") {
        ecs::component_registry registry = {};
        register_scene_test_components(registry);
        ecs::world world(registry);
        core::jobs::job_system jobs = {};
        assets::asset_manager_desc desc = {};
        desc.jobs = &jobs;
        desc.source = std::make_unique<assets::memory_asset_source>();
        assets::asset_manager assets(std::move(desc));
        scene::scene_manager manager(world, assets);
        ecs::scheduler scheduler(world, nullptr);
        CONJURE_REQUIRE_OK(scene::register_systems(scheduler, manager));
        CONJURE_REQUIRE_OK(scheduler.finalise());
        CHECK(scheduler.stages(ecs::system_phase::FRAME_BEGIN).size() == 1);
        CHECK(scheduler.stages(ecs::system_phase::TRANSFORM).size() == 2);
        CHECK(scheduler.stages(ecs::system_phase::FRAME_END).size() == 1);
        CHECK(scheduler.stages(ecs::system_phase::UPDATE).empty());
        // A system name is unique within its phase (code sweep F259): a second registration is refused at
        // its first system and adds nothing, instead of running every scene system twice per frame.
        CONJURE_CHECK_ERROR(scene::register_systems(scheduler, manager), ecs::ecs_error::NAME_CONFLICT);
        CHECK(scheduler.system_count() == 4);
        CONJURE_REQUIRE_OK(scheduler.finalise());
        CHECK(scheduler.stages(ecs::system_phase::TRANSFORM).size() == 2);
    }

    TEST_CASE("register_systems - destroy_pending runs after scripting.frame_end when present") {
        scene_fixture fixture = {};
        ecs::scheduler &scheduler = fixture.scheduler;
        std::vector<std::string> order = {};
        ecs::system_desc scripting = {};
        scripting.name = core::string_id("scripting.frame_end");
        scripting.phase = ecs::system_phase::FRAME_END;
        scripting.flags = ecs::system_flags::EXCLUSIVE | ecs::system_flags::RUNS_WHILE_PAUSED;
        scripting.run = [&order](ecs::system_context &) {
            order.push_back("scripting.frame_end");
        };
        CONJURE_REQUIRE_OK(scheduler.add_system(std::move(scripting)));
        CONJURE_REQUIRE_OK(scheduler.finalise());

        const ecs::entity doomed = fixture.create("doomed");
        const ecs::observer_id observer =
          fixture.world.observe_destroyed([&order](ecs::world &, ecs::entity, ecs::command_buffer &) {
              order.push_back("destroyed");
          });
        fixture.active().destroy(doomed);
        scheduler.run_phase(ecs::system_phase::FRAME_END, fixture.frame);
        fixture.world.unobserve(observer);
        REQUIRE(order.size() == 2);
        CHECK(order[0] == "scripting.frame_end");
        CHECK(order[1] == "destroyed");
        const std::span<const ecs::stage_info> stages = scheduler.stages(ecs::system_phase::FRAME_END);
        REQUIRE(stages.size() == 2);
        CHECK(scheduler.system(stages[0].systems[0])->name == core::string_id("scripting.frame_end"));
        CHECK(scheduler.system(stages[1].systems[0])->name == core::string_id(scene::SYSTEM_DESTROY_PENDING));
    }

    TEST_CASE("register_asset_types - registers two types and two importers") {
        scene_fixture fixture = {};
        assets::importer_registry importers = {};
        const usize typesBefore = fixture.assets.types().size();
        CONJURE_REQUIRE_OK(scene::register_asset_types(fixture.assets.types(), importers, fixture.registry));
        CHECK(fixture.assets.types().size() == typesBefore + 2);
        const assets::asset_type_desc *sceneType = fixture.assets.types().find(scene::SCENE_ASSET_TYPE_NAME);
        REQUIRE(sceneType != nullptr);
        CHECK(sceneType->id == assets::asset_type_traits<scene::scene_asset>::id());
        CHECK(sceneType->version == 1);
        CHECK(sceneType->flags == assets::asset_type_flags::NONE);
        CHECK(sceneType->loader != nullptr);
        const assets::asset_type_desc *prefabType = fixture.assets.types().find(scene::PREFAB_ASSET_TYPE_NAME);
        REQUIRE(prefabType != nullptr);
        CHECK(prefabType->id == assets::asset_type_traits<scene::prefab_asset>::id());
        CHECK(prefabType->loader->type() == prefabType->id);
        CHECK(importers.importers().size() == 2);
        CHECK(importers.find(scene::SCENE_IMPORTER_ID) != nullptr);
        CHECK(importers.find(scene::PREFAB_IMPORTER_ID) != nullptr);
        CHECK(importers.find_for_extension(".scene.json") == importers.find(scene::SCENE_IMPORTER_ID));
        CHECK(importers.find_for_extension(".prefab.json") == importers.find(scene::PREFAB_IMPORTER_ID));
        // Registering again is refused by the registries.
        CONJURE_CHECK_ERROR(scene::register_asset_types(fixture.assets.types(), importers, fixture.registry),
          assets::assets_error::TYPE_DUPLICATE);
    }

    TEST_CASE("register_components - fails cleanly on a conflicting registry") {
        ecs::component_registry registry = {};
        // A type already registered under a scene name with another layout conflicts.
        struct impostor {
            u64 a = 0;
            u64 b = 0;
        };
        ecs::dynamic_component_desc desc = {};
        desc.name = core::string_id("conjure.scene.layer");
        desc.size = sizeof(impostor);
        desc.alignment = 8;
        const std::array<ecs::dynamic_field_desc, 1> fields = {
            ecs::dynamic_field_desc{core::string_id("a"), core::reflect::type_id_of<u64>(), 0},
        };
        desc.fields = fields;
        CONJURE_REQUIRE_OK(registry.register_dynamic(desc));
        CONJURE_CHECK_ERROR(scene::register_components(registry), ecs::ecs_error::NAME_CONFLICT);
        CHECK_FALSE(scene::scene_component_ids::resolve(registry).is_complete());
    }
}
