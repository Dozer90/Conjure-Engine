#pragma once

#include "conjure/modules/module_error.h"
#include "conjure/modules/module_manifest.h"
#include "conjure/modules/module_registrar.h"
#include "conjure/modules/module_service.h"
#include "conjure/modules/module_types.h"
#include "conjure/modules/module_version.h"

#include "conjure/core/api.h"
#include "conjure/core/event.h"
#include "conjure/core/result.h"

#include <filesystem>
#include <vector>

// What a module sees of the host (docs/design/07_modules.md 4.3). One context exists per LOADED
// module; it is valid during the entry and in both lifecycle callbacks and is destroyed before the
// binary is unmapped, taking every retained subscription with it.

namespace conjure::modules {

    class CONJURE_API module_context {
        private:
            module_id mId = {};
            module_host_kind mHost = module_host_kind::RUNTIME;
            semantic_version mEngineVersion = {};
            const module_manifest *mManifest = nullptr;
            std::filesystem::path mRootDirectory = {};
            module_registrar *mRegistrar = nullptr;             // bound while the entry runs
            const service_table *mServices = nullptr;
            std::vector<core::event_subscription> mSubscriptions = {};

        public:
            module_context(
              module_id id,
              module_host_kind host,
              semantic_version engineVersion,
              const module_manifest &manifest,
              std::filesystem::path rootDirectory,
              const service_table &services
            );
            ~module_context();                                  // releases retained subscriptions

            module_context(const module_context &) = delete;
            module_context &operator=(const module_context &) = delete;

        public:
            [[nodiscard]] module_id id() const noexcept;
            [[nodiscard]] module_host_kind host() const noexcept;
            [[nodiscard]] semantic_version engine_version() const noexcept;
            [[nodiscard]] const module_manifest &manifest() const noexcept;
            [[nodiscard]] const std::filesystem::path &root_directory() const noexcept;

            /// <summary>
            /// The registrar, available only while the entry runs; asserts (and returns a registrar
            /// over a scratch set) outside of it.
            /// </summary>
            [[nodiscard]] module_registrar &registrar() noexcept;

            /// <summary>
            /// True while the entry runs and registrar() may be called.
            /// </summary>
            [[nodiscard]] bool is_entry_running() const noexcept;

            /// <summary>
            /// The host service, or nullptr when the host did not provide it.
            /// </summary>
            template <typename ServiceType>
            [[nodiscard]] ServiceType *try_service() const noexcept {
                return mServices->find<ServiceType>();
            }

            /// <summary>
            /// The host service, or SERVICE_MISSING naming the interface.
            /// </summary>
            template <typename ServiceType>
            [[nodiscard]] core::result<ServiceType *> require_service() const {
                ServiceType *service = mServices->find<ServiceType>();
                if (service == nullptr) {
                    return make_module_error(
                      module_error::SERVICE_MISSING,
                      "module '{}' requires service '{}', which the host does not provide",
                      mManifest->name,
                      ServiceType::SERVICE_NAME
                    );
                }
                return service;
            }

            /// <summary>
            /// Owns an event subscription so its callback (module code) is released before the
            /// binary is unmapped. Module code never hands the host a callback any other way except
            /// through descriptors.
            /// </summary>
            void retain(core::event_subscription &&subscription);

            /// <summary>
            /// Number of retained subscriptions.
            /// </summary>
            [[nodiscard]] usize retained_subscription_count() const noexcept;

            /// <summary>
            /// Binds the registrar for the duration of the entry. Called by the module manager.
            /// </summary>
            void begin_entry(module_registrar &registrar) noexcept;

            /// <summary>
            /// Unbinds the registrar once the entry returned. Called by the module manager.
            /// </summary>
            void end_entry() noexcept;
    };
}
