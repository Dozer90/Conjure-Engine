#pragma once

#include "conjure/physics/physics_layers.h"

#include "conjure/core/api.h"
#include "conjure/core/json/value.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

// World construction parameters (docs/design/14_physics.md 2.1). Everything a physics_world sizes at
// construction (P15) lives here; the engine's physics_subsystem fills it with from_json() from the raw
// project.json "physics" section (snake_case keys) and "extensions.physics_layers" (R53).

namespace conjure::physics {

    struct CONJURE_API physics_settings {
        math::vec3 gravity = math::vec3(0.0f, -9.81f, 0.0f);
        u32 maxBodies = 65536;                  // JPH::PhysicsSystem::Init inMaxBodies; also sizes the record table
        u32 numBodyMutexes = 0;                 // 0 = Jolt default
        u32 maxBodyPairs = 65536;               // 4 .. 1 << 24; a step past it reports BodyPairCacheFull
        u32 maxContactConstraints = 16384;      // 4 .. 1 << 24; a step past it reports ContactConstraintsFull
        u32 collisionSteps = 1;                 // PhysicsSystem::Update inCollisionSteps
        usize tempAllocatorBytes = 32 * MIB;    // JPH::TempAllocatorImpl
        // jolt_job_system pool: a power of two of at least 64 (the pool pages by it) that covers the jobs
        // one PhysicsSystem::Update creates, roughly ten per worker thread plus a dozen; Jolt's own
        // cMaxPhysicsJobs is 2048. An exhausted pool is fatal (CONJURE_FATAL_ASSERT in CreateJob).
        u32 maxJobs = 2048;
        u32 maxBarriers = 8;                    // JPH::JobSystemWithBarrier::Init
        u32 maxContactEventsPerStep = 16384;    // merged events; every per-thread raw buffer holds the same count
        u32 maxQueuedCommands = 8192;           // P9 command queue
        u32 maxCharacters = 256;                // at least 1
        bool deterministic = true;              // PhysicsSettings::mDeterministicSimulation
        bool allowSleeping = true;
        f32 timeBeforeSleepSeconds = 0.5f;
        f32 pointVelocitySleepThreshold = 0.03f;
        u32 numVelocitySteps = 10;
        u32 numPositionSteps = 2;
        f32 speculativeContactDistance = 0.02f;
        // PhysicsSettings::mPenetrationSlop; Jolt's 0.02 leaves bodies resting 1.5 cm deep
        f32 penetrationSlop = 0.005f;
        bool estimateContactImpulses = true;    // JPH::EstimateCollisionResponse on ENTER events
        physics_layer_table layers = {};        // one layer "default" unless project.json says otherwise

        /// <summary>
        /// Rejects maxBodies == 0 or above 1 &lt;&lt; 23, a maxBodyPairs or maxContactConstraints below 4
        /// or above 1 &lt;&lt; 24, collisionSteps == 0, tempAllocatorBytes below 1 MiB, a maxJobs below 64
        /// or not a power of two, maxBarriers == 0, maxContactEventsPerStep == 0, maxQueuedCommands == 0,
        /// maxCharacters == 0, numVelocitySteps == 0, numPositionSteps == 0, a negative
        /// timeBeforeSleepSeconds, pointVelocitySleepThreshold, speculativeContactDistance or
        /// penetrationSlop, a non-finite gravity, and whatever layers.validate() rejects. The
        /// INVALID_SETTINGS message names the field.
        /// </summary>
        [[nodiscard]] core::result<void> validate() const;

        /// <summary>
        /// Reads the project.json "physics" object (snake_case keys matching the fields above; "enabled"
        /// is the engine's and is ignored; unknown keys log one WARN each) and the
        /// "extensions.physics_layers" object (an invalid or missing value keeps the default table).
        /// Fails with INVALID_SETTINGS for a value of the wrong type and with the layer errors of
        /// physics_layer_table::from_json. The result is not validated; call validate().
        /// </summary>
        [[nodiscard]] static core::result<physics_settings> from_json(
          const core::json::value &physicsSection,
          const core::json::value &layersSection
        );
    };
}
