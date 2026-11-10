#include "physics/shape_builder.h"

#include "physics/collision_mesh_internal.h"
#include "physics/jolt_conversions.h"

#include "conjure/physics/collision_mesh_asset.h"
#include "conjure/physics/log_category.h"

#include "conjure/core/hash.h"
#include "conjure/core/log.h"
#include "conjure/core/math/math_utils.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/PhysicsSettings.h>

#include <algorithm>
#include <array>
#include <bit>

namespace conjure::physics {

namespace {

    enum class leaf_kind : u8 {
        BOX = 1,
        SPHERE,
        CAPSULE,
        CONVEX,
        MESH
    };

    /// <summary>
    /// One collider turned into a leaf: the shape plus its placement inside the entity.
    /// </summary>
    struct leaf {
        JPH::RefConst<JPH::Shape> shape = {};
        math::vec3 offset = math::vec3(0.0f);
        math::quat rotation = math::quat(1.0f, 0.0f, 0.0f, 0.0f);
        u64 key = 0;
    };

    /// <summary>
    /// Accumulates the cache key of a leaf from its kind and dimensions (float bits, so equal values
    /// hash equal).
    /// </summary>
    class key_builder {
        private:
            u64 mHash = core::hash::FNV1A64_OFFSET_BASIS;

        public:
            void add(u8 value) noexcept {
                mHash = core::hash::fnv1a64_step(mHash, value);
            }

            void add(u32 value) noexcept {
                for (u32 shift = 0; shift < 32; shift += 8) {
                    add(static_cast<u8>((value >> shift) & 0xFFu));
                }
            }

            void add(u64 value) noexcept {
                add(static_cast<u32>(value & 0xFFFFFFFFu));
                add(static_cast<u32>(value >> 32));
            }

            void add(f32 value) noexcept {
                add(std::bit_cast<u32>(value));
            }

            void add(const math::vec3 &value) noexcept {
                add(value.x);
                add(value.y);
                add(value.z);
            }

            void add(const core::uuid &value) noexcept {
                add(value.high());
                add(value.low());
            }

            [[nodiscard]] u64 value() const noexcept {
                return mHash;
            }
    };

    [[nodiscard]] bool is_identity_placement(const math::vec3 &offset, const math::quat &rotation) noexcept {
        return math::nearly_zero(offset) && math::same_rotation(rotation, math::quat(1.0f, 0.0f, 0.0f, 0.0f));
    }

    [[nodiscard]] bool is_unit_scale(const math::vec3 &scale) noexcept {
        return math::nearly_equal(scale, math::vec3(1.0f), 1.0e-6f);
    }

    [[nodiscard]] f32 min_component(const math::vec3 &value) noexcept {
        return std::min(value.x, std::min(value.y, value.z));
    }

    /// <summary>
    /// Finds or creates a leaf shape through the cache.
    /// </summary>
    template <typename Factory>
    [[nodiscard]] JPH::RefConst<JPH::Shape> cached_leaf(
      shape_cache &cache,
      u64 key,
      ecs::entity entity,
      const char *what,
      bool &createFailed,
      Factory &&factory
    ) {
        JPH::RefConst<JPH::Shape> found = cache.find(key);
        if (found != nullptr) {
            return found;
        }
        const JPH::Shape::ShapeResult created = factory();
        if (created.HasError()) {
            createFailed = true;
            CONJURE_LOG_WARN(
              LOG_PHYSICS,
              "entity {}: {} shape failed (SHAPE_CREATE_FAILED): {}",
              entity,
              what,
              created.GetError().c_str()
            );
            return nullptr;
        }
        JPH::RefConst<JPH::Shape> shape(created.Get());
        cache.insert(key, shape);
        return shape;
    }

    void append_box(
      const shape_build_input &input,
      shape_cache &cache,
      std::array<leaf, 5> &leaves,
      usize &count,
      shape_build_result &out
    ) {
        const box_collider &box = *input.box;
        const math::vec3 halfExtents = glm::max(glm::abs(box.halfExtents), math::vec3(1.0e-4f));
        const f32 convexRadius = std::min(JPH::cDefaultConvexRadius, min_component(halfExtents) * 0.5f);
        key_builder key = {};
        key.add(static_cast<u8>(leaf_kind::BOX));
        key.add(halfExtents);
        key.add(convexRadius);
        leaf &entry = leaves[count];
        entry.shape = cached_leaf(cache, key.value(), input.entity, "box", out.createFailed, [&]() {
            return JPH::BoxShapeSettings(jolt::to_jolt(halfExtents), convexRadius).Create();
        });
        if (entry.shape == nullptr) {
            return;
        }
        entry.offset = box.offset;
        entry.rotation = box.rotation;
        entry.key = key.value();
        ++count;
    }

