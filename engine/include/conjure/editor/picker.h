#pragma once

#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/entity.h"
#include "conjure/scene/scene.h"

#include <optional>
#include <span>
#include <string_view>
#include <vector>

// CPU picking (docs/design/04_editor.md D10, 3.8): a slab test over every candidate's world bounds sorted by
// entry distance, then, for mode MESH, the triangles of the mesh asset's CPU geometry (sprites test their
// quad) until a hit is closer than the next candidate's entry distance. Ties between bounds-only hits go to
// the smaller volume so a child inside its parent stays selectable. Mode PHYSICS delegates to Jolt.

namespace conjure::core::jobs {
    class job_system;
}

namespace conjure::physics {
    class physics_world;
}

namespace conjure::editor {

    enum class pick_mode : u8 {
        MESH = 0,
        BOUNDS,
        PHYSICS
    };

    /// <summary>
    /// The wire name of a mode ("mesh").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(pick_mode mode) noexcept;

    /// <summary>
    /// Parses a wire name; nullopt for anything else.
    /// </summary>
    [[nodiscard]] CONJURE_API std::optional<pick_mode> parse_pick_mode(std::string_view text) noexcept;

    struct pick_candidate {
        core::uuid guid = {};
        math::aabb worldBounds = {};
        ecs::entity entity = {};
    };

    struct CONJURE_API pick_result {
        core::uuid guid = {};
        math::vec3 point = math::vec3(0.0f);
        math::vec3 normal = math::vec3(0.0f, 1.0f, 0.0f);
        f32 distance = 0.0f;
        bool hit = false;

        /// <summary>
        /// {hit, guid, point, normal, distance} as view.pick returns it.
        /// </summary>
        [[nodiscard]] core::json::document to_json() const;
    };

    /// <summary>
    /// What the narrow phase needs: the scene (components and transforms) and, for mode PHYSICS, the physics
    /// world. Jobs parallelise the broad phase above 4096 candidates when given.
    /// </summary>
    struct pick_context {
        const scene::scene *scene = nullptr;
        physics::physics_world *physics = nullptr;
        core::jobs::job_system *jobs = nullptr;
        f32 maxDistance = 1.0e6f;       // bounds both the bounds entry and the narrow-phase (triangle) hit
    };

    /// <summary>
    /// Every active entity (scene::is_active: it and every ancestor enabled) with renderable or framing bounds:
    /// mesh renderers and sprites use scene::world_bounds (their local_bounds), lights and cameras a fixed 0.5 m
    /// box around their position. Entities without a transform are skipped.
    /// </summary>
    [[nodiscard]] CONJURE_API std::vector<pick_candidate> gather_pick_candidates(const scene::scene &scene);

    /// <summary>
    /// Picks among the candidates (see the header comment). With mode PHYSICS the candidates are ignored and
    /// context.physics is queried (a miss when it is null).
    /// </summary>
    [[nodiscard]] CONJURE_API pick_result pick(
      const math::ray &ray,
      std::span<const pick_candidate> candidates,
      pick_mode mode,
      const pick_context &context = {}
    );
}
