#include "conjure/testing/doctest.h"
#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/approx.h"
#include "scene_test_fixture.h"

#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/system.h"
#include "conjure/scene/transform_system.h"

#include <random>
#include <vector>

using namespace conjure;
using namespace scene_test;

namespace {

    /// <summary>
    /// A deterministic TRS from the test seed.
    /// </summary>
    scene::local_transform random_transform(std::mt19937_64 &engine) {
        std::uniform_real_distribution<f32> unit(-1.0f, 1.0f);
        std::uniform_real_distribution<f32> scaleRange(0.5f, 2.0f);
        scene::local_transform value = {};
        value.position = math::vec3(unit(engine), unit(engine), unit(engine)) * 5.0f;
        const math::vec3 axis = math::safe_normalize(math::vec3(unit(engine), unit(engine), unit(engine)));
        value.rotation = glm::angleAxis(unit(engine) * math::PI, axis);
        const f32 uniform = scaleRange(engine);
        value.scale = math::vec3(uniform);
        return value;
    }

    /// <summary>
    /// Runs the propagation pass through a system_context built like the scheduler does, so the pass
    /// alone (not the scheduler's own per-stage bookkeeping) can be gated for allocations.
    /// </summary>
    class direct_pass {
        private:
            scene_fixture *mFixture = nullptr;
            core::jobs::job_system *mJobs = nullptr;
            ecs::system_desc mDesc = {};
            ecs::system_scratch mScratch = {};
            ecs::command_buffer mCommands;
            u64 mSinceVersion = 0;

        public:
            direct_pass(scene_fixture &fixture, core::jobs::job_system *jobs) :
                mFixture(&fixture),
                mJobs(jobs),
                mDesc(fixture.manager.transforms().describe()),
                mCommands(fixture.world) {}

            void run() {
                ++mFixture->frame.frameIndex;
                mFixture->active().apply_hierarchy_changes();
                const u64 runVersion = mFixture->world.bump_version();
                ecs::system_context context(
                  mFixture->world,
                  mFixture->frame,
                  mSinceVersion,
                  runVersion,
                  mCommands,
                  mJobs,
                  mDesc,
                  mScratch,
                  true,
                  1024
                );
                mFixture->manager.transforms().run(context);
                mSinceVersion = runVersion;
                const ecs::flush_report report = mFixture->world.flush(mCommands);
                CONJURE_UNUSED(report);
            }
    };

    /// <summary>
    /// 10,000 entities in two trees: 5,000 under each of two roots, depth 4 per branch.
    /// </summary>
    struct two_trees {
        ecs::entity rootA = {};
        ecs::entity rootB = {};
        std::vector<ecs::entity> all = {};
    };

    two_trees build_two_trees(scene_fixture &fixture, std::mt19937_64 &engine) {
        two_trees trees = {};
        trees.rootA = fixture.create("a");
        trees.rootB = fixture.create("b");
        // Root b carries an extra component so the two roots live in different chunks and a change to
        // one root never drags the other root's chunk (and therefore its whole tree) along.
        REQUIRE(fixture.world.add<test_speed>(trees.rootB, test_speed{2.0f}).has_value());
        trees.all.push_back(trees.rootA);
        trees.all.push_back(trees.rootB);
        for (const ecs::entity root : {trees.rootA, trees.rootB}) {
            // Four levels below the root: 9 + 90 + 900 children, then the fourth level fills up to 5000.
            std::vector<ecs::entity> level = {root};
            const u32 fanOut[4] = {9, 10, 10, 5};
            u32 created = 1;
            for (u32 depth = 0; depth < 4 && created < 5000; ++depth) {
                std::vector<ecs::entity> next = {};
                for (const ecs::entity node : level) {
                    for (u32 index = 0; index < fanOut[depth] && created < 5000; ++index) {
                        scene::scene &target = fixture.active();
                        const core::result<ecs::entity> child = target.create_entity(core::string_id("n"), node);
                        REQUIRE(child.has_value());
                        REQUIRE(target.set_local_transform(child.value(), random_transform(engine)).has_value());
                        next.push_back(child.value());
                        trees.all.push_back(child.value());
                        ++created;
                    }
                }
                level = std::move(next);
            }
            REQUIRE(created == 5000);
        }
        REQUIRE(trees.all.size() == 10000);
        return trees;
    }
}

