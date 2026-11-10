#pragma once

#include "physics/jolt_include.h"

#include "conjure/core/math/glm_types.h"
#include "conjure/core/types.h"

#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/ContactListener.h>

#include <atomic>
#include <span>
#include <vector>

// Lock-free contact collection (docs/design/14_physics.md P11, 3.6). Jolt calls the listener from any
// job thread with the bodies locked; every call appends one raw record to the buffer of the calling
// thread's job_system::thread_index(). A global budget (maxContactEventsPerStep) bounds the total so the
// merge buffers are sized once. The activation listener writes the awake flag of a row.

namespace conjure::physics {

    enum class raw_contact_kind : u8 {
        ADDED,
        PERSISTED,
        REMOVED
    };

    /// <summary>
    /// One listener callback, as recorded on the job thread.
    /// </summary>
    struct raw_contact {
        u32 bodyA = 0;                                    // BodyID::GetIndexAndSequenceNumber(), A < B
        u32 bodyB = 0;
        u32 subShapeA = 0;
        u32 subShapeB = 0;
        math::vec3 point = math::vec3(0.0f);
        math::vec3 normal = math::vec3(0.0f);
        f32 impulse = 0.0f;
        raw_contact_kind kind = raw_contact_kind::ADDED;
        u8 pointCount = 0;
    };

    class contact_collector final : public JPH::ContactListener {
        private:
            std::vector<std::vector<raw_contact>> mBuffers = {};   // one per job thread index
            std::atomic<u32> mRecorded = 0;                         // records accepted this step
            std::atomic<u32> mDropped = 0;                          // records refused this step
            u32 mCapacity = 0;
            bool mEstimateImpulses = true;

        public:
            contact_collector();
            ~contact_collector() override;

        public:
            /// <summary>
            /// Sizes one buffer of 'capacity' records per thread index (0 = main, 1..workers).
            /// </summary>
            void init(u32 threadCount, u32 capacity, bool estimateImpulses);

            /// <summary>
            /// Grows the per-thread buffer set when the job system gained workers.
            /// </summary>
            void ensure_threads(u32 threadCount);

            [[nodiscard]] u32 thread_count() const noexcept {
                return static_cast<u32>(mBuffers.size());
            }

            /// <summary>
            /// The records of one thread.
            /// </summary>
            [[nodiscard]] std::span<const raw_contact> buffer(u32 threadIndex) const noexcept;

            /// <summary>
            /// Records accepted since the last clear.
            /// </summary>
            [[nodiscard]] u32 recorded_count() const noexcept {
                return mRecorded.load(std::memory_order_relaxed);
            }

            /// <summary>
            /// Records refused since the last clear.
            /// </summary>
            [[nodiscard]] u32 dropped_count() const noexcept {
                return mDropped.load(std::memory_order_relaxed);
            }

            /// <summary>
            /// Empties every buffer and the counters (main thread, outside a step).
            /// </summary>
            void clear() noexcept;

            // JPH::ContactListener
            [[nodiscard]] JPH::ValidateResult OnContactValidate(
              const JPH::Body &body1,
              const JPH::Body &body2,
              JPH::RVec3Arg baseOffset,
              const JPH::CollideShapeResult &collisionResult
            ) override;
            void OnContactAdded(
              const JPH::Body &body1,
              const JPH::Body &body2,
              const JPH::ContactManifold &manifold,
              JPH::ContactSettings &settings
            ) override;
            void OnContactPersisted(
              const JPH::Body &body1,
              const JPH::Body &body2,
              const JPH::ContactManifold &manifold,
              JPH::ContactSettings &settings
            ) override;
            void OnContactRemoved(const JPH::SubShapeIDPair &subShapePair) override;

        private:
            void record(const raw_contact &contact) noexcept;
    };

    /// <summary>
    /// Mirrors Jolt's activation callbacks into a per-row atomic flag array indexed by body index.
    /// </summary>
    class activation_collector final : public JPH::BodyActivationListener {
        private:
            std::atomic<u8> *mAwake = nullptr;
            u32 mCount = 0;

        public:
            activation_collector();
            ~activation_collector() override;

        public:
            /// <summary>
            /// Binds the flag array (one entry per body index).
            /// </summary>
            void bind(std::atomic<u8> *awake, u32 count) noexcept;

            void OnBodyActivated(const JPH::BodyID &bodyId, JPH::uint64 userData) override;
            void OnBodyDeactivated(const JPH::BodyID &bodyId, JPH::uint64 userData) override;
    };
}
