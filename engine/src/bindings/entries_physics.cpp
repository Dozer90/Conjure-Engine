#include "bindings/entry_support.h"

#include "conjure/core/math/glm_types.h"
#include "conjure/core/math/ray.h"
#include "conjure/core/math/transform.h"
#include "conjure/core/string_id.h"
#include "conjure/physics/character_controller.h"
#include "conjure/physics/components.h"
#include "conjure/physics/physics_layers.h"
#include "conjure/physics/physics_world.h"
#include "conjure/physics/queries.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

// physics_* (docs/design/15_bindings.md 2.3, 14_physics.md 8, R49): queries and the entity-keyed body API
// of physics_world. Velocities are read from the rigid_body mirror (the pull writes them), force modes map
// onto add_force / add_impulse with the component's mass, characters go through character_move /
// character_ground. This file is built only with CONJURE_WITH_PHYSICS; the stubs cover the group otherwise.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::bindings::detail;

namespace {

    constexpr std::string_view RIGID_BODY = "rigid_body";
    constexpr std::string_view CHARACTER_CONTROLLER = "character_controller";

    /// <summary>
    /// Results a count query (capacity 0) can report: physics_world writes at most the scratch it is handed
    /// and returns the count written, so a count query is exact up to this ceiling and a larger capacity
    /// grows the scratch to that capacity (the caller's buffer is then filled completely).
    /// </summary>
    constexpr usize MAX_QUERY_RESULTS = 1024;

    [[nodiscard]] physics::physics_world *world() noexcept {
        return active_context().physics;
    }

    /// <summary>
    /// The scratch size one query needs: the count ceiling, or the caller's capacity when that is larger.
    /// </summary>
    [[nodiscard]] usize scratch_size(int32_t capacity) noexcept {
        return std::max(MAX_QUERY_RESULTS, static_cast<usize>(capacity));
    }

    [[nodiscard]] std::vector<physics::raycast_hit> &hit_scratch(int32_t capacity) noexcept {
        static std::vector<physics::raycast_hit> sHits(MAX_QUERY_RESULTS);
        sHits.resize(scratch_size(capacity));
        return sHits;
    }

    [[nodiscard]] physics::query_filter filter_of(uint32_t layerMask, uint64_t ignore) noexcept {
        physics::query_filter filter = {};
        filter.layers = physics::physics_layer_mask{layerMask};
        filter.ignore = marshal::to_entity(ignore);
        return filter;
    }

    void write_hit(const physics::raycast_hit &hit, conjure_raycast_hit &out) noexcept {
        out.entity = marshal::from_entity(hit.entity);
        out.point = marshal::from_vec3(hit.point);
        out.normal = marshal::from_vec3(hit.normal);
        out.distance = hit.distance;
        out.layer = static_cast<int32_t>(hit.layer);
    }

    [[nodiscard]] bool unit_direction(
      native_function_id id,
      const conjure_vec3 &value,
      math::vec3 &out,
      int32_t &status
    ) noexcept {
        const math::vec3 direction = marshal::to_vec3(value);
        const f32 length = glm::length(direction);
        if (!(length > 0.0f) || !std::isfinite(length)) {
            status = fail_entry(id, bindings_error::INVALID_ARGUMENT, "direction must be non-zero");
            return false;
        }
        out = direction / length;
        return true;
    }

    /// <summary>
    /// A query's max_distance must be finite and non-negative: it scales the unit direction, so an infinity
    /// would turn the zero components into NaN and a negative or NaN distance would cast backwards or nowhere.
    /// Zero is allowed and misses.
    /// </summary>
    [[nodiscard]] bool valid_distance(native_function_id id, float distance, int32_t &status) noexcept {
        if (!std::isfinite(distance) || distance < 0.0f) {
            status = fail_entry(id, bindings_error::INVALID_ARGUMENT, "max_distance must be finite and >= 0");
            return false;
        }
        return true;
    }

    /// <summary>
    /// The mass a VELOCITY_CHANGE or ACCELERATION request scales by: the component's explicit mass (a
    /// derived mass, 0, cannot be known here).
    /// </summary>
    [[nodiscard]] bool explicit_mass(native_function_id id, uint64_t entity, f32 &mass, int32_t &status) noexcept {
        const physics::rigid_body *body = component_for_read<physics::rigid_body>(id, entity, RIGID_BODY, status);
        if (body == nullptr) {
            return false;
        }
        if (!(body->mass > 0.0f)) {
            status =
              fail_entry(id, bindings_error::INVALID_ARGUMENT, "mode needs an explicit mass (rigid_body.mass > 0)");
            return false;
        }
        mass = body->mass;
        return true;
    }

