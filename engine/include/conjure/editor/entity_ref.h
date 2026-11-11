#pragma once

#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/entity.h"
#include "conjure/scene/scene.h"

#include <string>
#include <vector>

// Entity addressing (docs/design/04_editor.md D5, 3.5): a protocol argument names an entity by guid (a uuid
// string) or by hierarchy path ("Root/Child/Leaf"); ecs::entity ids never cross the protocol. Responses
// always carry guids.

namespace conjure::editor {

    class CONJURE_API entity_ref {
        public:
            /// <summary>
            /// Resolves a reference: a uuid string is a guid (ENTITY_NOT_FOUND when no live entity carries
            /// it); any other string is a path resolved from the roots by name (ENTITY_NOT_FOUND when a
            /// segment does not match, INVALID_ARGS with details.candidates when a segment matches several
            /// siblings); a non-string is INVALID_ARGS. 'details' receives the candidate guids when given.
            /// </summary>
            [[nodiscard]] static core::result<ecs::entity> resolve(
              const core::json::value &reference,
              const scene::scene &target,
              core::json::document *details = nullptr
            );

            /// <summary>
            /// Resolves every element of an array of references, in order. INVALID_ARGS for a non-array.
            /// </summary>
            [[nodiscard]] static core::result<std::vector<ecs::entity>> resolve_many(
              const core::json::value &references,
              const scene::scene &target,
              core::json::document *details = nullptr
            );

            /// <summary>
            /// The guid of a live entity, generated when missing. ENTITY_NOT_FOUND for a dead entity.
            /// </summary>
            [[nodiscard]] static core::result<core::uuid> guid_of(scene::scene &target, ecs::entity entity);

            /// <summary>
            /// The canonical string form of a guid.
            /// </summary>
            [[nodiscard]] static std::string to_json(const core::uuid &guid);

            /// <summary>
            /// The hierarchy path of an entity ("Root/Child"); unnamed entities print as "#index".
            /// </summary>
            [[nodiscard]] static std::string path_of(const scene::scene &target, ecs::entity entity);

            /// <summary>
            /// True when the text parses as a uuid.
            /// </summary>
            [[nodiscard]] static bool looks_like_guid(std::string_view text) noexcept;
    };
}
