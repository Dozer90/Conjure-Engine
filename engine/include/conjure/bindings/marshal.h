#pragma once

#include "conjure/bindings/bindings_error.h"

#include "conjure/core/api.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/math/transform.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/world.h"
#include "conjure/scripting/native_api.h"
#include "conjure/scripting/script_runtime.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <type_traits>

// Marshalling rules of the C ABI (docs/design/15_bindings.md 2.1, B5): everything crosses blittable or as
// (pointer, length). Math records are memcpy-compatible with the core types (the static_asserts below pin
// that, including the x, y, z, w storage of math::quat, R58); entities travel as the packed u64 of
// ecs::entity; uuids as two u64 halves; strings in as UTF-8 pointer + byte length (never terminated);
// strings and arrays out follow the needed-length / count protocol.

namespace conjure::bindings::marshal {

    /*---------------------------------------------------------------*/
    // Layout pins

    static_assert(sizeof(conjure_vec2) == sizeof(math::vec2), "conjure_vec2 mirrors math::vec2");
    static_assert(sizeof(conjure_vec3) == sizeof(math::vec3), "conjure_vec3 mirrors math::vec3");
    static_assert(sizeof(conjure_vec4) == sizeof(math::vec4), "conjure_vec4 mirrors math::vec4");
    static_assert(sizeof(conjure_quat_xyzw) == sizeof(math::quat), "conjure_quat_xyzw mirrors math::quat");
    static_assert(sizeof(conjure_mat4) == sizeof(math::mat4), "conjure_mat4 mirrors math::mat4");
    static_assert(sizeof(conjure_transform) == sizeof(math::transform), "conjure_transform mirrors math::transform");
    static_assert(sizeof(conjure_uuid) == sizeof(core::uuid), "conjure_uuid mirrors core::uuid");
    static_assert(offsetof(conjure_transform, rotation) == offsetof(math::transform, rotation));
    static_assert(offsetof(conjure_transform, scale) == offsetof(math::transform, scale));
    static_assert(std::is_trivially_copyable_v<math::transform>, "math::transform is memcpy-marshalled");

    /// <summary>
    /// True when math::quat stores its components x, y, z, w in memory (glm 1.0.1 without
    /// GLM_FORCE_QUAT_DATA_WXYZ, R58), which is what makes to_transform / from_transform a memcpy. glm's
    /// quaternion is a union of anonymous structs on MSVC, so the check is a runtime byte comparison
    /// rather than an offsetof.
    /// </summary>
    [[nodiscard]] CONJURE_API bool quat_storage_is_xyzw() noexcept;

    /*---------------------------------------------------------------*/
    // Entities and uuids

    [[nodiscard]] constexpr ecs::entity to_entity(uint64_t packed) noexcept {
        return ecs::entity::from_u64(packed);
    }

    [[nodiscard]] constexpr uint64_t from_entity(ecs::entity value) noexcept {
        return value.to_u64();
    }

    [[nodiscard]] constexpr core::uuid to_uuid(const conjure_uuid &value) noexcept {
        return core::uuid(value.high, value.low);
    }

    [[nodiscard]] constexpr conjure_uuid from_uuid(const core::uuid &value) noexcept {
        return conjure_uuid{value.high(), value.low()};
    }

    /*---------------------------------------------------------------*/
    // Math (memcpy after the pins above)

    [[nodiscard]] inline math::vec2 to_vec2(const conjure_vec2 &value) noexcept {
        return math::vec2(value.x, value.y);
    }

    [[nodiscard]] inline conjure_vec2 from_vec2(const math::vec2 &value) noexcept {
        return conjure_vec2{value.x, value.y};
    }

    [[nodiscard]] inline math::vec3 to_vec3(const conjure_vec3 &value) noexcept {
        return math::vec3(value.x, value.y, value.z);
    }

    [[nodiscard]] inline conjure_vec3 from_vec3(const math::vec3 &value) noexcept {
        return conjure_vec3{value.x, value.y, value.z};
    }

    [[nodiscard]] inline math::vec4 to_vec4(const conjure_vec4 &value) noexcept {
        return math::vec4(value.x, value.y, value.z, value.w);
    }

    [[nodiscard]] inline conjure_vec4 from_vec4(const math::vec4 &value) noexcept {
        return conjure_vec4{value.x, value.y, value.z, value.w};
    }