    [[nodiscard]] int32_t finish(native_function_id id, const core::result<void> &outcome) noexcept {
        if (!outcome.has_value()) {
            return fail_entry(id, outcome.get_error());
        }
        return 0;
    }

    [[nodiscard]] int32_t apply_force(
      native_function_id id,
      uint64_t entity,
      const conjure_vec3 *force,
      const conjure_vec3 *worldPoint,
      uint8_t mode
    ) noexcept {
        if (force == nullptr) {
            return fail_entry(id, bindings_error::INVALID_ARGUMENT, "force");
        }
        if (mode > static_cast<uint8_t>(conjure_force_mode::ACCELERATION)) {
            return fail_entry(id, bindings_error::INVALID_ARGUMENT, "mode");
        }
        int32_t status = 0;
        const ecs::entity target = resolve_alive(id, entity, status);
        if (target.is_null()) {
            return status;
        }
        if (!active_world()->has<physics::rigid_body>(target)) {
            return fail_entry(id, bindings_error::COMPONENT_MISSING, RIGID_BODY);
        }
        math::vec3 value = marshal::to_vec3(*force);
        const conjure_force_mode forceMode = static_cast<conjure_force_mode>(mode);
        if (forceMode == conjure_force_mode::VELOCITY_CHANGE || forceMode == conjure_force_mode::ACCELERATION) {
            f32 mass = 0.0f;
            if (!explicit_mass(id, entity, mass, status)) {
                return status;
            }
            value *= mass;
        }
        const bool impulse =
          forceMode == conjure_force_mode::IMPULSE || forceMode == conjure_force_mode::VELOCITY_CHANGE;
        if (worldPoint == nullptr) {
            return finish(id, impulse ? world()->add_impulse(target, value) : world()->add_force(target, value));
        }
        const math::vec3 point = marshal::to_vec3(*worldPoint);
        return finish(
          id, impulse ? world()->add_impulse_at(target, value, point) : world()->add_force_at(target, value, point)
        );
    }

    [[nodiscard]] int32_t write_overlap(
      std::span<ecs::entity> found,
      u32 count,
      uint64_t *out,
      int32_t capacity
    ) noexcept {
        return marshal::write_entities(found.subspan(0, std::min<usize>(count, found.size())), out, capacity);
    }
}

int32_t conjure::bindings::entries::physics_raycast(const conjure_ray_query *query, conjure_raycast_hit *outHit) {
    constexpr native_function_id ID = native_function_id::physics_raycast;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (query == nullptr || outHit == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "query / outHit");
    }
    if (world() == nullptr) {
        return unavailable(ID, "physics");
    }
    int32_t status = 0;
    math::ray ray = {};
    ray.origin = marshal::to_vec3(query->origin);
    if (!unit_direction(ID, query->direction, ray.direction, status)) {
        return status;
    }
    if (!valid_distance(ID, query->max_distance, status)) {
        return status;
    }
    const std::optional<physics::raycast_hit> hit =
      world()->raycast(ray, query->max_distance, filter_of(query->layer_mask, query->ignore_entity));
    if (!hit.has_value()) {
        return 0;
    }
    write_hit(*hit, *outHit);
    return 1;
}

int32_t conjure::bindings::entries::physics_raycast_all(
  const conjure_ray_query *query,
  conjure_raycast_hit *outHits,
  int32_t capacity
) {
    constexpr native_function_id ID = native_function_id::physics_raycast_all;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (query == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "query");
    }
    if (capacity < 0) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "capacity");
    }
    if (world() == nullptr) {
        return unavailable(ID, "physics");
    }
    int32_t status = 0;
    math::ray ray = {};
    ray.origin = marshal::to_vec3(query->origin);
    if (!unit_direction(ID, query->direction, ray.direction, status)) {
        return status;
    }
    if (!valid_distance(ID, query->max_distance, status)) {
        return status;
    }
    std::vector<physics::raycast_hit> &hits = hit_scratch(capacity);
    const u32 count =
      world()->raycast_all(ray, query->max_distance, hits, filter_of(query->layer_mask, query->ignore_entity));
    const usize written = outHits == nullptr ? 0 : std::min<usize>(count, static_cast<usize>(capacity));
    for (usize index = 0; index < written; ++index) {
        write_hit(hits[index], outHits[index]);
    }
    return static_cast<int32_t>(count);
}

