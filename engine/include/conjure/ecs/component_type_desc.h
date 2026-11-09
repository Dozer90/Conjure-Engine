#pragma once

#include "conjure/ecs/component.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/ecs/system.h"

#include "conjure/core/reflect/type_info.h"
#include "conjure/core/reflect/type_registry.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#include <type_traits>

// Module extension records (docs/design/01_ecs.md 4.7, R2/R19). Plugins cannot hand the module system
// a system_desc (std::function, small_vector), so these trivially copyable mirrors travel through
// modules::module_registrar::register_extension; the engine's ecs_module_consumer turns them into
// component_registry::register_described / scheduler::add_system calls on attach.

namespace conjure::ecs {

    /// <summary>
    /// A component type exported by a module.
    /// </summary>
    struct component_type_desc {
        static constexpr core::string_id EXTENSION_POINT{"ecs.component"};

        const char *name = nullptr;                 // "com.studio.spinner"
        u32 size = 0;
        u32 alignment = 1;
        component_flags flags = component_flags::NONE;
        component_ops ops = {};
        const core::reflect::type_info *reflection = nullptr;   // deep-copied by the registry at attach
    };

    static_assert(std::is_trivially_copyable_v<component_type_desc>, "component_type_desc crosses the module ABI");

    /// <summary>
    /// The record of a C++ type named with CONJURE_COMPONENT: size, alignment, TAG/TRIVIAL flags, the
    /// lifecycle hooks and the reflection description (registered in core::reflect on first use, so
    /// this is not a constant expression).
    /// </summary>
    template <named_component ComponentType>
    [[nodiscard]] component_type_desc describe_component(component_flags flags = component_flags::NONE) noexcept {
        using plain_type = std::remove_cvref_t<ComponentType>;
        component_type_desc desc = {};
        desc.name = component_name<plain_type>::VALUE.data();
        desc.size = std::is_empty_v<plain_type> ? 0u : static_cast<u32>(sizeof(plain_type));
        desc.alignment = static_cast<u32>(alignof(plain_type));
        desc.flags = derive_component_flags<plain_type>(flags);
        desc.ops = make_component_ops<plain_type>();
        if constexpr (core::reflect::reflected_struct<plain_type>) {
            desc.reflection = core::reflect::type_info_of<plain_type>();
        }
        return desc;
    }

    /// <summary>
    /// A system exported by a module. Component and system names are resolved by the consumer.
    /// </summary>
    struct system_extension_desc {
        static constexpr core::string_id EXTENSION_POINT{"ecs.system"};

        const char *name = nullptr;
        system_phase phase = system_phase::UPDATE;
        system_flags flags = system_flags::NONE;
        const core::string_id *reads = nullptr;         // component names
        u32 readCount = 0;
        const core::string_id *writes = nullptr;
        u32 writeCount = 0;
        const core::string_id *after = nullptr;         // system labels
        u32 afterCount = 0;
        const core::string_id *before = nullptr;
        u32 beforeCount = 0;
        void (*run)(system_context &context) = nullptr;
    };

    static_assert(std::is_trivially_copyable_v<system_extension_desc>, "system_extension_desc crosses the module ABI");
}
