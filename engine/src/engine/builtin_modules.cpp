#include "engine/builtin_modules.h"

#include "conjure/core/build_info.h"
#include "conjure/core/platform_detect.h"

#include <array>
#include <vector>

// Each manifest names the library, carries the engine version and accepts every engine ("*"). The text
// is assembled from the build's version string so the manifests never drift from project() in CMake.

#define CONJURE_BUILTIN_MANIFEST(libraryName)                                                                \
    R"({"schema": 1, "name": ")" libraryName R"(", "version": ")" CONJURE_VERSION_STRING                     \
    R"(", "kind": "engine", "engine": "*"})"

namespace {

    CONJURE_DECLARE_STATIC_MODULE(conjure_core, CONJURE_BUILTIN_MANIFEST("conjure.core"), nullptr)
    CONJURE_DECLARE_STATIC_MODULE(conjure_ecs, CONJURE_BUILTIN_MANIFEST("conjure.ecs"), nullptr)
    CONJURE_DECLARE_STATIC_MODULE(conjure_platform, CONJURE_BUILTIN_MANIFEST("conjure.platform"), nullptr)
    CONJURE_DECLARE_STATIC_MODULE(conjure_rhi, CONJURE_BUILTIN_MANIFEST("conjure.rhi"), nullptr)
    CONJURE_DECLARE_STATIC_MODULE(conjure_assets, CONJURE_BUILTIN_MANIFEST("conjure.assets"), nullptr)
    CONJURE_DECLARE_STATIC_MODULE(conjure_modules, CONJURE_BUILTIN_MANIFEST("conjure.modules"), nullptr)
    CONJURE_DECLARE_STATIC_MODULE(conjure_scene, CONJURE_BUILTIN_MANIFEST("conjure.scene"), nullptr)
    CONJURE_DECLARE_STATIC_MODULE(conjure_engine, CONJURE_BUILTIN_MANIFEST("conjure.engine"), nullptr)
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    CONJURE_DECLARE_STATIC_MODULE(conjure_renderer, CONJURE_BUILTIN_MANIFEST("conjure.renderer"), nullptr)
#endif
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
    CONJURE_DECLARE_STATIC_MODULE(conjure_physics, CONJURE_BUILTIN_MANIFEST("conjure.physics"), nullptr)
#endif
#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
    CONJURE_DECLARE_STATIC_MODULE(conjure_audio, CONJURE_BUILTIN_MANIFEST("conjure.audio"), nullptr)
#endif
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    CONJURE_DECLARE_STATIC_MODULE(conjure_scripting, CONJURE_BUILTIN_MANIFEST("conjure.scripting"), nullptr)
#endif

    const std::array<const conjure::modules::static_module_desc *, 12> &all_descriptors() noexcept {
        static const std::array<const conjure::modules::static_module_desc *, 12> DESCRIPTORS = {
            &conjure_static_module_conjure_core(),
            &conjure_static_module_conjure_ecs(),
            &conjure_static_module_conjure_platform(),
            &conjure_static_module_conjure_rhi(),
            &conjure_static_module_conjure_assets(),
#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
            &conjure_static_module_conjure_audio(),
#else
            nullptr,
#endif
            &conjure_static_module_conjure_modules(),
            &conjure_static_module_conjure_scene(),
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
            &conjure_static_module_conjure_renderer(),
#else
            nullptr,
#endif
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
            &conjure_static_module_conjure_physics(),
#else
            nullptr,
#endif
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
            &conjure_static_module_conjure_scripting(),
#else
            nullptr,
#endif
            &conjure_static_module_conjure_engine(),
        };
        return DESCRIPTORS;
    }

    /// <summary>
    /// The descriptors above without the holes of compiled-out libraries.
    /// </summary>
    const std::vector<const conjure::modules::static_module_desc *> &enabled_descriptors() noexcept {
        static const std::vector<const conjure::modules::static_module_desc *> ENABLED = [] {
            std::vector<const conjure::modules::static_module_desc *> list = {};
            for (const conjure::modules::static_module_desc *desc : all_descriptors()) {
                if (desc != nullptr) {
                    list.push_back(desc);
                }
            }
            return list;
        }();
        return ENABLED;
    }
}

std::span<const conjure::modules::static_module_desc *const> conjure::engine::builtin_static_modules() noexcept {
    return enabled_descriptors();
}