int32_t conjure::bindings::entries::physics_shape_cast(const conjure_shape_query *query, conjure_raycast_hit *outHit) {
    constexpr native_function_id ID = native_function_id::physics_shape_cast;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (query == nullptr || outHit == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "query / outHit");
    }
    if (query->shape > static_cast<uint8_t>(conjure_cast_shape::CAPSULE)) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "shape");
    }
    if (world() == nullptr) {
        return unavailable(ID, "physics");
    }
    int32_t status = 0;
    math::vec3 direction = math::vec3(0.0f);
    if (!unit_direction(ID, query->ray.direction, direction, status)) {
        return status;
    }
    if (!valid_distance(ID, query->ray.max_distance, status)) {
        return status;
    }
    physics::cast_shape_desc shape = physics::sphere_shape_desc{query->radius};
    switch (static_cast<conjure_cast_shape>(query->shape)) {
        case conjure_cast_shape::SPHERE:
            break;
        case conjure_cast_shape::BOX:
            shape = physics::box_shape_desc{marshal::to_vec3(query->half_extents)};
            break;
        case conjure_cast_shape::CAPSULE:
            shape = physics::capsule_shape_desc{query->radius, query->half_extents.y};
            break;
    }
    math::transform start = {};
    start.position = marshal::to_vec3(query->ray.origin);
    start.rotation = marshal::to_quat(query->rotation);
    const std::optional<physics::shape_cast_hit> hit = world()->shape_cast(
      shape,
      start,
      direction * query->ray.max_distance,
      filter_of(query->ray.layer_mask, query->ray.ignore_entity)
    );
    if (!hit.has_value()) {
        return 0;
    }
    outHit->entity = marshal::from_entity(hit->entity);
    outHit->point = marshal::from_vec3(hit->point);
    outHit->normal = marshal::from_vec3(hit->normal);
    outHit->distance = hit->distance;
    outHit->layer = static_cast<int32_t>(hit->layer);
    return 1;
}

int32_t conjure::bindings::entries::physics_overlap_sphere(
  const conjure_vec3 *center,
  float radius,
  uint32_t layerMask,
  uint64_t *out,
  int32_t capacity
) {
    constexpr native_function_id ID = native_function_id::physics_overlap_sphere;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (center == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "center");
    }
    if (capacity < 0 || !(radius >= 0.0f)) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "capacity / radius");
    }
    if (world() == nullptr) {
        return unavailable(ID, "physics");
    }
    std::vector<ecs::entity> &found = scratch_entities();
    found.resize(scratch_size(capacity));
    const u32 count = world()->overlap_sphere(marshal::to_vec3(*center), radius, found, filter_of(layerMask, 0));
    return write_overlap(found, count, out, capacity);
}

int32_t conjure::bindings::entries::physics_overlap_box(
  const conjure_vec3 *center,
  const conjure_vec3 *halfExtents,
  const conjure_quat_xyzw *rotation,
  uint32_t layerMask,
  uint64_t *out,
  int32_t capacity
) {
    constexpr native_function_id ID = native_function_id::physics_overlap_box;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (center == nullptr || halfExtents == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "center / halfExtents");
    }
    if (capacity < 0) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "capacity");
    }
    if (world() == nullptr) {
        return unavailable(ID, "physics");
    }
    const math::quat orientation =
      rotation == nullptr ? math::quat(1.0f, 0.0f, 0.0f, 0.0f) : marshal::to_quat(*rotation);
    std::vector<ecs::entity> &found = scratch_entities();
    found.resize(scratch_size(capacity));
    const u32 count = world()->overlap_box(
      marshal::to_vec3(*center),
      marshal::to_vec3(*halfExtents),
      orientation,
      found,
      filter_of(layerMask, 0)
    );
    return write_overlap(found, count, out, capacity);
}

