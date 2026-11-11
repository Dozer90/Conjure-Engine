#pragma once

#include "conjure/assets/asset_slot.h"
#include "conjure/assets/asset_types.h"

#include "conjure/core/api.h"
#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/type_id.h"
#include "conjure/core/types.h"

#include <atomic>
#include <string_view>
#include <type_traits>

// asset_ref<T> (non-owning, trivially copyable, one atomic load to read) and asset_handle (owning,
// type-erased, ref-counted). See docs/design/05_assets.md 2.3 and 3.1.
//
// Layout contract (R25): the uuid is the first 16 bytes of an asset_ref and the slot pointer follows.
// core::reflect serialises an ASSET_REF field wider than a uuid as that leading uuid and zeroes the
// rest on read (an unbound reference), and the ecs serializer records the same offsets for the scene
// bind pass; both would break if the field order below changed.

namespace conjure::assets {

    namespace detail {
        /// <summary>
        /// Logs the type mismatch of a typed access once per (uuid, requested type) pair.
        /// </summary>
        CONJURE_API void report_type_mismatch(const asset_id &id, asset_type_id requested, asset_type_id actual);
    }

    /// <summary>
    /// The name every asset_ref<T> field reflects under (reflect_kind_override below). The kind is
    /// type-erased on purpose: the uuid alone travels, the asset type is known to the binding pass.
    /// </summary>
    inline constexpr std::string_view ASSET_REF_TYPE_NAME = "conjure.assets.asset_ref";

    /// <summary>
    /// Non-owning reference to an asset: the uuid plus the bound slot. Components store these
    /// by value in ECS chunks; copying is a memcpy. get() is one atomic load.
    /// </summary>
    template <typename AssetType>
    class asset_ref {
        private:
            asset_id mId = {};                  // Leads the object: see the layout contract above
            const asset_slot *mSlot = nullptr;

        public:
            constexpr asset_ref() noexcept = default;

            /// <summary>
            /// An unbound reference: carries the uuid, reads nullptr until bound by the manager.
            /// </summary>
            constexpr explicit asset_ref(const asset_id &id) noexcept :
                mId(id) {}

            /// <summary>
            /// A bound reference. The slot must belong to 'id'.
            /// </summary>
            constexpr asset_ref(const asset_id &id, const asset_slot *slot) noexcept :
                mId(id),
                mSlot(slot) {}

        public:
            [[nodiscard]] constexpr const asset_id &id() const noexcept {
                return mId;
            }

            [[nodiscard]] constexpr bool is_bound() const noexcept {
                return mSlot != nullptr;
            }

            /// <summary>
            /// The bound slot, or nullptr.
            /// </summary>
            [[nodiscard]] constexpr const asset_slot *slot() const noexcept {
                return mSlot;
            }

            /// <summary>
            /// Hot path: one atomic load, no hashing, no locking. Returns nullptr unless LOADED.
            /// The pointer is valid until the end of the next frame.
            /// </summary>
            [[nodiscard]] AssetType *get() const noexcept {
                if (mSlot == nullptr) {
                    return nullptr;
                }
                return static_cast<AssetType *>(mSlot->object.load(std::memory_order_acquire));
            }

            /// <summary>
            /// UNLOADED when unbound.
            /// </summary>
            [[nodiscard]] asset_state state() const noexcept {
                if (mSlot == nullptr) {
                    return asset_state::UNLOADED;
                }
                return mSlot->state.load(std::memory_order_acquire);
            }

            /// <summary>
            /// Publish counter of the slot; 0 when unbound. Changes on every publish and unload.
            /// </summary>
            [[nodiscard]] u32 version() const noexcept {
                if (mSlot == nullptr) {
                    return 0;
                }
                return mSlot->version.load(std::memory_order_acquire);
            }

            /// <summary>
            /// Equality is by uuid: two references to the same asset compare equal whether or
            /// not they are bound.
            /// </summary>
            [[nodiscard]] constexpr bool operator==(const asset_ref &other) const noexcept {
                return mId == other.mId;
            }
    };

