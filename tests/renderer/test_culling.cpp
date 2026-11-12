#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/test_environment.h"

#include "renderer/culling.h"

#include "conjure/renderer/camera.h"
#include "conjure/renderer/render_scene.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/jobs/parallel_for.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <span>
#include <vector>

// Frustum culling (docs/design/02_renderer.md 3.2, 10 "test_culling"): the plane-vs-AABB test,
// layer masks, the caster filter, the five-plane cascade mode and the job-parallel path that must
// equal the serial one.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

namespace {

    [[nodiscard]] math::frustum test_frustum() {
        camera value = {};
        value.nearPlane = 1.0f;
        value.farPlane = 100.0f;
        return frustum_from_clip(clip_from_view(value, 1.0f));
    }

    [[nodiscard]] math::aabb box_at(const math::vec3 &centre, f32 halfSize) {
        math::aabb box = {};
        box.min = centre - math::vec3(halfSize);
        box.max = centre + math::vec3(halfSize);
        return box;
    }

    [[nodiscard]] mesh_instance instance_with(const math::aabb &bounds, u32 layerMask = 1, bool castsShadows = true) {
        mesh_instance instance = {};
        instance.worldBounds = bounds;
        instance.layerMask = layerMask;
        instance.castsShadows = castsShadows;
        return instance;
    }
}

