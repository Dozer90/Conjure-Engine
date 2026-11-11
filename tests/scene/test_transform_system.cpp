#include "conjure/testing/doctest.h"
#include "conjure/testing/approx.h"
#include "scene_test_fixture.h"

#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/scene/register.h"
#include "conjure/scene/transform_system.h"

#include <span>
#include <utility>

using namespace conjure;
using namespace scene_test;

TEST_SUITE("scene/transform_system") {

    TEST_CASE("transform_system - describe declares reads and writes") {
        scene_fixture fixture = {};
        scene::transform_system &system = fixture.manager.transforms();
        const ecs::system_desc desc = system.describe();
        const scene::scene_component_ids &ids = fixture.active().ids();
        CHECK(desc.name == core::string_id(scene::SYSTEM_PROPAGATE_TRANSFORMS));
        CHECK(desc.name.view() == "scene.propagate_transforms");
        CHECK(desc.phase == ecs::system_phase::TRANSFORM);
        CHECK(has_flag(desc.flags, ecs::system_flags::RUNS_WHILE_PAUSED));
        CHECK_FALSE(has_flag(desc.flags, ecs::system_flags::EXCLUSIVE));
        CHECK_FALSE(has_flag(desc.flags, ecs::system_flags::MAIN_THREAD));
        CHECK(desc.reads.test(ids.localTransform));
        CHECK(desc.reads.test(ids.parent));
        CHECK(desc.reads.test(ids.children));
        CHECK(desc.reads.test(ids.interpolatedTransform));
        CHECK(desc.reads.test(ids.transformDirty));
        CHECK(desc.writes.test(ids.worldTransform));
        CHECK(desc.writes.test(ids.renderWorldTransform));
        CHECK(desc.writes.test(ids.transformState));
        CHECK_FALSE(desc.writes.test(ids.localTransform));
        CHECK(static_cast<bool>(desc.run));
        CHECK(system.interpolation_enabled());
        CHECK(system.run_count() == 0);
    }

    TEST_CASE("regression - review 4 a TRANSFORM writer of children is staged apart from propagate_transforms") {
        // The pass reads `children` from job threads (it marks a recomputed row's children pending) and the
        // scheduler packs a phase's stages from the declared masks alone, so an undeclared read would let
        // a system writing `children` in the same phase share the pass's stage and mutate the lists the
        // jobs iterate.
        scene_fixture fixture = {};
        const scene::scene_component_ids &ids = fixture.active().ids();
        ecs::system_desc writer = {};
        writer.name = core::string_id("test.children_writer");
        writer.phase = ecs::system_phase::TRANSFORM;
        writer.writes.set(ids.children);
        writer.run = [](ecs::system_context &) {};
        const core::result<ecs::system_id> writerId = fixture.scheduler.add_system(std::move(writer));
        CONJURE_REQUIRE_OK(writerId);
        CONJURE_REQUIRE_OK(fixture.scheduler.finalise());

        const std::span<const ecs::stage_info> stages = fixture.scheduler.stages(ecs::system_phase::TRANSFORM);
        const usize notFound = stages.size();
        usize propagateStage = notFound;
        usize writerStage = notFound;
        for (usize index = 0; index < stages.size(); ++index) {
            for (const ecs::system_id id : stages[index].systems) {
                const ecs::system_desc *member = fixture.scheduler.system(id);
                REQUIRE(member != nullptr);
                if (member->name == core::string_id(scene::SYSTEM_PROPAGATE_TRANSFORMS)) {
                    propagateStage = index;
                }
                if (id == writerId.value()) {
                    writerStage = index;
                }
            }
        }
        REQUIRE(propagateStage != notFound);
        REQUIRE(writerStage != notFound);
        CHECK(propagateStage != writerStage);
    }

    TEST_CASE("transform_system - stats count visited and skipped") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        scene::transform_system &system = fixture.manager.transforms();
        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child", root);
        const ecs::entity other = fixture.create("other");
        CONJURE_UNUSED(other);

        fixture.step_transform();
        const scene::propagation_stats first = system.last_stats();
        CHECK(system.run_count() == 1);
        CHECK(first.levels == 2);
        CHECK(first.entitiesVisited == 3);
        CHECK(first.worldRecomputed == 3);
        CHECK(first.renderRecomputed == 3);
        CHECK(first.chunksVisited >= 2);
        CHECK(first.chunksSkipped == 0);

        // The dirty tags left the chunks: the next frame recomputes once more (the rows moved) ...
        fixture.step_transform();
        CHECK(system.last_stats().worldRecomputed == 3);
        // ... and then nothing is touched.
        fixture.step_transform();
        const scene::propagation_stats quiet = system.last_stats();
        CHECK(quiet.entitiesVisited == 0);
        CHECK(quiet.worldRecomputed == 0);
        CHECK(quiet.renderRecomputed == 0);
        CHECK(quiet.chunksVisited == 0);
        CHECK(quiet.chunksSkipped >= 2);
        CHECK(quiet.levels == 2);

        // Moving the child alone touches only its chunk.
        CONJURE_REQUIRE_OK(target.set_local_transform(child, at(1.0f, 0.0f, 0.0f)));
        fixture.step_transform();
        CHECK(system.last_stats().worldRecomputed == 1);
        CHECK(system.last_stats().chunksVisited == 1);
        CHECK(system.last_stats().chunksSkipped >= 1);
        CONJURE_CHECK_APPROX(translation_of(fixture.world_matrix(child)), math::vec3(1.0f, 0.0f, 0.0f));
    }

    TEST_CASE("transform_system - interpolation disabled uses current pose") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        scene::transform_system &system = fixture.manager.transforms();
        const ecs::entity body = fixture.create("body");
        CONJURE_REQUIRE_OK(target.set_local_transform(body, at(10.0f, 0.0f, 0.0f)));
        scene::interpolated_transform pose = {};
        pose.previous.position = math::vec3(0.0f, 0.0f, 0.0f);
        pose.current.position = math::vec3(4.0f, 0.0f, 0.0f);
        CONJURE_REQUIRE_OK(fixture.world.add<scene::interpolated_transform>(body, pose));
        fixture.frame.interpolationAlpha = 0.5f;

        fixture.step_transform();
        CONJURE_CHECK_APPROX(translation_of(fixture.render_matrix(body)), math::vec3(2.0f, 0.0f, 0.0f));
        CONJURE_CHECK_APPROX(translation_of(fixture.world_matrix(body)), math::vec3(10.0f, 0.0f, 0.0f));

        system.set_interpolation_enabled(false);
        CHECK_FALSE(system.interpolation_enabled());
        fixture.step_transform();
        CONJURE_CHECK_APPROX(translation_of(fixture.render_matrix(body)), math::vec3(4.0f, 0.0f, 0.0f));
        CHECK(system.last_stats().renderRecomputed == 1);

        system.set_interpolation_enabled(true);
        fixture.step_transform();
        CONJURE_CHECK_APPROX(translation_of(fixture.render_matrix(body)), math::vec3(2.0f, 0.0f, 0.0f));
    }

    TEST_CASE("compute_world_transform - composes the chain exactly") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child", root);
        scene::local_transform rootLocal = at(1.0f, 0.0f, 0.0f);
        rootLocal.rotation = glm::angleAxis(math::HALF_PI, math::vec3(0.0f, 0.0f, 1.0f));
        rootLocal.scale = math::vec3(2.0f);
        CONJURE_REQUIRE_OK(target.set_local_transform(root, rootLocal));
        CONJURE_REQUIRE_OK(target.set_local_transform(child, at(1.0f, 0.0f, 0.0f)));
        const math::transform world = scene::compute_world_transform(target, child);
        // Rotating (2, 0, 0) by 90 degrees about Z gives (0, 2, 0), plus the root's offset.
        CONJURE_CHECK_APPROX(world.position, math::vec3(1.0f, 2.0f, 0.0f));
        CONJURE_CHECK_APPROX(world.scale, math::vec3(2.0f));
        const math::mat4 childMatrix = at(1.0f, 0.0f, 0.0f).to_transform().to_matrix();
        const math::mat4 product = rootLocal.to_transform().to_matrix() * childMatrix;
        CONJURE_CHECK_APPROX(world.to_matrix(), product);
        // An entity without a transform yields the identity for itself; a dead one too.
        const core::result<ecs::entity> raw = fixture.world.create_entity();
        CONJURE_REQUIRE_OK(raw);
        CHECK(scene::compute_world_transform(target, raw.value()) == math::transform{});
        CHECK(scene::compute_world_transform(target, ecs::entity{900, 1}) == math::transform{});
        CONJURE_REQUIRE_OK(fixture.world.destroy_entity(raw.value()));
    }
}
