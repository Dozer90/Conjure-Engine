#pragma once

#include "conjure/core/api.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/types.h"
#include "conjure/ecs/entity.h"

#include <string_view>
#include <type_traits>

// Contact events (docs/design/14_physics.md 2.4, 3.6, R6). physics_world::contact_events() returns the
// events of the last step sorted by ascending (bodyIdA, bodyIdB), then kind: per pair per step at most one
// ENTER / STAY (or TRIGGER_ENTER) and at most one EXIT (or TRIGGER_EXIT), the exit after the enter.
// Consumers reporting to b flip the normal. Destroying a body in contact synthesises its EXIT / TRIGGER_EXIT,
// reported by the next step and naming the body's entity. When the body went because that entity was
// destroyed, the entity is no longer alive: check world::is_alive before touching a participant. The entity's
// generation keeps the dead id from matching a reused slot, so no later event references it.
//
// Sleep (code sweep F408). A solid contact is reported only while one of its two bodies is awake: when the
// last awake one falls asleep (a box on static ground, a stack come to rest) the pair reports EXIT, and ENTER
// again when one wakes, although the bodies never parted. A trigger is different: a static trigger reports a
// body until it really leaves, asleep or not, and a body that is asleep when the trigger meets it; a
// kinematic or dynamic trigger does the same while it is awake itself (awake it never falls asleep on its
// own; START_ASLEEP or physics_world::sleep leaves it seeing awake bodies only).

namespace conjure::physics {

    /// <summary>
    /// What happened to a body pair in the step. Trigger pairs report only the TRIGGER_ kinds.
    /// </summary>
    enum class contact_kind : u8 {
        ENTER,
        STAY,
        EXIT,
        TRIGGER_ENTER,
        TRIGGER_EXIT
    };

    /// <summary>
    /// The upper-case enumerator name of a kind ("ENTER"); "UNKNOWN" for out-of-range values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(contact_kind kind) noexcept;

    /// <summary>
    /// One contact event. 48 bytes, trivially copyable.
    /// </summary>
    struct contact_event {
        ecs::entity a = {};                               // body with the lower Jolt BodyID
        ecs::entity b = {};
        math::vec3 point = math::vec3(0.0f);              // world space, first manifold point (zero for exits)
        math::vec3 normal = math::vec3(0.0f);             // world space, points from a towards b (zero for exits)
        f32 impulse = 0.0f;                               // kg*m/s, estimated on ENTER when enabled; else 0
        contact_kind kind = contact_kind::ENTER;
        u8 contactCount = 0;                              // manifold points summed over sub-shapes
        u8 layerA = 0;
        u8 layerB = 0;

        [[nodiscard]] bool operator==(const contact_event &) const = default;
    };

    static_assert(sizeof(contact_event) == 48, "contact_event is 48 bytes (14_physics.md 2.4)");
    static_assert(std::is_trivially_copyable_v<contact_event>, "contact_event must be trivially copyable");
}