TEST_SUITE("renderer/culling") {

    TEST_CASE("box_intersects - boxes inside, straddling and outside the frustum classify as expected") {
        const math::frustum frustum = test_frustum();
        CHECK(box_intersects(box_at(math::vec3(0.0f, 0.0f, -10.0f), 1.0f), frustum, false));
        // straddles the near plane
        CHECK(box_intersects(box_at(math::vec3(0.0f, 0.0f, -1.0f), 0.5f), frustum, false));
        // straddles the far plane
        CHECK(box_intersects(box_at(math::vec3(0.0f, 0.0f, -100.0f), 1.0f), frustum, false));
        CHECK(box_intersects(box_at(math::vec3(30.0f, 0.0f, -50.0f), 1.0f), frustum, false));   // inside the side
        CHECK_FALSE(box_intersects(box_at(math::vec3(0.0f, 0.0f, 10.0f), 1.0f), frustum, false));   // behind
        CHECK_FALSE(box_intersects(box_at(math::vec3(0.0f, 0.0f, -200.0f), 1.0f), frustum, false)); // beyond far
        CHECK_FALSE(box_intersects(box_at(math::vec3(500.0f, 0.0f, -50.0f), 1.0f), frustum, false));
        CHECK_FALSE(box_intersects(box_at(math::vec3(0.0f, 500.0f, -50.0f), 1.0f), frustum, false));
        // A huge box that encloses the whole frustum intersects.
        CHECK(box_intersects(box_at(math::vec3(0.0f), 1000.0f), frustum, false));
        // An invalid box never intersects.
        CHECK_FALSE(box_intersects(math::aabb(), frustum, false));
    }

    TEST_CASE("box_intersects - ignoring the far plane keeps casters beyond it") {
        const math::frustum frustum = test_frustum();
        const math::aabb beyond = box_at(math::vec3(0.0f, 0.0f, -200.0f), 1.0f);
        CHECK_FALSE(box_intersects(beyond, frustum, false));
        CHECK(box_intersects(beyond, frustum, true));
        const math::aabb behind = box_at(math::vec3(0.0f, 0.0f, 10.0f), 1.0f);
        CHECK_FALSE(box_intersects(behind, frustum, true));   // still rejected by the near plane
        const math::aabb beside = box_at(math::vec3(500.0f, 0.0f, -200.0f), 1.0f);
        CHECK_FALSE(box_intersects(beside, frustum, true));
    }

    TEST_CASE("instance_visible - the layer mask and the caster filter are tested before the planes") {
        const math::frustum frustum = test_frustum();
        const mesh_instance visible = instance_with(box_at(math::vec3(0.0f, 0.0f, -10.0f), 1.0f), 0b0101, false);
        cull_options options = {};
        options.layerMask = 0b0100;
        CHECK(instance_visible(visible, frustum, options));
        options.layerMask = 0b1010;
        CHECK_FALSE(instance_visible(visible, frustum, options));
        options.layerMask = 0xFFFFFFFFu;
        options.shadowCastersOnly = true;
        CHECK_FALSE(instance_visible(visible, frustum, options));
        options.shadowCastersOnly = false;
        CHECK(instance_visible(visible, frustum, options));
        const mesh_instance hidden = instance_with(box_at(math::vec3(0.0f, 0.0f, 10.0f), 1.0f));
        CHECK_FALSE(instance_visible(hidden, frustum, options));
        options.ignoreFarPlane = true;
        const mesh_instance far = instance_with(box_at(math::vec3(0.0f, 0.0f, -300.0f), 1.0f));
        CHECK(instance_visible(far, frustum, options));
    }

    TEST_CASE("cull_instances - returns the visible indices in input order and clears the output first") {
        const math::frustum frustum = test_frustum();
        std::vector<mesh_instance> instances = {};
        instances.push_back(instance_with(box_at(math::vec3(0.0f, 0.0f, -10.0f), 1.0f)));
        instances.push_back(instance_with(box_at(math::vec3(0.0f, 0.0f, 10.0f), 1.0f)));
        instances.push_back(instance_with(box_at(math::vec3(2.0f, 0.0f, -20.0f), 1.0f)));
        instances.push_back(instance_with(box_at(math::vec3(0.0f, 0.0f, -30.0f), 1.0f), 2));
        std::vector<u32> out = {7, 7, 7};
        cull_instances(instances, frustum, cull_options(), nullptr, out);
        CHECK(out == std::vector<u32>{0, 2, 3});
        cull_options layerTwo = {};
        layerTwo.layerMask = 2;
        cull_instances(instances, frustum, layerTwo, nullptr, out);
        CHECK(out == std::vector<u32>{3});
        cull_instances({}, frustum, cull_options(), nullptr, out);
        CHECK(out.empty());
    }

    TEST_CASE("cull_instances - an instance with invalid world bounds is never visible") {
        // The renderer builds its draw lists from these indices only and relies on every visible
        // instance having valid bounds (the depth sort uses their centre).
        const math::frustum frustum = test_frustum();
        std::vector<mesh_instance> instances = {};
        instances.push_back(instance_with(math::aabb()));
        instances.push_back(instance_with(box_at(math::vec3(0.0f, 0.0f, -10.0f), 1.0f)));
        instances.push_back(instance_with(math::aabb(), 0xFFFFFFFFu, true));
        std::vector<u32> out = {};
        cull_instances(instances, frustum, cull_options(), nullptr, out);
        CHECK(out == std::vector<u32>{1});
        cull_options casters = {};
        casters.shadowCastersOnly = true;
        casters.ignoreFarPlane = true;
        cull_instances(instances, frustum, casters, nullptr, out);
        CHECK(out == std::vector<u32>{1});
        CHECK_FALSE(instance_visible(instances[0], frustum, casters));
        CHECK_FALSE(instance_visible(instances[2], frustum, cull_options()));
    }

    TEST_CASE("cull_instances - the job-parallel path above the threshold equals the serial result") {
        const math::frustum frustum = test_frustum();
        const u32 count = CULLING_PARALLEL_THRESHOLD * 3 + 17;
        std::vector<mesh_instance> instances = {};
        instances.reserve(count);
        for (u32 index = 0; index < count; ++index) {
            const f32 x = static_cast<f32>(index % 97) - 48.0f;
            const f32 z = -static_cast<f32>(index % 131) - 0.5f;
            mesh_instance instance = instance_with(
              box_at(math::vec3(x, 0.0f, z), 0.5f),
              (index % 5 == 0) ? 2u : 1u,
              index % 7 != 0
            );
            instances.push_back(instance);
        }
        std::vector<u32> serial = {};
        cull_options options = {};
        options.layerMask = 1;
        options.shadowCastersOnly = true;
        cull_instances(instances, frustum, options, nullptr, serial);
        CHECK_FALSE(serial.empty());
        CHECK(serial.size() < count);

        core::jobs::job_system jobs = {};
        core::jobs::job_system_desc desc = {};
        desc.workerCount = environment().jobThreadCount;
        CONJURE_REQUIRE_OK(jobs.start(desc));
        std::vector<u32> parallel = {1, 2, 3};
        cull_instances(instances, frustum, options, &jobs, parallel);
        CHECK(parallel == serial);
        // Below the threshold the job system is not used; the result is the same.
        std::vector<u32> small = {};
        cull_instances(std::span<const mesh_instance>(instances).subspan(0, 100), frustum, options, &jobs, small);
        std::vector<u32> smallSerial = {};
        cull_instances(
          std::span<const mesh_instance>(instances).subspan(0, 100),
          frustum,
          options,
          nullptr,
          smallSerial
        );
        CHECK(small == smallSerial);
        jobs.stop();
        // A job system without workers runs inline and still matches.
        core::jobs::job_system inlineJobs = {};
        core::jobs::job_system_desc inlineDesc = {};
        inlineDesc.workerCount = 0;
        CONJURE_REQUIRE_OK(inlineJobs.start(inlineDesc));
        std::vector<u32> inlineResult = {};
        cull_instances(instances, frustum, options, &inlineJobs, inlineResult);
        CHECK(inlineResult == serial);
        inlineJobs.stop();
    }

    TEST_CASE("cull_instances - a warm cull_scratch makes the parallel path allocation-free [parallel]") {
        // The renderer culls every view and shadow slice through one persistent scratch
        // (renderer.extract_10k, allocs 0): the first cull sizes it for the instance count, the
        // following ones write into the same slices and the result still equals the serial cull.
        const math::frustum frustum = test_frustum();
        const u32 count = CULLING_PARALLEL_THRESHOLD * 4 + 5;
        std::vector<mesh_instance> instances = {};
        instances.reserve(count);
        for (u32 index = 0; index < count; ++index) {
            const f32 x = static_cast<f32>(index % 61) - 30.0f;
            const f32 z = -static_cast<f32>(index % 113) - 0.5f;
            instances.push_back(instance_with(box_at(math::vec3(x, 0.0f, z), 0.5f)));
        }
        std::vector<u32> serial = {};
        cull_instances(instances, frustum, cull_options(), nullptr, serial);
        CHECK_FALSE(serial.empty());

        core::jobs::job_system jobs = {};
        core::jobs::job_system_desc desc = {};
        desc.workerCount = environment().jobThreadCount;
        CONJURE_REQUIRE_OK(jobs.start(desc));
        cull_scratch scratch = {};
        std::vector<u32> parallel = {};
        cull_instances(instances, frustum, cull_options(), &jobs, scratch, parallel);
        CHECK(parallel == serial);
        CHECK(scratch.chunkVisible.size() >= count);
        CHECK(scratch.chunkCounts.size() == core::jobs::range_count(count, CULLING_PARALLEL_GRAIN));
        const usize scratchSize = scratch.chunkVisible.size();

        cull_instances(instances, frustum, cull_options(), &jobs, scratch, parallel);   // the job pool warms up
        CONJURE_CHECK_NO_ALLOCATIONS(cull_instances(instances, frustum, cull_options(), &jobs, scratch, parallel));
        CHECK(parallel == serial);
        CHECK(scratch.chunkVisible.size() == scratchSize);

        // A smaller instance count reuses the scratch; a larger one grows it once.
        const std::span<const mesh_instance> fewer = std::span<const mesh_instance>(instances).subspan(0, count / 2);
        std::vector<u32> fewerSerial = {};
        cull_instances(fewer, frustum, cull_options(), nullptr, fewerSerial);
        CONJURE_CHECK_NO_ALLOCATIONS(cull_instances(fewer, frustum, cull_options(), &jobs, scratch, parallel));
        CHECK(parallel == fewerSerial);
        CHECK(scratch.chunkVisible.size() == scratchSize);
        jobs.stop();
    }

    TEST_CASE("regression - the overload without a scratch allocates nothing on the serial path") {
        // The convenience overload used to build its cull_scratch before deciding the path; under the
        // MSVC Debug runtime two empty std::vectors cost two heap blocks (iterator-debugging proxies),
        // so every serial cull through it failed a Debug allocation gate. The serial path (no job
        // system, no workers, or a count at or below the threshold) now touches nothing but 'out'.
        const math::frustum frustum = test_frustum();
        const u32 count = CULLING_PARALLEL_THRESHOLD + 1;
        std::vector<mesh_instance> instances = {};
        instances.reserve(count);
        for (u32 index = 0; index < count; ++index) {
            const f32 x = static_cast<f32>(index % 61) - 30.0f;
            const f32 z = -static_cast<f32>(index % 113) - 0.5f;
            instances.push_back(instance_with(box_at(math::vec3(x, 0.0f, z), 0.5f)));
        }
        std::vector<u32> expected = {};
        cull_instances(instances, frustum, cull_options(), nullptr, expected);
        CHECK_FALSE(expected.empty());
        const std::span<const mesh_instance> atThreshold =
          std::span<const mesh_instance>(instances).subspan(0, CULLING_PARALLEL_THRESHOLD);
        std::vector<u32> expectedAtThreshold = {};
        cull_instances(atThreshold, frustum, cull_options(), nullptr, expectedAtThreshold);
        std::vector<u32> out = {};
        out.reserve(count);

        // No job system.
        CONJURE_CHECK_NO_ALLOCATIONS(cull_instances(instances, frustum, cull_options(), nullptr, out));
        CHECK(out == expected);

        // A job system without workers runs inline: the serial path above the threshold.
        core::jobs::job_system inlineJobs = {};
        core::jobs::job_system_desc inlineDesc = {};
        inlineDesc.workerCount = 0;
        CONJURE_REQUIRE_OK(inlineJobs.start(inlineDesc));
        CONJURE_CHECK_NO_ALLOCATIONS(cull_instances(instances, frustum, cull_options(), &inlineJobs, out));
        CHECK(out == expected);
        inlineJobs.stop();

        // Workers, but a count that does not exceed the threshold.
        core::jobs::job_system jobs = {};
        core::jobs::job_system_desc desc = {};
        desc.workerCount = environment().jobThreadCount;
        CONJURE_REQUIRE_OK(jobs.start(desc));
        cull_instances(instances, frustum, cull_options(), &jobs, out);   // the job pool warms up
        CHECK(out == expected);
        CONJURE_CHECK_NO_ALLOCATIONS(cull_instances(atThreshold, frustum, cull_options(), &jobs, out));
        CHECK(out == expectedAtThreshold);
        jobs.stop();
    }
}
