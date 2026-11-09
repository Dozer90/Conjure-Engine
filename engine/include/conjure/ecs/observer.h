#pragma once

#include "conjure/ecs/component.h"
#include "conjure/ecs/entity.h"

#include "conjure/core/types.h"

#include <functional>

// Observers (docs/design/01_ecs.md 4.6). Observers run synchronously on the owner thread inside the
// structural operation that triggered them and may only enqueue follow-up work into the deferred
// command buffer they receive; the world drains that buffer once the operation has finished.

namespace conjure::ecs {

    class world;
    class command_buffer;

    /// <summary>
    /// Component lifecycle events observers can subscribe to. ADDED fires after the component data
    /// exists, REMOVED before it is destroyed.
    /// </summary>
    enum class component_event : u8 {
        ADDED,
        REMOVED
    };

    /// <summary>
    /// Callback for a component event on one entity.
    /// </summary>
    using component_observer_fn =
      std::function<void(world &owner, entity target, component_id id, command_buffer &deferred)>;

    /// <summary>
    /// Callback invoked when an entity is about to be destroyed (its components still exist).
    /// </summary>
    using entity_observer_fn = std::function<void(world &owner, entity target, command_buffer &deferred)>;

    /// <summary>
    /// Handle of a registered observer. A default-constructed id refers to nothing.
    /// </summary>
    struct observer_id {
        u32 index = 0;
        u32 generation = 0;

        /// <summary>
        /// True for a handle returned by observe()/observe_destroyed() (generation 0 is never issued).
        /// </summary>
        [[nodiscard]] constexpr bool is_valid() const noexcept {
            return generation != 0;
        }

        [[nodiscard]] constexpr bool operator==(const observer_id &other) const noexcept = default;
    };
}
