#pragma once

#include "physics/jolt_include.h"

#include "conjure/physics/components.h"
#include "conjure/physics/physics_layers.h"
#include "conjure/physics/queries.h"

#include "conjure/core/types.h"

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>

#include <memory>

// Layers to Jolt tables (docs/design/14_physics.md P10, 3.2): object layer = (moving ? 32 : 0) + layer,
// two broad phase layers (NON_MOVING, MOVING), a pair filter table over 64 object layers in which
// static-static pairs stay disabled, and the query filters built from a query_filter.

namespace conjure::physics::jolt {

    inline constexpr JPH::BroadPhaseLayer BROAD_PHASE_NON_MOVING(0);
    inline constexpr JPH::BroadPhaseLayer BROAD_PHASE_MOVING(1);
    inline constexpr JPH::uint BROAD_PHASE_LAYER_COUNT = 2;
    inline constexpr JPH::uint OBJECT_LAYER_COUNT = 2 * MAX_PHYSICS_LAYERS;

    /// <summary>
    /// The Jolt object layer of a physics layer for a motion type.
    /// </summary>
    [[nodiscard]] constexpr JPH::ObjectLayer object_layer(u8 layer, motion_type motion) noexcept {
        const JPH::ObjectLayer movingOffset =
          motion == motion_type::STATIC ? 0 : static_cast<JPH::ObjectLayer>(MAX_PHYSICS_LAYERS);
        return static_cast<JPH::ObjectLayer>(movingOffset + layer);
    }

    /// <summary>
    /// The physics layer of a Jolt object layer.
    /// </summary>
    [[nodiscard]] constexpr u8 physics_layer_of(JPH::ObjectLayer objectLayer) noexcept {
        return static_cast<u8>(objectLayer % MAX_PHYSICS_LAYERS);
    }

    /// <summary>
    /// True for the moving half of the object layers.
    /// </summary>
    [[nodiscard]] constexpr bool is_moving_layer(JPH::ObjectLayer objectLayer) noexcept {
        return objectLayer >= MAX_PHYSICS_LAYERS;
    }

    /// <summary>
    /// The three tables PhysicsSystem::Init takes, built once per world.
    /// </summary>
    class layer_tables {
        private:
            JPH::BroadPhaseLayerInterfaceTable mBroadPhase;              // no default: sized in the constructor
            JPH::ObjectLayerPairFilterTable mPairFilter;                 // no default: sized in the constructor
            std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilterTable> mObjectVsBroadPhase = nullptr;

        public:
            explicit layer_tables(const physics_layer_table &layers);
            ~layer_tables();

            layer_tables(const layer_tables &) = delete;
            layer_tables &operator=(const layer_tables &) = delete;

        public:
            [[nodiscard]] const JPH::BroadPhaseLayerInterface &broad_phase() const noexcept {
                return mBroadPhase;
            }

            [[nodiscard]] const JPH::ObjectLayerPairFilter &pair_filter() const noexcept {
                return mPairFilter;
            }

            [[nodiscard]] const JPH::ObjectVsBroadPhaseLayerFilter &object_vs_broad_phase() const noexcept {
                return *mObjectVsBroadPhase;
            }
    };

    /// <summary>
    /// Object layer filter of a query: the mask over physics layers plus the static switch.
    /// </summary>
    class layer_mask_object_filter final : public JPH::ObjectLayerFilter {
        private:
            physics_layer_mask mMask = {};
            bool mIncludeStatic = true;

        public:
            explicit layer_mask_object_filter(const query_filter &filter) noexcept;

        public:
            [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer layer) const override;
    };

    /// <summary>
    /// Broad phase filter of a query: drops NON_MOVING when static bodies are excluded.
    /// </summary>
    class broad_phase_mask_filter final : public JPH::BroadPhaseLayerFilter {
        private:
            bool mIncludeStatic = true;

        public:
            explicit broad_phase_mask_filter(const query_filter &filter) noexcept;

        public:
            [[nodiscard]] bool ShouldCollide(JPH::BroadPhaseLayer layer) const override;
    };

    /// <summary>
    /// Body filter of a query: skips the ignored body and, unless asked for, sensors.
    /// </summary>
    class query_body_filter final : public JPH::BodyFilter {
        private:
            JPH::BodyID mIgnored = {};
            bool mIncludeTriggers = false;

        public:
            query_body_filter(const JPH::BodyID &ignored, bool includeTriggers) noexcept;

        public:
            [[nodiscard]] bool ShouldCollide(const JPH::BodyID &bodyId) const override;
            [[nodiscard]] bool ShouldCollideLocked(const JPH::Body &body) const override;
    };
}