int32_t conjure::bindings::entries::physics_get_linear_velocity(uint64_t entity, conjure_vec3 *out) {
    constexpr native_function_id ID = native_function_id::physics_get_linear_velocity;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (out == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "out");
    }
    int32_t status = 0;
    const physics::rigid_body *body = component_for_read<physics::rigid_body>(ID, entity, RIGID_BODY, status);
    if (body == nullptr) {
        return status;
    }
    *out = marshal::from_vec3(body->linearVelocity);
    return 0;
}

int32_t conjure::bindings::entries::physics_set_linear_velocity(uint64_t entity, const conjure_vec3 *value) {
    constexpr native_function_id ID = native_function_id::physics_set_linear_velocity;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (value == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "value");
    }
    if (world() == nullptr) {
        return unavailable(ID, "physics");
    }
    int32_t status = 0;
    physics::rigid_body *body = component_for_write<physics::rigid_body>(ID, entity, RIGID_BODY, status);
    if (body == nullptr) {
        return status;
    }
    // The queued command reaches Jolt at the push; the mirror field answers reads until the next pull. The mirror
    // is written only once the command is queued: the push diffs it, so a refused request must leave it alone.
    const math::vec3 velocity = marshal::to_vec3(*value);
    const core::result<void> queued = world()->set_linear_velocity(marshal::to_entity(entity), velocity);
    if (!queued.has_value()) {
        return fail_entry(ID, queued.get_error());
    }
    body->linearVelocity = velocity;
    return 0;
}

int32_t conjure::bindings::entries::physics_get_angular_velocity(uint64_t entity, conjure_vec3 *out) {
    constexpr native_function_id ID = native_function_id::physics_get_angular_velocity;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (out == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "out");
    }
    int32_t status = 0;
    const physics::rigid_body *body = component_for_read<physics::rigid_body>(ID, entity, RIGID_BODY, status);
    if (body == nullptr) {
        return status;
    }
    *out = marshal::from_vec3(body->angularVelocity);
    return 0;
}

int32_t conjure::bindings::entries::physics_set_angular_velocity(uint64_t entity, const conjure_vec3 *value) {
    constexpr native_function_id ID = native_function_id::physics_set_angular_velocity;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (value == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "value");
    }
    if (world() == nullptr) {
        return unavailable(ID, "physics");
    }
    int32_t status = 0;
    physics::rigid_body *body = component_for_write<physics::rigid_body>(ID, entity, RIGID_BODY, status);
    if (body == nullptr) {
        return status;
    }
    // Mirror after the queue, as in physics_set_linear_velocity.
    const math::vec3 velocity = marshal::to_vec3(*value);
    const core::result<void> queued = world()->set_angular_velocity(marshal::to_entity(entity), velocity);
    if (!queued.has_value()) {
        return fail_entry(ID, queued.get_error());
    }
    body->angularVelocity = velocity;
    return 0;
}

int32_t conjure::bindings::entries::physics_add_force(uint64_t entity, const conjure_vec3 *force, uint8_t mode) {
    constexpr native_function_id ID = native_function_id::physics_add_force;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (world() == nullptr) {
        return unavailable(ID, "physics");
    }
    return apply_force(ID, entity, force, nullptr, mode);
}

int32_t conjure::bindings::entries::physics_add_force_at(
  uint64_t entity,
  const conjure_vec3 *force,
  const conjure_vec3 *worldPoint,
  uint8_t mode
) {
    constexpr native_function_id ID = native_function_id::physics_add_force_at;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (worldPoint == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "worldPoint");
    }
    if (world() == nullptr) {
        return unavailable(ID, "physics");
    }
    return apply_force(ID, entity, force, worldPoint, mode);
}

int32_t conjure::bindings::entries::physics_add_torque(uint64_t entity, const conjure_vec3 *torque, uint8_t mode) {
    constexpr native_function_id ID = native_function_id::physics_add_torque;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (torque == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "torque");
    }
    const bool supportedMode = mode == static_cast<uint8_t>(conjure_force_mode::FORCE)
      || mode == static_cast<uint8_t>(conjure_force_mode::IMPULSE);
    if (!supportedMode) {
        return fail_entry(
          ID, bindings_error::INVALID_ARGUMENT, "torque supports FORCE and IMPULSE (no inertia tensor here)"
        );
    }
    if (world() == nullptr) {
        return unavailable(ID, "physics");
    }
    int32_t status = 0;
    const ecs::entity target = resolve_alive(ID, entity, status);
    if (target.is_null()) {
        return status;
    }
    if (!active_world()->has<physics::rigid_body>(target)) {
        return fail_entry(ID, bindings_error::COMPONENT_MISSING, RIGID_BODY);
    }
    const math::vec3 value = marshal::to_vec3(*torque);
    if (mode == static_cast<uint8_t>(conjure_force_mode::IMPULSE)) {
        return finish(ID, world()->add_angular_impulse(target, value));
    }
    return finish(ID, world()->add_torque(target, value));
}