    /// <summary>
    /// Owning, type-erased reference: every live handle counts as one strong reference on the
    /// slot, which keeps the payload resident across collect(). Copy and move are atomics; the
    /// handle may be copied, moved and destroyed on any thread.
    /// </summary>
    class CONJURE_API asset_handle {
        private:
            asset_slot *mSlot = nullptr;

        public:
            constexpr asset_handle() noexcept = default;

            /// <summary>
            /// Acquires one strong reference on 'slot' (nullptr yields an unbound handle).
            /// </summary>
            explicit asset_handle(asset_slot *slot) noexcept;
            ~asset_handle();

            asset_handle(const asset_handle &other) noexcept;
            asset_handle &operator=(const asset_handle &other) noexcept;
            asset_handle(asset_handle &&other) noexcept;
            asset_handle &operator=(asset_handle &&other) noexcept;

        public:
            /// <summary>
            /// The uuid, or the null uuid when unbound.
            /// </summary>
            [[nodiscard]] asset_id id() const noexcept;

            /// <summary>
            /// The slot's type id (INVALID when unbound or not yet known).
            /// </summary>
            [[nodiscard]] asset_type_id type() const noexcept;

            [[nodiscard]] constexpr bool is_bound() const noexcept {
                return mSlot != nullptr;
            }

            /// <summary>
            /// The bound slot, or nullptr.
            /// </summary>
            [[nodiscard]] constexpr asset_slot *slot() const noexcept {
                return mSlot;
            }

            [[nodiscard]] asset_state state() const noexcept;

            /// <summary>
            /// True when the slot is LOADED or FAILED (also true for an unbound handle, which can
            /// never make progress).
            /// </summary>
            [[nodiscard]] bool is_terminal() const noexcept;

            [[nodiscard]] u32 version() const noexcept;

            /// <summary>
            /// A typed reference to the same slot. A type mismatch logs an ERROR once per uuid and
            /// yields an unbound reference that still carries the uuid; a slot whose type is not
            /// known yet (never bound typed, never loaded) yields an unbound reference quietly.
            /// </summary>
            template <declared_asset_type AssetType>
            [[nodiscard]] asset_ref<AssetType> ref() const noexcept {
                if (mSlot == nullptr) {
                    return asset_ref<AssetType>();
                }
                const asset_type_id actual = mSlot->type.load(std::memory_order_acquire);
                if (actual == asset_type_id::INVALID) {
                    return asset_ref<AssetType>(mSlot->id);
                }
                if (actual != asset_type_traits<AssetType>::id()) {
                    detail::report_type_mismatch(mSlot->id, asset_type_traits<AssetType>::id(), actual);
                    return asset_ref<AssetType>(mSlot->id);
                }
                return asset_ref<AssetType>(mSlot->id, mSlot);
            }

            /// <summary>
            /// The payload as AssetType, or nullptr when unbound, not LOADED or of another type.
            /// </summary>
            template <declared_asset_type AssetType>
            [[nodiscard]] AssetType *get() const noexcept {
                return ref<AssetType>().get();
            }

            /// <summary>
            /// Releases the strong reference and makes the handle unbound.
            /// </summary>
            void reset() noexcept;

            /// <summary>
            /// Two handles are equal when they refer to the same slot.
            /// </summary>
            [[nodiscard]] constexpr bool operator==(const asset_handle &other) const noexcept {
                return mSlot == other.mSlot;
            }
    };
}

// asset_ref<T> fields of reflected components serialise through the ASSET_REF kind (R25): the uuid
// in the leading 16 bytes travels as a string, the slot pointer reads back null and the scene bind
// pass binds it later. The same override serves every AssetType.
template <typename AssetType>
struct conjure::core::reflect::reflect_kind_override<conjure::assets::asset_ref<AssetType>> {
    static constexpr conjure::core::reflect::field_kind KIND = conjure::core::reflect::field_kind::ASSET_REF;
    static constexpr std::string_view NAME = conjure::assets::ASSET_REF_TYPE_NAME;
};
