#include "bench_support.h"

#include "conjure/core/math.h"
#include "conjure/core/string_id.h"
#include "conjure/core/uuid.h"

#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    #include "conjure/scripting/script_host.h"
#endif

#include <fstream>
#include <string>
#include <system_error>
#include <utility>

#ifndef CONJURE_BENCH_SHADER_DIR
    #define CONJURE_BENCH_SHADER_DIR ""
#endif
#ifndef CONJURE_BENCH_HAS_DOTNET
    #define CONJURE_BENCH_HAS_DOTNET 0
#endif
#ifndef CONJURE_BENCH_MANAGED_DIR
    #define CONJURE_BENCH_MANAGED_DIR ""
#endif
#ifndef CONJURE_BENCH_TEST_SCRIPTS_DIR
    #define CONJURE_BENCH_TEST_SCRIPTS_DIR ""
#endif

using namespace conjure;
using namespace conjure::bench::support;

/*===========================================================================*/
/*===========================================================================*/
// Paths and probes

std::filesystem::path conjure::bench::support::shader_directory() {
    return std::filesystem::path(CONJURE_BENCH_SHADER_DIR);
}

std::filesystem::path conjure::bench::support::managed_directory() {
    return std::filesystem::path(CONJURE_BENCH_MANAGED_DIR);
}

std::filesystem::path conjure::bench::support::test_scripts_assembly() {
    const std::filesystem::path root(CONJURE_BENCH_TEST_SCRIPTS_DIR);
    if (root.empty()) {
        return {};
    }
    return root / "v1" / "Conjure.TestScripts.dll";
}

bool conjure::bench::support::dotnet_available(std::string &reason) {
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    if (CONJURE_BENCH_HAS_DOTNET == 0) {
        reason = "the managed SDK was not built with this tree (CONJURE_BUILD_MANAGED or the .NET SDK is off)";
        return false;
    }
    std::error_code probe = {};
    if (!std::filesystem::is_regular_file(managed_directory() / "Conjure.Managed.dll", probe)) {
        reason = "Conjure.Managed.dll is missing from " + managed_directory().string();
        return false;
    }
    if (!std::filesystem::is_regular_file(test_scripts_assembly(), probe)) {
        reason = "Conjure.TestScripts.dll (v1) is missing from " + test_scripts_assembly().parent_path().string();
        return false;
    }
    if (!scripting::is_runtime_available()) {
        reason = "no .NET runtime was found (hostfxr)";
        return false;
    }
    reason.clear();
    return true;
#else
    reason = "the scripting module is compiled out (CONJURE_WITH_SCRIPTING=OFF)";
    return false;
#endif
}


/*===========================================================================*/
/*===========================================================================*/
// scratch_directory

scratch_directory::scratch_directory() {
    std::error_code failure = {};
    const std::filesystem::path root = std::filesystem::temp_directory_path(failure);
    mPath = root / ("conjure_bench_" + core::uuid::generate().to_string());
    std::filesystem::create_directories(mPath, failure);
}

scratch_directory::~scratch_directory() {
    std::error_code ignored = {};
    std::filesystem::remove_all(mPath, ignored);
}

bool scratch_directory::write_text(std::string_view relativePath, std::string_view text) const {
    const std::filesystem::path target = mPath / std::filesystem::path(relativePath);
    std::error_code failure = {};
    std::filesystem::create_directories(target.parent_path(), failure);
    std::ofstream stream(target, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        return false;
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    return stream.good();
}


/*===========================================================================*/
/*===========================================================================*/
// Layout helpers

scene::local_transform conjure::bench::support::random_transform(core::random::engine &engine) {
    scene::local_transform value = {};
    value.position = math::vec3(
      core::random::uniform(engine, -5.0f, 5.0f),
      core::random::uniform(engine, -5.0f, 5.0f),
      core::random::uniform(engine, -5.0f, 5.0f)
    );
    const math::vec3 axis = math::safe_normalize(math::vec3(
      core::random::uniform(engine, -1.0f, 1.0f),
      core::random::uniform(engine, -1.0f, 1.0f),
      core::random::uniform(engine, -1.0f, 1.0f)
    ));
    value.rotation = glm::angleAxis(core::random::uniform(engine, -math::PI, math::PI), axis);
    value.scale = math::vec3(core::random::uniform(engine, 0.5f, 2.0f));
    return value;
}

bench_hierarchy conjure::bench::support::spawn_hierarchy(scene::scene &target, const hierarchy_shape &shape, u64 seed) {
    core::random::engine engine(seed);
    bench_hierarchy built = {};
    const core::string_id nodeName("node");

    const auto create = [&](ecs::entity parent) -> bool {
        const core::result<ecs::entity> created = target.create_entity(nodeName, parent);
        if (!created.has_value()) {
            return false;
        }
        const core::result<void> posed = target.set_local_transform(created.value(), random_transform(engine));
        if (!posed.has_value()) {
            return false;
        }
        built.all.push_back(created.value());
        if (parent == ecs::NULL_ENTITY) {
            built.roots.push_back(created.value());
        }
        return true;
    };

    for (u32 index = 0; index < shape.rootCount && built.all.size() < shape.totalEntityCount; ++index) {
        if (!create(ecs::NULL_ENTITY)) {
            return {};
        }
    }

    std::vector<ecs::entity> level = built.roots;
    for (u32 depth = 0; depth < shape.depth && built.all.size() < shape.totalEntityCount; ++depth) {
        std::vector<ecs::entity> next = {};
        for (const ecs::entity node : level) {
            for (u32 child = 0; child < shape.childrenPerNode && built.all.size() < shape.totalEntityCount; ++child) {
                if (!create(node)) {
                    return {};
                }
                next.push_back(built.all.back());
            }
        }
        level = std::move(next);
    }

    // Pad: extra leaves under the deepest level, round robin, until the total is reached.
    usize parentIndex = 0;
    while (built.all.size() < shape.totalEntityCount && !level.empty()) {
        if (!create(level[parentIndex % level.size()])) {
            return {};
        }
        ++parentIndex;
    }
    return built;
}
