#pragma once

#include "conjure/assets/asset_loader.h"
#include "conjure/assets/asset_types.h"

#include "conjure/core/api.h"
#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Registration of asset types (docs/design/05_assets.md 2.4, 4.2). The assets module knows no
// concrete type: renderer, audio, scene, physics, scripting and plugins register theirs here.

namespace conjure::assets {

    /// <summary>
    /// One registered type: identity, version, flags and the loader that produces its payloads.
    /// </summary>
    struct asset_type_desc {
        asset_type_id id = asset_type_id::INVALID;
        std::string name = {};                              // "conjure.renderer.texture"
        u32 version = 1;                                    // bump to invalidate every cooked blob of the type
        asset_type_flags flags = asset_type_flags::NONE;
        std::unique_ptr<asset_loader> loader = nullptr;
    };

    /// <summary>
    /// Builds a description from the loader's type and the traits name; the id comes from the name.
    /// </summary>
    template <declared_asset_type AssetType>
    [[nodiscard]] asset_type_desc make_asset_type_desc(
      std::unique_ptr<asset_loader> loader,
      u32 version = 1,
      asset_type_flags flags = asset_type_flags::NONE
    ) {
        asset_type_desc desc = {};
        desc.id = asset_type_traits<AssetType>::id();
        desc.name = std::string(asset_type_traits<AssetType>::NAME);
        desc.version = version;
        desc.flags = flags;
        desc.loader = std::move(loader);
        return desc;
    }

    /// <summary>
    /// Callbacks the owning asset_manager installs so the registry can refuse to unregister a
    /// type that is in use and can prepare pinned fallbacks at registration.
    /// </summary>
    struct asset_type_registry_hooks {
        std::function<bool(asset_type_id)> isInUse = {};                       // any slot loaded or in flight
        std::function<void(const asset_type_desc &)> onRegistered = {};
        std::function<void(asset_type_id)> onUnregistered = {};                // after the description is gone
    };

    /// <summary>
    /// The set of registered asset types. Registration and removal happen on the main thread
    /// before systems run (or at plugin attach/detach); lookups are safe from any thread between
    /// those calls. Every description lives in its own heap block, so a pointer returned by
    /// find() stays valid until that type is unregistered; unregistering one type never moves
    /// another description.
    /// </summary>
    class CONJURE_API asset_type_registry {
        private:
            std::vector<std::unique_ptr<asset_type_desc>> mTypes = {};   // registration order, never moved
            std::vector<const asset_type_desc *> mView = {};             // the same descriptions, for types()
            core::flat_hash_map<asset_type_id, usize> mIndexById = {};
            asset_type_registry_hooks mHooks = {};

        public:
            asset_type_registry();
            ~asset_type_registry();

            asset_type_registry(const asset_type_registry &) = delete;
            asset_type_registry &operator=(const asset_type_registry &) = delete;

        public:
            /// <summary>
            /// Main thread, before systems run. Fails with TYPE_DUPLICATE if the id is taken, with
            /// INVALID_ARGUMENT for an INVALID id, a name whose hash is not the id, or a missing
            /// loader whose type differs from the id, and with OUT_OF_RANGE past MAX_ASSET_TYPES.
            /// </summary>
            [[nodiscard]] core::result<void> register_type(asset_type_desc desc);

            /// <summary>
            /// Plugin unload. TYPE_UNKNOWN for an unregistered id; INVALID_STATE while any slot of
            /// the type is loaded or in flight. Only pointers to the removed description die.
            /// </summary>
            [[nodiscard]] core::result<void> unregister_type(asset_type_id id);

            [[nodiscard]] const asset_type_desc *find(asset_type_id id) const noexcept;
            [[nodiscard]] const asset_type_desc *find(std::string_view name) const noexcept;
            [[nodiscard]] const asset_loader *find_loader(asset_type_id id) const noexcept;

            /// <summary>
            /// Every registered type in registration order. The span is valid until the next
            /// registration or removal; the descriptions it points at follow the find() rule.
            /// </summary>
            [[nodiscard]] std::span<const asset_type_desc *const> types() const noexcept;

            [[nodiscard]] usize size() const noexcept {
                return mTypes.size();
            }

            /// <summary>
            /// Installs the manager's callbacks. Engine plumbing, not for general use.
            /// </summary>
            void set_hooks(asset_type_registry_hooks hooks);

        private:
            void rebuild_index();
    };
}
