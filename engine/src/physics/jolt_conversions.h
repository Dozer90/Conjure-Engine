#pragma once

#include "physics/jolt_include.h"

#include "conjure/core/math/aabb.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/math/transform.h"

#include <Jolt/Geometry/AABox.h>
#include <Jolt/Math/Mat44.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>

// The only place math types cross into Jolt (docs/design/14_physics.md P5). Every conversion goes through
// named components: math::quat is constructed (w, x, y, z) and stored x, y, z, w like JPH::Quat (R58), but
// neither side is handed a positional constructor of the other.

namespace conjure::physics::jolt {

    [[nodiscard]] inline JPH::Vec3 to_jolt(const math::vec3 &value) noexcept {
        return JPH::Vec3(value.x, value.y, value.z);
    }

    [[nodiscard]] inline JPH::Quat to_jolt(const math::quat &value) noexcept {
        return JPH::Quat(value.x, value.y, value.z, value.w);
    }

    [[nodiscard]] inline math::vec3 from_jolt(JPH::Vec3Arg value) noexcept {
        return math::vec3(value.GetX(), value.GetY(), value.GetZ());
    }

    [[nodiscard]] inline math::quat from_jolt(JPH::QuatArg value) noexcept {
        math::quat result = {};
        result.x = value.GetX();
        result.y = value.GetY();
        result.z = value.GetZ();
        result.w = value.GetW();
        return result;
    }

    [[nodiscard]] inline math::aabb from_jolt(const JPH::AABox &box) noexcept {
        math::aabb result = {};
        result.min = from_jolt(box.mMin);
        result.max = from_jolt(box.mMax);
        return result;
    }

    /// <summary>
    /// A world matrix (rotation then translation, unit scale) for a pose.
    /// </summary>
    [[nodiscard]] inline JPH::Mat44 to_jolt_matrix(const math::vec3 &position, const math::quat &rotation) noexcept {
        return JPH::Mat44::sRotationTranslation(to_jolt(rotation), to_jolt(position));
    }

    /// <summary>
    /// A unit-scale transform from a Jolt position and rotation.
    /// </summary>
    [[nodiscard]] inline math::transform to_transform(JPH::Vec3Arg position, JPH::QuatArg rotation) noexcept {
        math::transform result = {};
        result.position = from_jolt(position);
        result.rotation = from_jolt(rotation);
        return result;
    }
}