    void append_sphere(
      const shape_build_input &input,
      shape_cache &cache,
      std::array<leaf, 5> &leaves,
      usize &count,
      shape_build_result &out
    ) {
        const sphere_collider &sphere = *input.sphere;
        const f32 radius = std::max(std::abs(sphere.radius), 1.0e-4f);
        key_builder key = {};
        key.add(static_cast<u8>(leaf_kind::SPHERE));
        key.add(radius);
        leaf &entry = leaves[count];
        entry.shape = cached_leaf(cache, key.value(), input.entity, "sphere", out.createFailed, [&]() {
            return JPH::SphereShapeSettings(radius).Create();
        });
        if (entry.shape == nullptr) {
            return;
        }
        entry.offset = sphere.offset;
        entry.key = key.value();
        ++count;
    }

    void append_capsule(
      const shape_build_input &input,
      shape_cache &cache,
      std::array<leaf, 5> &leaves,
      usize &count,
      shape_build_result &out
    ) {
        const capsule_collider &capsule = *input.capsule;
        const f32 radius = std::max(std::abs(capsule.radius), 1.0e-4f);
        const f32 halfHeight = std::max(std::abs(capsule.halfHeight), 1.0e-4f);
        key_builder key = {};
        key.add(static_cast<u8>(leaf_kind::CAPSULE));
        key.add(radius);
        key.add(halfHeight);
        leaf &entry = leaves[count];
        entry.shape = cached_leaf(cache, key.value(), input.entity, "capsule", out.createFailed, [&]() {
            return JPH::CapsuleShapeSettings(halfHeight, radius).Create();
        });
        if (entry.shape == nullptr) {
            return;
        }
        entry.offset = capsule.offset;
        entry.rotation = capsule.rotation;
        entry.key = key.value();
        ++count;
    }

    void append_convex(
      const shape_build_input &input,
      shape_cache &cache,
      std::array<leaf, 5> &leaves,
      usize &count,
      shape_build_result &out
    ) {
        const convex_collider &convex = *input.convex;
        if (convex.mesh.id().is_null()) {
            return;
        }
        const collision_mesh_asset *mesh = convex.mesh.get();
        if (mesh == nullptr) {
            out.pendingAsset = true;
            return;
        }
        out.convexAsset = convex.mesh.id();
        const f32 convexRadius = std::max(convex.convexRadius, 0.0f);
        key_builder key = {};
        key.add(static_cast<u8>(leaf_kind::CONVEX));
        key.add(convex.mesh.id());
        key.add(convex.mesh.version());
        key.add(convexRadius);
        leaf &entry = leaves[count];
        JPH::RefConst<JPH::Shape> found = cache.find(key.value());
        if (found == nullptr) {
            core::result<JPH::RefConst<JPH::Shape>> built = detail::build_convex_shape(*mesh, convexRadius);
            if (!built.has_value()) {
                out.createFailed = true;
                CONJURE_LOG_WARN(LOG_PHYSICS, "entity {}: {}", input.entity, built.get_error().to_string());
                return;
            }
            found = std::move(built).value();
            cache.insert(key.value(), found);
        }
        entry.shape = found;
        entry.offset = convex.offset;
        entry.rotation = convex.rotation;
        entry.key = key.value();
        ++count;
    }

    void append_mesh(
      const shape_build_input &input,
      std::array<leaf, 5> &leaves,
      usize &count,
      shape_build_result &out
    ) {
        const mesh_collider &meshCollider = *input.mesh;
        if (meshCollider.mesh.id().is_null()) {
            return;
        }
        if (input.motion == motion_type::DYNAMIC) {
            out.unsupportedShape = true;
            CONJURE_LOG_WARN(
              LOG_PHYSICS,
              "entity {}: mesh_collider on a DYNAMIC body is ignored (UNSUPPORTED_SHAPE_FOR_MOTION)",
              input.entity
            );
            return;
        }
        const collision_mesh_asset *mesh = meshCollider.mesh.get();
        if (mesh == nullptr || mesh->shapes() == nullptr || mesh->shapes()->meshShape == nullptr) {
            out.pendingAsset = true;
            return;
        }
        out.meshAsset = meshCollider.mesh.id();
        leaf &entry = leaves[count];
        entry.shape = mesh->shapes()->meshShape;
        key_builder key = {};
        key.add(static_cast<u8>(leaf_kind::MESH));
        key.add(meshCollider.mesh.id());
        key.add(meshCollider.mesh.version());
        entry.key = key.value();
        ++count;
    }

