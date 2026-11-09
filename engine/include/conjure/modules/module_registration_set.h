#pragma once

#include "conjure/core/containers/small_vector.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>

// What a module registers (docs/design/07_modules.md D3 and 2).
//
// Registration is data: trivially copyable descriptors appended to one aligned byte buffer under a
// string_id extension point ("ecs.component", "editor.panel", ...). The descriptor types are
// defined by the consuming area and must expose `static constexpr core::string_id EXTENSION_POINT`.
// Consumers read them back with extensions_of<DescType>() after the entry returned.

namespace conjure::modules {

    class module_context;

    /// <summary>
    /// One descriptor inside module_registration_set::extensionStorage.
    /// </summary>
    struct extension_record {
        core::string_id point = {};
        u32 offset = 0;
        u32 size = 0;
        u32 alignment = 0;
    };

    /// <summary>
    /// Optional module callbacks: onActivated runs after every consumer attached, onDeactivating
    /// before any consumer detaches. Both receive the module's context.
    /// </summary>
    struct module_lifecycle {
        void (*onActivated)(module_context &context) = nullptr;
        void (*onDeactivating)(module_context &context) = nullptr;
    };

    /// <summary>
    /// Everything one module entry registered. Owned by the manager, read by consumers.
    /// </summary>
    struct module_registration_set {
        std::vector<std::string> managedAssemblies = {};
        std::vector<extension_record> extensions = {};
        std::vector<std::byte> extensionStorage = {};       // offsets aligned per record
        module_lifecycle lifecycle = {};
        std::vector<std::string> errors = {};               // non-empty => load rejected
    };

    /// <summary>
    /// Every descriptor of type DescType, in registration order. Records under the same point
    /// whose size does not match DescType are skipped.
    /// </summary>
    template <typename DescType>
    [[nodiscard]] core::small_vector<const DescType *, 8> extensions_of(const module_registration_set &set) {
        static_assert(std::is_trivially_copyable_v<DescType>, "extension descriptors must be trivially copyable");

        core::small_vector<const DescType *, 8> found = {};
        for (const extension_record &record : set.extensions) {
            if (record.point != DescType::EXTENSION_POINT || record.size != sizeof(DescType)) {
                continue;
            }
            if (static_cast<usize>(record.offset) + record.size > set.extensionStorage.size()) {
                continue;
            }
            found.push_back(reinterpret_cast<const DescType *>(set.extensionStorage.data() + record.offset));
        }
        return found;
    }
}
