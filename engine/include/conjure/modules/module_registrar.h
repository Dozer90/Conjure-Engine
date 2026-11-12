#pragma once

#include "conjure/modules/module_registration_set.h"

#include "conjure/core/api.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <string_view>
#include <type_traits>

// The object a module entry registers through (docs/design/07_modules.md 4.2).
//
//   registrar.register_extension(ecs::describe_component<spinner>());
//   registrar.register_managed_assembly("managed/Game.dll");
//   registrar.set_lifecycle({ &on_activated, &on_deactivating });
//   registrar.fail("needs the renderer");          // the load is rejected after the entry returns

namespace conjure::modules {

    class CONJURE_API module_registrar {
        private:
            module_registration_set *mSet = nullptr;

        public:
            explicit module_registrar(module_registration_set &set) noexcept;
            ~module_registrar();

            module_registrar(const module_registrar &) = delete;
            module_registrar &operator=(const module_registrar &) = delete;

        public:
            /// <summary>
            /// Stores a trivially copyable descriptor under DescType::EXTENSION_POINT. The consuming
            /// area defines the descriptor; strings inside may point at module literals, consumers
            /// copy what they keep.
            /// </summary>
            template <typename DescType>
            void register_extension(const DescType &desc) {
                static_assert(std::is_trivially_copyable_v<DescType>, "descriptors must be trivially copyable");
                static_assert(alignof(DescType) <= alignof(std::max_align_t), "over-aligned descriptors unsupported");
                append_extension(DescType::EXTENSION_POINT, &desc, sizeof(DescType), alignof(DescType));
            }

            /// <summary>
            /// Adds a managed assembly path relative to the module root. Duplicates are ignored. A
            /// path that leaves the root (a ".." segment, a leading separator, a drive) asserts and
            /// is recorded as a failure, like fail(), so the load is rejected.
            /// </summary>
            void register_managed_assembly(std::string_view relativePath);

            /// <summary>
            /// Installs the lifecycle callbacks; the last call wins.
            /// </summary>
            void set_lifecycle(const module_lifecycle &lifecycle) noexcept;

            /// <summary>
            /// Records a reason the module cannot load. Reasons accumulate; one is enough to reject
            /// the load after the entry returns.
            /// </summary>
            void fail(std::string_view reason);

            /// <summary>
            /// The set being filled.
            /// </summary>
            [[nodiscard]] const module_registration_set &registrations() const noexcept;

        private:
            void append_extension(core::string_id point, const void *data, usize size, usize alignment);
    };
}
