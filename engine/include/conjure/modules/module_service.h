#pragma once

#include "conjure/core/api.h"
#include "conjure/core/containers/flat_map.h"
#include "conjure/core/hash.h"
#include "conjure/core/types.h"

#include <compare>
#include <string_view>

// Host services modules may ask for (docs/design/07_modules.md 4.3).
//
//   class world { public: CONJURE_MODULE_SERVICE(conjure::ecs::world) ... };
//   manager.provide_service(world);                      // host, before anything loads
//   ecs::world *world = context.try_service<ecs::world>();  // module code
//
// The table is host-owned and immutable while any module is LOADED; it may be read from job threads.

namespace conjure::modules {

    /// <summary>
    /// Identity of a service interface: the FNV-1a 64 hash of its qualified name.
    /// </summary>
    struct service_id {
        u64 hash = 0;

        [[nodiscard]] constexpr auto operator<=>(const service_id &) const noexcept = default;
    };

    [[nodiscard]] constexpr service_id make_service_id(std::string_view qualifiedName) noexcept {
        return service_id{core::hash::fnv1a64(qualifiedName)};
    }

    /// <summary>
    /// Maps service ids to host objects. Services are provided by host code only.
    /// </summary>
    class CONJURE_API service_table {
        private:
            core::flat_map<service_id, void *> mServices = {};

        public:
            service_table();
            ~service_table();

            service_table(const service_table &) = delete;
            service_table &operator=(const service_table &) = delete;

        public:
            /// <summary>
            /// Registers a host object under ServiceType::SERVICE_ID. Providing the same service
            /// twice asserts and keeps the first.
            /// </summary>
            template <typename ServiceType>
            void provide(ServiceType &service) {
                provide_raw(ServiceType::SERVICE_ID, static_cast<void *>(&service), ServiceType::SERVICE_NAME);
            }

            /// <summary>
            /// The object provided for ServiceType, or nullptr.
            /// </summary>
            template <typename ServiceType>
            [[nodiscard]] ServiceType *find() const noexcept {
                return static_cast<ServiceType *>(find_raw(ServiceType::SERVICE_ID));
            }

            /// <summary>
            /// True when a service is registered under the id.
            /// </summary>
            [[nodiscard]] bool contains(service_id id) const noexcept;

            /// <summary>
            /// Number of provided services.
            /// </summary>
            [[nodiscard]] usize size() const noexcept;

            /// <summary>
            /// Forgets every service.
            /// </summary>
            void clear() noexcept;

        private:
            void provide_raw(service_id id, void *service, std::string_view name);
            [[nodiscard]] void *find_raw(service_id id) const noexcept;
    };
}

// Declares the service identity inside an interface: `CONJURE_MODULE_SERVICE(conjure::ecs::world)`.
#define CONJURE_MODULE_SERVICE(InterfaceType)                                                               \
    static constexpr std::string_view SERVICE_NAME = #InterfaceType;                                        \
    static constexpr ::conjure::modules::service_id SERVICE_ID = ::conjure::modules::make_service_id(#InterfaceType)
