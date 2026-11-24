#include "engine/subsystems/modules_subsystem.h"

#include "engine/builtin_modules.h"
#include "engine/module_consumers/assets_module_consumer.h"
#include "engine/module_consumers/ecs_module_consumer.h"
#include "engine/module_consumers/scripting_module_consumer.h"

#include "conjure/engine/application.h"
#include "conjure/engine/engine_config.h"
#include "conjure/engine/engine_error.h"
#include "conjure/engine/log_category.h"
#include "conjure/engine/project.h"

#include "conjure/core/log.h"
#include "conjure/modules/module_error.h"
#include "conjure/modules/module_manifest.h"

#include <algorithm>
#include <format>
#include <mutex>
#include <system_error>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::engine;

namespace {

    // The module_manager_desc::isInsideFrame hook carries no user data, so the applications of this process
    // register here for as long as their modules subsystem runs. The hook answers for all of them: a load is
    // refused while any registered application is inside a frame, and one application's shutdown never
    // silences another's answer (two applications in one process: tests, an editor hosting a runtime).
    std::mutex sInsideFrameOwnersMutex = {};
    std::vector<const application *> sInsideFrameOwners = {};

    void register_inside_frame_owner(const application *owner) {
        if (owner == nullptr) {
            return;
        }
        const std::scoped_lock lock(sInsideFrameOwnersMutex);
        if (std::find(sInsideFrameOwners.begin(), sInsideFrameOwners.end(), owner) == sInsideFrameOwners.end()) {
            sInsideFrameOwners.push_back(owner);
        }
    }

    void unregister_inside_frame_owner(const application *owner) {
        const std::scoped_lock lock(sInsideFrameOwnersMutex);
        std::erase(sInsideFrameOwners, owner);
    }

