#include "physics/jolt_layers.h"

#include <Jolt/Physics/Body/Body.h>

namespace conjure::physics::jolt {

/*===========================================================================*/
/*===========================================================================*/
// layer_tables

layer_tables::layer_tables(const physics_layer_table &layers) :
    mBroadPhase(OBJECT_LAYER_COUNT, BROAD_PHASE_LAYER_COUNT),
    mPairFilter(OBJECT_LAYER_COUNT) {
    for (JPH::ObjectLayer objectLayer = 0; objectLayer < OBJECT_LAYER_COUNT; ++objectLayer) {
        const JPH::BroadPhaseLayer target = is_moving_layer(objectLayer) ? BROAD_PHASE_MOVING : BROAD_PHASE_NON_MOVING;
        mBroadPhase.MapObjectToBroadPhaseLayer(objectLayer, target);
    }

    // Every colliding (a, b) enables moving-vs-static both ways and moving-vs-moving; static pairs never
    // meet, so their bit stays off.
    const u32 layerCount = layers.count();
    for (u8 a = 0; a < layerCount; ++a) {
        for (u8 b = 0; b < layerCount; ++b) {
            if (!layers.collides(a, b)) {
                continue;
            }
            const JPH::ObjectLayer staticA = object_layer(a, motion_type::STATIC);
            const JPH::ObjectLayer staticB = object_layer(b, motion_type::STATIC);
            const JPH::ObjectLayer movingA = object_layer(a, motion_type::DYNAMIC);
            const JPH::ObjectLayer movingB = object_layer(b, motion_type::DYNAMIC);
            mPairFilter.EnableCollision(movingA, staticB);
            mPairFilter.EnableCollision(staticA, movingB);
            mPairFilter.EnableCollision(movingA, movingB);
        }
    }

    mObjectVsBroadPhase = std::make_unique<JPH::ObjectVsBroadPhaseLayerFilterTable>(
      mBroadPhase,
      BROAD_PHASE_LAYER_COUNT,
      mPairFilter,
      OBJECT_LAYER_COUNT
    );
}

layer_tables::~layer_tables() {}


/*===========================================================================*/
/*===========================================================================*/
// Query filters

layer_mask_object_filter::layer_mask_object_filter(const query_filter &filter) noexcept :
    mMask(filter.layers),
    mIncludeStatic(filter.includeStatic) {}

bool layer_mask_object_filter::ShouldCollide(JPH::ObjectLayer layer) const {
    if (!mIncludeStatic && !is_moving_layer(layer)) {
        return false;
    }
    return mMask.includes(physics_layer_of(layer));
}

broad_phase_mask_filter::broad_phase_mask_filter(const query_filter &filter) noexcept :
    mIncludeStatic(filter.includeStatic) {}

bool broad_phase_mask_filter::ShouldCollide(JPH::BroadPhaseLayer layer) const {
    return mIncludeStatic || layer == BROAD_PHASE_MOVING;
}

query_body_filter::query_body_filter(const JPH::BodyID &ignored, bool includeTriggers) noexcept :
    mIgnored(ignored),
    mIncludeTriggers(includeTriggers) {}

bool query_body_filter::ShouldCollide(const JPH::BodyID &bodyId) const {
    return mIgnored.IsInvalid() || bodyId != mIgnored;
}

bool query_body_filter::ShouldCollideLocked(const JPH::Body &body) const {
    return mIncludeTriggers || !body.IsSensor();
}
}
