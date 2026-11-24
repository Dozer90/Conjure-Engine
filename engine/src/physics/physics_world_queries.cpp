#include "physics/jolt_conversions.h"
#include "physics/jolt_layers.h"
#include "physics/physics_world_internal.h"

#include "conjure/core/assert.h"
#include "conjure/core/math/math_utils.h"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollector.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/PhysicsSettings.h>

#include <algorithm>
#include <cmath>
#include <variant>

namespace conjure::physics {

namespace {

    /// <summary>
    /// The longest ray or sweep a query casts: maxDistance is clamped to it, so an infinite one means unlimited
    /// instead of scaling the direction into infinities and NaNs.
    /// </summary>
    constexpr f32 MAX_RAY_DISTANCE = 1.0e6f;

    /// <summary>
    /// Keeps the out.size() nearest ray hits in the caller's span, unordered (raycast_all sorts them).
    /// The body under test is handed to OnBody with its lock held, so the normal and entity can be
    /// resolved right away. Once the span is full a hit replaces the farthest one kept and the early-out
    /// fraction follows the farthest kept, so the broad phase skips everything beyond it.
    /// </summary>
    class span_ray_collector final : public JPH::CastRayCollector {
        private:
            std::span<raycast_hit> mOut = {};
            const JPH::RRayCast *mRay = nullptr;
            const JPH::Body *mBody = nullptr;
            f32 mMaxDistance = 0.0f;
            u32 mCount = 0;

        public:
            span_ray_collector(std::span<raycast_hit> out, const JPH::RRayCast &ray, f32 maxDistance) noexcept :
                mOut(out),
                mRay(&ray),
                mMaxDistance(maxDistance) {}

            [[nodiscard]] u32 count() const noexcept {
                return mCount;
            }

            void OnBody(const JPH::Body &body) override {
                mBody = &body;
            }

            void AddHit(const JPH::RayCastResult &result) override {
                if (mBody == nullptr) {
                    return;
                }
                const bool full = mCount == mOut.size();
                const u32 slot = full ? farthest() : mCount;
                if (full && result.mFraction >= mOut[slot].fraction) {
                    return;
                }
                raycast_hit &hit = mOut[slot];
                const JPH::Vec3 point = mRay->GetPointOnRay(result.mFraction);
                hit.entity = ecs::entity::from_u64(mBody->GetUserData());
                hit.point = jolt::from_jolt(point);
                hit.normal = jolt::from_jolt(mBody->GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point));
                hit.fraction = result.mFraction;
                hit.distance = result.mFraction * mMaxDistance;
                hit.layer = jolt::physics_layer_of(mBody->GetObjectLayer());
                hit.isTrigger = mBody->IsSensor();
                if (!full) {
                    ++mCount;
                }
                if (mCount == mOut.size()) {
                    // Only ever shrinks: the farthest kept hit is replaced by a nearer one or stays.
                    UpdateEarlyOutFraction(mOut[farthest()].fraction);
                }
            }

        private:
            [[nodiscard]] u32 farthest() const noexcept {
                u32 found = 0;
                for (u32 index = 1; index < mCount; ++index) {
                    if (mOut[index].fraction > mOut[found].fraction) {
                        found = index;
                    }
                }
                return found;
            }
    };

    /// <summary>
    /// One entity per overlapping body.
    /// </summary>
    class span_overlap_collector final : public JPH::CollideShapeCollector {
        private:
            std::span<ecs::entity> mOut = {};
            const JPH::Body *mBody = nullptr;
            bool mAddedCurrent = false;
            u32 mCount = 0;

        public:
            explicit span_overlap_collector(std::span<ecs::entity> out) noexcept :
                mOut(out) {}

            [[nodiscard]] u32 count() const noexcept {
                return mCount;
            }

            void OnBody(const JPH::Body &body) override {
                mBody = &body;
                mAddedCurrent = false;
            }

            void AddHit(const JPH::CollideShapeResult &result) override {
                CONJURE_UNUSED(result);
                if (mAddedCurrent || mBody == nullptr) {
                    return;
                }
                if (mCount >= mOut.size()) {
                    ForceEarlyOut();
                    return;
                }
                mOut[mCount] = ecs::entity::from_u64(mBody->GetUserData());
                ++mCount;
                mAddedCurrent = true;
            }
    };