    /// <summary>
    /// The module_manager_desc::isInsideFrame hook: module loading is refused while a frame runs (R19).
    /// </summary>
    [[nodiscard]] bool is_inside_frame() {
        const std::scoped_lock lock(sInsideFrameOwnersMutex);
        return std::any_of(sInsideFrameOwners.begin(), sInsideFrameOwners.end(), [](const application *owner) {
            return owner->is_inside_frame();
        });
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

modules_subsystem::modules_subsystem(engine_services &services) :
    mServices(&services) {}

modules_subsystem::~modules_subsystem() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

std::string_view modules_subsystem::name() const noexcept {
    return "modules";
}

core::result<void> modules_subsystem::startup(engine_context &context) {
    const engine_config &config = *context.config;
    mRequired.clear();
    mDiscovered = 0;
    mLoaded = 0;

    modules::module_manager_desc desc = {};
    desc.host = host_kind(config);
    // The editor loads copies so module.build can relink a binary Windows keeps locked while loaded (07 D9);
    // the runtime loads in place.
    if (desc.host == modules::module_host_kind::EDITOR && context.project != nullptr) {
        desc.shadowCopyBinaries = true;
        desc.shadowCopyDirectory = context.project->root_directory() / ".conjure" / "module_cache";
    }
    desc.allowSharedLibraries = !config.headless || context.project != nullptr;
    desc.isInsideFrame = &is_inside_frame;
    register_inside_frame_owner(context.app);
    mServices->modules = std::make_unique<modules::module_manager>(desc, mServices->events);
    modules::module_manager &manager = *mServices->modules;

    // Consumers in attach order (ecs first so the components exist when assets or scripting look them up).
    mServices->ecsConsumer =
      std::make_unique<ecs_module_consumer>(*mServices->components, *mServices->world, *mServices->scheduler);
    mServices->assetsConsumer = std::make_unique<assets_module_consumer>(
      mServices->assetManager->types(),
      mServices->importers,
      std::make_shared<module_content_file_system>()
    );
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    mServices->scriptingConsumer = std::make_unique<scripting_module_consumer>(mServices->scripting.get());
#else
    mServices->scriptingConsumer = std::make_unique<scripting_module_consumer>();
#endif
    manager.add_consumer(*mServices->ecsConsumer);
    manager.add_consumer(*mServices->assetsConsumer);
    manager.add_consumer(*mServices->scriptingConsumer);

    // modules://<name>/ (R24): one backend whose first path segment is the module name.
    if (mServices->mounts.find_mount("modules") == nullptr) {
        // The consumer owns the backend; the mount table gets a non-owning alias.
        core::vfs::file_system *content = &mServices->assetsConsumer->content();
        std::shared_ptr<core::vfs::file_system> alias(content, [](core::vfs::file_system *) {});
        const core::result<void> mounted = mServices->mounts.mount("modules", std::move(alias));
        mContentMounted = mounted.has_value();
        if (!mContentMounted) {
            CONJURE_LOG_WARN(
              LOG_ENGINE,
              "modules: modules:// could not be mounted: {}",
              mounted.get_error().to_string()
            );
        }
    }

    // Services (R19): modules reach the engine through these two; everything else hangs off engine_context.
    manager.provide_service(context);
    if (context.app != nullptr) {
        manager.provide_service(*context.app);
    }

    // Built-in libraries as manifest-only static modules (R18).
    for (const modules::static_module_desc *builtin : builtin_static_modules()) {
        const core::result<modules::module_id> added = manager.add_static_module(*builtin);
        if (!added.has_value()) {
            CONJURE_LOG_WARN(
              LOG_ENGINE,
              "modules: built-in '{}' not registered: {}",
              builtin->identifier,
              added.get_error().to_string()
            );
            continue;
        }
        ++mDiscovered;
    }

    // The project's manifests: the game module and the search paths.
    mRequired = config.modules.required;
    if (context.project != nullptr) {
        for (const project_module_entry &entry : context.project->module_manifests()) {
            const core::result<modules::module_id> added = manager.add_manifest(entry.manifestPath);
            if (!added.has_value()) {
                if (entry.required) {
                    return core::make_error(
                      engine_error::MODULE_LOAD_FAILED,
                      std::format(
                        "required module manifest '{}' was rejected: {}",
                        entry.manifestPath.generic_string(),
                        added.get_error().to_string()
                      )
                    );
                }
                CONJURE_LOG_WARN(
                  LOG_ENGINE,
                  "modules: manifest '{}' rejected: {}",
                  entry.manifestPath.generic_string(),
                  added.get_error().to_string()
                );
                continue;
            }
            ++mDiscovered;
            if (entry.required) {
                const std::string &moduleName = manager.registry().manifest(added.value()).name;
                if (std::find(mRequired.begin(), mRequired.end(), moduleName) == mRequired.end()) {
                    mRequired.push_back(moduleName);
                }
            }
        }
    } else {
        for (const std::filesystem::path &searchPath : config.modules.searchPaths) {
            if (!searchPath.is_absolute()) {
                continue;
            }
            const core::result<u32> found = manager.discover(searchPath);
            if (found.has_value()) {
                mDiscovered += found.value();
            } else if (!found.get_error().is(modules::module_error::MANIFEST_NOT_FOUND)) {
                CONJURE_LOG_WARN(
                  LOG_ENGINE,
                  "modules: discovering '{}' failed: {}",
                  searchPath.generic_string(),
                  found.get_error().to_string()
                );
            }
        }
    }

    // Engine plugins next to the executable.
    if (config.modules.enginePlugins && !mServices->engineDirectory.empty()) {
        const std::filesystem::path plugins = mServices->engineDirectory / "plugins";
        std::error_code ignored = {};
        if (std::filesystem::is_directory(plugins, ignored)) {
            const core::result<u32> found = manager.discover(plugins);
            if (found.has_value()) {
                mDiscovered += found.value();
            } else if (!found.get_error().is(modules::module_error::MANIFEST_NOT_FOUND)) {
                CONJURE_LOG_WARN(
                  LOG_ENGINE,
                  "modules: discovering engine plugins failed: {}",
                  found.get_error().to_string()
                );
            }
        }
    }

    const core::result<void> loaded = manager.load_all();
    if (!loaded.has_value()) {
        CONJURE_LOG_WARN(LOG_ENGINE, "modules: load_all reported: {}", loaded.get_error().to_string());
    }
    const modules::module_registry &registry = manager.registry();
    mLoaded = static_cast<u32>(registry.load_order().size());
    for (const std::string &required : mRequired) {
        const modules::module_id id = registry.find(core::string_id(std::string_view(required)));
        if (!id.is_valid() || registry.state(id) != modules::module_state::LOADED) {
            const std::string reason = id.is_valid() ? registry.last_error(id).to_string() : "not registered";
            return core::make_error(
              engine_error::MODULE_LOAD_FAILED,
              std::format("required module '{}' did not load: {}", required, reason)
            );
        }
    }
    for (const modules::module_id id : registry.all()) {
        if (registry.state(id) == modules::module_state::FAILED) {
            CONJURE_LOG_WARN(
              LOG_ENGINE,
              "modules: optional module '{}' failed to load: {}",
              registry.manifest(id).name,
              registry.last_error(id).to_string()
            );
        }
    }

    context.modules = &manager;
    CONJURE_LOG_INFO(LOG_ENGINE, "modules: {} registered, {} loaded", mDiscovered, mLoaded);
    return core::result<void>::success();
}

void modules_subsystem::begin_frame(engine_context &context) {
    CONJURE_UNUSED(context);
    if (mServices->modules != nullptr) {
        mServices->modules->apply_pending();
    }
}

void modules_subsystem::shutdown(engine_context &context) {
    context.modules = nullptr;
    if (mServices->modules != nullptr) {
        mServices->modules->unload_all();
    }
    if (mContentMounted) {
        CONJURE_UNUSED(mServices->mounts.unmount("modules"));
        mContentMounted = false;
    }
    mServices->modules.reset();
    mServices->scriptingConsumer.reset();
    mServices->assetsConsumer.reset();
    mServices->ecsConsumer.reset();
    unregister_inside_frame_owner(context.app);
    mRequired.clear();
}

modules::module_host_kind modules_subsystem::host_kind(const engine_config &config) noexcept {
    return config.editor ? modules::module_host_kind::EDITOR : modules::module_host_kind::RUNTIME;
}
