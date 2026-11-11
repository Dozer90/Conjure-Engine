#include "conjure/testing/doctest.h"

#include "engine_test_support.h"

#include "conjure/core/event_bus.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_version.h"
#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_manifest.h"
#include "conjure/modules/module_registry.h"
#include "conjure/modules/module_types.h"
#include "engine/builtin_modules.h"

#include <span>
#include <string>
#include <string_view>

// builtin_modules.h (docs/design/07_modules.md 4.1, R18): one manifest-only static module per built-in
// library, all accepted by a module_manager and all LOADED by a headless application.

using namespace conjure;
using namespace conjure::engine;

namespace {

    [[nodiscard]] u32 compiled_library_count() noexcept {
        u32 count = 8;   // core, ecs, platform, rhi, assets, modules, scene, engine
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
        ++count;
#endif
#if defined(CONJURE_WITH_PHYSICS) && CONJURE_WITH_PHYSICS
        ++count;
#endif
#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO
        ++count;
#endif
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
        ++count;
#endif
        return count;
    }
}

TEST_SUITE("engine/builtin_modules") {

    TEST_CASE("builtin_modules - one descriptor per compiled library, each a valid engine manifest") {
        const std::span<const modules::static_module_desc *const> descriptors = builtin_static_modules();
        CHECK(descriptors.size() == compiled_library_count());
        for (const modules::static_module_desc *desc : descriptors) {
            REQUIRE(desc != nullptr);
            CHECK(desc->entry == nullptr);
            const core::result<modules::module_manifest> manifest =
              modules::parse_module_manifest(desc->manifestJson, desc->identifier);
            CONJURE_REQUIRE_OK(manifest);
            CHECK(manifest.value().kind == modules::module_kind::ENGINE);
            CHECK(std::string_view(manifest.value().name).starts_with("conjure."));
            CHECK(manifest.value().version == modules::host_engine_version());
            CHECK(manifest.value().binary.empty());
        }
        CHECK(std::string_view(descriptors.front()->identifier) == "conjure_core");
        CHECK(std::string_view(descriptors.back()->identifier) == "conjure_engine");
    }

    TEST_CASE("builtin_modules - a module manager registers and loads them all") {
        core::event_bus events = {};
        modules::module_manager_desc desc = {};
        desc.host = modules::module_host_kind::TEST;
        desc.allowSharedLibraries = false;
        modules::module_manager manager(desc, events);
        for (const modules::static_module_desc *builtin : builtin_static_modules()) {
            CONJURE_REQUIRE_OK(manager.add_static_module(*builtin));
        }
        CONJURE_REQUIRE_OK(manager.load_all());
        CHECK(manager.registry().load_order().size() == builtin_static_modules().size());
        const modules::module_id engine = manager.registry().find(core::string_id("conjure.engine"));
        REQUIRE(engine.is_valid());
        CHECK(manager.registry().state(engine) == modules::module_state::LOADED);
        manager.unload_all();
    }

    TEST_CASE("builtin_modules - a headless application loads the built-in modules") {
        application app(engine_test::make_desc(engine_test::inline_config()));
        CONJURE_REQUIRE_OK(app.initialise());
        REQUIRE(app.context().modules != nullptr);
        const modules::module_registry &registry = app.context().modules->registry();
        CHECK(registry.load_order().size() == builtin_static_modules().size());
        const modules::module_id scene = registry.find(core::string_id("conjure.scene"));
        REQUIRE(scene.is_valid());
        CHECK(registry.state(scene) == modules::module_state::LOADED);
        CHECK(registry.manifest(scene).kind == modules::module_kind::ENGINE);
        app.shutdown();
    }
}