int32_t conjure::bindings::entries::physics_move_kinematic(uint64_t entity, const conjure_transform *target) {
    constexpr native_function_id ID = native_function_id::physics_move_kinematic;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (target == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "target");
    }
    if (world() == nullptr) {
        return unavailable(ID, "physics");
    }
    int32_t status = 0;
    const ecs::entity body = resolve_alive(ID, entity, status);
    if (body.is_null()) {
        return status;
    }
    if (!active_world()->has<physics::rigid_body>(body)) {
        return fail_entry(ID, bindings_error::COMPONENT_MISSING, RIGID_BODY);
    }
    return finish(ID, world()->move_kinematic(body, marshal::to_transform(*target)));
}

int32_t conjure::bindings::entries::physics_teleport(uint64_t entity, const conjure_transform *target) {
    constexpr native_function_id ID = native_function_id::physics_teleport;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (target == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "target");
    }
    if (world() == nullptr) {
        return unavailable(ID, "physics");
    }
    int32_t status = 0;
    const ecs::entity body = resolve_alive(ID, entity, status);
    if (body.is_null()) {
        return status;
    }
    if (!active_world()->has<physics::rigid_body>(body)) {
        return fail_entry(ID, bindings_error::COMPONENT_MISSING, RIGID_BODY);
    }
    return finish(ID, world()->teleport(body, marshal::to_transform(*target)));
}

int32_t conjure::bindings::entries::physics_is_sleeping(uint64_t entity) {
    constexpr native_function_id ID = native_function_id::physics_is_sleeping;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (world() == nullptr) {
        return unavailable(ID, "physics");
    }
    int32_t status = 0;
    const ecs::entity body = resolve_alive(ID, entity, status);
    if (body.is_null()) {
        return status;
    }
    if (!active_world()->has<physics::rigid_body>(body)) {
        return fail_entry(ID, bindings_error::COMPONENT_MISSING, RIGID_BODY);
    }
    return world()->is_sleeping(body) ? 1 : 0;
}

int32_t conjure::bindings::entries::physics_wake(uint64_t entity) {
    constexpr native_function_id ID = native_function_id::physics_wake;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (world() == nullptr) {
        return unavailable(ID, "physics");
    }
    int32_t status = 0;
    const ecs::entity body = resolve_alive(ID, entity, status);
    if (body.is_null()) {
        return status;
    }
    if (!active_world()->has<physics::rigid_body>(body)) {
        return fail_entry(ID, bindings_error::COMPONENT_MISSING, RIGID_BODY);
    }
    return finish(ID, world()->wake(body));
}

int32_t conjure::bindings::entries::physics_get_gravity(conjure_vec3 *out) {
    constexpr native_function_id ID = native_function_id::physics_get_gravity;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (out == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "out");
    }
    if (world() == nullptr) {
        return unavailable(ID, "physics");
    }
    *out = marshal::from_vec3(world()->gravity());
    return 0;
}

int32_t conjure::bindings::entries::physics_set_gravity(const conjure_vec3 *value) {
    constexpr native_function_id ID = native_function_id::physics_set_gravity;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (value == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "value");
    }
    if (world() == nullptr) {
        return unavailable(ID, "physics");
    }
    return finish(ID, world()->set_gravity(marshal::to_vec3(*value)));
}

int32_t conjure::bindings::entries::physics_layer_index(const char *utf8, int32_t length) {
    constexpr native_function_id ID = native_function_id::physics_layer_index;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    const core::result<std::string_view> name = marshal::read_utf8(utf8, length);
    if (!name.has_value()) {
        return fail_entry(ID, name.get_error());
    }
    if (world() == nullptr) {
        return unavailable(ID, "physics");
    }
    // from_hash: a lookup never interns the queried string (project.json interned the real layer names).
    const std::optional<u8> index = world()->layers().find(core::string_id::from_hash(core::hash::fnv1a64(*name)));
    if (!index.has_value()) {
        return fail_entry(ID, bindings_error::NOT_FOUND, *name);
    }
    return static_cast<int32_t>(*index);
}

