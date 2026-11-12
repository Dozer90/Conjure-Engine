#pragma once

#include "conjure/ecs/component.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/serialization.h"

#include "conjure/core/api.h"
#include "conjure/core/event.h"
#include "conjure/core/reflect/type_id.h"
#include "conjure/core/reflect/type_info.h"
#include "conjure/core/reflect/type_registry.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <span>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

// The component registry (docs/design/01_ecs.md 2.2, 4.2).
//
// One explicit registry object; worlds hold a non-owning reference to it and must be destroyed before
// it. Ids are assigned per name in registration order and never reused, so re-registering a name
// yields the same id (or NAME_CONFLICT when the layout differs). The constructor registers
// unresolved_components as id 0.

namespace conjure::ecs {

    class world;
    struct component_type_desc;

    /// <summary>
    /// One field of a runtime-described (C#) component.
    /// </summary>
    struct dynamic_field_desc {
        core::string_id name = {};
        core::reflect::type_id type = {};       // a primitive, math type, or type_id_of<entity>()
        u32 offset = 0;
    };

    /// <summary>
    /// A runtime-described component type. DYNAMIC | TRIVIAL are forced on; the alignment must be a
    /// power of two in [1, 16].
    /// </summary>
    struct dynamic_component_desc {
        core::string_id name = {};
        u32 size = 0;
        u32 alignment = 1;
        std::span<const dynamic_field_desc> fields = {};
        std::span<const std::byte> defaultValue = {};
        component_flags flags = component_flags::NONE;
    };

    /// <summary>
    /// Owns the component_info of every registered type. Thread-safe: every method takes the registry
    /// mutex except info() and count(), which read the pre-reserved, append-only vector guarded by the
    /// atomic count. Non-copyable and non-movable (worlds and caches hold its address).
    /// </summary>
    class CONJURE_API component_registry {
        private:
            friend class world;

            u32 mSerial = 0;                                                // process-unique, from an atomic counter
            std::vector<component_info> mTypes = {};                        // reserved to MAX_COMPONENTS, append-only
            std::atomic<u32> mCount = 0;
            std::unordered_map<core::string_id, component_id> mByName = {};
            std::vector<std::unique_ptr<core::reflect::type_info>> mDynamicTypes = {};
            std::vector<std::unique_ptr<core::reflect::enum_info>> mDynamicEnums = {};   // nested enums of copies
            mutable std::vector<world *> mWorlds = {};     // bookkeeping, not type state: worlds hold a const &
            mutable std::mutex mMutex = {};
            core::event<const component_info &> mOnRegistered = {};

        public:
            component_registry();
            ~component_registry();

            component_registry(const component_registry &) = delete;
            component_registry &operator=(const component_registry &) = delete;
            component_registry(component_registry &&) = delete;
            component_registry &operator=(component_registry &&) = delete;

        public:
            /// <summary>
            /// Registers a C++ type named with CONJURE_COMPONENT. Size, alignment, TAG/TRIVIAL flags and
            /// the lifecycle hooks come from the type, the reflection description from
            /// core::reflect::type_info_of (required unless the type is TRANSIENT or a tag), the entity
            /// and asset-reference field offsets and the default serializer from the description.
            /// Re-registering a name with the same layout and flags returns the existing id;
            /// NAME_CONFLICT otherwise. COMPONENT_LIMIT_REACHED, COMPONENT_TOO_LARGE, BAD_ALIGNMENT and
            /// UNSUPPORTED_FIELD as documented in 01_ecs.md 6.
            /// </summary>
            template <named_component ComponentType>
            [[nodiscard]] core::result<component_id> register_component(component_flags flags = component_flags::NONE) {
                using plain_type = std::remove_cvref_t<ComponentType>;
                static_assert(!std::is_polymorphic_v<plain_type>, "components must be plain data types");

                component_info info = {};
                info.name = core::string_id(component_name<plain_type>::VALUE);
                info.flags = derive_component_flags<plain_type>(flags);
                info.size = std::is_empty_v<plain_type> ? 0u : static_cast<u32>(sizeof(plain_type));
                info.alignment = static_cast<u32>(alignof(plain_type));
                info.ops = make_component_ops<plain_type>();
                info.defaultValue = default_component_bytes<plain_type>();
                if constexpr (core::reflect::reflected_struct<plain_type>) {
                    info.reflection = core::reflect::type_info_of<plain_type>();
                    if (!has_flag(info.flags, component_flags::TRANSIENT)) {
                        info.serializer = make_default_serializer<plain_type>();
                    }
                }
                return register_info(std::move(info));
            }