    /// <summary>
    /// The closest shape cast hit with its entity resolved while the body is locked. distance is the
    /// fraction of the sweep times its length in metres (the clamped maxDistance).
    /// </summary>
    class closest_cast_collector final : public JPH::CastShapeCollector {
        private:
            const JPH::Body *mBody = nullptr;
            math::vec3 mBaseOffset = math::vec3(0.0f);
            f32 mDistance = 0.0f;
            bool mHasHit = false;
            shape_cast_hit mHit = {};

        public:
            closest_cast_collector(const math::vec3 &baseOffset, f32 distance) noexcept :
                mBaseOffset(baseOffset),
                mDistance(distance) {}

            [[nodiscard]] bool has_hit() const noexcept {
                return mHasHit;
            }

            [[nodiscard]] const shape_cast_hit &hit() const noexcept {
                return mHit;
            }

            void OnBody(const JPH::Body &body) override {
                mBody = &body;
            }

            void AddHit(const JPH::ShapeCastResult &result) override {
                if (mBody == nullptr) {
                    return;
                }
                const f32 earlyOut = result.GetEarlyOutFraction();
                if (mHasHit && earlyOut >= GetEarlyOutFraction()) {
                    return;
                }
                UpdateEarlyOutFraction(earlyOut);
                mHasHit = true;
                mHit.entity = ecs::entity::from_u64(mBody->GetUserData());
                mHit.point = mBaseOffset + jolt::from_jolt(result.mContactPointOn2);
                mHit.normal = -math::safe_normalize(
                  jolt::from_jolt(result.mPenetrationAxis),
                  math::vec3(0.0f, 1.0f, 0.0f)
                );
                mHit.fraction = result.mFraction;
                mHit.distance = result.mFraction * mDistance;
                mHit.penetrationDepth = result.mPenetrationDepth;
                mHit.layer = jolt::physics_layer_of(mBody->GetObjectLayer());
            }
    };

