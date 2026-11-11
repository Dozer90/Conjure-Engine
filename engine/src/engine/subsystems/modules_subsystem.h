#pragma once

#include "engine/engine_services.h"

#include "conjure/engine/engine_subsystem.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/modules/module_types.h"

#include <filesystem>
#include <string>
#include <vector>

// Startup step 15 (docs/design/06_engine_loop.md 3.4, R18/R19/R20/R24): the module manager with the ecs,
// assets and scripting consumers, the engine_context and application services, the manifest-only static
// modules of the built-in libraries, the project's manifests (<project>/module.json plus modules.searchPaths)
// and <engine>/plugins, then load_all(). A module named in modules.required (or the game module) that is not
// LOADED afterwards is MODULE_LOAD_FAILED; other failures warn. Module entries may add subsystems through the
// application service during this step only. apply_pending() runs at frame step 5; modules:// is mounted
// over the assets consumer's content file system.

namespace conjure::engine {

    class modules_subsystem final : public engine_subsystem {
        private:
            engine_services *mServices = nullptr;
            std::vector<std::string> mRequired = {};
            u32 mDiscovered = 0;
            u32 mLoaded = 0;
            bool mContentMounted = false;

        public:
            explicit modules_subsystem(engine_services &services);
            ~modules_subsystem() override;

        public:
            [[nodiscard]] std::string_view name() const noexcept override;
            [[nodiscard]] core::result<void> startup(engine_context &context) override;
            void begin_frame(engine_context &context) override;
            void shutdown(engine_context &context) override;

            /// <summary>
            /// Manifests registered at startup (static, project and plugin directories).
            /// </summary>
            [[nodiscard]] u32 discovered_count() const noexcept {
                return mDiscovered;
            }

            /// <summary>
            /// Modules LOADED after load_all().
            /// </summary>
            [[nodiscard]] u32 loaded_count() const noexcept {
                return mLoaded;
            }

            /// <summary>
            /// The host kind the manager runs as: EDITOR under config.editor, otherwise RUNTIME.
            /// </summary>
            [[nodiscard]] static modules::module_host_kind host_kind(const engine_config &config) noexcept;
    };
}