            /// <summary>
            /// Registers a type from a module extension record (component_type_desc.h). The reflection
            /// description is deep-copied into the registry, nested struct and enum descriptions
            /// included, so the module may unload. The record's serializer hooks are copied as given:
            /// null for a reflected type (describe_component leaves them null and the registry's
            /// reflection walker serialises the deep copy, so nothing points into the module), while a
            /// type without reflection must supply writeJson and readJson (UNSUPPORTED_FIELD otherwise).
            /// A record whose size and TAG flag disagree (size 0 without TAG, or TAG with a non-zero
            /// size) is INVALID_ARGUMENT.
            /// </summary>
            [[nodiscard]] core::result<component_id> register_described(const component_type_desc &desc);

            /// <summary>
            /// Registers a runtime-described type. Builds a reflection description from the fields,
            /// forces DYNAMIC | TRIVIAL and accepts an alignment in {1, 2, 4, 8, 16}. Same conflict
            /// and limit rules as register_component; for a dynamic type the fields are part of the
            /// layout, so re-registering a known name whose fields differ (order, name, type or offset)
            /// is NAME_CONFLICT even with the same size and alignment. redefine_dynamic replaces the
            /// description of such a type.
            /// </summary>
            [[nodiscard]] core::result<component_id> register_dynamic(const dynamic_component_desc &desc);

            /// <summary>
            /// Replaces the layout of a dynamic type while keeping its id (hot reload). COMPONENT_IN_USE
            /// while any live world holds an instance, UNKNOWN_COMPONENT for an id that is not dynamic,
            /// NAME_CONFLICT when the new description carries a different name.
            /// </summary>
            [[nodiscard]] core::result<void> redefine_dynamic(component_id id, const dynamic_component_desc &desc);

            /// <summary>
            /// The id of a C++ type, INVALID when it is not registered here. Cached per type and
            /// registry serial, so the lookup costs one atomic load after the first call.
            /// </summary>
            template <named_component ComponentType>
            [[nodiscard]] component_id id_of() const noexcept {
                using plain_type = std::remove_cvref_t<ComponentType>;
                std::atomic<u64> &cache = component_type_cache<plain_type>::slot();
                const u64 packed = cache.load(std::memory_order_acquire);
                if (static_cast<u32>(packed >> 16) == mSerial) {
                    return component_id{static_cast<u16>(packed & 0xFFFFu)};
                }

                const component_id found = find(core::string_id(component_name<plain_type>::VALUE));
                if (found.is_valid()) {
                    cache.store((static_cast<u64>(mSerial) << 16) | found.value, std::memory_order_release);
                }
                return found;
            }

            /// <summary>
            /// The id registered under 'name', or INVALID.
            /// </summary>
            [[nodiscard]] component_id find(core::string_id name) const noexcept;

            /// <summary>
            /// The description of a registered id. Asserts and returns the unresolved_components
            /// description for an id that is not registered. Lock-free.
            /// </summary>
            [[nodiscard]] const component_info &info(component_id id) const noexcept;

            /// <summary>
            /// True when 'id' names a registered type. Lock-free.
            /// </summary>
            [[nodiscard]] bool is_registered(component_id id) const noexcept {
                return id.is_valid() && id.value < mCount.load(std::memory_order_acquire);
            }

            /// <summary>
            /// Number of registered types, unresolved_components included. Lock-free.
            /// </summary>
            [[nodiscard]] u32 count() const noexcept {
                return mCount.load(std::memory_order_acquire);
            }

            /// <summary>
            /// Every registered type in id order (editor and test enumeration, R32). The view stays
            /// valid for the registry's lifetime; it grows with later registrations.
            /// </summary>
            [[nodiscard]] std::span<const component_info> types() const noexcept;

            /// <summary>
            /// The process-unique serial of this registry (never 0).
            /// </summary>
            [[nodiscard]] u32 serial() const noexcept {
                return mSerial;
            }

            /// <summary>
            /// True when any live world created on this registry holds an instance of 'id'.
            /// </summary>
            [[nodiscard]] bool has_instances(component_id id) const;

            /// <summary>
            /// Fires once per new registration, on the registering thread, after the registry lock
            /// is released.
            /// </summary>
            [[nodiscard]] core::event<const component_info &> &on_registered() noexcept {
                return mOnRegistered;
            }

        private:
            [[nodiscard]] core::result<component_id> register_info(component_info info);
            [[nodiscard]] core::result<void> validate_layout(const component_info &info) const;
            [[nodiscard]] core::result<void> collect_field_offsets(component_info &info) const;
            [[nodiscard]] core::result<std::unique_ptr<core::reflect::type_info>> build_dynamic_type(
              const dynamic_component_desc &desc
            ) const;
            void attach_world(world *target) const;
            void detach_world(world *target) const;
    };
}