    /// <summary>
    /// By-name conversion: math::quat is constructed (w, x, y, z) but stored x, y, z, w (R58).
    /// </summary>
    [[nodiscard]] inline math::quat to_quat(const conjure_quat_xyzw &value) noexcept {
        return math::quat(value.w, value.x, value.y, value.z);
    }

    [[nodiscard]] inline conjure_quat_xyzw from_quat(const math::quat &value) noexcept {
        return conjure_quat_xyzw{value.x, value.y, value.z, value.w};
    }

    [[nodiscard]] inline math::mat4 to_mat4(const conjure_mat4 &value) noexcept {
        math::mat4 result = math::mat4(1.0f);
        std::memcpy(&result, value.m, sizeof(result));
        return result;
    }

    [[nodiscard]] inline conjure_mat4 from_mat4(const math::mat4 &value) noexcept {
        conjure_mat4 result = {};
        std::memcpy(result.m, &value, sizeof(result.m));
        return result;
    }

    [[nodiscard]] inline math::transform to_transform(const conjure_transform &value) noexcept {
        math::transform result = {};
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    [[nodiscard]] inline conjure_transform from_transform(const math::transform &value) noexcept {
        conjure_transform result = {};
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    /*---------------------------------------------------------------*/
    // Strings, arrays and colours

    /// <summary>
    /// A UTF-8 (pointer, byte length) argument as a view. INVALID_ARGUMENT (bindings category) for a null
    /// pointer with a positive length or a negative length; (nullptr, 0) and (anything, 0) are "".
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<std::string_view> read_utf8(const char *utf8, int32_t length) noexcept;

    /// <summary>
    /// The needed-length protocol of string outputs: writes min(text.size(), capacity) bytes into 'utf8'
    /// (no terminator) and returns the full length; capacity 0 (or a null buffer) is a length query, a
    /// negative capacity writes nothing.
    /// </summary>
    [[nodiscard]] CONJURE_API int32_t write_utf8(std::string_view text, char *utf8, int32_t capacity) noexcept;

    /// <summary>
    /// The count protocol of entity arrays: writes min(entities.size(), capacity) packed ids and returns
    /// the full count; capacity 0 (or a null buffer) is a count query.
    /// </summary>
    [[nodiscard]] CONJURE_API int32_t write_entities(
      std::span<const ecs::entity> entities,
      uint64_t *out,
      int32_t capacity
    ) noexcept;

    /// <summary>
    /// 0xRRGGBBAA to a linear float colour with channels in [0, 1].
    /// </summary>
    [[nodiscard]] constexpr math::vec4 rgba_to_color(uint32_t rgba) noexcept {
        return math::vec4(
          static_cast<f32>((rgba >> 24) & 0xFFu) / 255.0f,
          static_cast<f32>((rgba >> 16) & 0xFFu) / 255.0f,
          static_cast<f32>((rgba >> 8) & 0xFFu) / 255.0f,
          static_cast<f32>(rgba & 0xFFu) / 255.0f
        );
    }

    /*---------------------------------------------------------------*/
    // Structural routing (R5, 15_bindings.md 3.3)

    /// <summary>
    /// Decides where a structural request goes: straight to the world while it is unlocked, into
    /// script_runtime::deferred_commands() while a DOD query scope holds the lock. Constructed per
    /// structural entry; 'runtime' may be null (then a locked request has no buffer and reports
    /// STRUCTURE_LOCKED through can_defer()).
    /// </summary>
    struct structural_guard {
        ecs::world *world = nullptr;
        scripting::script_runtime *runtime = nullptr;

        /// <summary>
        /// True while the world is locked and the request must be recorded instead of applied.
        /// </summary>
        [[nodiscard]] bool deferred() const noexcept {
            return world != nullptr && world->is_locked();
        }

        /// <summary>
        /// True when a deferred request has a buffer to go to (a runtime with the world attached).
        /// </summary>
        [[nodiscard]] bool can_defer() const noexcept {
            return runtime != nullptr && runtime->world() == world;
        }

        /// <summary>
        /// The buffer deferred requests are recorded into; only valid when can_defer().
        /// </summary>
        [[nodiscard]] ecs::command_buffer &commands() noexcept {
            return runtime->deferred_commands();
        }
    };
}
