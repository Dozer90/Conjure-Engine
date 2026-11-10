#pragma once

#include "conjure/core/types.h"

// Process-wide Jolt state, reference counted (docs/design/14_physics.md P4, 3.1): the first acquire
// installs the memory hooks, JPH::Trace and JPH::AssertFailed, creates JPH::Factory::sInstance and calls
// JPH::RegisterTypes(); the last release runs UnregisterTypes() and deletes the factory so memory_guard
// stays clean across tests. Guarded by a mutex.

namespace conjure::physics::jolt_runtime {

    /// <summary>
    /// Adds one reference; initialises Jolt on the first.
    /// </summary>
    void acquire();

    /// <summary>
    /// Drops one reference; tears Jolt down on the last. Unbalanced calls assert and are ignored.
    /// </summary>
    void release();

    /// <summary>
    /// Live references.
    /// </summary>
    [[nodiscard]] u32 reference_count() noexcept;

    /// <summary>
    /// RAII reference.
    /// </summary>
    class guard {
        public:
            guard();
            ~guard();

            guard(const guard &) = delete;
            guard &operator=(const guard &) = delete;
    };
}
