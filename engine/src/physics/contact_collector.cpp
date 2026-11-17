#include "physics/contact_collector.h"

#include "physics/jolt_conversions.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/platform_detect.h"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Collision/EstimateCollisionResponse.h>

#include <algorithm>

namespace conjure::physics {

namespace {

    [[nodiscard]] u8 clamp_count(JPH::uint count) noexcept {
        return static_cast<u8>(std::min<JPH::uint>(count, 255u));
    }
}

/*===========================================================================*/
/*===========================================================================*/
// contact_collector

contact_collector::contact_collector() {}

contact_collector::~contact_collector() {}

void contact_collector::init(u32 threadCount, u32 capacity, bool estimateImpulses) {
    mCapacity = capacity;
    mEstimateImpulses = estimateImpulses;
    mBuffers.clear();
    ensure_threads(threadCount);
    clear();
}

void contact_collector::ensure_threads(u32 threadCount) {
    while (mBuffers.size() < threadCount) {
        std::vector<raw_contact> buffer = {};
        buffer.reserve(mCapacity);
        mBuffers.push_back(std::move(buffer));
    }
}

std::span<const raw_contact> contact_collector::buffer(u32 threadIndex) const noexcept {
    if (threadIndex >= mBuffers.size()) {
        return {};
    }
    return mBuffers[threadIndex];
}

void contact_collector::clear() noexcept {
    for (std::vector<raw_contact> &buffer : mBuffers) {
        buffer.clear();
    }
    mRecorded.store(0, std::memory_order_relaxed);
    mDropped.store(0, std::memory_order_relaxed);
}

JPH::ValidateResult contact_collector::OnContactValidate(
  const JPH::Body &body1,
  const JPH::Body &body2,
  JPH::RVec3Arg baseOffset,
  const JPH::CollideShapeResult &collisionResult
) {
    CONJURE_UNUSED(body1);
    CONJURE_UNUSED(body2);
    CONJURE_UNUSED(baseOffset);
    CONJURE_UNUSED(collisionResult);
    return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
}

void contact_collector::OnContactAdded(
  const JPH::Body &body1,
  const JPH::Body &body2,
  const JPH::ContactManifold &manifold,
  JPH::ContactSettings &settings
) {
    raw_contact contact = {};
    contact.bodyA = body1.GetID().GetIndexAndSequenceNumber();
    contact.bodyB = body2.GetID().GetIndexAndSequenceNumber();
    contact.subShapeA = manifold.mSubShapeID1.GetValue();
    contact.subShapeB = manifold.mSubShapeID2.GetValue();
    contact.kind = raw_contact_kind::ADDED;
    contact.pointCount = clamp_count(manifold.mRelativeContactPointsOn1.size());
    contact.normal = jolt::from_jolt(manifold.mWorldSpaceNormal);
    if (contact.pointCount > 0) {
        contact.point = jolt::from_jolt(manifold.GetWorldSpaceContactPointOn1(0));
    }
    if (mEstimateImpulses && !body1.IsSensor() && !body2.IsSensor() && contact.pointCount > 0) {
        JPH::CollisionEstimationResult estimate = {};
        JPH::EstimateCollisionResponse(
          body1,
          body2,
          manifold,
          estimate,
          settings.mCombinedFriction,
          settings.mCombinedRestitution
        );
        f32 total = 0.0f;
        for (const JPH::CollisionEstimationResult::Impulse &impulse : estimate.mImpulses) {
            total += impulse.mContactImpulse;
        }
        contact.impulse = total;
    }
    record(contact);
}

void contact_collector::OnContactPersisted(
  const JPH::Body &body1,
  const JPH::Body &body2,
  const JPH::ContactManifold &manifold,
  JPH::ContactSettings &settings
) {
    CONJURE_UNUSED(settings);
    if (body1.IsSensor() || body2.IsSensor()) {
        // A trigger pair has no STAY, so nothing reads this record (collect_events). Recorded, every body
        // inside a static trigger would take one record of the step's budget in every step for as long as it
        // is there, asleep too (code sweep F408), and a full budget drops the ADDED and REMOVED records that
        // other pairs' ENTER and EXIT are made of.
        return;
    }
    raw_contact contact = {};
    contact.bodyA = body1.GetID().GetIndexAndSequenceNumber();
    contact.bodyB = body2.GetID().GetIndexAndSequenceNumber();
    contact.subShapeA = manifold.mSubShapeID1.GetValue();
    contact.subShapeB = manifold.mSubShapeID2.GetValue();
    contact.kind = raw_contact_kind::PERSISTED;
    contact.pointCount = clamp_count(manifold.mRelativeContactPointsOn1.size());
    contact.normal = jolt::from_jolt(manifold.mWorldSpaceNormal);
    if (contact.pointCount > 0) {
        contact.point = jolt::from_jolt(manifold.GetWorldSpaceContactPointOn1(0));
    }
    record(contact);
}

void contact_collector::OnContactRemoved(const JPH::SubShapeIDPair &subShapePair) {
    // Bodies may already be gone here (Jolt forbids dereferencing them); the ids suffice.
    raw_contact contact = {};
    contact.bodyA = subShapePair.GetBody1ID().GetIndexAndSequenceNumber();
    contact.bodyB = subShapePair.GetBody2ID().GetIndexAndSequenceNumber();
    contact.subShapeA = subShapePair.GetSubShapeID1().GetValue();
    contact.subShapeB = subShapePair.GetSubShapeID2().GetValue();
    contact.kind = raw_contact_kind::REMOVED;
    record(contact);
}

void contact_collector::record(const raw_contact &contact) noexcept {
    const u32 threadIndex = core::jobs::job_system::thread_index();
    if (threadIndex >= mBuffers.size()) {
        mDropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    // The global budget keeps the merge buffers at mCapacity whatever the thread count does.
    if (mRecorded.fetch_add(1, std::memory_order_relaxed) >= mCapacity) {
        mRecorded.fetch_sub(1, std::memory_order_relaxed);
        mDropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    std::vector<raw_contact> &buffer = mBuffers[threadIndex];
    if (buffer.size() >= buffer.capacity()) {
        mRecorded.fetch_sub(1, std::memory_order_relaxed);
        mDropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    buffer.push_back(contact);
}


/*===========================================================================*/
/*===========================================================================*/
// activation_collector

activation_collector::activation_collector() {}

activation_collector::~activation_collector() {}

void activation_collector::bind(std::atomic<u8> *awake, u32 count) noexcept {
    mAwake = awake;
    mCount = count;
}

void activation_collector::OnBodyActivated(const JPH::BodyID &bodyId, JPH::uint64 userData) {
    CONJURE_UNUSED(userData);
    const u32 index = bodyId.GetIndex();
    if (mAwake == nullptr || index >= mCount) {
        return;
    }
    mAwake[index].store(1, std::memory_order_relaxed);
}

void activation_collector::OnBodyDeactivated(const JPH::BodyID &bodyId, JPH::uint64 userData) {
    CONJURE_UNUSED(userData);
    const u32 index = bodyId.GetIndex();
    if (mAwake == nullptr || index >= mCount) {
        return;
    }
    mAwake[index].store(0, std::memory_order_relaxed);
}
}
