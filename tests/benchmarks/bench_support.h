#pragma once

// Helpers shared by the scenario files of conjure_benchmarks: the build-time paths (shader index, managed
// SDK, test scripts), the .NET availability probe, a scratch directory without doctest, a deterministic
// transform generator and the 10,000-entity hierarchy every scene and engine scenario builds.

#include "conjure/core/random.h"
#include "conjure/core/types.h"
#include "conjure/ecs/entity.h"
#include "conjure/scene/components.h"
#include "conjure/scene/scene.h"

#include <filesystem>
#include <string_view>
#include <vector>

namespace conjure::bench::support {

    /// <summary>
    /// Where conjure_shaders wrote index.json (CONJURE_BENCH_SHADER_DIR); empty when the renderer is off.
    /// </summary>
    [[nodiscard]] std::filesystem::path shader_directory();

    /// <summary>
    /// The directory holding Conjure.Bootstrap.* and Conjure.Managed.dll; empty when the SDK was not built.
    /// </summary>
    [[nodiscard]] std::filesystem::path managed_directory();

    /// <summary>
    /// Conjure.TestScripts.dll of the v1 variant; empty when the SDK was not built.
    /// </summary>
    [[nodiscard]] std::filesystem::path test_scripts_assembly();

    /// <summary>
    /// True when the scripting module is compiled in, the managed SDK was built with this tree, its
    /// outputs exist on disk and nethost finds a .NET runtime. The probe never starts the CLR. The reason
    /// the probe failed is written into 'reason' (empty when it holds).
    /// </summary>
    [[nodiscard]] bool dotnet_available(std::string &reason);

    /// <summary>
    /// A unique directory under the system temporary path, removed recursively on destruction. The
    /// benchmark executable has no doctest main, so conjure::testing::temp_directory (whose destructor
    /// reports through doctest) is not used here.
    /// </summary>
    class scratch_directory {
        private:
            std::filesystem::path mPath = {};

        public:
            scratch_directory();
            ~scratch_directory();

            scratch_directory(const scratch_directory &) = delete;
            scratch_directory &operator=(const scratch_directory &) = delete;

        public:
            [[nodiscard]] const std::filesystem::path &path() const noexcept {
                return mPath;
            }

            /// <summary>
            /// Writes a text file inside the directory (parent directories created). False on failure.
            /// </summary>
            [[nodiscard]] bool write_text(std::string_view relativePath, std::string_view text) const;
    };

    /// <summary>
    /// A deterministic TRS: position in [-5, 5]^3, a rotation about a random axis, a uniform scale in
    /// [0.5, 2].
    /// </summary>
    [[nodiscard]] scene::local_transform random_transform(core::random::engine &engine);

    /// <summary>
    /// The shape of a benchmark hierarchy: rootCount roots, each with a complete childrenPerNode-ary
    /// subtree of 'depth' levels below it; the total is padded with extra leaves under the deepest
    /// level (or trimmed) to totalEntityCount (08_testing.md 2.3 hierarchy_shape).
    /// </summary>
    struct hierarchy_shape {
        u32 rootCount = 100;
        u32 depth = 3;
        u32 childrenPerNode = 4;
        u32 totalEntityCount = 10000;
    };

    /// <summary>
    /// What spawn_hierarchy built.
    /// </summary>
    struct bench_hierarchy {
        std::vector<ecs::entity> roots = {};
        std::vector<ecs::entity> all = {};          // creation order (roots first, then level by level)
    };

    /// <summary>
    /// Builds the hierarchy into a scene, every entity with a random local transform. Fails (an empty
    /// result) only when the scene refuses an entity.
    /// </summary>
    [[nodiscard]] bench_hierarchy spawn_hierarchy(scene::scene &target, const hierarchy_shape &shape, u64 seed);
}
