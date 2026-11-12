#pragma once

#include "conjure/core/platform_detect.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/modules/module_consumer.h"
#include "conjure/modules/module_registration_set.h"
#include "conjure/modules/module_registry.h"
#include "conjure/modules/module_types.h"

#include <filesystem>
#include <string>
#include <vector>

#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
    #include "conjure/scripting/script_runtime.h"
#endif

// The scripting module consumer (docs/design/07_modules.md 9 "Needs from scripting", R36): records the
// managed assemblies a module brings (its manifest list plus module_registrar::register_managed_assembly)
// and takes part in MANAGED_ONLY reloads by asking the runtime to reload the game assembly, which preserves
// script fields itself (the reload_context stays unused). script_runtime has no entry point for appending
// a module assembly to the single game load context yet, so the paths are recorded and logged; engine/src/
// engine/README.md lists the request. Compiles without the scripting module (then it only records).

namespace conjure::engine {

    class scripting_module_consumer final : public modules::module_consumer {
        private:
            struct module_record {
                modules::module_id id = {};
                std::vector<std::filesystem::path> assemblies = {};
            };

#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
            scripting::script_runtime *mRuntime = nullptr;
#endif
            core::string_id mName = {};
            std::vector<module_record> mRecords = {};
            u32 mManagedReloads = 0;

        public:
#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
            /// <summary>
            /// 'runtime' may be null (scripting disabled): the consumer then only records assemblies.
            /// </summary>
            explicit scripting_module_consumer(scripting::script_runtime *runtime);
#else
            scripting_module_consumer();
#endif
            ~scripting_module_consumer() override;

        public:
            /// <summary>
            /// "scripting".
            /// </summary>
            [[nodiscard]] core::string_id name() const noexcept override;

            /// <summary>
            /// Always accepts: assembly paths are checked when they load.
            /// </summary>
            [[nodiscard]] core::result<void> validate(
              modules::module_id id,
              const modules::module_registration_set &set,
              const modules::module_registry &registry
            ) override;

            /// <summary>
            /// Records the module's assemblies (manifest and registrar lists, relative to the module root).
            /// </summary>
            void attach(
              modules::module_id id,
              modules::module_context &context,
              const modules::module_registration_set &set
            ) override;

            [[nodiscard]] core::result<void> can_detach(modules::module_id id, modules::unload_policy policy) override;

            /// <summary>
            /// Forgets the assemblies; during a reload the game assembly is reloaded through the runtime.
            /// </summary>
            void detach(modules::module_id id, modules::reload_context *reload) override;

            void restore(modules::module_id id, const modules::reload_context &reload) override;

            [[nodiscard]] u32 live_use_count(modules::module_id id) const override;

            /// <summary>
            /// True: the consumer takes part in reload_scope::MANAGED_ONLY (R36).
            /// </summary>
            [[nodiscard]] bool participates_in_managed_reload() const noexcept override;

            /// <summary>
            /// The assemblies recorded for a LOADED module (empty for an unknown id).
            /// </summary>
            [[nodiscard]] std::vector<std::filesystem::path> assemblies_of(modules::module_id id) const;

            /// <summary>
            /// Game-assembly reloads this consumer requested.
            /// </summary>
            [[nodiscard]] u32 managed_reload_count() const noexcept {
                return mManagedReloads;
            }

#if defined(CONJURE_WITH_SCRIPTING) && CONJURE_WITH_SCRIPTING
            /// <summary>
            /// Replaces the runtime; null (scripting disabled or shut down) makes managed reloads a no-op. The
            /// scripting subsystem clears it before it destroys the runtime.
            /// </summary>
            void set_runtime(scripting::script_runtime *runtime) noexcept {
                mRuntime = runtime;
            }
#endif

        private:
            [[nodiscard]] module_record *find_record(modules::module_id id) noexcept;
    };
}