TEST_SUITE("scene/transform_propagation") {

    TEST_CASE("propagation - depth 8 chain matches glm product [parallel]") {
        for_each_job_mode([](core::jobs::job_system *jobs) {
            scene_fixture fixture(jobs);
            scene::scene &target = fixture.active();
            std::mt19937_64 engine(testing::environment().seed);
            std::vector<ecs::entity> chain = {fixture.create("root")};
            std::vector<math::mat4> locals = {};
            for (u32 level = 0; level < 8; ++level) {
                const scene::local_transform local = random_transform(engine);
                locals.push_back(local.to_transform().to_matrix());
                CONJURE_REQUIRE_OK(target.set_local_transform(chain.back(), local));
                if (level < 7) {
                    chain.push_back(fixture.create("link", chain.back()));
                }
            }
            fixture.step();
            math::mat4 expected = math::mat4(1.0f);
            for (u32 level = 0; level < 8; ++level) {
                CAPTURE(level);
                expected = expected * locals[level];
                const math::mat4 actual = fixture.world_matrix(chain[level]);
                CHECK(testing::approx_equal(actual, expected, 1.0e-4f));
                CHECK(testing::approx_equal(fixture.render_matrix(chain[level]), expected, 1.0e-4f));
                const math::mat4 walked = scene::compute_world_transform(target, chain[level]).to_matrix();
                CHECK(testing::approx_equal(walked, expected, 1.0e-3f));
            }
            CHECK(fixture.manager.transforms().last_stats().levels == 8);
        });
    }

    TEST_CASE("propagation - moving root updates leaf only through stamps") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        scene::transform_system &system = fixture.manager.transforms();
        const ecs::entity root = fixture.create("root");
        const ecs::entity middle = fixture.create("middle", root);
        const ecs::entity leaf = fixture.create("leaf", middle);
        CONJURE_REQUIRE_OK(target.set_local_transform(middle, at(0.0f, 1.0f, 0.0f)));
        CONJURE_REQUIRE_OK(target.set_local_transform(leaf, at(0.0f, 0.0f, 1.0f)));
        fixture.step(3);
        CHECK(system.last_stats().worldRecomputed == 0);

        // Only the root's local changes: the chunk filter catches the root, the stamps carry it down.
        CONJURE_REQUIRE_OK(target.set_local_transform(root, at(5.0f, 0.0f, 0.0f)));
        fixture.step_transform();
        CHECK(system.last_stats().worldRecomputed == 3);
        CHECK(system.last_stats().levels == 3);
        CONJURE_CHECK_APPROX(translation_of(fixture.world_matrix(leaf)), math::vec3(5.0f, 1.0f, 1.0f));
        CONJURE_CHECK_APPROX(translation_of(fixture.render_matrix(leaf)), math::vec3(5.0f, 1.0f, 1.0f));
        const scene::transform_state *state = fixture.world.get<scene::transform_state>(leaf);
        REQUIRE(state != nullptr);
        CHECK(state->lastUpdateRun == system.run_count());
        fixture.step_transform();
        CHECK(system.last_stats().worldRecomputed == 0);
    }

    TEST_CASE("propagation - unchanged chunks are skipped") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        scene::transform_system &system = fixture.manager.transforms();
        std::vector<ecs::entity> roots(300);
        CONJURE_REQUIRE_OK(target.create_entities(300, ecs::NULL_ENTITY, ecs::NULL_ENTITY, roots));
        fixture.step(3);
        CHECK(system.last_stats().chunksVisited == 0);
        const u32 skipped = system.last_stats().chunksSkipped;
        CHECK(skipped >= 2);

        // One write touches one chunk; every other chunk stays skipped and unstamped.
        CONJURE_REQUIRE_OK(target.set_local_transform(roots[150], at(1.0f, 2.0f, 3.0f)));
        fixture.step_transform();
        CHECK(system.last_stats().chunksVisited == 1);
        CHECK(system.last_stats().chunksSkipped == skipped - 1);
        CHECK(system.last_stats().worldRecomputed > 0);
        CHECK(system.last_stats().worldRecomputed < 300);
        fixture.step_transform();
        CHECK(system.last_stats().chunksVisited == 0);
    }

    TEST_CASE("propagation - dirty propagation touches only the subtree [parallel]") {
        for_each_job_mode([](core::jobs::job_system *jobs) {
            scene_fixture fixture(jobs);
            scene::scene &target = fixture.active();
            scene::transform_system &system = fixture.manager.transforms();
            std::mt19937_64 engine(testing::environment().seed);
            const two_trees trees = build_two_trees(fixture, engine);
            fixture.step(3);
            CHECK(system.last_stats().worldRecomputed == 0);

            CONJURE_REQUIRE_OK(target.set_local_transform(trees.rootB, at(1.0f, 2.0f, 3.0f)));
            fixture.step_transform();
            // Every descendant of b follows through the stamps and nothing under a moves.
            CHECK(system.last_stats().worldRecomputed == target.tree().subtree_size(trees.rootB));
            CHECK(system.last_stats().worldRecomputed == 5000);
            fixture.step_transform();
            CHECK(system.last_stats().worldRecomputed == 0);
        });
    }

    TEST_CASE("propagation - 10k entities allocation-free after frame 2 [parallel]") {
        for_each_job_mode([](core::jobs::job_system *jobs) {
            scene_fixture fixture(jobs);
            scene::scene &target = fixture.active();
            std::mt19937_64 engine(testing::environment().seed);
            const two_trees trees = build_two_trees(fixture, engine);
            direct_pass pass(fixture, jobs);
            pass.run();
            pass.run();
            // Steady state: frames 3 .. 10 touch nothing and allocate nothing, with and without a
            // root moving every frame.
            for (u32 frame = 3; frame <= 10; ++frame) {
                CAPTURE(frame);
                if (frame % 2 == 0) {
                    const scene::local_transform moved = at(static_cast<f32>(frame), 0.0f, 0.0f);
                    CONJURE_REQUIRE_OK(target.set_local_transform(trees.rootA, moved));
                }
                CONJURE_CHECK_NO_ALLOCATIONS(pass.run());
            }
            CHECK(fixture.manager.transforms().last_stats().worldRecomputed == 5000);
            CONJURE_CHECK_APPROX(translation_of(fixture.world_matrix(trees.rootA)), math::vec3(10.0f, 0.0f, 0.0f));
        });
    }

    TEST_CASE("propagation - whole TRANSFORM phase through the scheduler allocation-free after frame 2 [parallel]") {
        // 13_scene.md 10: the scheduler keeps its per-stage bookkeeping in the phase state, so the phase
        // (apply_hierarchy_changes, propagate_transforms, the command buffer flushes) allocates nothing
        // once it has run twice. The fixture's scheduler runs its stages inline (parallelSystems = false);
        // parallel_for_chunks still fans the pass out over the workers.
        for_each_job_mode([](core::jobs::job_system *jobs) {
            scene_fixture fixture(jobs);
            scene::scene &target = fixture.active();
            std::mt19937_64 engine(testing::environment().seed);
            const two_trees trees = build_two_trees(fixture, engine);
            fixture.step_transform();
            fixture.step_transform();
            for (u32 frame = 3; frame <= 10; ++frame) {
                CAPTURE(frame);
                if (frame % 2 == 0) {
                    const scene::local_transform moved = at(static_cast<f32>(frame), 0.0f, 0.0f);
                    CONJURE_REQUIRE_OK(target.set_local_transform(trees.rootA, moved));
                }
                CONJURE_CHECK_NO_ALLOCATIONS(fixture.step_transform());
            }
            CHECK(fixture.manager.transforms().last_stats().worldRecomputed == 5000);
            CONJURE_CHECK_APPROX(translation_of(fixture.world_matrix(trees.rootA)), math::vec3(10.0f, 0.0f, 0.0f));
        });
    }

    TEST_CASE("propagation - interpolated entity renders lerp at alpha half") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity body = fixture.create("body");
        CONJURE_REQUIRE_OK(target.set_local_transform(body, at(100.0f, 0.0f, 0.0f)));
        scene::interpolated_transform pose = {};
        pose.previous.position = math::vec3(0.0f, 0.0f, 0.0f);
        pose.current.position = math::vec3(2.0f, 4.0f, 6.0f);
        CONJURE_REQUIRE_OK(fixture.world.add<scene::interpolated_transform>(body, pose));
        fixture.frame.interpolationAlpha = 0.5f;
        fixture.step(2);
        CONJURE_CHECK_APPROX(translation_of(fixture.render_matrix(body)), math::vec3(1.0f, 2.0f, 3.0f));
        CONJURE_CHECK_APPROX(translation_of(fixture.world_matrix(body)), math::vec3(100.0f, 0.0f, 0.0f));
        const scene::transform_state *state = fixture.world.get<scene::transform_state>(body);
        REQUIRE(state != nullptr);
        CHECK(has_flag(state->flags, scene::transform_state_flags::INTERPOLATED_SUBTREE));
        // Every frame re-lerps even when nothing else changed.
        fixture.frame.interpolationAlpha = 0.25f;
        fixture.step_transform();
        CONJURE_CHECK_APPROX(translation_of(fixture.render_matrix(body)), math::vec3(0.5f, 1.0f, 1.5f));
        CHECK(fixture.manager.transforms().last_stats().renderRecomputed == 1);
        CHECK(fixture.manager.transforms().last_stats().worldRecomputed == 0);
    }

    TEST_CASE("propagation - child of interpolated parent follows render parent") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity body = fixture.create("body");
        const ecs::entity child = fixture.create("child", body);
        const ecs::entity grand = fixture.create("grand", child);
        CONJURE_REQUIRE_OK(target.set_local_transform(body, at(100.0f, 0.0f, 0.0f)));
        CONJURE_REQUIRE_OK(target.set_local_transform(child, at(0.0f, 1.0f, 0.0f)));
        CONJURE_REQUIRE_OK(target.set_local_transform(grand, at(0.0f, 0.0f, 1.0f)));
        scene::interpolated_transform pose = {};
        pose.previous.position = math::vec3(0.0f);
        pose.current.position = math::vec3(10.0f, 0.0f, 0.0f);
        CONJURE_REQUIRE_OK(fixture.world.add<scene::interpolated_transform>(body, pose));
        fixture.frame.interpolationAlpha = 0.5f;
        fixture.step(3);
        CONJURE_CHECK_APPROX(translation_of(fixture.render_matrix(child)), math::vec3(5.0f, 1.0f, 0.0f));
        CONJURE_CHECK_APPROX(translation_of(fixture.render_matrix(grand)), math::vec3(5.0f, 1.0f, 1.0f));
        CONJURE_CHECK_APPROX(translation_of(fixture.world_matrix(grand)), math::vec3(100.0f, 1.0f, 1.0f));
        const scene::transform_state *state = fixture.world.get<scene::transform_state>(grand);
        REQUIRE(state != nullptr);
        CHECK(has_flag(state->flags, scene::transform_state_flags::INTERPOLATED_SUBTREE));
        // Descendants follow every frame while the world matrices stay put.
        fixture.frame.interpolationAlpha = 1.0f;
        fixture.step_transform();
        CONJURE_CHECK_APPROX(translation_of(fixture.render_matrix(grand)), math::vec3(10.0f, 1.0f, 1.0f));
        CHECK(fixture.manager.transforms().last_stats().renderRecomputed == 3);
        CHECK(fixture.manager.transforms().last_stats().worldRecomputed == 0);
    }

    TEST_CASE("propagation - root without pose render equals world") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity root = fixture.create("root");
        const ecs::entity child = fixture.create("child", root);
        scene::local_transform local = at(3.0f, 2.0f, 1.0f);
        local.rotation = glm::angleAxis(0.4f, math::vec3(0.0f, 1.0f, 0.0f));
        CONJURE_REQUIRE_OK(target.set_local_transform(root, local));
        CONJURE_REQUIRE_OK(target.set_local_transform(child, at(1.0f, 0.0f, 0.0f)));
        fixture.step();
        CHECK(fixture.render_matrix(root) == fixture.world_matrix(root));
        CHECK(fixture.render_matrix(child) == fixture.world_matrix(child));
        const scene::transform_state *state = fixture.world.get<scene::transform_state>(child);
        REQUIRE(state != nullptr);
        CHECK(state->flags == scene::transform_state_flags::NONE);
    }

    TEST_CASE("propagation - pose removed restores render equals world") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        const ecs::entity body = fixture.create("body");
        const ecs::entity child = fixture.create("child", body);
        CONJURE_REQUIRE_OK(target.set_local_transform(body, at(100.0f, 0.0f, 0.0f)));
        CONJURE_REQUIRE_OK(target.set_local_transform(child, at(0.0f, 1.0f, 0.0f)));
        scene::interpolated_transform pose = {};
        pose.current.position = math::vec3(10.0f, 0.0f, 0.0f);
        pose.previous = pose.current;
        CONJURE_REQUIRE_OK(fixture.world.add<scene::interpolated_transform>(body, pose));
        fixture.step(2);
        CONJURE_CHECK_APPROX(translation_of(fixture.render_matrix(child)), math::vec3(10.0f, 1.0f, 0.0f));

        CONJURE_REQUIRE_OK(fixture.world.remove<scene::interpolated_transform>(body));
        fixture.step_transform();
        CHECK(fixture.render_matrix(body) == fixture.world_matrix(body));
        CHECK(fixture.render_matrix(child) == fixture.world_matrix(child));
        CONJURE_CHECK_APPROX(translation_of(fixture.render_matrix(child)), math::vec3(100.0f, 1.0f, 0.0f));
        const scene::transform_state *state = fixture.world.get<scene::transform_state>(child);
        REQUIRE(state != nullptr);
        CHECK(state->flags == scene::transform_state_flags::NONE);
        fixture.step_transform();
        fixture.step_transform();
        CHECK(fixture.manager.transforms().last_stats().renderRecomputed == 0);
    }

    TEST_CASE("propagation - reparent same depth sets dirty and recomputes") {
        scene_fixture fixture = {};
        scene::scene &target = fixture.active();
        scene::transform_system &system = fixture.manager.transforms();
        const ecs::entity left = fixture.create("left");
        const ecs::entity right = fixture.create("right");
        const ecs::entity child = fixture.create("child", left);
        const ecs::entity leaf = fixture.create("leaf", child);
        CONJURE_REQUIRE_OK(target.set_local_transform(left, at(-10.0f, 0.0f, 0.0f)));
        CONJURE_REQUIRE_OK(target.set_local_transform(right, at(10.0f, 0.0f, 0.0f)));
        CONJURE_REQUIRE_OK(target.set_local_transform(child, at(0.0f, 1.0f, 0.0f)));
        CONJURE_REQUIRE_OK(target.set_local_transform(leaf, at(0.0f, 0.0f, 1.0f)));
        fixture.step(3);
        CHECK(system.last_stats().worldRecomputed == 0);

        // Same depth, no local change (keepWorld false): only the dirty tag marks the move.
        CONJURE_REQUIRE_OK(target.set_parent(child, right, false));
        CHECK(fixture.world.has<scene::transform_dirty>(child));
        fixture.step_transform();
        CHECK_FALSE(fixture.world.has<scene::transform_dirty>(child));
        CHECK(system.last_stats().worldRecomputed >= 2);
        CONJURE_CHECK_APPROX(translation_of(fixture.world_matrix(child)), math::vec3(10.0f, 1.0f, 0.0f));
        CONJURE_CHECK_APPROX(translation_of(fixture.world_matrix(leaf)), math::vec3(10.0f, 1.0f, 1.0f));
        fixture.step_transform();
        fixture.step_transform();
        CHECK(system.last_stats().worldRecomputed == 0);
    }

    TEST_CASE("propagation - inline and parallel give identical matrices [parallel]") {
        std::vector<math::mat4> inlineMatrices = {};
        std::vector<math::mat4> parallelMatrices = {};
        for (const u32 workers : {0u, 4u}) {
            CAPTURE(workers);
            core::jobs::job_system jobs = {};
            core::jobs::job_system_desc desc = {};
            desc.workerCount = workers;
            REQUIRE(jobs.start(desc).has_value());
            {
                scene_fixture fixture(&jobs);
                std::mt19937_64 engine(testing::environment().seed);
                const two_trees trees = build_two_trees(fixture, engine);
                fixture.step(2);
                CONJURE_REQUIRE_OK(fixture.active().set_local_transform(trees.rootA, at(1.0f, 1.0f, 1.0f)));
                fixture.step();
                std::vector<math::mat4> &out = workers == 0 ? inlineMatrices : parallelMatrices;
                for (const ecs::entity entity : trees.all) {
                    out.push_back(fixture.world_matrix(entity));
                }
            }
            jobs.stop();
        }
        REQUIRE(inlineMatrices.size() == parallelMatrices.size());
        for (usize index = 0; index < inlineMatrices.size(); ++index) {
            if (inlineMatrices[index] != parallelMatrices[index]) {
                CAPTURE(index);
                CHECK(inlineMatrices[index] == parallelMatrices[index]);
                break;
            }
        }
    }
}
