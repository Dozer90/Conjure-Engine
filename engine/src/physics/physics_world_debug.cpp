#include "physics/jolt_conversions.h"
#include "physics/physics_world_internal.h"

#include "conjure/core/platform_detect.h"

#ifdef JPH_DEBUG_RENDERER
    #include <Jolt/Physics/Body/BodyManager.h>
    #include <Jolt/Physics/Collision/Shape/Shape.h>
    #include <Jolt/Renderer/DebugRendererSimple.h>
#endif

#include <string_view>

namespace conjure::physics {

#ifdef JPH_DEBUG_RENDERER

namespace {

    /// <summary>
    /// Forwards Jolt's primitives into the engine sink (docs/design/14_physics.md 3.10). Jolt colours are
    /// stored as 0xAABBGGRR, the sink's convention.
    /// </summary>
    class debug_renderer_adapter final : public JPH::DebugRendererSimple {
        private:
            physics_debug_draw *mSink = nullptr;

        public:
            explicit debug_renderer_adapter(physics_debug_draw &sink) :
                mSink(&sink) {}

            void DrawLine(JPH::RVec3Arg from, JPH::RVec3Arg to, JPH::ColorArg color) override {
                mSink->line(jolt::from_jolt(from), jolt::from_jolt(to), color.GetUInt32());
            }

            void DrawTriangle(
              JPH::RVec3Arg a,
              JPH::RVec3Arg b,
              JPH::RVec3Arg c,
              JPH::ColorArg color,
              ECastShadow castShadow
            ) override {
                CONJURE_UNUSED(castShadow);
                mSink->triangle(jolt::from_jolt(a), jolt::from_jolt(b), jolt::from_jolt(c), color.GetUInt32());
            }

            void DrawText3D(
              JPH::RVec3Arg position,
              const JPH::string_view &text,
              JPH::ColorArg color,
              float height
            ) override {
                mSink->text(
                  jolt::from_jolt(position),
                  std::string_view(text.data(), text.size()),
                  color.GetUInt32(),
                  height
                );
            }
    };

    constexpr JPH::Color CHARACTER_COLOR(0xFF00FFFFu);
}

void physics_world::debug_draw(physics_debug_draw &sink, physics_debug_draw_flags flags) const {
    const state &s = *mState;
    if (flags == physics_debug_draw_flags::NONE || s.busy.load(std::memory_order_acquire)) {
        return;
    }
    debug_renderer_adapter adapter(sink);

    JPH::BodyManager::DrawSettings settings = {};
    settings.mDrawShape = has_flag(flags, physics_debug_draw_flags::SHAPES);
    settings.mDrawShapeWireframe = has_flag(flags, physics_debug_draw_flags::WIREFRAME);
    settings.mDrawBoundingBox = has_flag(flags, physics_debug_draw_flags::BOUNDING_BOXES);
    settings.mDrawCenterOfMassTransform = has_flag(flags, physics_debug_draw_flags::CENTER_OF_MASS);
    settings.mDrawVelocity = has_flag(flags, physics_debug_draw_flags::VELOCITIES);
    settings.mDrawSleepStats = has_flag(flags, physics_debug_draw_flags::SLEEP_STATE);
    settings.mDrawShapeColor = JPH::BodyManager::EShapeColor::MotionTypeColor;
    const bool anyBodyFlag = settings.mDrawShape || settings.mDrawBoundingBox || settings.mDrawCenterOfMassTransform
      || settings.mDrawVelocity || settings.mDrawSleepStats;
    if (anyBodyFlag) {
        s.physicsSystem->DrawBodies(settings, &adapter);
    }

    if (has_flag(flags, physics_debug_draw_flags::CHARACTERS)) {
        for (const body_record &row : s.records) {
            if (row.is_free() || row.kind != record_kind::CHARACTER || row.character == nullptr) {
                continue;
            }
            row.character->GetShape()->Draw(
              &adapter,
              row.character->GetCenterOfMassTransform(),
              JPH::Vec3::sOne(),
              CHARACTER_COLOR,
              false,
              settings.mDrawShapeWireframe
            );
        }
    }

    if (has_flag(flags, physics_debug_draw_flags::CONTACTS)) {
        constexpr u32 CONTACT_COLOR = 0xFF0000FFu;
        for (const contact_event &event : s.events) {
            if (event.kind == contact_kind::EXIT || event.kind == contact_kind::TRIGGER_EXIT) {
                continue;
            }
            sink.line(event.point, event.point + event.normal * 0.25f, CONTACT_COLOR);
        }
    }
}

#else

void physics_world::debug_draw(physics_debug_draw &sink, physics_debug_draw_flags flags) const {
    // Jolt was built without its debug renderer (Distribution): nothing to draw.
    CONJURE_UNUSED(sink);
    CONJURE_UNUSED(flags);
}

#endif
}
