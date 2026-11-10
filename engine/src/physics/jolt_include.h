#pragma once

// The one place Jolt's umbrella header is included (docs/design/14_physics.md P1). Every Jolt header must
// come after Jolt/Jolt.h, so private physics headers include this file first and the specific Jolt
// headers they need after it. Public headers never include it (CONVENTIONS 2).

#include <Jolt/Jolt.h>

#include <Jolt/Core/Reference.h>

#include <utility>

namespace conjure::physics::jolt {

    /// <summary>
    /// Creates a Jolt reference-counted object. Jolt's RefTarget classes carry JPH_OVERRIDE_NEW_DELETE,
    /// which routes the allocation through the engine's tracked allocator (P3), and JPH::Ref adopts the
    /// pointer, so this is the one sanctioned `new` outside an allocator (CONVENTIONS 3, 14_physics.md
    /// P4): Jolt offers no factory for these objects.
    /// </summary>
    template <typename T, typename... Args>
    [[nodiscard]] JPH::Ref<T> make_jolt_ref(Args &&...args) {
        return JPH::Ref<T>(new T(std::forward<Args>(args)...));
    }
}