int32_t conjure::bindings::entries::physics_layer_name(int32_t layer, char *utf8, int32_t capacity) {
    constexpr native_function_id ID = native_function_id::physics_layer_name;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (capacity < 0) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "capacity");
    }
    if (world() == nullptr) {
        return unavailable(ID, "physics");
    }
    const physics::physics_layer_table &layers = world()->layers();
    if (layer < 0 || static_cast<u32>(layer) >= layers.count()) {
        return fail_entry(ID, bindings_error::NOT_FOUND, "layer index");
    }
    const core::string_id name = layers.name(static_cast<u8>(layer));
    const std::string_view interned = name.view();
    if (!interned.empty()) {
        return marshal::write_utf8(interned, utf8, capacity);
    }
    // A name that was never interned prints as its hash; rare (every project.json layer is interned).
    return marshal::write_utf8(name.to_string(), utf8, capacity);
}

int32_t conjure::bindings::entries::physics_layers_collide(int32_t layerA, int32_t layerB) {
    constexpr native_function_id ID = native_function_id::physics_layers_collide;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (world() == nullptr) {
        return unavailable(ID, "physics");
    }
    const physics::physics_layer_table &layers = world()->layers();
    const bool known = layerA >= 0 && layerB >= 0 && static_cast<u32>(layerA) < layers.count()
      && static_cast<u32>(layerB) < layers.count();
    if (!known) {
        // NOT_FOUND like physics_layer_name: a layer is looked up, not validated as a shape (15_bindings.md 2.2).
        return fail_entry(ID, bindings_error::NOT_FOUND, "layer index");
    }
    return layers.collides(static_cast<u8>(layerA), static_cast<u8>(layerB)) ? 1 : 0;
}

int32_t conjure::bindings::entries::physics_character_move(uint64_t entity, const conjure_vec3 *velocity) {
    constexpr native_function_id ID = native_function_id::physics_character_move;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (velocity == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "velocity");
    }
    if (world() == nullptr) {
        return unavailable(ID, "physics");
    }
    int32_t status = 0;
    physics::character_controller *controller =
      component_for_write<physics::character_controller>(ID, entity, CHARACTER_CONTROLLER, status);
    if (controller == nullptr) {
        return status;
    }
    // The component velocity is written only once the move is queued: the step feeds it to Jolt whenever no
    // move is waiting, so a refused request (non-finite, queue full, not a character) must leave it alone.
    const math::vec3 desired = marshal::to_vec3(*velocity);
    const core::result<void> queued = world()->character_move(marshal::to_entity(entity), desired);
    if (!queued.has_value()) {
        return fail_entry(ID, queued.get_error());
    }
    controller->velocity = desired;
    return 0;
}

int32_t conjure::bindings::entries::physics_character_is_grounded(uint64_t entity) {
    constexpr native_function_id ID = native_function_id::physics_character_is_grounded;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (world() == nullptr) {
        return unavailable(ID, "physics");
    }
    int32_t status = 0;
    const physics::character_controller *controller =
      component_for_read<physics::character_controller>(ID, entity, CHARACTER_CONTROLLER, status);
    if (controller == nullptr) {
        return status;
    }
    const std::optional<physics::ground_state> ground = world()->character_ground(marshal::to_entity(entity));
    const physics::ground_state state = ground.has_value() ? *ground : controller->ground;
    return state == physics::ground_state::ON_GROUND ? 1 : 0;
}

int32_t conjure::bindings::entries::physics_character_velocity(uint64_t entity, conjure_vec3 *out) {
    constexpr native_function_id ID = native_function_id::physics_character_velocity;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (out == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "out");
    }
    int32_t status = 0;
    const physics::character_controller *controller =
      component_for_read<physics::character_controller>(ID, entity, CHARACTER_CONTROLLER, status);
    if (controller == nullptr) {
        return status;
    }
    *out = marshal::from_vec3(controller->velocity);
    return 0;
}