    /// <summary>
    /// Runs 'callable' with an embedded (stack) Jolt shape built from the description.
    /// </summary>
    template <typename Callable>
    void with_embedded_shape(const cast_shape_desc &desc, Callable &&callable) {
        if (const sphere_shape_desc *sphere = std::get_if<sphere_shape_desc>(&desc)) {
            JPH::SphereShape shape(std::max(std::abs(sphere->radius), 1.0e-4f));
            shape.SetEmbedded();
            callable(shape);
            return;
        }
        if (const box_shape_desc *box = std::get_if<box_shape_desc>(&desc)) {
            const math::vec3 halfExtents = glm::max(glm::abs(box->halfExtents), math::vec3(1.0e-4f));
            const f32 convexRadius = std::min(
              JPH::cDefaultConvexRadius,
              std::min(halfExtents.x, std::min(halfExtents.y, halfExtents.z)) * 0.5f
            );
            JPH::BoxShape shape(jolt::to_jolt(halfExtents), convexRadius);
            shape.SetEmbedded();
            callable(shape);
            return;
        }
        const capsule_shape_desc &capsule = std::get<capsule_shape_desc>(desc);
        JPH::CapsuleShape shape(
          std::max(std::abs(capsule.halfHeight), 1.0e-4f),
          std::max(std::abs(capsule.radius), 1.0e-4f)
        );
        shape.SetEmbedded();
        callable(shape);
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Queries

std::optional<raycast_hit> physics_world::raycast(
  const math::ray &ray,
  f32 maxDistance,
  const query_filter &filter
) const {
    const state &s = *mState;
    CONJURE_ASSERT(
      !s.busy.load(std::memory_order_acquire),
      "physics_world::raycast during a push, step or pull (STEP_IN_PROGRESS)"
    );
    if (s.busy.load(std::memory_order_acquire) || !(maxDistance > 0.0f)) {
        return std::nullopt;
    }
    if (!is_finite(ray.origin) || !is_finite(ray.direction)) {
        return std::nullopt;
    }
    const f32 distance = std::min(maxDistance, MAX_RAY_DISTANCE);
    const body_record *ignored = s.find_row(filter.ignore);
    const jolt::broad_phase_mask_filter broadPhase(filter);
    const jolt::layer_mask_object_filter objectLayers(filter);
    const jolt::query_body_filter bodies(ignored == nullptr ? JPH::BodyID() : ignored->bodyId, filter.includeTriggers);

    const math::vec3 direction = math::safe_normalize(ray.direction) * distance;
    const JPH::RRayCast cast(jolt::to_jolt(ray.origin), jolt::to_jolt(direction));
    JPH::RayCastResult result = {};
    if (!s.physicsSystem->GetNarrowPhaseQuery().CastRay(cast, result, broadPhase, objectLayers, bodies)) {
        return std::nullopt;
    }

    const JPH::BodyLockRead lock(s.physicsSystem->GetBodyLockInterface(), result.mBodyID);
    if (!lock.Succeeded()) {
        return std::nullopt;
    }
    const JPH::Body &body = lock.GetBody();
    const JPH::Vec3 point = cast.GetPointOnRay(result.mFraction);
    raycast_hit hit = {};
    hit.entity = ecs::entity::from_u64(body.GetUserData());
    hit.point = jolt::from_jolt(point);
    hit.normal = jolt::from_jolt(body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point));
    hit.fraction = result.mFraction;
    hit.distance = result.mFraction * distance;
    hit.layer = jolt::physics_layer_of(body.GetObjectLayer());
    hit.isTrigger = body.IsSensor();
    return hit;
}

u32 physics_world::raycast_all(
  const math::ray &ray,
  f32 maxDistance,
  std::span<raycast_hit> out,
  const query_filter &filter
) const {
    const state &s = *mState;
    CONJURE_ASSERT(
      !s.busy.load(std::memory_order_acquire),
      "physics_world::raycast_all during a push, step or pull (STEP_IN_PROGRESS)"
    );
    if (s.busy.load(std::memory_order_acquire) || out.empty() || !(maxDistance > 0.0f)) {
        return 0;
    }
    if (!is_finite(ray.origin) || !is_finite(ray.direction)) {
        return 0;
    }
    const f32 distance = std::min(maxDistance, MAX_RAY_DISTANCE);
    const body_record *ignored = s.find_row(filter.ignore);
    const jolt::broad_phase_mask_filter broadPhase(filter);
    const jolt::layer_mask_object_filter objectLayers(filter);
    const jolt::query_body_filter bodies(ignored == nullptr ? JPH::BodyID() : ignored->bodyId, filter.includeTriggers);

    const math::vec3 direction = math::safe_normalize(ray.direction) * distance;
    const JPH::RRayCast cast(jolt::to_jolt(ray.origin), jolt::to_jolt(direction));
    JPH::RayCastSettings settings = {};
    span_ray_collector collector(out, cast, distance);
    s.physicsSystem->GetNarrowPhaseQuery().CastRay(cast, settings, collector, broadPhase, objectLayers, bodies);

    const u32 count = collector.count();
    // Insertion sort by fraction: the span is small and already partly ordered.
    for (u32 index = 1; index < count; ++index) {
        const raycast_hit current = out[index];
        u32 position = index;
        while (position > 0 && out[position - 1].fraction > current.fraction) {
            out[position] = out[position - 1];
            --position;
        }
        out[position] = current;
    }
    return count;
}

u32 physics_world::overlap_sphere(
  const math::vec3 &center,
  f32 radius,
  std::span<ecs::entity> out,
  const query_filter &filter
) const {
    const state &s = *mState;
    CONJURE_ASSERT(
      !s.busy.load(std::memory_order_acquire),
      "physics_world::overlap_sphere during a push, step or pull (STEP_IN_PROGRESS)"
    );
    if (s.busy.load(std::memory_order_acquire) || out.empty()) {
        return 0;
    }
    const body_record *ignored = s.find_row(filter.ignore);
    const jolt::broad_phase_mask_filter broadPhase(filter);
    const jolt::layer_mask_object_filter objectLayers(filter);
    const jolt::query_body_filter bodies(ignored == nullptr ? JPH::BodyID() : ignored->bodyId, filter.includeTriggers);

    JPH::SphereShape shape(std::max(std::abs(radius), 1.0e-4f));
    shape.SetEmbedded();
    span_overlap_collector collector(out);
    const JPH::CollideShapeSettings settings = {};
    s.physicsSystem->GetNarrowPhaseQuery().CollideShape(
      &shape,
      JPH::Vec3::sOne(),
      JPH::RMat44::sTranslation(jolt::to_jolt(center)),
      settings,
      jolt::to_jolt(center),
      collector,
      broadPhase,
      objectLayers,
      bodies
    );
    return collector.count();
}

u32 physics_world::overlap_box(
  const math::vec3 &center,
  const math::vec3 &halfExtents,
  const math::quat &rotation,
  std::span<ecs::entity> out,
  const query_filter &filter
) const {
    const state &s = *mState;
    CONJURE_ASSERT(
      !s.busy.load(std::memory_order_acquire),
      "physics_world::overlap_box during a push, step or pull (STEP_IN_PROGRESS)"
    );
    if (s.busy.load(std::memory_order_acquire) || out.empty()) {
        return 0;
    }
    const body_record *ignored = s.find_row(filter.ignore);
    const jolt::broad_phase_mask_filter broadPhase(filter);
    const jolt::layer_mask_object_filter objectLayers(filter);
    const jolt::query_body_filter bodies(ignored == nullptr ? JPH::BodyID() : ignored->bodyId, filter.includeTriggers);

    const math::vec3 extents = glm::max(glm::abs(halfExtents), math::vec3(1.0e-4f));
    const f32 convexRadius = std::min(
      JPH::cDefaultConvexRadius,
      std::min(extents.x, std::min(extents.y, extents.z)) * 0.5f
    );
    JPH::BoxShape shape(jolt::to_jolt(extents), convexRadius);
    shape.SetEmbedded();
    span_overlap_collector collector(out);
    const JPH::CollideShapeSettings settings = {};
    s.physicsSystem->GetNarrowPhaseQuery().CollideShape(
      &shape,
      JPH::Vec3::sOne(),
      jolt::to_jolt_matrix(center, rotation),
      settings,
      jolt::to_jolt(center),
      collector,
      broadPhase,
      objectLayers,
      bodies
    );
    return collector.count();
}

std::optional<shape_cast_hit> physics_world::shape_cast(
  const cast_shape_desc &shapeDesc,
  const math::transform &start,
  const math::vec3 &direction,
  f32 maxDistance,
  const query_filter &filter
) const {
    const state &s = *mState;
    CONJURE_ASSERT(
      !s.busy.load(std::memory_order_acquire),
      "physics_world::shape_cast during a push, step or pull (STEP_IN_PROGRESS)"
    );
    if (s.busy.load(std::memory_order_acquire) || !(maxDistance > 0.0f)) {
        return std::nullopt;
    }
    if (!is_finite(start.position) || !is_finite(start.rotation) || !is_finite(direction)) {
        return std::nullopt;
    }
    const f32 distance = std::min(maxDistance, MAX_RAY_DISTANCE);
    const math::vec3 sweep = math::safe_normalize(direction) * distance;
    const body_record *ignored = s.find_row(filter.ignore);
    const jolt::broad_phase_mask_filter broadPhase(filter);
    const jolt::layer_mask_object_filter objectLayers(filter);
    const jolt::query_body_filter bodies(ignored == nullptr ? JPH::BodyID() : ignored->bodyId, filter.includeTriggers);

    closest_cast_collector collector(start.position, distance);
    with_embedded_shape(shapeDesc, [&](const JPH::Shape &shape) {
        const JPH::RShapeCast cast = JPH::RShapeCast::sFromWorldTransform(
          &shape,
          JPH::Vec3::sOne(),
          jolt::to_jolt_matrix(start.position, start.rotation),
          jolt::to_jolt(sweep)
        );
        const JPH::ShapeCastSettings settings = {};
        s.physicsSystem->GetNarrowPhaseQuery().CastShape(
          cast,
          settings,
          jolt::to_jolt(start.position),
          collector,
          broadPhase,
          objectLayers,
          bodies
        );
    });
    if (!collector.has_hit()) {
        return std::nullopt;
    }
    return collector.hit();
}
}
