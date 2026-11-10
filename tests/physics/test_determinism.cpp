#include "conjure/testing/doctest.h"
#include "physics_test_fixture.h"

#include "conjure/physics/contact_events.h"

#include <cstring>
#include <memory>
#include <vector>

using namespace conjure;
using namespace physics_test;

namespace {

    /// <summary>
    /// The bit pattern of every body pose plus the step's events, recorded per step.
    /// </summary>
    struct step_record {
        std::vector<math::transform> poses = {};
        std::vector<physics::contact_event> events = {};

        [[nodiscard]] bool operator==(const step_record &other) const noexcept {
            if (poses.size() != other.poses.size() || events.size() != other.events.size()) {
                return false;
            }
            for (usize index = 0; index < poses.size(); ++index) {
                if (std::memcmp(&poses[index], &other.poses[index], sizeof(math::transform)) != 0) {
                    return false;
                }
            }
            for (usize index = 0; index < events.size(); ++index) {
                if (std::memcmp(&events[index], &other.events[index], sizeof(physics::contact_event)) != 0) {
                    return false;
                }
            }
            return true;
        }
    };

    /// <summary>
    /// Drops 200 boxes in a 20 x 10 grid for 300 steps and records every step.
    /// </summary>
    [[nodiscard]] std::vector<step_record> run_scenario(core::jobs::job_system &jobs) {
        physics::physics_settings settings = test_settings();
        settings.allowSleeping = false;
        physics_fixture fixture(jobs, settings);
        (void)fixture.spawn_ground();
        std::vector<ecs::entity> boxes = {};
        for (u32 index = 0; index < 200; ++index) {
            const f32 x = static_cast<f32>(index % 20) * 1.1f - 10.0f;
            const f32 y = 2.0f + static_cast<f32>(index / 20) * 1.1f;
            boxes.push_back(fixture.spawn_box(math::vec3(x, y, 0.0f)));
        }
        std::vector<step_record> records = {};
        records.reserve(300);
        for (u32 step = 0; step < 300; ++step) {
            fixture.run_step();
            step_record record = {};
            record.poses.reserve(boxes.size());
            for (const ecs::entity box : boxes) {
                const std::optional<math::transform> pose = fixture.physics.body_pose(box);
                REQUIRE(pose.has_value());
                record.poses.push_back(*pose);
            }
            const std::span<const physics::contact_event> events = fixture.physics.contact_events();
            record.events.assign(events.begin(), events.end());
            records.push_back(std::move(record));
        }
        REQUIRE(fixture.physics.body_count() == 201);
        return records;
    }
}

TEST_SUITE("physics/determinism") {

    TEST_CASE("determinism - 200 boxes over 300 steps are bit-identical with 0 and 4 workers [parallel][slow]") {
        core::jobs::job_system inlineJobs = {};
        const std::vector<step_record> inlineRun = run_scenario(inlineJobs);

        core::jobs::job_system parallelJobs = {};
        core::jobs::job_system_desc desc = {};
        desc.workerCount = 4;
        REQUIRE(parallelJobs.start(desc).has_value());
        const std::vector<step_record> parallelRun = run_scenario(parallelJobs);
        parallelJobs.stop();

        REQUIRE(inlineRun.size() == parallelRun.size());
        u32 eventsSeen = 0;
        for (usize step = 0; step < inlineRun.size(); ++step) {
            CAPTURE(step);
            CHECK(inlineRun[step] == parallelRun[step]);
            eventsSeen += static_cast<u32>(inlineRun[step].events.size());
        }
        CHECK(eventsSeen > 0);
        // The boxes did land on something: the last step still has resting contacts.
        CHECK_FALSE(inlineRun.back().events.empty());
    }

    TEST_CASE("determinism - two inline runs are bit-identical") {
        core::jobs::job_system first = {};
        core::jobs::job_system second = {};
        const std::vector<step_record> runA = run_scenario(first);
        const std::vector<step_record> runB = run_scenario(second);
        REQUIRE(runA.size() == runB.size());
        for (usize step = 0; step < runA.size(); ++step) {
            CAPTURE(step);
            CHECK(runA[step] == runB[step]);
        }
    }
}
