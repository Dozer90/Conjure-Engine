#pragma once

#include "conjure/core/api.h"
#include "conjure/core/containers/small_vector.h"
#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#include <array>
#include <optional>

// Physics layers (docs/design/14_physics.md 2.2, P10): up to 32 named layers with a symmetric collision
// matrix, authored in project.json under extensions.physics_layers. Layer indices are positions in the
// "names" array; a physics_layer_mask is a u32 bit set over those indices (bit i = layer i).
//
//   "extensions": { "physics_layers": {
//       "names": ["default", "player", "enemy", "projectile", "trigger"],
//       "ignore": [["projectile", "projectile"], ["player", "player"], ["trigger", "trigger"]]
//   } }

namespace conjure::physics {

    /// <summary>
    /// Most layers a table may hold; the mask is a u32.
    /// </summary>
    inline constexpr u32 MAX_PHYSICS_LAYERS = 32;

    /// <summary>
    /// A set of layers as one bit per layer index. The default mask includes every layer.
    /// </summary>
    struct physics_layer_mask {
        u32 bits = 0xFFFFFFFFu;

        /// <summary>
        /// Every layer.
        /// </summary>
        [[nodiscard]] static constexpr physics_layer_mask all() noexcept {
            return physics_layer_mask{0xFFFFFFFFu};
        }

        /// <summary>
        /// No layer.
        /// </summary>
        [[nodiscard]] static constexpr physics_layer_mask none() noexcept {
            return physics_layer_mask{0u};
        }

        /// <summary>
        /// Exactly one layer (indices of 32 or more yield the empty mask).
        /// </summary>
        [[nodiscard]] static constexpr physics_layer_mask only(u8 layer) noexcept {
            if (layer >= MAX_PHYSICS_LAYERS) {
                return none();
            }
            return physics_layer_mask{u32{1} << layer};
        }

        /// <summary>
        /// True when the layer's bit is set (false for indices of 32 or more).
        /// </summary>
        [[nodiscard]] constexpr bool includes(u8 layer) const noexcept {
            if (layer >= MAX_PHYSICS_LAYERS) {
                return false;
            }
            return (bits & (u32{1} << layer)) != 0;
        }

        /// <summary>
        /// This mask with the layer added.
        /// </summary>
        [[nodiscard]] constexpr physics_layer_mask with(u8 layer) const noexcept {
            return physics_layer_mask{bits | only(layer).bits};
        }

        /// <summary>
        /// This mask with the layer removed.
        /// </summary>
        [[nodiscard]] constexpr physics_layer_mask without(u8 layer) const noexcept {
            return physics_layer_mask{bits & ~only(layer).bits};
        }

        [[nodiscard]] constexpr bool operator==(const physics_layer_mask &) const noexcept = default;
    };

    /// <summary>
    /// The named layers and their symmetric collision matrix. A value type; immutable once a
    /// physics_world was constructed from it (the world keeps its own copy).
    /// </summary>
    class CONJURE_API physics_layer_table {
        private:
            core::small_vector<core::string_id, 8> mNames = {};
            std::array<u32, MAX_PHYSICS_LAYERS> mCollides = {};

        public:
            /// <summary>
            /// One layer "default" that collides with itself.
            /// </summary>
            physics_layer_table();
            ~physics_layer_table();

            physics_layer_table(const physics_layer_table &) = default;
            physics_layer_table &operator=(const physics_layer_table &) = default;
            physics_layer_table(physics_layer_table &&) noexcept = default;
            physics_layer_table &operator=(physics_layer_table &&) noexcept = default;

        public:
            /// <summary>
            /// Builds a table from the project.json section: "names" (required, 1..32 unique strings) and
            /// "ignore" (optional pairs of names that do not collide). LAYER_CONFIG_INVALID,
            /// LAYER_LIMIT_REACHED, DUPLICATE_LAYER, UNKNOWN_LAYER. Everything collides unless ignored.
            /// </summary>
            [[nodiscard]] static core::result<physics_layer_table> from_json(const core::json::value &section);

            /// <summary>
            /// Appends a layer that collides with every existing layer and itself; returns its index.
            /// LAYER_LIMIT_REACHED, DUPLICATE_LAYER (also for the null id).
            /// </summary>
            [[nodiscard]] core::result<u8> add_layer(core::string_id name);

            /// <summary>
            /// Sets whether two layers collide (symmetric). Indices outside the table are ignored.
            /// </summary>
            void set_collides(u8 a, u8 b, bool collides) noexcept;

            /// <summary>
            /// True when the two layers collide; false for indices outside the table.
            /// </summary>
            [[nodiscard]] bool collides(u8 a, u8 b) const noexcept;

            /// <summary>
            /// The layers 'layer' collides with (the empty mask for an index outside the table).
            /// </summary>
            [[nodiscard]] physics_layer_mask collision_mask(u8 layer) const noexcept;

            /// <summary>
            /// The index of the layer with this name, or nullopt.
            /// </summary>
            [[nodiscard]] std::optional<u8> find(core::string_id name) const noexcept;

            /// <summary>
            /// The name of a layer (the null id for an index outside the table).
            /// </summary>
            [[nodiscard]] core::string_id name(u8 layer) const noexcept;

            /// <summary>
            /// Number of layers (at least 1).
            /// </summary>
            [[nodiscard]] u32 count() const noexcept;

            /// <summary>
            /// LAYER_CONFIG_INVALID when the table is empty or the matrix is not symmetric, DUPLICATE_LAYER
            /// when two layers share a name.
            /// </summary>
            [[nodiscard]] core::result<void> validate() const;

            /// <summary>
            /// Writes {"ignore": [[a, b], ...], "names": [...]} with sorted keys and the ignored pairs in
            /// ascending (a, b) index order, so the text equals the canonical form of its parse.
            /// Fails with LAYER_CONFIG_INVALID when a layer name was never interned (it cannot be spelt).
            /// </summary>
            [[nodiscard]] core::result<void> to_json(core::json::writer &out) const;

            [[nodiscard]] bool operator==(const physics_layer_table &other) const noexcept;
    };
}
