#pragma once

#include "conjure/core/types.h"

#include <compare>
#include <functional>
#include <type_traits>

// Handles to GPU mirrors of assets and to render targets (docs/design/02_renderer.md 2.2). Each id
// is a distinct core::slot_map key type with the compact 32-bit index + 16-bit generation layout, so a
// slot retires after 65535 reuses. A stale id resolves to the fallback object (unit cube, white
// texture, magenta material, grey environment) and logs a WARN once.

namespace conjure::renderer {

    namespace detail {
        /// <summary>
        /// Common body of every gpu id: the slot index and the generation the id was issued with.
        /// Generation 0 is never issued, so a default id is invalid.
        /// </summary>
        template <typename Tag>
        struct gpu_id {
            u32 index = 0;
            u16 generation = 0;

            /// <summary>
            /// True for an id a renderer handed out (whether or not it is still live).
            /// </summary>
            [[nodiscard]] constexpr bool is_valid() const noexcept {
                return generation != 0;
            }

            /// <summary>
            /// Packs the id into one integer (generation in the high 32 bits). Used for ImTextureID
            /// style opaque handles and for hashing.
            /// </summary>
            [[nodiscard]] constexpr u64 to_u64() const noexcept {
                return (static_cast<u64>(generation) << 32) | static_cast<u64>(index);
            }

            /// <summary>
            /// Inverse of to_u64().
            /// </summary>
            [[nodiscard]] static constexpr gpu_id from_u64(u64 packed) noexcept {
                gpu_id result = {};
                result.index = static_cast<u32>(packed & 0xFFFFFFFFull);
                result.generation = static_cast<u16>(packed >> 32);
                return result;
            }

            [[nodiscard]] constexpr auto operator<=>(const gpu_id &) const noexcept = default;
            [[nodiscard]] constexpr bool operator==(const gpu_id &) const noexcept = default;
        };
    }

    struct gpu_mesh_tag {};
    struct gpu_texture_tag {};
    struct gpu_material_tag {};
    struct gpu_environment_tag {};
    struct render_target_tag {};

    using gpu_mesh_id = detail::gpu_id<gpu_mesh_tag>;
    using gpu_texture_id = detail::gpu_id<gpu_texture_tag>;
    using gpu_material_id = detail::gpu_id<gpu_material_tag>;
    using gpu_environment_id = detail::gpu_id<gpu_environment_tag>;
    using render_target_id = detail::gpu_id<render_target_tag>;

    static_assert(sizeof(gpu_mesh_id) == 8, "gpu ids pack into 64 bits");
    static_assert(std::is_trivially_copyable_v<gpu_mesh_id>, "gpu ids are copied by value");
}

template <typename Tag>
struct std::hash<conjure::renderer::detail::gpu_id<Tag>> {
    [[nodiscard]] std::size_t operator()(const conjure::renderer::detail::gpu_id<Tag> &id) const noexcept {
        return static_cast<std::size_t>(id.to_u64());
    }
};
