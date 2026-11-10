#include "conjure/physics/physics_settings.h"

#include "conjure/physics/log_category.h"
#include "conjure/physics/physics_error.h"

#include "conjure/core/log.h"
#include "conjure/core/reflect/json_conversion.h"

#include <bit>
#include <cmath>
#include <string_view>

namespace conjure::physics {

namespace {

    /// <summary>
    /// Largest body count Jolt addresses (BodyID::cMaxBodyIndex + 1).
    /// </summary>
    constexpr u32 MAX_BODIES_LIMIT = u32{1} << 23;

    constexpr usize MIN_TEMP_ALLOCATOR_BYTES = 1 * MIB;
    constexpr u32 MIN_JOBS = 64;

    /// <summary>
    /// Jolt rounds the body pair and manifold hash maps up to a power of two but asserts below four
    /// buckets; the cap keeps the cache allocation (hundreds of bytes per entry) far below its own
    /// overflow limits.
    /// </summary>
    constexpr u32 MIN_CACHE_ENTRIES = 4;
    constexpr u32 MAX_CACHE_ENTRIES = u32{1} << 24;

    [[nodiscard]] core::error setting_error(std::string_view field, std::string_view why) {
        return core::make_error(physics_error::INVALID_SETTINGS, "{}: {}", field, why);
    }

    [[nodiscard]] core::error type_error(std::string_view key, std::string_view expected) {
        return core::make_error(physics_error::INVALID_SETTINGS, "physics.{} must be {}", key, expected);
    }

    [[nodiscard]] core::result<void> read_u32(const core::json::value &item, std::string_view key, u32 &target) {
        const core::result<u32> parsed = item.to<u32>();
        if (!parsed.has_value()) {
            return type_error(key, "an unsigned integer");
        }
        target = parsed.value();
        return core::result<void>::success();
    }

    [[nodiscard]] core::result<void> read_usize(const core::json::value &item, std::string_view key, usize &target) {
        const core::result<u64> parsed = item.to<u64>();
        if (!parsed.has_value()) {
            return type_error(key, "an unsigned integer");
        }
        target = static_cast<usize>(parsed.value());
        return core::result<void>::success();
    }

    [[nodiscard]] core::result<void> read_f32(const core::json::value &item, std::string_view key, f32 &target) {
        const core::result<f32> parsed = item.to<f32>();
        if (!parsed.has_value()) {
            return type_error(key, "a number");
        }
        target = parsed.value();
        return core::result<void>::success();
    }