    /// <summary>
    /// True when the collider is present and marked as a trigger.
    /// </summary>
    void gather_trigger_flags(const shape_build_input &input, u32 &triggers, u32 &colliders) noexcept {
        const auto count = [&](bool present, bool trigger) {
            if (!present) {
                return;
            }
            ++colliders;
            if (trigger) {
                ++triggers;
            }
        };
        count(input.box != nullptr, input.box != nullptr && input.box->isTrigger);
        count(input.sphere != nullptr, input.sphere != nullptr && input.sphere->isTrigger);
        count(input.capsule != nullptr, input.capsule != nullptr && input.capsule->isTrigger);
        count(input.convex != nullptr, input.convex != nullptr && input.convex->isTrigger);
        count(input.mesh != nullptr, input.mesh != nullptr && input.mesh->isTrigger);
    }
}

shape_build_result build_shape(const shape_build_input &input, shape_cache &cache) {
    shape_build_result out = {};
    u32 triggers = 0;
    u32 colliders = 0;
    gather_trigger_flags(input, triggers, colliders);
    out.hasCollider = colliders > 0;
    if (colliders == 0) {
        return out;
    }
    if (triggers == colliders) {
        out.isTrigger = true;
    } else if (triggers > 0) {
        out.mixedTriggers = true;
        CONJURE_LOG_WARN(
          LOG_PHYSICS,
          "entity {}: colliders disagree on isTrigger (MIXED_TRIGGER_FLAGS); the body is not a trigger",
          input.entity
        );
    }

    std::array<leaf, 5> leaves = {};
    usize leafCount = 0;
    if (input.box != nullptr) {
        append_box(input, cache, leaves, leafCount, out);
    }
    if (input.sphere != nullptr) {
        append_sphere(input, cache, leaves, leafCount, out);
    }
    if (input.capsule != nullptr) {
        append_capsule(input, cache, leaves, leafCount, out);
    }
    if (input.convex != nullptr) {
        append_convex(input, cache, leaves, leafCount, out);
    }
    if (input.mesh != nullptr) {
        append_mesh(input, leaves, leafCount, out);
    }
    if (leafCount == 0) {
        return out;
    }

    JPH::RefConst<JPH::Shape> shape = {};
    if (leafCount == 1 && is_identity_placement(leaves[0].offset, leaves[0].rotation)) {
        shape = leaves[0].shape;
        out.leafKey = leaves[0].key;
    } else {
        JPH::StaticCompoundShapeSettings compound = {};
        for (usize index = 0; index < leafCount; ++index) {
            compound.AddShape(
              jolt::to_jolt(leaves[index].offset),
              jolt::to_jolt(leaves[index].rotation),
              leaves[index].shape
            );
        }
        const JPH::Shape::ShapeResult created = compound.Create();
        if (created.HasError()) {
            out.createFailed = true;
            CONJURE_LOG_WARN(
              LOG_PHYSICS,
              "entity {}: compound shape failed (SHAPE_CREATE_FAILED): {}",
              input.entity,
              created.GetError().c_str()
            );
            return out;
        }
        shape = created.Get();
    }

    if (!is_unit_scale(input.scale)) {
        const JPH::Vec3 scale = jolt::to_jolt(input.scale);
        if (!shape->IsValidScale(scale)) {
            out.invalidScale = true;
            CONJURE_LOG_WARN(
              LOG_PHYSICS,
              "entity {}: scale {} cannot be applied to its shape (INVALID_SHAPE_SCALE); built unscaled",
              input.entity,
              input.scale
            );
        } else {
            const JPH::Shape::ShapeResult scaled = JPH::ScaledShapeSettings(shape, scale).Create();
            if (scaled.HasError()) {
                out.invalidScale = true;
                CONJURE_LOG_WARN(
                  LOG_PHYSICS,
                  "entity {}: scaled shape failed (INVALID_SHAPE_SCALE): {}; built unscaled",
                  input.entity,
                  scaled.GetError().c_str()
                );
            } else {
                shape = scaled.Get();
                out.leafKey = 0;
            }
        }
    }
    out.shape = shape;
    return out;
}
}
