#pragma once

// The determinism scenario shared by tests/physics/test_determinism.cpp and the [slow] parallel case of
// tests/physics/slow/test_determinism_parallel.cpp: 200 boxes dropped onto the ground for 300 steps, every
// step recorded as the bit pattern of every pose plus the step's contact events.

#include "conjure/testing/doctest.h"
#include "physics_test_fixture.h"

#include "conjure/physics/contact_events.h"

#include <cstring>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace physics_test {

    /// <summary>
    /// The bit pattern of every body pose plus the step's events, recorded per step.
    /// </summary>
    struct step_record {
        std::vector<conjure::math::transform> poses = {};
        std::vector<conjure::physics::contact_event> events = {};

        [[nodiscard]] bool operator==(const step_record &other) const noexcept {
            if (poses.size() != other.poses.size() || events.size() != other.events.size()) {
                return false;
            }
            for (conjure::usize index = 0; index < poses.size(); ++index) {
                if (std::memcmp(&poses[index], &other.poses[index], sizeof(conjure::math::transform)) != 0) {
                    return false;
                }
            }
            for (conjure::usize index = 0; index < events.size(); ++index) {
                if (std::memcmp(&events[index], &other.events[index], sizeof(conjure::physics::contact_event)) != 0) {
                    return false;
                }
            }
            return true;
        }
    };

    /// <summary>
    /// Drops 200 boxes in a 20 x 10 grid for 300 steps and records every step.
    /// </summary>
    [[nodiscard]] inline std::vector<step_record> run_determinism_scenario(conjure::core::jobs::job_system &jobs) {
        conjure::physics::physics_settings settings = test_settings();
        settings.allowSleeping = false;
        physics_fixture fixture(jobs, settings);
        (void)fixture.spawn_ground();
        std::vector<conjure::ecs::entity> boxes = {};
        for (conjure::u32 index = 0; index < 200; ++index) {
            const conjure::f32 x = static_cast<conjure::f32>(index % 20) * 1.1f - 10.0f;
            const conjure::f32 y = 2.0f + static_cast<conjure::f32>(index / 20) * 1.1f;
            boxes.push_back(fixture.spawn_box(conjure::math::vec3(x, y, 0.0f)));
        }
        std::vector<step_record> records = {};
        records.reserve(300);
        for (conjure::u32 step = 0; step < 300; ++step) {
            fixture.run_step();
            step_record record = {};
            record.poses.reserve(boxes.size());
            for (const conjure::ecs::entity box : boxes) {
                const std::optional<conjure::math::transform> pose = fixture.physics.body_pose(box);
                REQUIRE(pose.has_value());
                record.poses.push_back(*pose);
            }
            const std::span<const conjure::physics::contact_event> events = fixture.physics.contact_events();
            record.events.assign(events.begin(), events.end());
            records.push_back(std::move(record));
        }
        REQUIRE(fixture.physics.body_count() == 201);
        return records;
    }
}