    [[nodiscard]] core::result<void> read_bool(const core::json::value &item, std::string_view key, bool &target) {
        if (!item.is_bool()) {
            return type_error(key, "a boolean");
        }
        target = item.as_bool();
        return core::result<void>::success();
    }
}

core::result<void> physics_settings::validate() const {
    if (maxBodies == 0 || maxBodies > MAX_BODIES_LIMIT) {
        return setting_error("maxBodies", "must be between 1 and 8388608");
    }
    if (maxBodyPairs < MIN_CACHE_ENTRIES || maxBodyPairs > MAX_CACHE_ENTRIES) {
        return setting_error("maxBodyPairs", "must be between 4 and 16777216");
    }
    if (maxContactConstraints < MIN_CACHE_ENTRIES || maxContactConstraints > MAX_CACHE_ENTRIES) {
        return setting_error("maxContactConstraints", "must be between 4 and 16777216");
    }
    if (collisionSteps == 0) {
        return setting_error("collisionSteps", "must be at least 1");
    }
    if (tempAllocatorBytes < MIN_TEMP_ALLOCATOR_BYTES) {
        return setting_error("tempAllocatorBytes", "must be at least 1 MiB");
    }
    // The job pool is a JPH::FixedSizeFreeList paged by maxJobs, which requires a power of two.
    if (maxJobs < MIN_JOBS || !std::has_single_bit(maxJobs)) {
        return setting_error("maxJobs", "must be a power of two of at least 64");
    }
    if (maxBarriers == 0) {
        return setting_error("maxBarriers", "must be at least 1");
    }
    if (maxContactEventsPerStep == 0) {
        return setting_error("maxContactEventsPerStep", "must be at least 1");
    }
    if (maxQueuedCommands == 0) {
        return setting_error("maxQueuedCommands", "must be at least 1");
    }
    if (maxCharacters == 0) {
        return setting_error("maxCharacters", "must be at least 1");
    }
    if (numVelocitySteps == 0) {
        return setting_error("numVelocitySteps", "must be at least 1");
    }
    if (numPositionSteps == 0) {
        return setting_error("numPositionSteps", "must be at least 1");
    }
    if (!(timeBeforeSleepSeconds >= 0.0f)) {
        return setting_error("timeBeforeSleepSeconds", "must not be negative");
    }
    if (!(pointVelocitySleepThreshold >= 0.0f)) {
        return setting_error("pointVelocitySleepThreshold", "must not be negative");
    }
    if (!(speculativeContactDistance >= 0.0f)) {
        return setting_error("speculativeContactDistance", "must not be negative");
    }
    if (!(penetrationSlop >= 0.0f)) {
        return setting_error("penetrationSlop", "must not be negative");
    }
    if (!std::isfinite(gravity.x) || !std::isfinite(gravity.y) || !std::isfinite(gravity.z)) {
        return setting_error("gravity", "must be finite");
    }
    return layers.validate();
}

core::result<physics_settings> physics_settings::from_json(
  const core::json::value &physicsSection,
  const core::json::value &layersSection
) {
    physics_settings settings = {};
    if (physicsSection.is_valid() && !physicsSection.is_object()) {
        return core::make_error(physics_error::INVALID_SETTINGS, "physics must be an object");
    }

    for (const core::json::member entry : physicsSection.members()) {
        const std::string_view key = entry.key;
        const core::json::value &item = entry.item;
        if (key == "enabled") {
            // The engine's own switch (06_engine_loop.md 2.3); it never reaches the world.
            continue;
        }
        if (key == "gravity") {
            const core::result<math::vec3> parsed = item.to<math::vec3>();
            if (!parsed.has_value()) {
                return type_error(key, "an array of three numbers");
            }
            settings.gravity = parsed.value();
        } else if (key == "max_bodies") {
            CONJURE_TRY(read_u32(item, key, settings.maxBodies));
        } else if (key == "num_body_mutexes") {
            CONJURE_TRY(read_u32(item, key, settings.numBodyMutexes));
        } else if (key == "max_body_pairs") {
            CONJURE_TRY(read_u32(item, key, settings.maxBodyPairs));
        } else if (key == "max_contact_constraints") {
            CONJURE_TRY(read_u32(item, key, settings.maxContactConstraints));
        } else if (key == "collision_steps") {
            CONJURE_TRY(read_u32(item, key, settings.collisionSteps));
        } else if (key == "temp_allocator_bytes") {
            CONJURE_TRY(read_usize(item, key, settings.tempAllocatorBytes));
        } else if (key == "max_jobs") {
            CONJURE_TRY(read_u32(item, key, settings.maxJobs));
        } else if (key == "max_barriers") {
            CONJURE_TRY(read_u32(item, key, settings.maxBarriers));
        } else if (key == "max_contact_events_per_step") {
            CONJURE_TRY(read_u32(item, key, settings.maxContactEventsPerStep));
        } else if (key == "max_queued_commands") {
            CONJURE_TRY(read_u32(item, key, settings.maxQueuedCommands));
        } else if (key == "max_characters") {
            CONJURE_TRY(read_u32(item, key, settings.maxCharacters));
        } else if (key == "deterministic") {
            CONJURE_TRY(read_bool(item, key, settings.deterministic));
        } else if (key == "allow_sleeping") {
            CONJURE_TRY(read_bool(item, key, settings.allowSleeping));
        } else if (key == "time_before_sleep_seconds") {
            CONJURE_TRY(read_f32(item, key, settings.timeBeforeSleepSeconds));
        } else if (key == "point_velocity_sleep_threshold") {
            CONJURE_TRY(read_f32(item, key, settings.pointVelocitySleepThreshold));
        } else if (key == "num_velocity_steps") {
            CONJURE_TRY(read_u32(item, key, settings.numVelocitySteps));
        } else if (key == "num_position_steps") {
            CONJURE_TRY(read_u32(item, key, settings.numPositionSteps));
        } else if (key == "speculative_contact_distance") {
            CONJURE_TRY(read_f32(item, key, settings.speculativeContactDistance));
        } else if (key == "penetration_slop") {
            CONJURE_TRY(read_f32(item, key, settings.penetrationSlop));
        } else if (key == "estimate_contact_impulses") {
            CONJURE_TRY(read_bool(item, key, settings.estimateContactImpulses));
        } else {
            CONJURE_LOG_WARN(LOG_PHYSICS, "project.json physics: unknown key '{}' ignored", key);
        }
    }

    if (layersSection.is_valid() && !layersSection.is_null()) {
        CONJURE_TRY_ASSIGN(settings.layers, physics_layer_table::from_json(layersSection));
    }
    return settings;
}
}
